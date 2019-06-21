#!/bin/sh
#
# 编译 executable 的脚本
#
# 用法：$0 <workdir>
#
# <cpuset_opt> 编译使用的 CPU 集合
# <workdir> executable 的文件夹，比如 /tmp/cache/executable/cpp
#                   编译生成的可执行文件在该文件夹中，编译器输出也在该文件夹中
#
# 本脚本直接调用 executable 的 build 脚本进行编译
#
# 环境变量：
#   $RUNGUARD
#   $RUNUSER
#   $RUNGROUP
#   $SCRIPTMEMLIMIT 编译脚本运行内存限制
#   $SCRIPTTIMELIMIT 编译脚本执行时间
#   $SCRIPTFILELIMIT 编译脚本输出限制
#   $E_COMPILER_ERROR 编译失败返回码
#   $E_INTERNAL_ERROR 内部错误返回码

set -e
trap error EXIT

cleanexit ()
{
    trap - EXIT

    logmsg $LOG_DEBUG "exiting, code = '$1'"
    exit $1
}


CPUSET=""
CPUSET_OPT=""
OPTIND=1
while getopts "n:" opt; do
    case $opt in
        n)
            CPUSET="$OPTARG"
            ;;
        :)
            >&2 echo "Option -$OPTARG requires an argument."
            ;;
    esac
done

shift $((OPTIND-1))
[ "$1" == "--" ] && shift

if [ -n "$CPUSET" ]; then
    CPUSET_OPT="-P $CPUSET"
    LOGFILE="$LOGDIR/judge.$(hostname | cut -d . -f 1)-$CPUSET.log"
else
    LOGFILE="$LOGDIR/judge.$(hostname | cut -d . -f 1).log"
fi

LOGLEVEL=$LOG_DEBUG
PROGNAME="$(basename "$0")"

if [ "$DEBUG" ]; then
    export VERBOSE=$LOG_DEBUG
    logmsg $LOG_NOTICE "debugging enabled, DEBUG='$DEBUG'"
else
    export VERBOSE=$LOG_ERR
fi

[ $# -ge 1 ] || error "Not enough arguments."
WORKDIR="$1"; shift

if [ ! -d "$WORKDIR" ] || [ ! -w "$WORKDIR" ] || [ ! -x "$WORKDIR" ]; then
    error "Work directory is not found or not writable: $WORKDIR"
fi

[ -x "$RUNGUARD" ] || error "runguard not found or not executable: $RUNGUARD"

cd "$WORKDIR"
mkdir -p "$WORKDIR/compile"
chmod a+rwx "$WORKDIR/compile"
cd "$WORKDIR/compile"
touch compile.out compile.meta

RUNDIR="$WORKDIR/run-compile"
mkdir -p "$RUNDIR"
chmod a+rwx "$RUNDIR"

mkdir -p "$RUNDIR/work"
mkdir -p "$RUNDIR/work/judge"
mkdir -p "$RUNDIR/merged"
mount -t overlayfs overlayfs -o upperdir="$WORKDIR/compile" "$RUNDIR/work/judge"
mount -t overlayfs overlayfs -o lowerdir="$CHROOTDIR",upperdir="$RUNDIR/work" "$RUNDIR/merged"

# 调用 runguard 来执行编译命令
exitcode=0
$GAINROOT "$RUNGUARD" ${DEBUG:+-v} $CPUSET_OPT -c \
        --root "$RUNDIR/merged" \
        --work /judge \
        --user "$RUNUSER" \
        --group "$RUNGROUP" \
        --memory-limit $SCRIPTMEMLIMIT \
        --wall-time $SCRIPTTIMELIMIT \
        --file-limit $SCRIPTFILELIMIT \
        --outmeta compile.meta \
        -- \
        "./build" > compile.tmp 2>&1 || \
        exitcode=$?

# 删除挂载点，因为我们已经确保有用的数据在 $WORKDIR/compile 中，因此删除挂载点即可。
umount -f "$RUNDIR/work/judge" >/dev/null 2>&1  || /bin/true
umount -f "$RUNDIR/merged" >/dev/null 2>&1  || /bin/true
rm -rf "$RUNDIR"

$GAINROOT chown -R "$(id -un):" "$WORKDIR/compile"
chmod -R go-w "$WORKDIR/compile"

# 检查是否编译超时，time-result 可能为空、soft-timelimit、hard-timelimit，空表示没有超时
if grep '^time-result: .*timelimit' compile.meta >/dev/null 2>&1; then
    echo "Compilation aborted after $SCRIPTTIMELIMIT seconds." > compile.out
    cat compile.tmp >> compile.out
    cleanexit ${E_COMPILER_ERROR:--1}
fi

# 检查是否编译器出错/runguard 崩溃
if [ $exitcode -ne 0 ]; then
    echo "Compilation failed with exitcode $exitcode." > compile.out
    cat compile.tmp >> compile.out
    if [ ! -s compile.meta ]; then
        printf "\n****************runguard crash*****************\n" >> compile.out
    fi
    cleanexit ${E_INTERNAL_ERROR:--1}
fi

# 检查是否成功编译出程序
if [ ! -f run ] || [ ! -x run ]; then
    echo "Compilation failed: executable is not created." > compile.out
    cat compile.tmp >> compile.out
    cleanexit ${E_COMPILER_ERROR:--1}
fi

cat compile.tmp >> compile.out
cleanexit 0
