#!/bin/sh
#
# 创建 sudoers 允许评测系统调用 mount 等命令
# 用法：$0 <JUDGE_USER> <RUNGUARD>
# <JUDGE_USER>: 评测系统使用的用户名
# <RUNGUARD>: runguard 路径
#
# 注意！评测系统需要使用两个账户
# 1. 给评测系统使用的账户，允许使用 mount 命令
# 2. 给选手程序使用的账户，没有 sudoers

$JUDGE_USER="$1"
$RUNGUARD="$2"
if [ -z $JUDGE_USER ]; then
    echo "Judge username cannot be empty" 1>&2
    exit 1
fi

cat > "$DEST" <<EOF
# sudoers configuration for judge-system
# check the file paths, give it file mode 0440
# and place this file into /etc/sudoers.d/
#
# Consider a way with higher security

$JUDGE_USER ALL=(root) NOPASSWD: $RUNGUARD *
$JUDGE_USER ALL=(root) NOPASSWD: /bin/mount *
$JUDGE_USER ALL=(root) NOPASSWD: /bin/umount *
$JUDGE_USER ALL=(root) NOPASSWD: /usr/bin/lsof *
EOF