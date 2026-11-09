#!/usr/bin/env bash
# The full comparison of the pure-Python and the C++-backed column generation layers, tier after tier.
#   bash benchmark-results/colgen/full_comparison.sh        (from the repo root; resumable: done runs are skipped)
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
EXACT=layer-cpp-highspy-exact,layer-purepython-highspy-exact,layer-cpp-gurobipy-exact,layer-purepython-gurobipy-exact
SMALL=C101_25,R101_25,R201_25,RC201_12,C101_50,R101_50,RC101_50,R201_50

tier() {  # name init instances configs repeats
  local folder="$OUT/full-$1"
  echo "TIER $1 start $(date +%H:%M)"
  COLGEN_INIT=$2 python "$OUT/run_matrix.py" --output "$folder" --instances "$3" --configs "$4" --repeats "$5" \
    > "$folder.out" 2>&1
  local failed
  failed=$(awk -F, 'NR > 1 && $4 != 0' "$folder/results.csv" | wc -l)
  echo "TIER $1 done $(date +%H:%M), failed runs: $failed"
  if [ -f "$folder/STOP" ]; then echo "STOPPED by $folder/STOP"; exit 2; fi
  if ! sed -n '/## LP mismatches/,$p' "$folder/summary.md" | grep -qx none; then
    echo "LP MISMATCH in $1:"; sed -n '/## LP mismatches/,$p' "$folder/summary.md"; exit 1
  fi
}

tier small-single single "$SMALL" "$CONFIGS" 5
tier small-bigm bigm "$SMALL" "$CONFIGS" 5
tier mid-single single C101,R101,RC101,C201 "$CONFIGS" 3
tier mid-bigm bigm C101,R101,RC101,C201 "$CONFIGS" 3
echo "PROBES start $(date +%H:%M)"
mkdir -p "$OUT/full-probes"
for pricing in exact tabu; do
  python "$OUT/pricing_gap_probe.py" --instance C201 --pricing $pricing --cap 20 --repeats 3 \
    > "$OUT/full-probes/C201-$pricing.txt" 2>&1 || echo "probe C201 $pricing failed"
done
echo "PROBES done $(date +%H:%M)"
tier long-single single R201,RC201 "$CONFIGS" 1
tier long-bigm-tabu bigm R201,RC201 "$TABU" 1
tier long-bigm-exact bigm R201,RC201 "$EXACT" 1
echo "ALL DONE $(date +%H:%M)"
