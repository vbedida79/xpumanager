/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file psc_mgmt.cpp
 */

#include "psc_mgmt.h"

#include <igsc_lib.h>

#include <chrono>
#include <sstream>
#include <iomanip>

#include "api/device_model.h"
#include "firmware_manager.h"
#include "infrastructure/logger.h"
#include "load_igsc.h"
#include "igsc_err_msg.h"
#include "api/psc.h"
#include "psc_txcal_blob.h"
#include "handle_lock.h"

namespace xpum {

static LibIgsc libIgsc;

static std::string print_psc_version(const struct igsc_psc_version *psc_version) {
    std::stringstream ss;
    ss << "0x" << std::setfill('0') << std::setw(4) << std::hex << psc_version->cfg_version;
    ss << ".";
    ss << "0x" << std::setfill('0') << std::setw(4) << std::hex << psc_version->date;
    return ss.str();
}

static void progress_percentage_func(uint32_t done, uint32_t total, void* ctx) {
    uint32_t percent = (done * 100) / total;

    if (percent > 100) {
        percent = 100;
    }

    // store percent 
    PscMgmt* p = (PscMgmt*) ctx;
    p->percent.store(percent);
}

xpum_result_t PscMgmt::flashPscFw(FlashPscFwParam &param) {
    auto deviceModel = pDevice->getDeviceModel();
    if (deviceModel != XPUM_DEVICE_MODEL_PVC) {
        pDevice->unlock();
        return XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC;
    }
    if (!libIgsc.ok()) {
        pDevice->unlock();
        return XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC_IGSC;
    }
    std::lock_guard<std::mutex> lck(mtx);
    std::string filePath = param.filePath;
    if (task.valid()) {
        // task already running
        pDevice->unlock();
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    } else {
        auto buffer = readImageContent(filePath.c_str());

        // validate psc image
        std::vector<uint8_t> _buffer(buffer.begin(), buffer.end());
        auto _res = getPSCData(_buffer);
        if (_res.size() == 0) {
            return XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
        }

        if (!param.force) {
            auto meiDeviceName = getMeiDeviceNameFromPath(devicePath);
            auto txCalBlobBuffer = getTxCalBlobByMeiDevice(meiDeviceName);
            if (txCalBlobBuffer.size() > 0) {
                XPUM_LOG_INFO("Xe Link Calibration Blob found");
                buffer.insert(buffer.end(), txCalBlobBuffer.begin(), txCalBlobBuffer.end());
            }
        }

        // init fw-data update progress
        percent.store(0);

        task = std::async(std::launch::async, [this, buffer, filePath] {
            XPUM_LOG_INFO("Start update GSC_PSCBIN on device {}", devicePath);

            struct igsc_device_handle handle {};
            int ret;

            igsc_progress_func_t progress_func = progress_percentage_func;

            ret = igsc_device_init_by_device(&handle, devicePath.c_str());
            if (ret != IGSC_SUCCESS) {
                flashFwErrMsg = "Cannot initialize device: " + devicePath + ", error code: " + std::to_string(ret) + " error message: " + transIgscErrCodeToMsg(ret);
                XPUM_LOG_ERROR("Cannot initialize device: {}, error code: {}, error message: {}", devicePath, ret, transIgscErrCodeToMsg(ret));
                igsc_device_close(&handle);
                pDevice->unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            ret = igsc_iaf_psc_update(&handle, (const uint8_t*)buffer.data(), buffer.size(),
                                      progress_func, this);

            if (ret) {
                flashFwErrMsg = "GSC_PSCBIN update failed, error code: " + std::to_string(ret) + " error message: " + transIgscErrCodeToMsg(ret);
                XPUM_LOG_ERROR("GSC_PSCBIN update failed on device {}, error code: {}, error message: {}", devicePath, ret, transIgscErrCodeToMsg(ret));
                igsc_device_close(&handle);
                pDevice->unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            struct igsc_psc_version dev_version {};

            {
                ret = libIgsc.igsc_device_psc_version(&handle, &dev_version);
            }
            if (ret != IGSC_SUCCESS) {
                XPUM_LOG_ERROR("Failed to get GFX_PSCBIN firmware version after update from device {}", devicePath);
            } else {
                std::string version = print_psc_version(&dev_version);
                pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_PSCBIN_FIRMWARE_VERSION, version));
                XPUM_LOG_INFO("GFX_PSCBIN on device {} is successfully flashed to {}", devicePath, version);
            }

            igsc_device_close(&handle);

            auto meiDeviceName = getMeiDeviceNameFromPath(pDevice->getMeiDevicePath());
            auto txCalDate = getTxCalDateByMeiDevice(meiDeviceName);
            pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_XELINK_CALIBRATION_DATE, txCalDate));

            pDevice->unlock();
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
        });

        return xpum_result_t::XPUM_OK;
    }
}

xpum_firmware_flash_result_t PscMgmt::getFlashPscFwResult(GetFlashPscFwResultParam &param) {
    auto deviceModel = pDevice->getDeviceModel();
    if (deviceModel != XPUM_DEVICE_MODEL_PVC) {
        return XPUM_DEVICE_FIRMWARE_FLASH_UNSUPPORTED;
    }
    if (!libIgsc.ok()) {
        return XPUM_DEVICE_FIRMWARE_FLASH_UNSUPPORTED;
    }
    using namespace std::chrono_literals;
    param.errMsg = flashFwErrMsg;
    if (task.valid()) {
        auto status = task.wait_for(0ms);
        if (status == std::future_status::ready) {
            std::lock_guard<std::mutex> lck(mtx);
            return task.get();
        } else {
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
        }
    } else {
        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    }
}

void PscMgmt::getPscFwVersion() {
    if (!libIgsc.ok())
        return;

    struct igsc_device_handle handle {};
    int ret;

    ret = igsc_device_init_by_device(&handle, devicePath.c_str());
    if (ret != IGSC_SUCCESS) {
        XPUM_LOG_ERROR("Failed to initialize device: {}", devicePath);
        igsc_device_close(&handle);
        return;
    }
    struct igsc_psc_version dev_version {};
    {
        ret = libIgsc.igsc_device_psc_version(&handle, &dev_version);
    }
    if (ret != IGSC_SUCCESS) {
        XPUM_LOG_ERROR("Failed to get GFX_PSCBIN firmware version from device {}", devicePath);
    } else {
        std::string version = print_psc_version(&dev_version);
        pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_PSCBIN_FIRMWARE_VERSION, version));
        XPUM_LOG_INFO("GFX_PSCBIN version of device {} is {}", devicePath, version);
    }
    igsc_device_close(&handle);
}
} // namespace xpum