#!/bin/bash
#
# 提供给 Docker 的运行脚本
# 必须以特权模式运行

DIR="$(realpath "$(dirname "$0")")"

MOJ_OPT=""
if [ -n "$MOJ_CONF" ]; then
    MOJ_OPT="--enable-3 $MOJ_CONF"
fi

MCOURSE_OPT=""
if [ -n "$MCOURSE_CONF" ]; then
    MCOURSE_OPT="--enable-2 $MCOURSE_CONF"
fi

FORTH_OPT=""
if [ -n "$FORTH_CONF" ]; then
    FORTH_OPT="--enable $FORTH_CONF"
fi

SICILY_OPT=""
if [ -n "$SICILY_CONF" ]; then
    SICILY_OPT="--enable-sicily $SICILY_CONF"
fi

mkdir -p /tmp/judge/cache
mkdir -p /tmp/judge/run

bash "$DIR/exec/create_cgroups.sh" domjudge-run

export GLOG_log_dir=/var/log/judge-system
export GLOG_alsologtostderr=1
export GLOG_colorlogtostderr=1
export ELASTIC_APM_TRANSPORT_CLASS="elasticapm.transport.http.Transport"
export CACHEDIR="/tmp/judge/cache"
export RUNDIR="/tmp/judge/run"
export CHROOTDIR="/chroot"
export CACHERANDOMDATA=4
export RUNUSER=domjudge-run
export RUNGROUP=domjudge-run
"$DIR/bin/judge-system" $MOJ_OPT $MCOURSE_OPT $FORTH_OPT $SICILY_OPT "$@"
