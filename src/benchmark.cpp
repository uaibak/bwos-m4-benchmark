// Report-oriented BWOS queue experiment: correctness is always completed first.
// stdexec main's inspected API is experimental::execution::bwos::lifo_queue<T>
// with push_back(T)->bool, pop_back()->T, and steal_front()->T. A zero T is empty.
#include <exec/detail/bwos_lifo_queue.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <thread>

namespace bwos = experimental::execution::bwos;
using Queue = bwos::lifo_queue<std::uint64_t>;
using Clock = std::chrono::steady_clock;
constexpr std::array<unsigned, 5> kRates{0, 1, 5, 10, 20};
constexpr unsigned kCorrectnessRepetitions = 20;
constexpr std::uint64_t kTasks = 1000;

struct Options {
  bool skip_correctness = false;
  unsigned throughput_reps = 20;
  unsigned duration_seconds = 2;
  std::string out_dir = "artifacts";
};

// Parse the requested command line options without adding nonstandard deps.
bool parse_options(int argc, char** argv, Options& options) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "--skip-correctness") {
      options.skip_correctness = true;
    } else if ((arg == "--reps" || arg == "--duration" || arg == "--out") && i + 1 < argc) {
      const std::string_view value(argv[++i]);
      if (arg == "--out") {
        options.out_dir = value;
      } else {
        try {
          const unsigned parsed = static_cast<unsigned>(std::stoul(std::string(value)));
          if (parsed == 0) return false;
          if (arg == "--reps") options.throughput_reps = parsed;
          else options.duration_seconds = parsed;
        } catch (...) { return false; }
      }
    } else {
      return false;
    }
  }
  return true;
}

// Part A: run exactly 20 independent 4-block x 8-entry task-transfer checks.
// The thief polls until the owner has published all tasks and it observes empty.
unsigned correctness_suite(bool skipped) {
  if (skipped) {
    std::cout << "CORRECTNESS: SKIPPED (no throughput validity claim)\n";
    return std::numeric_limits<unsigned>::max();
  }
  unsigned passes = 0;
  for (unsigned rep = 1; rep <= kCorrectnessRepetitions; ++rep) {
    Queue queue(4, 8);
    std::atomic<bool> owner_done{false};
    std::atomic<std::uint64_t> stolen{0};
    std::thread thief([&] {
      while (true) {
        if (queue.steal_front() != 0) {
          stolen.fetch_add(1, std::memory_order_relaxed);
        } else if (owner_done.load(std::memory_order_acquire)) {
          break;
        } else {
          std::this_thread::yield();
        }
      }
    });

    bool stalled = false;
    for (std::uint64_t id = 1; id <= kTasks; ++id) {
      const auto deadline = Clock::now() + std::chrono::seconds(5);
      while (!queue.push_back(id)) {
        if (Clock::now() >= deadline) { stalled = true; break; }
        std::this_thread::yield();
      }
      if (stalled) break;
    }
    owner_done.store(true, std::memory_order_release);
    thief.join();

    std::uint64_t remaining = 0;
    while (queue.pop_back() != 0) ++remaining;
    const auto stolen_count = stolen.load(std::memory_order_relaxed);
    const bool pass = !stalled && stolen_count + remaining == kTasks;
    if (pass) ++passes;
    std::cout << "CORRECTNESS_RESULT," << rep << ',' << (pass ? 1 : 0) << ','
              << stolen_count << ',' << remaining << '\n';
    if (!pass) {
      std::cerr << "TASK LOSS DETECTED in correctness repetition " << rep
                << ": pushed=" << kTasks << " stolen=" << stolen_count
                << " remaining=" << remaining << " stalled=" << stalled << '\n';
    }
  }
  if (passes == kCorrectnessRepetitions) {
    std::cout << "CORRECTNESS: PASS (20/20)\n";
  } else {
    std::cerr << "*** TASK LOSS DETECTED: " << passes << "/20 checks passed. ***\n"
              << "WARNING: stdexec's BWOS implementation has a reported memory-ordering/task-loss "
                 "issue on Apple Silicon. THROUGHPUT RESULTS ARE INVALID; throughput was skipped.\n";
  }
  return passes;
}

struct Trial {
  std::uint64_t owner_ops = 0;
  std::uint64_t owner_transactions = 0;
  std::uint64_t successful_steals = 0;
  double elapsed_seconds = 0;
};

// Perform one trial at a fixed inter-steal delay. Successful steals divided by
// owner push/pop transactions is the observed steal fraction used for tuning.
Trial run_trial(std::chrono::nanoseconds delay,
                std::chrono::steady_clock::duration duration) {
  constexpr std::uint64_t batch_size = 768;
  Queue queue(4, 256);
  std::atomic<bool> stop{false};
  std::atomic<std::uint64_t> stolen{0};
  std::thread thief([&] {
    while (!stop.load(std::memory_order_relaxed)) {
      if (delay == std::chrono::nanoseconds::max()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      if (queue.steal_front() != 0) stolen.fetch_add(1, std::memory_order_relaxed);
      if (delay.count() > 0) {
        const auto wait_until = Clock::now() + delay;
        while (Clock::now() < wait_until) {}
      }
    }
  });

  const auto start = Clock::now();
  const auto deadline = start + duration;
  std::uint64_t owner_ops = 0;
  std::uint64_t transactions = 0;
  std::uint64_t next_task = 1;
  while (Clock::now() < deadline) {
    // A short burst gives the thief a live front while owner operations remain
    // a tight push/pop loop; one-at-a-time push/pop leaves almost no steal window.
    for (std::uint64_t i = 0; i < batch_size; ++i) {
      bool pushed = queue.push_back(next_task);
      ++owner_ops;
      while (!pushed) {
        std::this_thread::yield();
        pushed = queue.push_back(next_task);
        ++owner_ops;
      }
      ++next_task;
    }
    for (std::uint64_t i = 0; i < batch_size; ++i) {
      (void) queue.pop_back();
      ++owner_ops;
      ++transactions;
    }
  }
  const auto finish = Clock::now();
  stop.store(true, std::memory_order_relaxed);
  thief.join();
  return {owner_ops, transactions, stolen.load(std::memory_order_relaxed),
          std::chrono::duration<double>(finish - start).count()};
}

// Part B calibration: adjust sleep duration using short pilot windows until
// observed successful steals/owner transactions approaches the requested rate.
// A zero delay is the saturation point; reported targets remain nominal labels.
std::chrono::nanoseconds calibrate_delay(unsigned target_percent) {
  if (target_percent == 0) return std::chrono::nanoseconds::max();
  const double target = static_cast<double>(target_percent) / 100.0;
  auto delay = std::chrono::nanoseconds(10000);
  for (unsigned attempt = 0; attempt < 8; ++attempt) {
    const Trial pilot = run_trial(delay, std::chrono::milliseconds(180));
    const double observed = pilot.owner_transactions == 0 ? 0.0
        : static_cast<double>(pilot.successful_steals) / pilot.owner_transactions;
    std::cout << "CALIBRATION," << target_percent << ',' << attempt + 1 << ','
              << delay.count() << ',' << observed << '\n';
    if (observed >= target * 0.8 && observed <= target * 1.2) break;
    if (observed <= 0.0) {
      delay = std::chrono::nanoseconds::zero();
    } else {
      const double scale = std::clamp(observed / target, 0.2, 5.0);
      const auto next = static_cast<std::int64_t>(std::llround(delay.count() * scale));
      delay = std::chrono::nanoseconds(std::max<std::int64_t>(1, next));
    }
  }
  return delay;
}

// Part C: emit machine-readable records. The wrapper preserves these and calls
// the Python exporter even when a correctness run fails.
int main(int argc, char** argv) {
  Options options;
  if (!parse_options(argc, argv, options)) {
    std::cerr << "Usage: " << argv[0]
              << " [--skip-correctness] [--reps N] [--duration S] [--out DIR]\n";
    return 2;
  }
  std::cout << "QUEUE_API,experimental::execution::bwos::lifo_queue<T>,"
               "push_back(T)->bool,pop_back()->T,steal_front()->T\n";
  std::cout << "OUTPUT_DIR," << options.out_dir << '\n';
  std::cout << "CONFIG," << options.throughput_reps << ',' << options.duration_seconds << '\n';

  const unsigned passes = correctness_suite(options.skip_correctness);
  const bool correct = passes == kCorrectnessRepetitions;
  if (!options.skip_correctness && !correct) return 1;

  for (const unsigned rate : kRates) {
    const auto delay = calibrate_delay(rate);
    for (unsigned rep = 1; rep <= options.throughput_reps; ++rep) {
      const Trial sample = run_trial(delay, std::chrono::seconds(options.duration_seconds));
      const auto ops_per_sec = static_cast<std::uint64_t>(sample.owner_ops / sample.elapsed_seconds);
      const double achieved = sample.owner_transactions == 0 ? 0.0
          : 100.0 * static_cast<double>(sample.successful_steals) / sample.owner_transactions;
      const auto delay_ns = delay == std::chrono::nanoseconds::max() ? 0 : delay.count();
      std::cout << "THROUGHPUT_RESULT," << rate << ',' << rep << ',' << ops_per_sec << ','
                << achieved << ',' << delay_ns << '\n';
    }
  }
  return 0;
}
