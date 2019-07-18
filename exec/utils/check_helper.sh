#!/bin/bash
#
# 测试脚本帮助文件，帮助处理命令行参数，避免重复代码
#
# 用法：$0 <datadir> <timelimit> <chrootdir> <workdir> <run-uuid> <run> <compare> <source files> <assist files>
#
# <datadir>      包含数据文件的文件夹的绝对路径
# <timelimit>    运行时间限制，格式为 %d:%d，如 1:3 表示测试点时间限制
#                为 1s，如果运行时间超过 3s 则结束程序
# <chrootdir>    子环境
# <workdir>      程序的工作文件夹，为了保证安全，请务必将运行路径设置
#                为空文件夹，特别是保证不可以包含标准输出文件
# <run-uuid>     运行的 uuid，用于索引运行文件夹位置
# <run>          运行程序的脚本的文件夹
# <compare>      比较程序/脚本文件夹的文件夹
# <source-files> 源文件集，使用 : 隔开，如 a.cpp:b.cpp
# <assist-files> 头文件集，使用 : 隔开，如 a.hpp:b.hpp
#
# 必须包含的环境变量：
#   RUNGUARD        runguard 的路径
#   RUNUSER         选手程序运行的账户
#   RUNGROUP        选手程序运行的账户组
#   SCRIPTMEMLIMIT  比较脚本运行内存限制
#   SCRIPTTIMELIMIT 比较脚本执行时间
#   SCRIPTFILELIMIT 比较脚本输出限制

set -e
trap 'cleanup ; error' EXIT

cleanup ()
{
    # 删除创建过的文件

    if [ -s runguard.err ]; then
        echo "************* runguard error *************" >> system.out
        cat runguard.err >> system.out
    fi
}

cleanexit ()
{
    set +e
    trap - EXIT

    cleanup

    exit $1
}

read_metadata()
{
    local metafile
    metafile="$1"

    if [ ! -r $metafile ]; then
        error "'$metafile' is not readable"
    fi
    
    timeused=$(grep '^time-used: '    "$metafile" | sed 's/time-used: //'   )
    cputime=$( grep '^cpu-time: '     "$metafile" | sed 's/cpu-time: //'    )
    walltime=$(grep '^wall-time: '    "$metafile" | sed 's/wall-time: //'   )
    progexit=$(grep '^exitcode: '     "$metafile" | sed 's/exitcode: //'    )
    stdout=$(  grep '^stdout-bytes: ' "$metafile" | sed 's/stdout-bytes: //')
    stderr=$(  grep '^stderr-bytes: ' "$metafile" | sed 's/stderr-bytes: //')
    memused=$( grep '^memory-bytes: ' "$metafile" | sed 's/memory-bytes: //')
    signal=$(  grep '^signal: '       "$metafile" | sed 's/signal: //'      )
    resource_usage="\
    runtime: ${cputime}s cpu, ${walltime}s wall
    memory used: ${memused} bytes"
}

# 导入 runcheck 函数
. "$JUDGE_UTILS/utils.sh"
. "$JUDGE_UTILS/logging.sh"
. "$JUDGE_UTILS/chroot_setup.sh"

CPUSET=""
OPTINT=1
while getopts "n:" opt; do
    case $opt in
        n)
            OPTSET="$OPTARG"
            ;;
        :)
            >&2 echo "Option -$OPTARG requires an argument."
            ;;
    esac
done

shift $((OPTIND-1))
[ "$1" == "--" ] && shift

CPUSET_OPT=""
if [ -n "$CPUSET" ]; then
    CPUSET_OPT="-P $CPUSET"
fi

MEMLIMIT_OPT=""
if [ -n "$MEMLIMIT" ]; then
    MEMLIMIT_OPT="--memory-limit $MEMLIMIT"
fi

FILELIMIT_OPT=""
if [ -n "$FILELIMIT" ]; then
    FILELIMIT_OPT="--file-limit $FILELIMIT --stream-size $FILELIMIT"
fi

PROCLIMIT_OPT=""
if [ -n "$PROCLIMIT" ]; then
    PROCLIMIT_OPT="--nproc $PROCLIMIT"
fi

LOGFILE="$LOGDIR/judge.$(hostname | cut -d . -f 1).log"
LOGLEVEL=$LOG_DEBUG
PROGNAME="$(basename "$0")"

if [ "$DEBUG" ]; then
    export VERBOSE=$LOG_DEBUG
else
    export VERBOSE=$LOG_ERR
fi

GAINROOT="sudo -n"

[ $# -ge 9 ] || error "not enough arguments"

DATADIR="$1"; shift
TIMELIMIT="$1"; shift
CHROOTDIR="$1"; shift
WORKDIR="$1"; shift
RUN_UUID="$1"; shift
RUN_SCRIPT="$1"; shift
COMPARE_SCRIPT="$1"; shift
SOURCE_FILES="$1"; shift
ASSIST_FILES="$1"

if [ ! -d "$WORKDIR" ] || [ ! -w "$WORKDIR" ] || [ ! -x "$WORKDIR" ]; then
    error "Working directory does not exist: $WORKDIR"
fi

[ -x "$RUNGUARD" ] || error "runguard does not exist"

chmod a+x "$WORKDIR"

RUNDIR="$WORKDIR/run-$RUN_UUID"
mkdir -m 0777 -p "$RUNDIR"

cd "$RUNDIR"