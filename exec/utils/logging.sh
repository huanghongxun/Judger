LOG_FATAL=1
LOG_ERROR=2
LOG_WARNING=3
LOG_INFO=4
LOG_FINE=5
LOG_DEBUG=7

VERBOSE=$LOG_INFO
LOGLEVEL=$LOG_DEBUG

logmsg ()
{
    local msglevel stamp msg

    msglevel="$1"; shift
    msec=`date '+%N'`
    msg=`printf '%b' "$(echo "$@")"`

    if [ "$msglevel" -le "${VERBOSE:-$LOG_ERROR}" ]; then
        echo "$stamp $msg" >&2
    fi
    if [ "$msglevel" -le "${LOGLEVEL:-$LOG_DEBUG}" ]; then
        if [ "$LOGFILE" ]; then
            echo "$stamp $msg" >> $LOGFILE
        fi
        if [ "$SYSLOG" ]; then
            logger -i -t "$PROGNAME" -p "${SYSLOG#LOG_}.$msglevel" "$msg" > /dev/null 2>&1
        fi
    fi
}

error ()
{
    set +e
    trap - EXIT

    if [ "$@" ]; then
        logmsg $LOG_ERROR "error: $@"
    else
        logmsg $LOG_ERROR "unexpected error, aborting!"
    fi

    exit 127
}

warning ()
{
    logmsg $LOG_WARNING "warning: $@"
}