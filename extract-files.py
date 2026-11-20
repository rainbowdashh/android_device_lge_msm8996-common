#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

import re
from pathlib import Path

from extract_utils.fixups_blob import blob_fixup
from extract_utils.fixups_lib import lib_fixups
from extract_utils.main import ExtractUtils, ExtractUtilsModule


namespace_imports = [
    'device/lge/msm8996-common',
    'hardware/lge',
    'hardware/qcom-caf/msm8996',
    'hardware/qcom-caf/wlan',
    'vendor/qcom/opensource/dataservices',
    'vendor/qcom/opensource/display',
]


def lib_fixup_vendor(lib, partition, *args, **kwargs):
    if partition != 'vendor':
        return None
    return f'{lib}_vendor' if lib == 'com.qualcomm.qti.dpm.api@1.0' else ''


lib_fixups = {
    **lib_fixups,
    (
        'com.qualcomm.qti.dpm.api@1.0',
        'libgps.utils',
        'liblbs_core',
        'libloc_core',
        'libloc_api_v02',
        'libloc_pla',
        'liblocation_api',
    ): lib_fixup_vendor,
}


def enable_rmnet(ctx, file, file_path, *args, **kwargs):
    path = Path(file_path)

    def fix_target(match):
        target = re.sub(r'^.*name="rtm_rmnet_data_enabled".*\n', '', match[0], flags=re.M)
        target, count = re.subn(
            r'^(?=[ \t]*<data name="rmnet_data_enabled")',
            '      <data name="rtm_rmnet_data_enabled" type="int"> 1 </data>\n',
            target,
            flags=re.M,
        )
        if count != 1:
            raise ValueError('Expected one rmnet data flag')
        return target

    text, count = re.subn(
        r'<listitem name\s*=\s*"msm8996">.*?</listitem>',
        fix_target,
        path.read_text(),
        flags=re.S,
    )
    if count != 1:
        raise ValueError('Expected one msm8996 netmgr target')
    path.write_text(text)


blob_fixups = {
    'system/priv-app/UnifiedSettingsApp/UnifiedSettingsApp.apk': blob_fixup()
        .apktool_patch('patches/UnifiedSettingsApp'),
    'system_ext/etc/permissions/qti_libpermissions.xml': blob_fixup()
        .regex_replace(r'name="android\.hidl\.manager-V1\.0-java', 'name="android.hidl.manager@1.0-java'),
    (
        'system/lib/libdovi.so',
        'system/lib64/libdovi.so',
    ): blob_fixup()
        .add_needed('libgui_shim.so'),
    (
        'system/lib/libkeystore_binder.so',
        'system/lib64/libkeystore_binder.so',
    ): blob_fixup()
        .replace_needed('libbinder.so', 'libbinder-v32.so')
        .replace_needed('libprotobuf-cpp-lite.so', 'libprotobuf-cpp-lite-v29.so'),
    (
        'system/lib/liblgkm.so',
        'system/lib64/liblgkm.so',
    ): blob_fixup()
        .add_needed('libbase_shim.so'),
    (
        'system_ext/etc/init/dpmd.rc',
        'system_ext/etc/permissions/dpmapi.xml',
    ): blob_fixup()
        .regex_replace('/system/product/', '/system/system_ext/'),
    'system_ext/etc/permissions/qcrilhook.xml': blob_fixup()
        .regex_replace('/product/framework', '/system/system_ext/framework'),
    'system_ext/lib64/libdpmframework.so': blob_fixup()
        .replace_needed('libhidltransport.so', 'libcutils-v29.so'),
    (
        'vendor/bin/hw/vendor.display.color@1.0-service',
        'vendor/lib/vendor.display.color@1.0.so',
        'vendor/lib/vendor.display.color@1.1.so',
        'vendor/lib/vendor.display.color@1.2.so',
        'vendor/lib/vendor.display.postproc@1.0.so',
        'vendor/lib/vendor.qti.hardware.radio.am@1.0_vendor.so',
        'vendor/lib/vendor.qti.hardware.radio.atcmdfwd@1.0_vendor.so',
        'vendor/lib/vendor.qti.hardware.radio.ims@1.0_vendor.so',
        'vendor/lib/vendor.qti.hardware.radio.lpa@1.0_vendor.so',
        'vendor/lib/vendor.qti.hardware.radio.qcrilhook@1.0_vendor.so',
        'vendor/lib/vendor.qti.hardware.radio.qtiradio@1.0_vendor.so',
        'vendor/lib/vendor.qti.hardware.radio.uim@1.0_vendor.so',
        'vendor/lib/vendor.qti.hardware.radio.uim_remote_client@1.0_vendor.so',
        'vendor/lib/vendor.qti.hardware.radio.uim_remote_server@1.0_vendor.so',
        'vendor/lib/vendor.qti.hardware.tui_comm@1.0_vendor.so',
        'vendor/lib64/libsecureui_svcsock.so',
        'vendor/lib64/vendor.display.color@1.0.so',
        'vendor/lib64/vendor.display.color@1.1.so',
        'vendor/lib64/vendor.display.color@1.2.so',
        'vendor/lib64/vendor.display.postproc@1.0.so',
        'vendor/lib64/vendor.qti.hardware.radio.am@1.0_vendor.so',
        'vendor/lib64/vendor.qti.hardware.radio.atcmdfwd@1.0_vendor.so',
        'vendor/lib64/vendor.qti.hardware.radio.ims@1.0_vendor.so',
        'vendor/lib64/vendor.qti.hardware.radio.lpa@1.0_vendor.so',
        'vendor/lib64/vendor.qti.hardware.radio.qcrilhook@1.0_vendor.so',
        'vendor/lib64/vendor.qti.hardware.radio.qtiradio@1.0_vendor.so',
        'vendor/lib64/vendor.qti.hardware.radio.uim@1.0_vendor.so',
        'vendor/lib64/vendor.qti.hardware.radio.uim_remote_client@1.0_vendor.so',
        'vendor/lib64/vendor.qti.hardware.radio.uim_remote_server@1.0_vendor.so',
        'vendor/lib64/vendor.qti.hardware.tui_comm@1.0_vendor.so',
    ): blob_fixup()
        .replace_needed('libhidlbase.so', 'libhidlbase-v32.so'),
    'vendor/bin/pm-service': blob_fixup()
        .add_needed('libutils-v33.so'),
    'vendor/etc/data/netmgr_config.xml': blob_fixup()
        .call(enable_rmnet, need_tmp_dir=False),
    'vendor/lib/hw/camera.msm8996.so': blob_fixup()
        .binary_regex_replace(rb'service\.bootanim\.exit', b'service.bootanim.zzzz')
        .add_needed('libshim_camera.so'),
    (
        'vendor/lib/libAutoContrast.so',
        'vendor/lib/libCmcPdaf.so',
        'vendor/lib/libSJFingerDetect.so',
        'vendor/lib/libarcsoft_beauty_shot.so',
        'vendor/lib/libarcsoft_object_tracking.so',
        'vendor/lib/libchromaflash.so',
        'vendor/lib/libcir_driver.so',
        'vendor/lib/libfilm_emulation.so',
        'vendor/lib/libHDR.so',
        'vendor/lib/liblgmda.so',
        'vendor/lib/liblghdri.so',
        'vendor/lib/libmorpho_image_stab31.so',
        'vendor/lib/libmorpho_superzoom.so',
        'vendor/lib/libmpbase.so',
        'vendor/lib/liboptizoom.so',
        'vendor/lib/libseemore.so',
        'vendor/lib/libtrueportrait.so',
        'vendor/lib/libts_detected_face_hal.so',
        'vendor/lib/libts_face_beautify_hal.so',
        'vendor/lib/libubifocus.so',
        'vendor/lib64/libcir_driver.so',
        'vendor/lib64/libseemore.so',
        'vendor/lib64/libts_detected_face_hal.so',
        'vendor/lib64/libts_face_beautify_hal.so',
    ): blob_fixup()
        .replace_needed('libstdc++.so', 'libstdc++_vendor.so'),
    (
        'vendor/lib/libdovi.so',
        'vendor/lib64/libdovi.so',
    ): blob_fixup()
        .add_needed('libgui_shim_vendor.so'),
    'vendor/lib/libmmcamera_faceproc2.so': blob_fixup()
        .fix_soname(),
    'vendor/lib/libmmcamera_hdr_gb_lib.so': blob_fixup()
        .replace_needed('libstdc++.so', 'libstdc++_vendor.so')
        .add_needed('liblog.so'),
    'vendor/lib/libmmcamera_ppeiscore.so': blob_fixup()
        .add_needed('libshim_camera.so'),
    (
        'vendor/lib/libfpfactory_jni.so',
        'vendor/lib/liblgae_main.so',
        'vendor/lib/liblgawb_main.so',
        'vendor/lib/libmmcamera_pdaf.so',
        'vendor/lib/libmmcamera_pdafcamif.so',
        'vendor/lib/libmmcamera_tintless_bg_pca_algo.so',
        'vendor/lib64/libfpfactory_jni.so',
    ): blob_fixup()
        .add_needed('liblog.so'),
    (
        'vendor/lib/vulkan.msm8996.so',
        'vendor/lib64/vulkan.msm8996.so',
    ): blob_fixup()
        .fix_soname(),
    (
        'vendor/lib64/libril-qc-hal-qmi.so',
        'vendor/lib64/libsettings.so',
    ): blob_fixup()
        .replace_needed('libprotobuf-cpp-full.so', 'libprotobuf-cpp-full-v29.so'),
    'vendor/lib/libsymphony-1.1.1.so': blob_fixup()
        .fix_soname(),
    'vendor/lib/libsymphonypower-1.1.1.so': blob_fixup()
        .fix_soname(),
    (
        'vendor/lib64/libwvhidl.so',
        'vendor/lib64/mediadrm/libwvdrmengine.so',
    ): blob_fixup()
        .replace_needed('libprotobuf-cpp-lite.so', 'libprotobuf-cpp-lite-v29.so')
        .add_needed('libcrypto_shim.so'),
}  # fmt: skip

module = ExtractUtilsModule(
    'msm8996-common',
    'lge',
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
)

if __name__ == '__main__':
    ExtractUtils.device(module).run()
