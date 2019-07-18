#!/bin/sh
#
# 运行 executable 的脚本，将提供相关的环境变量

PROG=$(dirname $0)
JUDGER="$PROG/.."
export RUNGUARD="$JUDGER/runguard/bin/runguard"
export JUDGE_UTILS="$JUDGER/exec/utils"
export RUNUSER="$(id -un)"
export RUNGROUP="$(id -un)"
export SCRIPTTIMELIMIT=10
export SCRIPTMEMLIMIT=262144
export SCRIPTFILELIMIT=524288

"$@"
