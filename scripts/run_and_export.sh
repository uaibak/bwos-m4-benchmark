#!/usr/bin/env bash
# Fetch, build, run correctness before throughput, then export report artifacts.
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="build"
OUT_DIR="results"
for ((i=1; i<=$#; i++)); do
  if [[ "${!i}" == "--out" ]]; then
    next=$((i + 1))
    if (( next <= $# )); then OUT_DIR="${!next}"; fi
  fi
done
mkdir -p "$BUILD_DIR" "$OUT_DIR"

show_api() {
  local header="$BUILD_DIR/_deps/stdexec-src/include/exec/detail/bwos_lifo_queue.hpp"
  if [[ -f "$header" ]]; then
    echo "Inspected queue API declarations from $header:"
    grep -n -E 'namespace experimental::execution::bwos|class lifo_queue|auto pop_back|auto steal_front|auto push_back' "$header" | head -45 || true
  else
    echo "Queue header was not fetched; expected: include/exec/detail/bwos_lifo_queue.hpp"
  fi
}

if ! command -v cmake >/dev/null 2>&1; then
  echo "ERROR: CMake 3.28+ is required for stdexec main, but cmake was not found on PATH." >&2
  exit 2
fi
if ! cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >"$BUILD_DIR/configure.log" 2>&1; then
  cat "$BUILD_DIR/configure.log" >&2
  show_api >&2
  echo "ERROR: CMake configuration failed; see the exact diagnostic above." >&2
  exit 2
fi
if ! cmake --build "$BUILD_DIR" --config Release --parallel 2 >"$BUILD_DIR/build.log" 2>&1; then
  cat "$BUILD_DIR/build.log" >&2
  show_api >&2
  echo "ERROR: Compilation failed; see the exact compiler diagnostic and inspected declarations above." >&2
  exit 2
fi

# Preserve benchmark output and exit status so failure artifacts can still be exported.
set +e
"$BUILD_DIR/bwos_benchmark" "$@" >"$BUILD_DIR/benchmark.log" 2>&1
bench_status=$?
set -e
cat "$BUILD_DIR/benchmark.log"

# Keep plotting dependencies isolated from the user's Python installation.
if [[ ! -x "$BUILD_DIR/export-venv/bin/python" ]]; then
  python3 -m venv "$BUILD_DIR/export-venv"
fi
if ! "$BUILD_DIR/export-venv/bin/python" -c 'import matplotlib' >/dev/null 2>&1; then
  "$BUILD_DIR/export-venv/bin/python" -m pip install -r requirements.txt
fi
mkdir -p "$BUILD_DIR/matplotlib-cache"
MPLCONFIGDIR="$BUILD_DIR/matplotlib-cache" "$BUILD_DIR/export-venv/bin/python" scripts/export_artifacts.py \
  --input "$BUILD_DIR/benchmark.log" --out "$OUT_DIR" --methods docs/METHODS.txt

if (( bench_status != 0 )); then
  echo "*** ABORTED: correctness failed; throughput results are INVALID. Correctness artifacts were exported to $OUT_DIR. ***" >&2
  exit "$bench_status"
fi

echo "How to cite these numbers: report the mean and sample SD, the 95% CI, the nominal steal target and achieved calibration fraction, and identify this as a two-thread Apple M4 result. See results/table_throughput.tex and docs/METHODS.txt; do not compare it directly with x86/NUMA server results."
