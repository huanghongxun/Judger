JUDGER="$(dirname "$0")/../.."
$JUDGER/bin/judge-system --enable-3 config-moj.json --worker 1 --exec-dir $JUDGER/exec/ --cache-dir /tmp --run-dir /tmp --chroot-dir /chroot --cache-random-data 100 --run-user domjudge-run --run-group domjudge-run
