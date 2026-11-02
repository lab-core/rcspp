#!/usr/bin/env bash
# The runs of fixes A and B, one after another; appends each RESULT line to ab-runs.txt. From the repo root.
cd "$(dirname "$0")/../../.."
export PYTHONPATH=build-python-highs/python
OUT=benchmark-results/colgen/highs-dual-replay/ab-runs.txt
mkdir -p benchmark-results/colgen/highs-dual-replay/progress
run() {
    echo "# $(date +%H:%M:%S) $*" >> "$OUT"
    # One line per LP solve, to follow the run: progress/<arguments>.txt
    export AB_PROGRESS="benchmark-results/colgen/highs-dual-replay/progress/$(echo "$*" | tr -d '-' | tr ' ' '_').txt"
    rm -f "$AB_PROGRESS"
    timeout 7200 .venv/Scripts/python.exe benchmark-results/colgen/highs-dual-replay/ab_run.py "$@" 2>&1 \
        | grep RESULT >> "$OUT" || echo "NO RESULT (timeout 2 h or crash): $*" >> "$OUT"
}
# done 2026-11-02 16:05: run R201_50 --init single
# done 2026-11-02 16:05: run R201_50 --master fallback --factor 1e6
# done 2026-11-02 16:05: run R201_50 --master highs --factor 1e4
# done 2026-11-02 16:05: run R201_50 --master fallback --factor 1e4
# done 2026-11-02 16:05: run R201_50 --master fallback --factor 1e4 --pricing tabu
run R201 --master fallback --factor 1e4
run R201 --master fallback --factor 1e4 --pricing tabu
run R201 --init single
run R201 --init single --pricing tabu
echo "# done $(date +%H:%M:%S)" >> "$OUT"
