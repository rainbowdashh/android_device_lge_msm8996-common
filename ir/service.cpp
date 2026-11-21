/*
 * Copyright (C) 2017, 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define LOG_TAG "android.hardware.ir-service.lge"

#include "ConsumerIr.h"

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <log/log.h>

using aidl::android::hardware::ir::ConsumerIr;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    auto service = ndk::SharedRefBase::make<ConsumerIr>();
    const std::string instance = std::string(ConsumerIr::descriptor) + "/default";
    binder_status_t status = AServiceManager_addService(service->asBinder().get(), instance.c_str());
    if (status != STATUS_OK) {
        ALOGE("Cannot register IR HAL service: %d", status);
        return 1;
    }
    ABinderProcess_joinThreadPool();
    return 1;
}
