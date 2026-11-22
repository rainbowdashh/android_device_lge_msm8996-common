/*
 * Copyright (C) 2016 The Android Open Source Project
 * Copyright (C) 2016 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "ThermalHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/thermal.h>

#define MAX_LENGTH                    50

#define CPU_USAGE_FILE                "/proc/stat"
#define THERMAL_CLASS                  "/sys/class/thermal"
#define CPU_ONLINE_FILE_FORMAT        "/sys/devices/system/cpu/cpu%d/online"
#define CPU_NUM                       4

//qcom, therm-reset-temp
#define CPU_SHUTDOWN_THRESHOLD        115
//qcom, limit-temp
#define CPU_THROTTLING_THRESHOLD      60
#define BATTERY_SHUTDOWN_THRESHOLD    60
// device/google/marlin/thermal-engine-marlin.conf
#define SKIN_THROTTLING_THRESHOLD     44
#define SKIN_SHUTDOWN_THRESHOLD       70
#define VR_THROTTLED_BELOW_MIN        58

#define GPU_LABEL                     "GPU"
#define BATTERY_LABEL                 "battery"
#define SKIN_LABEL                    "skin"

static const char *const CPU_LABEL[CPU_NUM] = {"CPU0", "CPU1", "CPU2", "CPU3"};

struct sensor {
    const char *zone_type;
    const char *name;
    int type;
    float scale;
    float throttling_threshold;
    float shutdown_threshold;
    float vr_threshold;
};

#define CPU_SENSOR(zone, label) { zone, label, DEVICE_TEMPERATURE_CPU, 0.1f, \
        CPU_THROTTLING_THRESHOLD, CPU_SHUTDOWN_THRESHOLD, UNKNOWN_TEMPERATURE }

static const struct sensor sensors[] = {
    // Preserve the CPU sensor identities observed on H990 with the old HAL.
    CPU_SENSOR("tsens_tz_sensor3", "CPU0"),
    CPU_SENSOR("tsens_tz_sensor5", "CPU1"),
    CPU_SENSOR("tsens_tz_sensor8", "CPU2"),
    CPU_SENSOR("tsens_tz_sensor10", "CPU3"),
    // The KGSL DTS node selects sensor14 (the thermal-engine alias is separate).
    { "tsens_tz_sensor14", GPU_LABEL, DEVICE_TEMPERATURE_GPU, 0.1f,
        UNKNOWN_TEMPERATURE, UNKNOWN_TEMPERATURE, UNKNOWN_TEMPERATURE },
    { "battery", BATTERY_LABEL, DEVICE_TEMPERATURE_BATTERY, 0.001f,
        UNKNOWN_TEMPERATURE, BATTERY_SHUTDOWN_THRESHOLD, UNKNOWN_TEMPERATURE },
    // Keep the legacy skin proxy. The PMIC ADC already reports degrees Celsius.
    { "pa_therm0", SKIN_LABEL, DEVICE_TEMPERATURE_SKIN, 1.0f,
        SKIN_THROTTLING_THRESHOLD, SKIN_SHUTDOWN_THRESHOLD, VR_THROTTLED_BELOW_MIN },
};

#define TEMPERATURE_NUM (sizeof(sensors) / sizeof(sensors[0]))

static ssize_t get_temperatures(thermal_module_t *module, temperature_t *list, size_t size) {
    (void)module;
    if (list == NULL) {
        return TEMPERATURE_NUM;
    }
    if (size > TEMPERATURE_NUM) {
        size = TEMPERATURE_NUM;
    }
    if (size == 0) {
        return TEMPERATURE_NUM;
    }

    bool found[TEMPERATURE_NUM] = {false};
    for (size_t i = 0; i < size; ++i) {
        list[i] = (temperature_t) {
            .type = sensors[i].type,
            .name = sensors[i].name,
            .current_value = UNKNOWN_TEMPERATURE,
            .throttling_threshold = sensors[i].throttling_threshold,
            .shutdown_threshold = sensors[i].shutdown_threshold,
            .vr_throttling_threshold = sensors[i].vr_threshold,
        };
    }

    DIR *zones = opendir(THERMAL_CLASS);
    if (zones == NULL) {
        int error = errno;
        ALOGE("Cannot open thermal zones: %s", strerror(error));
        return -error;
    }

    const char prefix[] = "thermal_zone";
    struct dirent *entry;
    while ((entry = readdir(zones)) != NULL) {
        if (strncmp(entry->d_name, prefix, sizeof(prefix) - 1) != 0 ||
                !isdigit((unsigned char)entry->d_name[sizeof(prefix) - 1])) {
            continue;
        }
        char path[sizeof(THERMAL_CLASS) + sizeof(entry->d_name) + sizeof("/temp")];
        char type[MAX_LENGTH];
        snprintf(path, sizeof(path), "%s/%s/type", THERMAL_CLASS, entry->d_name);
        FILE *file = fopen(path, "r");
        if (file == NULL) {
            continue;
        }
        char *line = fgets(type, sizeof(type), file);
        fclose(file);
        if (line == NULL) {
            continue;
        }
        type[strcspn(type, "\n")] = '\0';

        for (size_t i = 0; i < size; ++i) {
            if (found[i] || strcmp(type, sensors[i].zone_type) != 0) {
                continue;
            }
            snprintf(path, sizeof(path), "%s/%s/temp", THERMAL_CLASS, entry->d_name);
            file = fopen(path, "r");
            if (file != NULL) {
                float raw;
                if (fscanf(file, "%f", &raw) == 1 && isfinite(raw)) {
                    list[i].current_value = raw * sensors[i].scale;
                    found[i] = true;
                }
                fclose(file);
            }
            break;
        }
    }
    closedir(zones);
    return TEMPERATURE_NUM;
}

static ssize_t get_cpu_usages(thermal_module_t *module, cpu_usage_t *list) {
    int vals, cpu_num, online;
    ssize_t read;
    uint64_t user, nice, system, idle, active, total;
    char *line = NULL;
    size_t len = 0;
    size_t size = 0;
    char file_name[MAX_LENGTH];
    FILE *file;
    FILE *cpu_file;

    if (list == NULL) {
        return CPU_NUM;
    }

    file = fopen(CPU_USAGE_FILE, "r");
    if (file == NULL) {
        ALOGE("%s: failed to open: %s", __func__, strerror(errno));
        return -errno;
    }

    while ((read = getline(&line, &len, file)) != -1) {
        // Skip non "cpu[0-9]" lines.
        if (strnlen(line, read) < 4 || strncmp(line, "cpu", 3) != 0 || !isdigit(line[3])) {
            free(line);
            line = NULL;
            len = 0;
            continue;
        }

        vals = sscanf(line, "cpu%d %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64, &cpu_num, &user,
                &nice, &system, &idle);

        free(line);
        line = NULL;
        len = 0;

        if (vals != 5 || size == CPU_NUM) {
            if (vals != 5) {
                ALOGE("%s: failed to read CPU information from file: %s", __func__,
                        strerror(errno));
            } else {
                ALOGE("/proc/stat file has incorrect format.");
            }
            fclose(file);
            return errno ? -errno : -EIO;
        }

        active = user + nice + system;
        total = active + idle;

        // Read online CPU information.
        snprintf(file_name, MAX_LENGTH, CPU_ONLINE_FILE_FORMAT, cpu_num);
        cpu_file = fopen(file_name, "r");
        online = 0;
        if (cpu_file == NULL) {
            ALOGE("%s: failed to open file: %s (%s)", __func__, file_name, strerror(errno));
            fclose(file);
            return -errno;
        }
        if (1 != fscanf(cpu_file, "%d", &online)) {
            ALOGE("%s: failed to read CPU online information from file: %s (%s)", __func__,
                    file_name, strerror(errno));
            fclose(file);
            fclose(cpu_file);
            return errno ? -errno : -EIO;
        }
        fclose(cpu_file);

        list[size] = (cpu_usage_t) {
            .name = CPU_LABEL[size],
            .active = active,
            .total = total,
            .is_online = online
        };

        size++;
    }
    fclose(file);

    if (size != CPU_NUM) {
        ALOGE("/proc/stat file has incorrect format.");
        return -EIO;
    }
    return CPU_NUM;
}

static struct hw_module_methods_t thermal_module_methods = {
    .open = NULL,
};

thermal_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = THERMAL_HARDWARE_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = THERMAL_HARDWARE_MODULE_ID,
        .name = "G5 Thermal HAL",
        .author = "The Android Open Source Project",
        .methods = &thermal_module_methods,
    },
    .getTemperatures = get_temperatures,
    .getCpuUsages = get_cpu_usages,
};
