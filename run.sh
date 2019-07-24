#!/bin/bash
#
# 提供给 Docker 的运行脚本

DIR="$(realpath "$(dirname "$0")")"

MOJ_OPT=""
if [ -n "$MOJ_CONF" ]; then
    MOJ_OPT="--enable-3 $MOJ_CONF"
fi

MCOURSE_OPT=""
if [ -n "$MCOURSE_CONF" ]; then
    MCOURSE_OPT="--enable-2 $MCOURSE_CONF"
fi

MEXAM_OPT=""
if [ -n "$MEXAM_CONF" ]; then
    MEXAM_OPT="--enable-2 $MEXAM_CONF"
fi

SICILY_OPT=""
if [ -n "$SICILY_CONF" ]; then
    SICILY_OPT="--enable-sicily $SICILY_CONF"
fi

GLOG_log_dir=/var/log/matrix GLOG_alsologtostderr=1 GLOG_colorlogtostderr=1 "$DIR/bin/judge-system" $MOJ_OPT $MCOURSE_OPT $MEXAM_OPT $SICILY_OPT --worker=1 --exec-dir="$DIR/exec" --cache-dir=/tmp --run-dir=/tmp --chroot-dir=/chroot --log-dir=/tmp --cache-random-data=100 --run-user=domjudge-run --run-group=domjudge-run
