#!/bin/sh
#
# 本脚本用于创建 cgroup
# 该脚本必须在每次开机后执行一次，你可以把本脚本设置为启动脚本
# 用法： $0 <judgehostuser>

JUDGEHOSTUSER=$1; shift
CGROUPBASE=/sys/fs/cgroup

for i in cpuset memory; do
    mkdir -p $CGROUPBASE/$i
    mount -t cgroup -o$i $i $CGROUPBASE/$i/
    mkdir -p $CGROUPBASE/$i/judge
done

chown -R $JUDGEHOSTUSER $CGROUPBASE/*/domjudge

cat $CGROUPBASE/cpuset/cpuset.cpus > $CGROUPBASE/cpuset/judge/cpuset.cpus
cat $CGROUPBASE/cpuset/cpuset.mems > $CGROUPBASE/cpuset/judge/cpuset.mems
