#!/usr/bin/env bash
# The runs of fixes A and B, one after another; appends each RESULT line to ab-runs.txt. From the repo root.
cd "$(dirname "$0")/../../.."
export PYTHONPATH=build-python-highs/python
OUT=benchmark-results/colgen/highs-dual-replay/ab-runs.txt
mkdir -p benchmark-results/colgen/highs-dual-replay/progress
run() {
    echo "# $(date +%H:%M:%S) $*" >> "$OUT"
    # One line per LP solve, to follow the run: progress/<arguments>.txt; the run's output: progress/<arguments>.log
    export AB_PROGRESS="benchmark-results/colgen/highs-dual-replay/progress/$(echo "$*" | tr -d '-' | tr ' ' '_').txt"
    local log="${AB_PROGRESS%.txt}.log"
    rm -f "$AB_PROGRESS"
    .venv/Scripts/python.exe benchmark-results/colgen/highs-dual-replay/ab_run.py "$@" > "$log" 2>&1 &
    local pid=$! start=$SECONDS
    # Capped at 2 h. The venv's python.exe is a launcher: kill the whole tree, not only the launcher
    while kill -0 "$pid" 2>/dev/null; do
        if (( SECONDS - start >= 7200 )); then
            taskkill //F //T //PID "$(cat /proc/$pid/winpid)" > /dev/null 2>&1
        fi
        sleep 10
    done
    wait "$pid"
    grep RESULT "$log" >> "$OUT" || echo "NO RESULT (2 h cap or crash, see the .log): $*" >> "$OUT"
}
# done 2026-11-02 16:05: run R201_50 --init single
# done 2026-11-02 16:05: run R201_50 --master fallback --factor 1e6
# done 2026-11-02 16:05: run R201_50 --master highs --factor 1e4
# done 2026-11-02 16:05: run R201_50 --master fallback --factor 1e4
# done 2026-11-02 16:05: run R201_50 --master fallback --factor 1e4 --pricing tabu
# Tabu first: the exact big-M run was still in its first pricing after 25 min on the laptop
# done 2026-11-02 23:53: run R201 --master fallback --factor 1e4 --pricing tabu
# done 2026-11-02 23:53: run R201 --init single --pricing tabu
# done 2026-11-02 23:53: run R201 --init single
# done 2026-11-02 23:53: run R201 --master fallback --factor 1e4
echo "# done $(date +%H:%M:%S)" >> "$OUT"
