#!/vendor/bin/sh

write() {
    if ! printf '%s' "$2" > "$1"; then
        /vendor/bin/log -p w -t init.power "Failed to write '$2' to $1"
        return 1
    fi
}

setup_policy() {
    policy="/sys/devices/system/cpu/cpufreq/policy$1"
    if write "$policy/scaling_governor" schedutil; then
        # Keep the existing rate limits from the Balanced Spectrum profile.
        write "$policy/schedutil/down_rate_limit_us" 1000
        write "$policy/schedutil/up_rate_limit_us" "$2"
    fi
}

core_control=/sys/module/msm_thermal/core_control/enabled
write "$core_control" 0
for cpu in 0 2; do
    online="/sys/devices/system/cpu/cpu$cpu/online"
    if [ -e "$online" ] && read -r state < "$online" && [ "$state" = 0 ]; then
        write "$online" 1
    fi
done
setup_policy 0 1750
setup_policy 2 2250
write "$core_control" 1

write /sys/devices/system/cpu/cpu_boost/input_boost_freq "0:1324800 2:1324800"
write /sys/devices/system/cpu/cpu_boost/input_boost_ms 40

# These are the vendor cpusets created by init.qcom.rc. Update parents first.
for group in kernel system system/performance system/background \
             application application/performance application/background; do
    write "/dev/cpuset/$group/cpus" 0-3
done

for cpubw in /sys/class/devfreq/*qcom,cpubw*; do
    [ -d "$cpubw" ] || continue
    write "$cpubw/governor" bw_hwmon || continue
    write "$cpubw/polling_interval" 50
    write "$cpubw/min_freq" 1525
    write "$cpubw/bw_hwmon/mbps_zones" "1525 5195 11863 13763"
    write "$cpubw/bw_hwmon/sample_ms" 4
    write "$cpubw/bw_hwmon/io_percent" 34
    write "$cpubw/bw_hwmon/hist_memory" 20
    write "$cpubw/bw_hwmon/hyst_length" 10
    write "$cpubw/bw_hwmon/guard_band_mbps" 0
    write "$cpubw/bw_hwmon/up_scale" 250
    write "$cpubw/bw_hwmon/idle_mbps" 1600
done

for memlat in /sys/class/devfreq/*qcom,memlat-cpu*; do
    [ -d "$memlat" ] || continue
    # Starting the monitor can fail if the CPU PMU is unavailable.
    if write "$memlat/governor" mem_latency; then
        write "$memlat/polling_interval" 10
    fi
done

# Preserve the MSM8996 v2.x workarounds pending suspend checks on those boards.
# MSM8996 Pro reports revision 1.x and doesn't use these workarounds.
read -r soc_revision < /sys/devices/soc0/revision
case "$soc_revision" in
    2.0)
        write /sys/power/wake_lock pwr_dbg
        ;;
    2.1)
        lpm=/sys/module/lpm_levels/system
        write "$lpm/pwr/pwr-l2-gdhs/idle_enabled" 0
        write "$lpm/perf/perf-l2-gdhs/idle_enabled" 0
        write "$lpm/pwr/cpu0/fpc-def/idle_enabled" N
        write "$lpm/pwr/cpu1/fpc-def/idle_enabled" N
        write "$lpm/perf/cpu2/fpc-def/idle_enabled" N
        write "$lpm/perf/cpu3/fpc-def/idle_enabled" N
        ;;
esac
write /sys/module/lpm_levels/parameters/sleep_disabled N

# Signal completion to perfd even when optional hardware setup has failed.
setprop sys.post_boot.parsed 1
