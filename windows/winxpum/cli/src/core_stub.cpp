/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file core_stub.cpp
 */

#include "core_stub.h"

namespace xpum::cli {

std::string CoreStub::isotimestamp(uint64_t t, bool withoutDate) {
    time_t seconds = t / 1000;
    int milli_seconds = t % 1000;
    tm* tm_p = localtime(&seconds);
    char buf[50];
    char milli_buf[10];
    sprintf_s(milli_buf, "%03d", milli_seconds);
    if (withoutDate) {
        strftime(buf, sizeof(buf), "%T", tm_p);
        return std::string(buf) + "." + std::string(milli_buf);
    } else {
        strftime(buf, sizeof(buf), "%FT%T", tm_p);
        return std::string(buf) + "." + std::string(milli_buf);
    }
}

struct MetricsTypeEntry {
    xpum_stats_type_t key;
    std::string keyStr;
};

static MetricsTypeEntry metricsTypeArray[]{
    {XPUM_STATS_GPU_UTILIZATION, "XPUM_STATS_GPU_UTILIZATION"},
    {XPUM_STATS_EU_ACTIVE, "XPUM_STATS_EU_ACTIVE"},
    {XPUM_STATS_EU_STALL, "XPUM_STATS_EU_STALL"},
    {XPUM_STATS_EU_IDLE, "XPUM_STATS_EU_IDLE"},
    {XPUM_STATS_POWER, "XPUM_STATS_POWER"},
    {XPUM_STATS_ENERGY, "XPUM_STATS_ENERGY"},
    {XPUM_STATS_GPU_FREQUENCY, "XPUM_STATS_GPU_FREQUENCY"},
    {XPUM_STATS_GPU_CORE_TEMPERATURE, "XPUM_STATS_GPU_CORE_TEMPERATURE"},
    {XPUM_STATS_MEMORY_USED, "XPUM_STATS_MEMORY_USED"},
    {XPUM_STATS_MEMORY_UTILIZATION, "XPUM_STATS_MEMORY_UTILIZATION"},
    {XPUM_STATS_MEMORY_BANDWIDTH, "XPUM_STATS_MEMORY_BANDWIDTH"},
    {XPUM_STATS_MEMORY_READ, "XPUM_STATS_MEMORY_READ"},
    {XPUM_STATS_MEMORY_WRITE, "XPUM_STATS_MEMORY_WRITE"},
    {XPUM_STATS_MEMORY_READ_THROUGHPUT, "XPUM_STATS_MEMORY_READ_THROUGHPUT"},
    {XPUM_STATS_MEMORY_WRITE_THROUGHPUT, "XPUM_STATS_MEMORY_WRITE_THROUGHPUT"},
    {XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION"},
    {XPUM_STATS_RAS_ERROR_CAT_RESET, "XPUM_STATS_RAS_ERROR_CAT_RESET"},
    {XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS, "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS"},
    {XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS, "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS"},
    {XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE"},
    {XPUM_STATS_GPU_REQUEST_FREQUENCY, "XPUM_STATS_GPU_REQUEST_FREQUENCY"},
    {XPUM_STATS_MEMORY_TEMPERATURE, "XPUM_STATS_MEMORY_TEMPERATURE"},
    {XPUM_STATS_FREQUENCY_THROTTLE, "XPUM_STATS_FREQUENCY_THROTTLE"},
    {XPUM_STATS_FREQUENCY_THROTTLE_REASON_GPU, "XPUM_STATS_FREQUENCY_THROTTLE_REASON_GPU"},
    {XPUM_STATS_PCIE_READ_THROUGHPUT, "XPUM_STATS_PCIE_READ_THROUGHPUT"},
    {XPUM_STATS_PCIE_WRITE_THROUGHPUT, "XPUM_STATS_PCIE_WRITE_THROUGHPUT"}};

std::string CoreStub::metricsTypeToString(xpum_stats_type_t metricsType) {
    for (auto item : metricsTypeArray) {
        if (item.key == metricsType) {
            return item.keyStr;
        }
    }
    return std::to_string(metricsType);
}
}