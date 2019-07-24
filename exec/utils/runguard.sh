read_metadata()
{
    local metafile
    metafile="$1"

    if [ ! -r $metafile ]; then
        error "'$metafile' is not readable"
    fi
    
    timeused=$(grep '^time-used: '      "$metafile" | sed 's/time-used: //'      )
    cputime=$( grep '^cpu-time: '       "$metafile" | sed 's/cpu-time: //'       )
    walltime=$(grep '^wall-time: '      "$metafile" | sed 's/wall-time: //'      )
    progexit=$(grep '^exitcode: '       "$metafile" | sed 's/exitcode: //'       )
    stdout=$(  grep '^stdout-bytes: '   "$metafile" | sed 's/stdout-bytes: //'   )
    stderr=$(  grep '^stderr-bytes: '   "$metafile" | sed 's/stderr-bytes: //'   )
    memused=$( grep '^memory-bytes: '   "$metafile" | sed 's/memory-bytes: //'   )
    signal=$(  grep '^signal: '         "$metafile" | sed 's/signal: //'         )
    interr=$(  grep '^internal-error: ' "$metafile" | sed 's/internal-error: //' )
    resource_usage="\
    runtime: ${cputime}s cpu, ${walltime}s wall
    memory used: ${memused} bytes"
}