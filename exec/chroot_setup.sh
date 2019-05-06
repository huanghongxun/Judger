#!/bin/sh

# 将 CHROOTORIGINAL 文件夹挂载到当前目录下
#    chroot_setup.sh <CHROOTORIGINAL> <OP>
#    CHROOTORIGINAL: 已经准备好的 chroot 环境
#    OP: check/start/stop
#
# 该脚本用于为解释型语言准备基础环境
# 
# 我们需要准备程序运行必要的最小环境，而环境内的所有数据
# 从 chroot_make.sh 来构建

set -e

SUBDIRMOUNTS="etc usr lib lib64 bin"

CHROOTORIGINAL="$1"

# $1: unmount mountpoint
dj_umount() {
    set +e
    if ! sudo -n umount "$1" < /dev/null; then
        >&2 echo "umount '$1' did not succeed, trying harder"
        if ! sudo -n umount -f -vvv "$1" < /dev/null; then
            >&2 echo "umount '$1' failed"
            >&2 lsof +c 15 +D "$1"
            exit 1
        fi
    fi
    set -e
}

case "$1" in
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

        ;;
    start)
        mkdir -p proc
        sudo -n mount -n --bind /proc proc < /dev/null

        for i in $SUBDIRMOUNTS; do
            if [ -L "$CHROOTORIGINAL/$i" ]; then
                ln -s "$(readlink "$CHROOTORIGINAL/$i")" "$i"
            elif [ -d "$CHROOTORIGINAL/$i" ]; then
                mkdir -p $i
                sudo -n mount --bind "$CHROOTORIGINAL/$i" "$i" < /dev/null
                # 为了更加安全，我们以只读的方式挂载目录防止潜在的漏洞
                sudo -n mount -o remount,ro,bind "$PWD/$i" </dev/null
            fi
        done

        mkdir -p dev

esac