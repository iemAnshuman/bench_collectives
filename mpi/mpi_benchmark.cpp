// MPI reference benchmark for collective operations.
//
// Times MPI_Bcast / MPI_Reduce / MPI_Scatter / MPI_Gather / MPI_Allgather /
// MPI_Allreduce / MPI_Alltoall over a configurable
// message size and iteration count, validates correctness, and appends the
// per-run mean/variance to result/mpi/<collective>/runtimes_<collective>_mpi.txt.
//
// All collectives share a single timing/recording harness (CollectiveBench::run);
// each collective only supplies its buffer setup, the timed MPI call, and a
// correctness check.

#include <mpi.h>

#include <cxxopts.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int kRoot = 0;

int mpi_rank() {
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    return rank;
}

int mpi_size() {
    int size = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    return size;
}

// Build `localities` rows of `test_size` ints (row i filled with start_val + i),
// flattened into one contiguous send buffer. Used as the scatter source so that
// rank r receives `test_size` copies of `start_val + r`.
std::vector<int> generate_matrix(int localities, int test_size, int start_val) {
    std::vector<int> send_data;
    send_data.reserve(static_cast<std::size_t>(localities) * test_size);
    for (int i = 0; i < localities; ++i) {
        send_data.insert(send_data.end(), test_size, start_val + i);
    }
    return send_data;
}

struct Stats {
    double mean = 0.0;
    double variance = 0.0;  // population variance
};

Stats compute_moments(const std::vector<double>& data) {
    if (data.empty()) {
        return {};
    }
    double sum = 0.0;
    for (double x : data) {
        sum += x;
    }
    const double mean = sum / static_cast<double>(data.size());

    double variance_sum = 0.0;
    for (double x : data) {
        variance_sum += (x - mean) * (x - mean);
    }
    return {mean, variance_sum / static_cast<double>(data.size())};
}

void create_parent_dir(const fs::path& file_path) {
    const fs::path dir = file_path.parent_path();
    if (dir.empty() || fs::exists(dir)) {
        return;
    }
    std::error_code ec;
    if (!fs::create_directories(dir, ec)) {
        throw std::runtime_error("Failed to create directory: " + dir.string() +
                                 " (" + ec.message() + ")");
    }
}

void write_to_file(const std::string& collective, const std::string& module,
                   int algorithm, int num_ranks, int rpn, int warmup,
                   int iterations, int size, const std::vector<double>& result) {
    const Stats stats = compute_moments(result);
    const int nodes = (rpn > 0) ? num_ranks / rpn : num_ranks;

    std::cout << "\nCollective:        " << collective
              << "\nModule:            " << module
              << "\nAlgorithm:         " << algorithm
              << "\nNodes:             " << nodes
              << "\nRanks:             " << num_ranks
              << "\nRanks/node:        " << rpn
              << "\nSize/Rank:         " << size
              << "\nWarmup iterations: " << warmup
              << "\nIterations:        " << iterations
              << "\nMean runtime:      " << stats.mean
              << "\nVariance:          " << stats.variance << '\n'
              << std::flush;

    const fs::path out_path = fs::path("result") / "mpi" / collective /
                              ("runtimes_" + collective + "_mpi.txt");
    create_parent_dir(out_path);

    static const std::string header =
        "collective;module;algorithm;nodes;ranks;rpn;size;warmup;iterations;mean;variance\n";

    // Write the header only once, when the file is new or empty.
    bool need_header = true;
    if (std::ifstream in{out_path}; in.good()) {
        need_header = (in.peek() == std::ifstream::traits_type::eof());
    }

    std::ofstream out(out_path, std::ios_base::app);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + out_path.string());
    }
    if (need_header) {
        out << header;
    }
    out << collective << ';' << module << ';' << algorithm << ';' << nodes << ';'
        << num_ranks << ';' << rpn << ';' << size << ';' << warmup << ';'
        << iterations << ';' << stats.mean << ';' << stats.variance << '\n';
}

// Shared driver for all collectives: runs `iterations` timed rounds and records
// the result on the root rank. Each collective supplies:
//   prepare(i)    - untimed per-iteration buffer setup
//   collective()  - the timed MPI call
//   barrier       - whether to MPI_Barrier inside the timed region
//   check(i)      - correctness validation
//   max_time      - if true, reduce elapsed times with MPI_MAX across all ranks
//                   so root records the slowest rank's latency (use for symmetric
//                   collectives: all_gather, all_reduce, all_to_all)
struct CollectiveBench {
    std::string name;
    std::string module;
    int algorithm = 0;
    int rpn = 16;
    int iterations = 10;
    int warmup = 3;
    int test_size = 1;

    template <typename Prepare, typename Collective, typename Check>
    void run(Prepare prepare, Collective collective, bool barrier, Check check,
             bool max_time = false) const {
        const int rank = mpi_rank();
        const int size = mpi_size();
        std::vector<double> result(static_cast<std::size_t>(std::max(iterations, 0)), 0.0);

        // Untimed warmup — allows MPI to initialise internal buffers and
        // routes so that iteration 0 of the timed loop is not an outlier.
        for (int i = 0; i < warmup; ++i) {
            prepare(i);
            MPI_Barrier(MPI_COMM_WORLD);
            collective();
            if (barrier) {
                MPI_Barrier(MPI_COMM_WORLD);
            }
        }

        for (int i = 0; i < iterations; ++i) {
            prepare(i);

            // Synchronise all ranks before timing to eliminate entry-skew noise.
            MPI_Barrier(MPI_COMM_WORLD);
            const double t_before = MPI_Wtime();
            collective();
            if (barrier) {
                MPI_Barrier(MPI_COMM_WORLD);
            }
            const double t_elapsed = MPI_Wtime() - t_before;

            // For symmetric collectives every rank participates equally; the
            // slowest rank determines the effective completion time. Reduce
            // with MPI_MAX so root records the true worst-case latency.
            // For asymmetric collectives root's local time is sufficient.
            double t_record = t_elapsed;
            if (max_time) {
                MPI_Reduce(rank == kRoot ? MPI_IN_PLACE : &t_elapsed, &t_record,
                           1, MPI_DOUBLE, MPI_MAX, kRoot, MPI_COMM_WORLD);
            }

            if (rank == kRoot) {
                result[static_cast<std::size_t>(i)] = t_record;
            }
            check(i);
        }

        if (rank == kRoot) {
            write_to_file(name, module, algorithm, size, rpn, warmup, iterations,
                          test_size, result);
        }
    }
};

void test_scatter(const CollectiveBench& cfg) {
    const int rank = mpi_rank();
    const int size = mpi_size();
    std::vector<int> send_data;
    std::vector<int> recv_data(static_cast<std::size_t>(cfg.test_size), 0);

    cfg.run(
        [&](int i) {
            if (rank == kRoot) {
                send_data = generate_matrix(size, cfg.test_size, 42 + i);
            }
        },
        [&] {
            MPI_Scatter(send_data.data(), cfg.test_size, MPI_INT, recv_data.data(),
                        cfg.test_size, MPI_INT, kRoot, MPI_COMM_WORLD);
        },
        /*barrier=*/true,
        [&](int i) {
            for (int value : recv_data) {
                if (value != 42 + i + rank) {
                    std::cerr << "ERROR: scatter mismatch (size " << cfg.test_size
                              << ", ranks " << size << ")\n";
                    break;
                }
            }
        });
}

void test_broadcast(const CollectiveBench& cfg) {
    const int rank = mpi_rank();
    const int size = mpi_size();
    std::vector<int> data(static_cast<std::size_t>(cfg.test_size), 0);

    cfg.run(
        [&](int i) {
            if (rank == kRoot) {
                std::fill(data.begin(), data.end(), i);
            }
        },
        [&] { MPI_Bcast(data.data(), cfg.test_size, MPI_INT, kRoot, MPI_COMM_WORLD); },
        /*barrier=*/false,
        [&](int i) {
            if (!data.empty() && data[0] != i) {
                std::cerr << "ERROR: broadcast mismatch (size " << cfg.test_size
                          << ", ranks " << size << ")\n";
            }
        });
}

void test_reduce(const CollectiveBench& cfg) {
    const int rank = mpi_rank();
    const int size = mpi_size();
    std::vector<int> send_data(static_cast<std::size_t>(cfg.test_size), 0);
    std::vector<int> recv_data(static_cast<std::size_t>(cfg.test_size), 0);

    cfg.run(
        [&](int i) { std::fill(send_data.begin(), send_data.end(), i); },
        [&] {
            MPI_Reduce(send_data.data(), recv_data.data(), cfg.test_size, MPI_INT,
                       MPI_SUM, kRoot, MPI_COMM_WORLD);
        },
        /*barrier=*/true,
        [&](int i) {
            if (rank == kRoot && !recv_data.empty() && recv_data[0] != size * i) {
                std::cerr << "ERROR: reduce mismatch (size " << cfg.test_size
                          << ", ranks " << size << ")\n";
            }
        });
}

void test_gather(const CollectiveBench& cfg) {
    const int rank = mpi_rank();
    const int size = mpi_size();
    std::vector<int> send_data(static_cast<std::size_t>(cfg.test_size), rank);
    std::vector<int> recv_data(static_cast<std::size_t>(cfg.test_size) * size, 0);

    cfg.run(
        [&](int) { std::fill(send_data.begin(), send_data.end(), rank); },
        [&] {
            MPI_Gather(send_data.data(), cfg.test_size, MPI_INT, recv_data.data(),
                       cfg.test_size, MPI_INT, kRoot, MPI_COMM_WORLD);
        },
        /*barrier=*/true,
        [&](int) {
            if (rank != kRoot) {
                return;
            }
            for (int j = 0; j < size; ++j) {
                if (recv_data[static_cast<std::size_t>(j) * cfg.test_size] != j) {
                    std::cerr << "ERROR: gather mismatch (size " << cfg.test_size
                              << ", ranks " << size << ")\n";
                    break;
                }
            }
        });
}

void test_all_gather(const CollectiveBench& cfg) {
    const int rank = mpi_rank();
    const int size = mpi_size();
    std::vector<int> send_data(static_cast<std::size_t>(cfg.test_size), rank);
    std::vector<int> recv_data(static_cast<std::size_t>(cfg.test_size) * size, 0);

    cfg.run(
        [&](int) { std::fill(send_data.begin(), send_data.end(), rank); },
        [&] {
            MPI_Allgather(send_data.data(), cfg.test_size, MPI_INT, recv_data.data(),
                          cfg.test_size, MPI_INT, MPI_COMM_WORLD);
        },
        /*barrier=*/false,
        [&](int) {
            for (int j = 0; j < size; ++j) {
                if (recv_data[static_cast<std::size_t>(j) * cfg.test_size] != j) {
                    std::cerr << "ERROR: all_gather mismatch (size " << cfg.test_size
                              << ", ranks " << size << ")\n";
                    break;
                }
            }
        },
        /*max_time=*/true);
}

void test_all_reduce(const CollectiveBench& cfg) {
    const int size = mpi_size();
    std::vector<int> send_data(static_cast<std::size_t>(cfg.test_size), 0);
    std::vector<int> recv_data(static_cast<std::size_t>(cfg.test_size), 0);

    cfg.run(
        [&](int i) { std::fill(send_data.begin(), send_data.end(), i); },
        [&] {
            MPI_Allreduce(send_data.data(), recv_data.data(), cfg.test_size, MPI_INT,
                          MPI_SUM, MPI_COMM_WORLD);
        },
        /*barrier=*/false,
        [&](int i) {
            if (!recv_data.empty() && recv_data[0] != size * i) {
                std::cerr << "ERROR: all_reduce mismatch (size " << cfg.test_size
                          << ", ranks " << size << ")\n";
            }
        },
        /*max_time=*/true);
}

void test_all_to_all(const CollectiveBench& cfg) {
    const int rank = mpi_rank();
    const int size = mpi_size();
    std::vector<int> send_data(static_cast<std::size_t>(cfg.test_size) * size, 0);
    std::vector<int> recv_data(static_cast<std::size_t>(cfg.test_size) * size, 0);

    cfg.run(
        [&](int i) {
            for (int j = 0; j < size; ++j) {
                std::fill(send_data.begin() + j * cfg.test_size,
                          send_data.begin() + (j + 1) * cfg.test_size,
                          rank + j + i);
            }
        },
        [&] {
            MPI_Alltoall(send_data.data(), cfg.test_size, MPI_INT, recv_data.data(),
                         cfg.test_size, MPI_INT, MPI_COMM_WORLD);
        },
        /*barrier=*/false,
        [&](int i) {
            for (int j = 0; j < size; ++j) {
                const int expected = j + rank + i;
                if (recv_data[static_cast<std::size_t>(j) * cfg.test_size] != expected) {
                    std::cerr << "ERROR: all_to_all mismatch (size " << cfg.test_size
                              << ", ranks " << size << ")\n";
                    break;
                }
            }
        },
        /*max_time=*/true);
}

}  // namespace

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    cxxopts::Options options("benchmark_collectives_mpi",
                             "MPI reference benchmark for collective operations");
    options.add_options()
        ("rpn", "Number of ranks per node",
         cxxopts::value<int>()->default_value("16"))
        ("algorithm", "Algorithm id for the operation (recorded in output)",
         cxxopts::value<int>()->default_value("0"))
        ("m,module", "Collective module label recorded in output",
         cxxopts::value<std::string>()->default_value("tuned"))
        ("iterations", "Number of timed iterations",
         cxxopts::value<int>()->default_value("10"))
        ("warmup_iterations", "Number of untimed warmup iterations",
         cxxopts::value<int>()->default_value("3"))
        ("t,test_size", "Number of ints sent per rank",
         cxxopts::value<int>()->default_value("1"))
        ("operation", "Collective to run: broadcast|reduce|scatter|gather|all_gather|all_reduce|all_to_all",
         cxxopts::value<std::string>()->default_value("scatter"))
        ("h,help", "Print usage");

    int exit_code = 0;
    try {
        const auto parsed = options.parse(argc, argv);
        if (parsed.count("help") != 0u) {
            if (mpi_rank() == kRoot) {
                std::cout << options.help() << '\n';
            }
            MPI_Finalize();
            return 0;
        }

        CollectiveBench cfg;
        cfg.rpn = parsed["rpn"].as<int>();
        cfg.algorithm = parsed["algorithm"].as<int>();
        cfg.module = parsed["module"].as<std::string>();
        cfg.iterations = parsed["iterations"].as<int>();
        cfg.warmup = parsed["warmup_iterations"].as<int>();
        cfg.test_size = parsed["test_size"].as<int>();
        cfg.name = parsed["operation"].as<std::string>();

        if (cfg.name == "scatter") {
            test_scatter(cfg);
        } else if (cfg.name == "reduce") {
            test_reduce(cfg);
        } else if (cfg.name == "broadcast") {
            test_broadcast(cfg);
        } else if (cfg.name == "gather") {
            test_gather(cfg);
        } else if (cfg.name == "all_gather") {
            test_all_gather(cfg);
        } else if (cfg.name == "all_reduce") {
            test_all_reduce(cfg);
        } else if (cfg.name == "all_to_all") {
            test_all_to_all(cfg);
        } else {
            if (mpi_rank() == kRoot) {
                std::cerr << "Unknown operation: " << cfg.name
                          << " (expected broadcast|reduce|scatter|gather|all_gather|all_reduce|all_to_all)\n";
            }
            exit_code = 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        exit_code = 1;
    }

    MPI_Finalize();
    return exit_code;
}
