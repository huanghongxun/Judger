#!/bin/sh
#
# Sicily 特殊评测转换脚本，用法和 compare scripts 一致
# 用法：$0 <std.in> <user.out> <std.out>
#
# 脚本会调用当前目录下的 spjudge 来进行测试

. "$JUDGE_UTILS/utils.sh"

TESTIN = "$1"
PROGRAM = "$2"
TESTOUT = "$3"

result="$(./spjudge "$TESTIN" "$PROGRAM" "$TESTOUT")"
exitcode=$?

if echo "$result" | head -n 1 - | grep '^[Yy]' >/dev/null 2>&1; then
    exit $RESULT_AC
else
    exit $RESULT_WA
fi