#!/bin/bash
#
# 本脚本用于创建 cgroup
# 该脚本必须在每次开机后执行一次，你可以把本脚本设置为启动脚本
# 用法： $0 <judgehostuser>

error() { echo "$*" 1>&2; exit 1; }

[ $# -ge 1 ] || error "judge host username required"

JUDGEHOSTUSER=$1; shift
CGROUPBASE=/sys/fs/cgroup

for i in cpuset memory; do
    mkdir -p $CGROUPBASE/$i
    if [ ! -d $CGROUPBASE/$i/ ]; then
        if ! mount -t cgroup -o$i $i $CGROUPBASE/$i/; then
            echo "Error: Can not mount $i cgroup. Probably cgroup support is missing from running kernel. Unable to continue.
            To fix this, please make the following changes:
            1. In /etc/default/grub, add 'cgroup_enable=memory swapaccount=1' to GRUB_CMDLINE_LINUX_DEFAULT
            2. Run update-grub
            3. Reboot" >&2
            exit 1
        fi
    fi
    mkdir -p $CGROUPBASE/$i/judger
done

if [ ! -f $CGROUPBASE/memory/memory.limit_in_bytes ] || [ ! -f $CGROUPBASE/memory/memory.memsw.limit_in_bytes ]; then
    echo "Error: cgroup support missing memory features in running kernel. Unable to continue.
    To fix this, please make the following changes:
    1. In /etc/default/grub, add 'cgroup_enable=memory swapaccount=1' to GRUB_CMDLINE_LINUX_DEFAULT
    2. Run update-grub
    3. Reboot" >&2
    exit 1
fi

chown -R $JUDGEHOSTUSER $CGROUPBASE/*/judger

cat $CGROUPBASE/cpuset/cpuset.cpus > $CGROUPBASE/cpuset/judger/cpuset.cpus
cat $CGROUPBASE/cpuset/cpuset.mems > $CGROUPBASE/cpuset/judger/cpuset.mems
