## Results

Across the five target steal settings, mean owner throughput peaked at 577,652,880 operations per second at 0% and measured 529,463,453 operations per second at 20%. Relative drops were 1%: 7.9%; 5%: 8.0%; 10%: 8.7%; 20%: 8.3%. Throughput fell under the 0.90% condition and then varied little as achieved rates plateaued near 5.4%. Means summarize 20 trials; error bars show sample SD. The reported steal percentages are calibration targets; timed trials achieved target 1% achieved 0.90%; target 5% achieved 5.38%; target 10% achieved 5.40%; target 20% achieved 5.39%; calibration.csv records each achieved rate and selected delay. Targets 10% and 20% were not reached. Correctness checks preceded all valid throughput measurements.

Word count: 120 (target: 120).

## Correctness

The correctness suite completed 20 independent repetitions, with 20 passes and 0 failures. Each repetition used one owner and one thief, a queue configured for four blocks of eight entries, and 1,000 nonzero task IDs. The owner retried bounded-queue insertions until every task was accepted; the thief polled until the owner finished and the queue appeared empty. After joining the thief, the owner drained remaining entries. A repetition passed when stolen plus remaining equaled the number pushed. This count-based check detects net loss but does not independently prove uniqueness or ordering. Throughput is valid only after all checks pass overall.

Word count: 100 (target: 100).

## Threats to validity

This experiment uses Apple Silicon unified memory and has no NUMA topology, so it cannot reproduce remote-memory placement, socket contention, or the x86/NUMA server conditions reported in the original study. The MacBook Air M4 provides 10 CPU cores, but the experiment deliberately uses two worker threads: one queue owner and one thief. The implementation is a third-party stdexec port of the BWoS queue, not a reimplementation validated against a formal assumption or source revision in the OSDI 2023 paper. FetchContent follows stdexec main, which can change over time. Spin-wait calibration consumes thief CPU time and is sensitive to operating-system scheduling, thermal state, and background activity. The correctness test checks total task accounting, not all histories, memory-order executions, or linearizability.

Word count: 120 (target: 120).

## Numbers to report

- Peak mean throughput: 577,652,880 ops/s at 0% target steal; 20% target steal: 529,463,453 ops/s; relative drops from 0% at 1/5/10/20%: 1%: 7.9%; 5%: 8.0%; 10%: 8.7%; 20%: 8.3%.
