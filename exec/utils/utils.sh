# 当开启 set -e 时，通过该函数来获得命令的返回值
runcheck ()
{
    set +e
    exitcode=0
    "$@"
    exitcode=$?
    set -e
}

# 当未开启 set -e 时，通过使用该函数来保证命令运行正确
expectsuccess ()
{
    set +e
    exitcode=0
    "$@"
    exitcode=$?
    [ "$exitcode" -ne 0 ] && exit $exitcode
    set -e
}

# 报错并退出
error()
{
    >&2 echo "Error: $*"
    exit 1
}

RESULT_ERROR = 1
RESULT_AC = 42 # Accepted
RESULT_WA = 43 # Wrong Answer
RESULT_PE = 44 # Presentation Error
RESULT_PC = 54 # Partial Correct，在这种情况下，compare 会在 score.txt 存储分数（以分数的形式存储）