#!/bin/sh

# 将 CHROOTORIGINAL 文件夹挂载到目标目录下
#    chroot_setup.sh <CHROOTORIGINAL> <DESTINATION> <OP>
#    CHROOTORIGINAL: 已经准备好的 chroot 环境
#    OP: check/start/stop
#
# 该脚本用于为解释型语言准备基础环境
# 
# 我们需要准备程序运行必要的最小环境，而环境内的所有数据
# 从 chroot_make.sh 来构建

set -e

SUBDIRMOUNTS="etc usr lib lib32 libx32 lib64 bin proc"

CHROOTORIGINAL="$1"
DEST="$2"

# $1: unmount mountpoint
all_umount() {
    set +e
    if ! umount "$1" < /dev/null; then
        >&2 echo "umount '$1' did not succeed, trying harder"
        if ! umount -f -vvv "$1" < /dev/null; then
            >&2 echo "umount '$1' failed"
            >&2 lsof +c 15 +D "$1"
            exit 1
        fi
    fi
    set -e
}

case "$3" in
    check)
        if [ ! -d "$CHROOTORIGINAL" ]; then
            >&2 echo "chroot dir '$CHROOTORIGINAL' does not exist"
            exit 2
        fi

        for i in $SUBDIRMOUNTS; do
            if [ ! -e "$CHROOTORIGINAL/$i" ]; then
                >&2 echo "chroot subdir '$CHROOTORIGINAL/$i' not found"
                exit 2
            fi
        done

        # /debootstrap 会在 debootstrap 完成 chroot 构建之后删除
        # 如果文件夹仍然存在，说明 debootstrap 未能正常完成 chroot 构建
        # 则不能进行绑定
        if [ -d "$CHROOTORIGINAL/debootstrap" ]; then
            >&2 echo "chroot dir '$CHROOTORIGINAL' incomplete, rerun chroot_make.sh"
            exit 2
        fi

        ;;
    start)
        # mount proc filesystem (needed by Java for /proc/self/stat)
#        mkdir -p $DEST/proc
#        mount -n --bind /proc $DEST/proc < /dev/null

        for i in $SUBDIRMOUNTS; do

            # Preserve links like /lib64 -> /lib
            if [ -L "$CHROOTORIGINAL/$i" ]; then
                ln -s "$(readlink "$CHROOTORIGINAL/$i")" "$DEST/$i"
            elif [ -d "$CHROOTORIGINAL/$i" ]; then
                mkdir -p $i
                mount --bind "$CHROOTORIGINAL/$i" "$DEST/$i" < /dev/null
                # 为了更加安全，我们以只读的方式挂载目录防止潜在的漏洞
                mount -o remount,ro,bind "$DEST/$i" </dev/null
            fi
        done

        mkdir -p $DEST/dev
        cp -pR /dev/random  $DEST/dev < /dev/null
        cp -pR /dev/urandom $DEST/dev < /dev/null
        chmod o-w $DEST/dev/random $DEST/dev/urandom < /dev/null
        ;;

    stop)
#        all_umount "$DEST/proc"

        rm -f $DEST/dev/random
        rm -f $DEST/dev/urandom
        rmdir $DEST/dev || true

        for i in $SUBDIRMOUNTS ; do
            if [ -L "$CHROOTORIGINAL/$i" ]; then
                rm -f $i
            elif [ -d "$CHROOTORIGINAL/$i" ]; then
                all_umount "$DEST/$i"
            fi
        done

        ;;
    *)
        echo "Unknown argument '$1' given"
        exit 1
esac

exit 0
