/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (C) 2017-2018, 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <aidl/android/hardware/ir/BnConsumerIr.h>
#include <mutex>

namespace aidl::android::hardware::ir {

class ConsumerIr : public BnConsumerIr {
  public:
    ndk::ScopedAStatus getCarrierFreqs(std::vector<ConsumerIrFreqRange>* ranges) override;
    ndk::ScopedAStatus transmit(int32_t carrierFreqHz,
                               const std::vector<int32_t>& pattern) override;

  private:
    std::mutex mLock;
};

}  // namespace aidl::android::hardware::ir
