/* 
 *  Copyright (C) 2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file vgpu_manager.h
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "../include/xpum_structs.h"
#include "./vgpu_types.h"
#include "device/device.h"

namespace xpum {

class VgpuManager {
private:

    bool loadSriovData(xpum_device_id_t deviceId ,DeviceSriovInfo &datas);

    bool readConfigFromFile(xpum_device_id_t deviceId, uint32_t numVfs, AttrFromConfigFile &attrs);

    void readFile(const std::string& path, std::string& content);

    void writeFile(const std::string& path, const std::string& content);

    std::mutex mutex;

public:

    // Check whether resources is enough before, then create VF
    xpum_result_t createVf(xpum_device_id_t deviceId, xpum_vgpu_config_t* config);

    // List VF info
    xpum_result_t getFunctionList(xpum_device_id_t deviceId, std::vector<xpum_vgpu_function_info_t> &functionList);

    // Clear all VFs
    xpum_result_t removeAllVf(xpum_device_id_t deviceId);
    
};

} // namespace xpum