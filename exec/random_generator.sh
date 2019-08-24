#!/bin/bash
#
# 调用随机数据生成器生成随机数据的脚本
#
# 用法：$0 <random_gen> <std_program> <timelimit> <chrootdir> <datadir> <run>
#
# <cpuset_opt>   编译使用的 CPU 集合
# <random_gen>   随机测试生成器所属文件夹，编译生成的可执行文件在该文件夹中
# <std_program>  标准程序所属文件夹，编译生成的可执行文件在该文件夹中
# <timelimit>    运行时间限制，格式为 %d:%d，如 1:3 表示测试点时间限制
#                为 1s，如果运行时间超过 3s 则结束程序
# <chrootdir>    executable 编译时的运行环境
# <datadir>      存放生成的数据的文件夹
# <run>          运行程序的脚本的绝对路径
#
# 必须包含的环境变量：
#   RUNGUARD        runguard 的路径
#   RUNUSER         选手程序运行的账户
#   RUNGROUP        选手程序运行的账户组
#   SCRIPTMEMLIMIT  随机数据生成器运行内存限制
#   SCRIPTTIMELIMIT 随机数据生成器执行时间
#   SCRIPTFILELIMIT 随机数据生成器输出限制
#
# 可选环境变量
#   MEMLIMIT     运行内存限制，单位为 KB
#   PROCLIMIT    进程数限制
#   FILELIMIT    文件写入限制，单位为 KB

set -e
trap error EXIT

cleanexit ()
{
    trap - EXIT

    force_umount "$RUNDIR/merged/run"
    force_umount "$RUNDIR/merged"
    rm -rf "$RUNDIR" || /bin/true

    logmsg $LOG_DEBUG "exiting, code = '$1'"
    exit $1
}

. "$JUDGE_UTILS/utils.sh" # runcheck
. "$JUDGE_UTILS/logging.sh" # logmsg, error
. "$JUDGE_UTILS/chroot_setup.sh" # chroot_setup
. "$JUDGE_UTILS/runguard.sh" # read_metadata


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

LOGLEVEL=$LOG_DEBUG
PROGNAME="$(basename "$0")"

if [ "$DEBUG" ]; then
    export VERBOSE=$LOG_DEBUG
    logmsg $LOG_NOTICE "debugging enabled, DEBUG='$DEBUG'"
else
    export VERBOSE=$LOG_ERR
fi

[ $# -ge 1 ] || error "Not enough arguments."
RAN_GEN="$1"; shift
STD_PROG="$1"; shift
TIMELIMIT="$1"; shift
CHROOTDIR="$1"; shift
WORKDIR="$1"; shift # WORKDIR 包含了生成的输入数据、输出数据和程序运行临时文件
RUN_SCRIPT="$1"; shift

if [ ! -d "$WORKDIR" ] || [ ! -w "$WORKDIR" ] || [ ! -x "$WORKDIR" ]; then
    error "Work directory is not found or not writable: $WORKDIR"
fi

[ -x "$RUNGUARD" ] || error "runguard not found or not executable: $RUNGUARD"

cd "$WORKDIR"

exec >>system.out
exec 2>&1

mkdir -p "$WORKDIR/input"
chmod a+rwx "$WORKDIR/input"
mkdir -p "$WORKDIR/output"
chmod a+rwx "$WORKDIR/output"

touch random.err

RUNDIR="$WORKDIR/run"
mkdir -m 0777 -p "$RUNDIR"

chmod -R +x "$RUN_SCRIPT/run"
chmod -R +x "$RAN_GEN/run"
chmod -R +x "$STD_PROG/run"

mkdir -m 0555 -p "$RUNDIR/work"
mkdir -m 0755 -p "$RUNDIR/work/judge"
mkdir -m 0755 -p "$RUNDIR/work/run"
mkdir -m 0755 -p "$RUNDIR/merged"
$GAINROOT mount -t aufs none -odirs="$RUNDIR/work"=rw:"$CHROOTDIR"=ro "$RUNDIR/merged"
$GAINROOT mount -t aufs none -odirs="$WORKDIR/input"=rw:"$RAN_GEN"=ro "$RUNDIR/merged/judge"
$GAINROOT mount --bind -o ro "$RUN_SCRIPT" "$RUNDIR/merged/run"

chroot_start "$CHROOTDIR" "$RUNDIR/merged"

# 调用 runguard 来执行随机生成器
runcheck $GAINROOT "$RUNGUARD" ${DEBUG:+-v} $CPUSET_OPT -c \
        --root "$RUNDIR/merged" \
        --work /judge \
        --user "$RUNUSER" \
        --group "$RUNGROUP" \
        --memory-limit "$SCRIPTMEMLIMIT" \
        --wall-time "$SCRIPTTIMELIMIT" \
        --file-limit "$SCRIPTFILELIMIT" \
        --out-meta random.meta \
        -VONLINE_JUDGE=1 \
        --standard-output-file "$WORKDIR/input/testdata.in" \
        --standard-error-file random.err -- \
        /judge/run

# 删除挂载点，因为我们已经确保有用的数据在 $WORKDIR/random 中，因此删除挂载点即可。
chroot_stop "$CHROOTDIR" "$RUNDIR/merged"
force_umount "$RUNDIR/merged/judge"

echo "Checking random generator run status"
if [ ! -s random.meta ]; then
    printf "\n****************runguard crash*****************\n"
    cleanexit ${E_RANDOM_GEN_ERROR:--1}
fi
read_metadata random.meta

# 检查是否运行超时，time-result 可能为空、soft-timelimit、hard-timelimit，空表示没有超时
if grep '^time-result: .*timelimit' random.meta >/dev/null 2>&1; then
    echo "Random data generation aborted after $SCRIPTTIMELIMIT seconds."
    cat random.err
    cleanexit ${E_RANDOM_GEN_ERROR:--1}
fi

# 检查是否运行出错/runguard 崩溃
if [ $progexit -ne 0 ]; then
    echo "Random data generation failed with exitcode $progexit."
    cat random.err
    cleanexit ${E_RANDOM_GEN_ERROR:--1}
fi

cat random.err

#################################

chmod -R a+rwx "$WORKDIR/input"

$GAINROOT mount -t aufs none -odirs="$WORKDIR/output"=rw:"$STD_PROG"=ro:"$WORKDIR/input"=ro "$RUNDIR/merged/judge"
chroot_start "$CHROOTDIR" "$RUNDIR/merged"

# 调用 runguard 来执行标准程序
runcheck $GAINROOT "$RUNGUARD" ${DEBUG:+-v} $CPUSET_OPT $MEMLIMIT_OPT $FILELIMIT_OPT $PROCLIMIT_OPT \
        --root "$RUNDIR/merged" \
        --work /judge \
        --no-core-dumps \
        --user "$RUNUSER" \
        --group "$RUNGROUP" \
        --wall-time "$TIMELIMIT" \
        --standard-error-file standard.err \
        -VONLINE_JUDGE=1 \
        --out-meta standard.meta -- \
        /run/run testdata.in testdata.out /judge/run "$@"

chroot_stop "$CHROOTDIR" "$RUNDIR/merged"
force_umount "$RUNDIR/merged/judge"

# 删除挂载点在 cleanexit 中完成，因为我们已经确保有用的数据在 $WORKDIR/standard 中，因此删除挂载点即可。

echo "Checking standard program run status"
if [ ! -s standard.meta ]; then
    printf "\n****************runguard crash*****************\n"
    cleanexit ${E_RANDOM_GEN_ERROR:--1}
fi
read_metadata standard.meta

# 检查是否运行超时，time-result 可能为空、soft-timelimit、hard-timelimit，空表示没有超时
if grep '^time-result: .*timelimit' standard.meta >/dev/null 2>&1; then
    echo "Standard program aborted after $TIMELIMIT seconds."
    cat standard.err
    cleanexit ${E_RANDOM_GEN_ERROR:--1}
fi

# 检查是否运行出错/runguard 崩溃
if [ $progexit -ne 0 ]; then
    echo "Standard program failed with exitcode $progexit."
    cat standard.err
    cleanexit ${E_RANDOM_GEN_ERROR:--1}
fi

cat standard.err

cleanexit 0
