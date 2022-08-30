#include "exit_code.h"

#include "xpum_structs.h"

int exit_code = XPUM_CLI_SUCCESS;

using namespace xpum;

int errorNumTranslate(int coreErrNo) {
    switch (coreErrNo) {
        case XPUM_OK:
            return XPUM_CLI_SUCCESS;
        case XPUM_GENERIC_ERROR:
            return XPUM_CLI_ERROR_GENERIC_ERROR;
        case XPUM_BUFFER_TOO_SMALL:
            return XPUM_CLI_ERROR_BUFFER_TOO_SMALL;
        case XPUM_RESULT_DEVICE_NOT_FOUND:
            return XPUM_CLI_ERROR_DEVICE_NOT_FOUND;
        case XPUM_RESULT_TILE_NOT_FOUND:
            return XPUM_CLI_ERROR_TILE_NOT_FOUND;
        case XPUM_RESULT_GROUP_NOT_FOUND:
            return XPUM_CLI_ERROR_GROUP_NOT_FOUND;
        case XPUM_RESULT_POLICY_TYPE_INVALID:
            return XPUM_CLI_ERROR_POLICY_TYPE_INVALID;
        case XPUM_RESULT_POLICY_ACTION_TYPE_INVALID:
            return XPUM_CLI_ERROR_POLICY_ACTION_TYPE_INVALID;
        case XPUM_RESULT_POLICY_CONDITION_TYPE_INVALID:
            return XPUM_CLI_ERROR_POLICY_CONDITION_TYPE_INVALID;
        case XPUM_RESULT_POLICY_TYPE_ACTION_NOT_SUPPORT:
            return XPUM_CLI_ERROR_POLICY_TYPE_ACTION_NOT_SUPPORT;
        case XPUM_RESULT_POLICY_TYPE_CONDITION_NOT_SUPPORT:
            return XPUM_CLI_ERROR_POLICY_TYPE_CONDITION_NOT_SUPPORT;
        case XPUM_RESULT_POLICY_INVALID_THRESHOLD:
            return XPUM_CLI_ERROR_POLICY_INVALID_THRESHOLD;
        case XPUM_RESULT_POLICY_INVALID_FREQUENCY:
            return XPUM_CLI_ERROR_POLICY_INVALID_FREQUENCY;
        case XPUM_RESULT_POLICY_NOT_EXIST:
            return XPUM_CLI_ERROR_POLICY_NOT_EXIST;
        case XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE:
            return XPUM_CLI_ERROR_DIAGNOSTIC_TASK_NOT_COMPLETE;
        case XPUM_GROUP_DEVICE_DUPLICATED:
            return XPUM_CLI_ERROR_GROUP_DEVICE_DUPLICATED;
        case XPUM_GROUP_CHANGE_NOT_ALLOWED:
            return XPUM_CLI_ERROR_GROUP_CHANGE_NOT_ALLOWED;
        case XPUM_NOT_INITIALIZED:
            return XPUM_CLI_ERROR_NOT_INITIALIZED;
        case XPUM_DUMP_RAW_DATA_TASK_NOT_EXIST:
            return XPUM_CLI_ERROR_DUMP_RAW_DATA_TASK_NOT_EXIST;
        case XPUM_DUMP_RAW_DATA_ILLEGAL_DUMP_FILE_PATH:
            return XPUM_CLI_ERROR_DUMP_RAW_DATA_ILLEGAL_DUMP_FILE_PATH;
        case XPUM_RESULT_UNKNOWN_AGENT_CONFIG_KEY:
            return XPUM_CLI_ERROR_UNKNOWN_AGENT_CONFIG_KEY;
        case XPUM_UPDATE_FIRMWARE_ILLEGAL_FILENAME:
            return XPUM_CLI_ERROR_UPDATE_FIRMWARE_ILLEGAL_FILENAME;
        case XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND:
            return XPUM_CLI_ERROR_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND;
        case XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC:
            return XPUM_CLI_ERROR_UPDATE_FIRMWARE_UNSUPPORTED_AMC;
        case XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE:
            return XPUM_CLI_ERROR_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE;
        case XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL:
            return XPUM_CLI_ERROR_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL;
        case XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE:
            return XPUM_CLI_ERROR_UPDATE_FIRMWARE_MODEL_INCONSISTENCE;
        case XPUM_UPDATE_FIRMWARE_IGSC_NOT_FOUND:
            return XPUM_CLI_ERROR_UPDATE_FIRMWARE_IGSC_NOT_FOUND;
        case XPUM_UPDATE_FIRMWARE_TASK_RUNNING:
            return XPUM_CLI_ERROR_UPDATE_FIRMWARE_TASK_RUNNING;
        case XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE:
            return XPUM_CLI_ERROR_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
        case XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE:
            return XPUM_CLI_ERROR_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE;
        case XPUM_RESULT_DUMP_METRICS_TYPE_NOT_SUPPORT:
            return XPUM_CLI_ERROR_DUMP_METRICS_TYPE_NOT_SUPPORT;
        case XPUM_METRIC_NOT_SUPPORTED:
            return XPUM_CLI_ERROR_METRIC_NOT_SUPPORTED;
        case XPUM_METRIC_NOT_ENABLED:
            return XPUM_CLI_ERROR_METRIC_NOT_ENABLED;
        case XPUM_RESULT_HEALTH_INVALID_TYPE:
            return XPUM_CLI_ERROR_HEALTH_INVALID_TYPE;
        case XPUM_RESULT_HEALTH_INVALID_CONIG_TYPE:
            return XPUM_CLI_ERROR_HEALTH_INVALID_CONIG_TYPE;
        case XPUM_RESULT_HEALTH_INVALID_THRESHOLD:
            return XPUM_CLI_ERROR_HEALTH_INVALID_THRESHOLD;
        case XPUM_RESULT_DIAGNOSTIC_INVALID_LEVEL:
            return XPUM_CLI_ERROR_DIAGNOSTIC_INVALID_LEVEL;
        case XPUM_RESULT_AGENT_SET_INVALID_VALUE:
            return XPUM_CLI_ERROR_AGENT_SET_INVALID_VALUE;
        case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
            return XPUM_CLI_ERROR_LEVEL_ZERO_INITIALIZATION_ERROR;
        case XPUM_UNSUPPORTED_SESSIONID:
            return XPUM_CLI_ERROR_UNSUPPORTED_SESSIONID;
        case XPUM_RESULT_MEMORY_ECC_LIB_NOT_SUPPORT:
            return XPUM_CLI_ERROR_MEMORY_ECC_LIB_NOT_SUPPORT;
        default:
            return XPUM_CLI_ERROR_GENERIC_ERROR;
    }
}