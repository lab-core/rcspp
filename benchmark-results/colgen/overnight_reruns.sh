#!/usr/bin/env bash
# Overnight re-runs after the full comparison (step H): the closest cell, a quiet small tier, a second long repeat.
#   bash benchmark-results/colgen/overnight_reruns.sh        (from anywhere; resumable: done runs are skipped)
# Stops at the first tier whose summary lists an LP mismatch. A file STOP in the tier's folder stops after its run.
set -u
cd "$(dirname "$0")/../.."
export PATH="$PWD/.venv/Scripts:$PATH" PYTHONPATH=python/src
OUT=benchmark-results/colgen
CONFIGS=""
for pricing in exact tabu; do
  for master in highspy gurobipy; do
    CONFIGS="$CONFIGS,layer-cpp-$master-$pricing,layer-purepython-$master-$pricing"
  done
done
CONFIGS=${CONFIGS#,}
TABU=layer-cpp-highspy-tabu,layer-purepython-highspy-tabu,layer-cpp-gurobipy-tabu,layer-purepython-gurobipy-tabu
SMALL=C101_25,R101_25,R201_25,RC201_12,C101_50,R101_50,RC101_50,R201_50

tier() {  # folder init instances configs repeats
  local folder="$OUT/$1"
  echo "TIER $1 start $(date +%H:%M)"
  COLGEN_INIT=$2 python "$OUT/run_matrix.py" --output "$folder" --instances "$3" --configs "$4" --repeats "$5" \
    >> "$folder.out" 2>&1
  local failed
  failed=$(awk -F, 'NR > 1 && $4 != 0' "$folder/results.csv" | wc -l)
  echo "TIER $1 done $(date +%H:%M), failed runs: $failed"
  if [ -f "$folder/STOP" ]; then echo "STOPPED by $folder/STOP"; exit 2; fi
  if ! sed -n '/## LP mismatches/,$p' "$folder/summary.md" | grep -qx none; then
    echo "LP MISMATCH in $1:"; sed -n '/## LP mismatches/,$p' "$folder/summary.md"; exit 1
  fi
}

tier rerun-rc201-tabu single RC201 "$TABU" 3           # the closest cell (+4.6 %, 1 repeat): 12 runs, ~10 min
tier rerun-small-single single "$SMALL" "$CONFIGS" 5     # the small tier on a quieter machine: 320 runs, ~30 min
tier full-long-single single R201,RC201 "$CONFIGS" 2     # repeat 1 of the long tier (repeat 0 is done): 16 runs, ~5 h
echo "ALL DONE $(date +%H:%M)"
