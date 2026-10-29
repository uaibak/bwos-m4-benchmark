# BWoS M4 Queue Study

A small, correctness-first study of NVIDIA stdexec's C++ LIFO BWoS queue on an Apple M4. It measures one owner and one thief on a single queue. The results are a workload-specific case study, not a reproduction of the OSDI paper's server or NUMA evaluation.

## Repository layout

- `src/benchmark.cpp` — correctness check and throughput benchmark.
- `CMakeLists.txt` — C++20 build; CMake FetchContent downloads stdexec.
- `scripts/run_and_export.sh` — configure, build, run the correctness gate, and export results.
- `scripts/export_artifacts.py` — generate CSV, plots, LaTeX tables, and report snippets.
- `docs/METHODS.txt` — machine and run details for the recorded experiment.
- `results/` — raw run log and generated CSV, figures, tables, and prose snippets.
- `paper/` — LaTeX report, bibliography, and compiled PDF.

## Reproduce the benchmark

Requirements: macOS with Apple Clang, CMake 3.28 or newer, Python 3, and network access for FetchContent. The exporter installs its plotting dependencies into the ignored `build/` directory.

From the repository root:

```sh
./scripts/run_and_export.sh
```

The script runs the 20-repetition correctness check before timed measurements. If the check fails, it exits nonzero and marks throughput invalid. Optional benchmark arguments can be passed through, for example:

```sh
./scripts/run_and_export.sh --reps 20 --duration 2 --out results
```

To rebuild the paper PDF after the results exist:

```sh
cd paper
tectonic report.tex
```

## Recorded run

The checked-in `results/` files correspond to the run described in `docs/METHODS.txt`. That file records the compiler, operating system, stdexec commit, queue parameters, calibration, and repetition counts. The CMake configuration fetches stdexec from `main`; a new run may resolve to a different commit, so preserve the generated methods file and raw log when comparing results.

## Scope

The benchmark uses one owner thread and one thief thread on one queue. It does not test multi-queue scheduling, NUMA placement, or scaling across worker counts. Owner throughput counts queue API calls, including failed push retries and empty pops; it is not application task throughput.
