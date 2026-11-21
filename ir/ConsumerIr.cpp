/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (C) 2017-2018, 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define LOG_TAG "ConsumerIrService"

#include "ConsumerIr.h"

#include <log/log.h>

extern "C" int transmitIr(const char* dev, int baudRate, int frequency, int pattern[],
                          int pattern_len);

namespace aidl::android::hardware::ir {

ndk::ScopedAStatus ConsumerIr::getCarrierFreqs(std::vector<ConsumerIrFreqRange>* ranges) {
    *ranges = {{.minHz = 25000, .maxHz = 125000}};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ConsumerIr::transmit(int32_t carrierFreqHz,
                                      const std::vector<int32_t>& pattern) {
    if (carrierFreqHz < 25000 || carrierFreqHz > 125000 || pattern.empty()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    int64_t duration = 0;
    for (int32_t slice : pattern) {
        if (slice <= 0 || (duration += slice) > 2000000) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }

    // libcir_driver accepts a mutable pattern and its size in bytes.
    std::vector<int32_t> mutablePattern(pattern);
    std::lock_guard<std::mutex> lock(mLock);
    int rc = transmitIr("/dev/ttyMSM1", 115200, carrierFreqHz, mutablePattern.data(),
                        static_cast<int>(mutablePattern.size() * sizeof(int32_t)));
    if (rc != 0) {
        ALOGE("transmitIr failed: %d", rc);
        return ndk::ScopedAStatus::fromServiceSpecificError(rc);
    }
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::ir
