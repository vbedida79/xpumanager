/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_firmware.cpp
 */

#include "comlet_firmware.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <regex>
#include <thread>
#include <igsc_lib.h>
#include <unistd.h>
#include <dirent.h>

#include "core_stub.h"
#include "xpum_structs.h"
#include "utility.h"
#include "exit_code.h"
#include "xpum_api.h"
#include "psc.h"

namespace xpum::cli {

static std::string getSysVendor() {
    std::string sysVendorPath = "/sys/class/dmi/id/sys_vendor";
    std::string vendor = "";
    std::ifstream f(sysVendorPath);
    int N = 1024;
    char buf[N]{0};
    if (f) {
        f.getline(buf, N);
        if (f) {
            vendor = std::string(buf);
        }
        f.close();
    }
    return vendor;
}

static void printProgress(int percentage, std::ostream &out) {
    int barWidth = 60;

    out << "[";
    int pos = barWidth * (percentage / 100.0);
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos)
            std::cout << "=";
        else if (i == pos)
            std::cout << ">";
        else
            std::cout << " ";
    }
    out << "] " << percentage << " %\r";
    out.flush();
}

static const std::string igscPath{"igsc"};

ComletFirmware::ComletFirmware() : ComletBase("updatefw", "Update GPU firmware") {
    this->printHelpWhenNoArgs = true;
}

ComletFirmware::~ComletFirmware() {
}

void ComletFirmware::setupOptions() {
    opts = std::unique_ptr<FlashFirmwareOptions>(new FlashFirmwareOptions());

    auto deviceIdOpt = addOption("-d, --device", opts->deviceIdStr, "The device ID or PCI BDF address");
    deviceIdOpt->check([](const std::string &str) {
        std::string errStr = "Device id should be a non-negative integer or a BDF string";
        if (isValidDeviceId(str)) {
            return std::string();
        } else if (isBDF(str)) {
            return std::string();
        }
        return errStr;
    });

    auto fwTypeOpt = addOption("-t, --type", opts->firmwareType, "The firmware name. Valid options: GFX, GFX_DATA, GFX_CODE_DATA, GFX_PSCBIN, AMC. AMC firmware update just works on Intel M50CYP server (BMC firmware version is 2.82 or newer) and Supermicro SYS-620C-TN12R server (BMC firmware version is 11.01 or newer).");

    fwTypeOpt->check([](const std::string &str) {
        std::string errStr = "Invalid firmware type";
        if (str.compare("GFX") == 0 || str.compare("AMC") == 0 || str.compare("GFX_DATA") == 0 || str.compare("GFX_CODE_DATA") == 0 || str.compare("GFX_PSCBIN") == 0) {
            return std::string();
        } else {
            return errStr;
        }
    });

    auto fwPathOpt = addOption("-f, --file", opts->firmwarePath, "The firmware image file path on this server");
    // fwPathOpt->required();
    fwPathOpt->transform([](const std::string &str) {
        if (FILE *file = fopen(str.c_str(), "r")) {
            fclose(file);
            // get full path of firmware image path
            char resolved_path[PATH_MAX];
            char *fullpath = realpath(str.c_str(), resolved_path);
            return std::string(fullpath);
        } else {
            throw CLI::ValidationError("Invalid file path.");
        }
    });
    
    fwPathOpt->needs(fwTypeOpt);
    fwTypeOpt->needs(fwPathOpt);

    deviceIdOpt->needs(fwTypeOpt);
    deviceIdOpt->needs(fwPathOpt);

    opts->deviceId = XPUM_DEVICE_ID_ALL_DEVICES;

    addOption("-u,--username", this->opts->username, "Username used to authenticate for host redfish access");
    addOption("-p,--password", this->opts->password, "Password used to authenticate for host redfish access");

    addFlag("-y, --assumeyes", opts->assumeyes, "Assume that the answer to any question which would be asked is yes");

    auto forceFlag = addFlag("--force", opts->forceUpdate, "Force GFX firmware update. This parameter only works for GFX firmware.");

    forceFlag->needs(fwTypeOpt);

}

nlohmann::json ComletFirmware::validateArguments() {
    nlohmann::json result;

    if (opts->deviceIdStr.empty()) {
        // do nothing
    } else if (isBDF(opts->deviceIdStr)) {
        int deviceId;
        auto json = coreStub->getDeivceIdByBDF(opts->deviceIdStr.c_str(), &deviceId);
        if (json->contains("error")) {
            return *json;
        } else{
            opts->deviceId = deviceId;
        }
    } else {
        opts->deviceId = std::stoi(opts->deviceIdStr);
    }
    // GFX
    if (opts->deviceId == XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType.compare("GFX") == 0) {
        result["error"] = "Updating GFX firmware on all devices is not supported";
        result["errno"] = XPUM_CLI_ERROR_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL;
        return result;
    }

    if (opts->forceUpdate && opts->firmwareType.compare("GFX") != 0 && opts->firmwareType.compare("GFX_PSCBIN") != 0) {
        result["error"] = "Force flag only works for GFX firmware";
        result["errno"] = XPUM_CLI_ERROR_BAD_ARGUMENT;
        return result;
    }

    if (opts->deviceId == XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType.compare("GFX_DATA") == 0) {
        result["error"] = "Updating GFX_DATA firmware on all devices is not supported";
        result["errno"] = XPUM_CLI_ERROR_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL;
        return result;
    }

    if (opts->deviceId == XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType.compare("GFX_PSCBIN") == 0) {
        result["error"] = "Updating GFX_PSCBIN firmware on all devices is not supported";
        result["errno"] = XPUM_CLI_ERROR_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL;
        return result;
    }

    if (opts->deviceId == XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType.compare("GFX_CODE_DATA") == 0) {
        result["error"] = "Updating GFX_CODE_DATA firmware on all devices is not supported";
        result["errno"] = XPUM_CLI_ERROR_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL;
        return result;
    }

    // AMC
    if (opts->deviceId != XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType.compare("AMC") == 0) {
        result["error"] = "Updating AMC firmware on single device is not supported";
         result["errno"] = XPUM_CLI_ERROR_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE;
        return result;
    }
    return result;
}

std::unique_ptr<nlohmann::json> ComletFirmware::run() {
    std::unique_ptr<nlohmann::json> json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    return json;
}

static void printJson(std::shared_ptr<nlohmann::json> json, std::ostream &out, bool raw) {
    if (raw) {
        out << json->dump() << std::endl;
        return;
    } else {
        out << json->dump(4) << std::endl;
        return;
    }
}

static int getIntFirmwareType(std::string firmwareType) {
    if (firmwareType.compare("GFX") == 0)
        return XPUM_DEVICE_FIRMWARE_GFX;
    if (firmwareType.compare("AMC") == 0)
        return XPUM_DEVICE_FIRMWARE_AMC;
    if(firmwareType.compare("GFX_DATA") == 0)
        return XPUM_DEVICE_FIRMWARE_GFX_DATA;
    if(firmwareType.compare("GFX_PSCBIN") == 0)
        return XPUM_DEVICE_FIRMWARE_GFX_PSCBIN;
    if (firmwareType.compare("GFX_CODE_DATA") == 0)
        return XPUM_DEVICE_FIRMWARE_GFX_CODE_DATA;
    return -1;
}

void ComletFirmware::getJsonResult(std::ostream &out, bool raw) {
    auto validateResultJson = validateArguments();
    if (validateResultJson.contains("error")) {
        printJson(std::make_shared<nlohmann::json>(validateResultJson), out, raw);
        setExitCodeByJson(validateResultJson);
        return;
    }

    int type = getIntFirmwareType(opts->firmwareType);
    auto uniqueJson = coreStub->runFirmwareFlash(opts->deviceId, type, opts->firmwarePath, opts->username, opts->password, opts->forceUpdate);
    std::shared_ptr<nlohmann::json> json = std::move(uniqueJson);
    if (json->contains("error")) {
        printJson(json, out, raw);
        setExitCodeByJson(*json);
        return;
    }
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        json = coreStub->getFirmwareFlashResult(opts->deviceId, type);
        if (json->contains("error")) {
            printJson(json, out, raw);
            setExitCodeByJson(*json);
            return;
        }
        if (!json->contains("result")) {
            nlohmann::json tmp;
            tmp["error"] = "Failed to get firmware reuslt";
            printJson(std::make_shared<nlohmann::json>(tmp), out, raw);
            exit_code = XPUM_CLI_ERROR_GENERIC_ERROR;
            return;
        }

        std::string flashStatus = (*json)["result"].get<std::string>();

        if (flashStatus.compare("OK") == 0) {
            nlohmann::json tmp;
            tmp["result"] = "OK";
            printJson(std::make_shared<nlohmann::json>(tmp), out, raw);
            return;
        } else if (flashStatus.compare("FAILED") == 0) {
            nlohmann::json tmp;
            tmp["result"] = "FAILED";
            tmp["errno"] = XPUM_CLI_ERROR_UPDATE_FIRMWARE_FAIL;
            printJson(std::make_shared<nlohmann::json>(tmp), out, raw);
            return;
        } else {
            // do nothing
        }
    }
}

nlohmann::json ComletFirmware::getDeviceProperties(int deviceId) {
    auto json = coreStub->getDeviceProperties(deviceId);
    return *json;
}

std::string ComletFirmware::getCurrentFwVersion(nlohmann::json json) {
    std::string res = "unknown";
    int type = getIntFirmwareType(opts->firmwareType);
    if (type == XPUM_DEVICE_FIRMWARE_GFX) {
        if (!json.contains("gfx_firmware_version")) {
            return res;
        }
        return json["gfx_firmware_version"];
    } else if (type == XPUM_DEVICE_FIRMWARE_GFX_DATA) {
        if (!json.contains("gfx_data_firmware_version")) {
            return res;
        }
        return json["gfx_data_firmware_version"];
    } else if (type == XPUM_DEVICE_FIRMWARE_GFX_PSCBIN) {
        if (!json.contains("gfx_pscbin_firmware_version")) {
            return res;
        }
        return json["gfx_pscbin_firmware_version"];
    } else {
        return res;
    }
}

static std::string print_fw_version(const struct igsc_fw_version* fw_version) {
    std::stringstream ss;
    ss << fw_version->project[0];
    ss << fw_version->project[1];
    ss << fw_version->project[2];
    ss << fw_version->project[3];
    ss << "_";
    ss << fw_version->hotfix;
    ss << ".";
    ss << fw_version->build;
    return ss.str();
}

std::string ComletFirmware::getImageFwVersion() {
    std::string version = "unknown";
    auto &buffer = imgBuffer;
    if (buffer.size() == 0) return version;

    struct igsc_fw_version fw_version;
    int ret;
    ret = igsc_image_fw_version((const uint8_t *)buffer.data(), buffer.size(), &fw_version);
    if (ret == IGSC_SUCCESS) {
        version = print_fw_version(&fw_version);
    }
    return version;
}

static std::string print_fwdata_version(const struct igsc_fwdata_version *fwdata_version) {
    std::stringstream ss;
    ss << "0x" << std::hex << fwdata_version->oem_manuf_data_version;
    return ss.str();
}

std::string ComletFirmware::getFwDataImageFwVersion() {
    std::string version = "unknown";
    auto &buffer = imgBuffer;
    if (buffer.size() == 0) return version;

    struct igsc_fwdata_image *oimg = NULL;
    struct igsc_fwdata_version fwdata_version;
    int ret;

    ret = igsc_image_fwdata_init(&oimg, (const uint8_t *)buffer.data(), buffer.size());
    if (ret != IGSC_SUCCESS) {
        igsc_image_fwdata_release(oimg);
        return version;
    }

    ret = igsc_image_fwdata_version(oimg, &fwdata_version);
    if (ret == IGSC_SUCCESS) {
        version = print_fwdata_version(&fwdata_version);
    }
    return version;
}

std::string ComletFirmware::getPSCImageFwVersion() {
    psc_data *hdr = (psc_data *)imgBuffer.data();
    std::string version = getPscVersion(hdr->cfg_version, hdr->date);
    if (version.length()) {
        return version;
    }
    return "unknown";
}

bool ComletFirmware::checkImageValid() {
    auto& buffer = imgBuffer;
    if (buffer.size() == 0) return false;
    uint8_t type;
    int ret;
    ret = igsc_image_get_type((const uint8_t*)buffer.data(), buffer.size(), &type);
    if (ret != IGSC_SUCCESS)
    {
        return false;
    }
    return type == IGSC_IMAGE_TYPE_GFX_FW;
}

bool ComletFirmware::validateFwDataImage(){
    auto& buffer = imgBuffer;
    if (buffer.size() == 0) return false;
    uint8_t type;
    int ret;
    ret = igsc_image_get_type((const uint8_t*)buffer.data(), buffer.size(), &type);
    if (ret != IGSC_SUCCESS)
    {
        return false;
    }
    return type == IGSC_IMAGE_TYPE_FW_DATA;
}

bool ComletFirmware::checkIgscExist() {
    std::string cmd = igscPath + " -V 2>&1";
    FILE *f = popen(cmd.c_str(), "r");
    char c_line[1024];
    while (fgets(c_line, 1024, f) != NULL) {
    }
    return pclose(f) == 0;
}

static bool findFileInDir(std::string dirPath, std::regex pattern, std::string &filePath){
    DIR* dir = opendir(dirPath.c_str());
    struct dirent* ent;
    if (nullptr != dir) {
        while ((ent = readdir(dir)) != nullptr) {
            if(std::regex_search(ent->d_name, pattern)){
                filePath = dirPath + "/" + ent->d_name;
                return true;
            }
        }
        closedir(dir);
    }
    return false;
}

static std::string findSubDir(const char* dirPath, const char* sudDirName){
    std::string path;
    DIR* dir;
    struct dirent* ent;
    if ((dir = opendir(dirPath)) == nullptr)
        return "";
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.' || strcmp(ent->d_name, "..") == 0) {
                continue;
        }
        if (ent->d_type == DT_DIR) {
            std::string fullPath = std::string(dirPath) + "/" + std::string(ent->d_name);
            if (strcmp(ent->d_name, sudDirName) == 0){
                path = fullPath;
                break;
            } else {
                path = findSubDir(fullPath.c_str(), sudDirName);
                if (path != "")
                    break;
            }
        }
    }
    closedir(dir);
    return path;
}

static bool unpackAndGetImagePath(const char* filePath, const char* dirName, int eccState, std::string &codeImagePath, std::string &dataImagePath){
    std::string unpack_cmd = "unzip -q -o " + std::string(filePath) + " -d " + std::string(dirName);
    int status = std::system(unpack_cmd.c_str());
    if (status != 0)
        return false;
    //check if follow the standard format
    std::string eccStateStr = (eccState == 1) ? "ECC_ON" : "ECC_OFF";
    std::string dirPath = findSubDir(dirName, eccStateStr.c_str());
    if (dirPath.empty())
        return false;
    
    std::regex codePattern(".*gfx_fwupdate.*\\.bin");
    if (!findFileInDir(dirPath, codePattern, codeImagePath)) {
        return false;
    }
    std::regex dataPattern(".*DataUpdate_"+ eccStateStr + ".*\\.bin");
    if (!findFileInDir(dirPath, dataPattern, dataImagePath)) {
        return false;
    }
    return true;
}

static bool removeDir(const char* dirPath){
    DIR* dir;
    struct dirent* ent;
    bool success = true;
    if ((dir = opendir(dirPath)) == nullptr)
        return success;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.' || strcmp(ent->d_name, "..") == 0)
            continue;
        if (ent->d_type == DT_DIR) {
            std::string subDir = std::string(dirPath) + "/" + std::string(ent->d_name);
            if (!removeDir(subDir.c_str())) {
                success = false;
            }
        } else if (ent->d_type == DT_REG) {
            std::string filePath = std::string(dirPath) + "/" + std::string(ent->d_name);
            if (unlink(filePath.c_str()) != 0) {
                success = false;
            }
        }
    }
    closedir(dir);
    if (rmdir(dirPath) != 0) {
        success = false;
    }
    return success;
}

static std::string getCurrentFwCodeDataVersion(nlohmann::json json, std::string firmwareType) {
    std::string res = "unknown";
    int type = getIntFirmwareType(firmwareType);
    if (type == XPUM_DEVICE_FIRMWARE_GFX) {
        if (!json.contains("gfx_firmware_version")) {
            return res;
        }
        return json["gfx_firmware_version"];
    } else if (type == XPUM_DEVICE_FIRMWARE_GFX_DATA) {
        if (!json.contains("gfx_data_firmware_version")) {
            return res;
        }
        return json["gfx_data_firmware_version"];
    } else {
        return res;
    }
}

void ComletFirmware::getTableResult(std::ostream &out) {
    auto validateResultJson = validateArguments();
    if (validateResultJson.contains("error")) {
        out << "Error: " << validateResultJson["error"].get<std::string>() << std::endl;
        setExitCodeByJson(validateResultJson);
        return;
    }

    // read file
    readImageContent(opts->firmwarePath.c_str());
    // warn user
    int type = getIntFirmwareType(opts->firmwareType);
    if (type == XPUM_DEVICE_FIRMWARE_AMC) { // AMC caution
        std::string amcWarnMsg = coreStub->getRedfishAmcWarnMsg();
        if (amcWarnMsg.length()) {
            std::cout << coreStub->getRedfishAmcWarnMsg() << std::endl;
            std::cout << "Do you want to continue? (y/n) ";
            if (!opts->assumeyes) {
                std::string confirm;
                std::cin >> confirm;
                if (confirm != "Y" && confirm != "y") {
                    out << "update aborted" << std::endl;
                    return;
                }
            } else {
                out << std::endl;
            }
        }
        std::cout << "CAUTION: it will update the AMC firmware of all cards and please make sure that you install the GPUs of the same model." << std::endl;
        std::cout << "Please confirm to proceed (y/n) ";
        if (!opts->assumeyes) {
            std::string confirm;
            std::cin >> confirm;
            if (confirm != "Y" && confirm != "y") {
                out << "update aborted" << std::endl;
                return;
            }
        } else {
            out << std::endl;
        }
    } else if (type == XPUM_DEVICE_FIRMWARE_GFX_CODE_DATA) {
        //check unzip
        if (std::system("which unzip >/dev/null 2>&1") != 0) {
            out << "Error: unzip not found, please install unzip at first." << std::endl;
            exit_code = XPUM_CLI_ERROR_OPEN_FILE;
            return;
        }

        //check ecc state
        auto json = coreStub->getDeviceConfig(opts->deviceId, -1);
        if (json->contains("error")) {
            out << "Error: " << (*json)["error"].get<std::string>() << std::endl;
            setExitCodeByJson(*json);
            return;
        }
        if (!json->contains("memory_ecc_current_state")) {
            out << "Error: This device cannot get the ecc state to get a matching image." << std::endl;
            exit_code = XPUM_CLI_ERROR_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
            return;
        }
        std::string current = (*json)["memory_ecc_current_state"];
        int eccState;
        if (current == "enabled")
            eccState = 1;
        else if (current == "disabled")
            eccState = 2;
        else {
            out << "Error: This device cannot get the ecc state to get a matching image." << std::endl;
            exit_code = XPUM_CLI_ERROR_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
            return;
        }

        const char *dirName = "/tmp/tmp_fw_update_for_xpum";
        if (!removeDir(dirName)) {
            out << "Error: "<< std::string(dirName) << " exist and remove failed." << std::endl;
            exit_code = XPUM_CLI_ERROR_GENERIC_ERROR;
            removeDir(dirName);
            return;
        }
        std::string codeImagePath, dataImagePath;
        std::string codeImageVersion, dataImageVersion;
        int ret = unpackAndGetImagePath(opts->firmwarePath.c_str(), dirName, eccState, codeImagePath, dataImagePath);
        if (!ret) {
            out << "Error: The image file is not a right GFX_CODE_DATA firmware image file." << std::endl;
            exit_code = XPUM_CLI_ERROR_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
            removeDir(dirName);
            return;
        }
        readImageContent(codeImagePath.c_str());
        if (!checkImageValid()) {
            out << "Error: The GFX firmware image in package is not a right file." << std::endl;
            exit_code = XPUM_CLI_ERROR_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
            removeDir(dirName);
            return;
        }
        codeImageVersion = getImageFwVersion();
        readImageContent(dataImagePath.c_str());
        if (!validateFwDataImage()) {
            out << "Error: The GFX_DATA firmware image in package is not a right file." << std::endl;
            exit_code = XPUM_CLI_ERROR_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE;
            removeDir(dirName);
            return;
        }
        dataImageVersion = getFwDataImageFwVersion();
        // for ats-m3
        auto allGroups = coreStub->groupListAll();
        std::vector<int> deviceIdsToFlashFirmware;
        if (allGroups != nullptr && allGroups->contains("group_list")) {
            for (auto groupJson : (*allGroups)["group_list"]) {
                int groupId = groupJson["group_id"].get<int>();
                if (groupId & 0x80000000) {
                    auto deviceIdList = groupJson["device_id_list"];
                    for (auto deviceIdInGroup : deviceIdList) {
                        if (deviceIdInGroup.get<int>() == opts->deviceId) {
                            std::cout << "This GPU card has multiple cores. This operation will update all firmwares. Do you want to continue? (y/n) ";
                            if (!opts->assumeyes) {
                                std::string confirm;
                                std::cin >> confirm;
                                if (confirm != "Y" && confirm != "y") {
                                    out << "update aborted" << std::endl;
                                    removeDir(dirName);
                                    return;
                                }
                            } else {
                                out << std::endl;
                            }
                            for (auto tmpId : deviceIdList) {
                                deviceIdsToFlashFirmware.push_back(tmpId.get<int>());
                            }
                            break;
                        }
                    }
                    if (deviceIdsToFlashFirmware.size() > 0)
                        break;
                }
            }
        }
        if(deviceIdsToFlashFirmware.size()==0){
            deviceIdsToFlashFirmware.push_back(opts->deviceId);
        }
        // version confirmation
        for (int deviceId : deviceIdsToFlashFirmware) {
            auto json = getDeviceProperties(deviceId);
            if (json.contains("error")) {
                out << "Error: " << json["error"].get<std::string>() << std::endl;
                setExitCodeByJson(json);
                removeDir(dirName);
                return;
            }
            out << "Device " << deviceId << " FW Code version: " << getCurrentFwCodeDataVersion(json, "GFX") << std::endl;
        }
        out << "Image FW Code version: " << codeImageVersion << std::endl;
        bool isImageNewer = false;
        for (int deviceId : deviceIdsToFlashFirmware) {
            auto json = getDeviceProperties(deviceId);
            if (json.contains("error")) {
                out << "Error: " << json["error"].get<std::string>() << std::endl;
                setExitCodeByJson(json);
                removeDir(dirName);
                return;
            }
            std::string fwDataVersion = getCurrentFwCodeDataVersion(json, "GFX_DATA");
            if (std::stoi(dataImageVersion, nullptr, 16) > std::stoi(fwDataVersion, nullptr, 16)){
                out << "Device " << deviceId << " FW Data version: " << fwDataVersion << std::endl;
                isImageNewer = true;
            }
        }
        if (isImageNewer)
            out << "Image FW Data version: " << dataImageVersion << std::endl;
        out << "Do you want to continue? (y/n) ";
        if (!opts->assumeyes) {
            std::string confirm;
            std::cin >> confirm;
            if (confirm != "Y" && confirm != "y") {
                out << "update aborted" << std::endl;
                removeDir(dirName);
                return;
            }
        } else {
            out << std::endl;
        }
        removeDir(dirName);
    } else {
        if (type == XPUM_DEVICE_FIRMWARE_GFX) {
            if (!checkImageValid()) {
                out << "Error: The image file is not a right GFX firmware image file." << std::endl;
                exit_code = XPUM_CLI_ERROR_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
                return;
            }
        } else if (type == XPUM_DEVICE_FIRMWARE_GFX_DATA) {
            if (!validateFwDataImage()) {
                out << "Error: The image file is not a right GFX_DATA firmware image file." << std::endl;
                exit_code = XPUM_CLI_ERROR_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE;
                return;
            }
        }
        // for ats-m3
        auto allGroups = coreStub->groupListAll();
        std::vector<int> deviceIdsToFlashFirmware;
        if (allGroups != nullptr && allGroups->contains("group_list")) {
            for (auto groupJson : (*allGroups)["group_list"]) {
                int groupId = groupJson["group_id"].get<int>();
                if (groupId & 0x80000000) {
                    auto deviceIdList = groupJson["device_id_list"];
                    for (auto deviceIdInGroup : deviceIdList) {
                        if (deviceIdInGroup.get<int>() == opts->deviceId) {
                            std::cout << "This GPU card has multiple cores. This operation will update all firmwares. Do you want to continue? (y/n) ";
                            if (!opts->assumeyes) {
                                std::string confirm;
                                std::cin >> confirm;
                                if (confirm != "Y" && confirm != "y") {
                                    out << "update aborted" << std::endl;
                                    return;
                                }
                            } else {
                                out << std::endl;
                            }
                            for (auto tmpId : deviceIdList) {
                                deviceIdsToFlashFirmware.push_back(tmpId.get<int>());
                            }
                            break;
                        }
                    }
                    if (deviceIdsToFlashFirmware.size() > 0)
                        break;
                }
            }
        }
        if(deviceIdsToFlashFirmware.size()==0){
            deviceIdsToFlashFirmware.push_back(opts->deviceId);
        }
        // version confirmation
        for (int deviceId : deviceIdsToFlashFirmware) {
            auto json = getDeviceProperties(deviceId);
            if (json.contains("error")) {
                out << "Error: " << json["error"].get<std::string>() << std::endl;
                setExitCodeByJson(json);
                return;
            }
            out << "Device " << deviceId << " FW version: " << getCurrentFwVersion(json) << std::endl;
        }
        if (type == XPUM_DEVICE_FIRMWARE_GFX) {
            out << "Image FW version: " << getImageFwVersion() << std::endl;
        } else if (type == XPUM_DEVICE_FIRMWARE_GFX_DATA) {
            out << "Image FW version: " << getFwDataImageFwVersion() << std::endl;
        } else if (type == XPUM_DEVICE_FIRMWARE_GFX_PSCBIN) {
            out << "Image FW version: " << getPSCImageFwVersion() << std::endl;
        }
        out << "Do you want to continue? (y/n) ";
        if (!opts->assumeyes) {
            std::string confirm;
            std::cin >> confirm;
            if (confirm != "Y" && confirm != "y") {
                out << "update aborted" << std::endl;
                return;
            }
        } else {
            out << std::endl;
        }
    }

    // start run
    auto json = coreStub->runFirmwareFlash(opts->deviceId, type, opts->firmwarePath, opts->username, opts->password, opts->forceUpdate);

    auto status = (*json)["error"];
    if (!status.is_null()) {
        out << "Error: " << status.get<std::string>() << std::endl;
        setExitCodeByJson(*json);
        return;
    }
    out << "Start to update firmware" << std::endl;
    out << "Firmware Name: " << opts->firmwareType << std::endl;
    out << "Image path: " << opts->firmwarePath << std::endl;

    printProgress(0, out);
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // out << "." << std::flush;

        json = coreStub->getFirmwareFlashResult(opts->deviceId, type);
        if (json->contains("error")) {
            out << std::endl;
            out << "Error: " << (*json)["error"] << std::endl;
            setExitCodeByJson(*json);
            return;
        }
        if (!json->contains("result")) {
            out << std::endl;
            out << "Error: Failed to get firmware reuslt" << std::endl;
            exit_code = XPUM_CLI_ERROR_GENERIC_ERROR;
            return;
        }

        std::string flashStatus = (*json)["result"].get<std::string>();

        if (flashStatus.compare("OK") == 0) {
            printProgress(100, out);
            out << std::endl;
            out << "Update firmware successfully." << std::endl;
            if (type == XPUM_DEVICE_FIRMWARE_GFX_DATA) {
                out << "Please reboot OS to take effect." << std::endl;
            } else if (type == XPUM_DEVICE_FIRMWARE_AMC && getSysVendor() == "Supermicro") {
                out << "Please reboot OS to take effect." << std::endl;
            }
            return;
        } else if (flashStatus.compare("FAILED") == 0) {
            std::string errormsg;
            if (json->contains("error")) {
                errormsg = (*json)["error"];
            } else {
                errormsg = "Update firmware failed";
            }
            out << std::endl;
            out << errormsg << std::endl;
            exit_code = XPUM_CLI_ERROR_UPDATE_FIRMWARE_FAIL;
            return;
        } else {
            // print progress bar
            if (json->contains("percentage"))
                printProgress((*json)["percentage"], out);
        }
    }

    out << "unknown error" << std::endl;
    exit_code = XPUM_CLI_ERROR_GENERIC_ERROR;
}

void ComletFirmware::readImageContent(const char *filePath) {
    struct stat s;
    if (stat(filePath, &s) != 0 || !(s.st_mode & S_IFREG))
        return;
    std::ifstream is(std::string(filePath), std::ifstream::binary);
    if (!is) {
        return;
    }
    // get length of file:
    is.seekg(0, is.end);
    int length = is.tellg();
    is.seekg(0, is.beg);

    std::vector<char> buffer(length);

    is.read(buffer.data(), length);
    is.close();
    imgBuffer = buffer;
}
} // namespace xpum::cli