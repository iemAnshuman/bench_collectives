#include <mpi.h>
#include <cxxopts.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

std::vector<int> generateMatrix(int localities, int test_size, int start_val) {
    std::vector<std::vector<int>> result;
    for (int i = 0; i < localities; ++i) {
        result.push_back(std::vector<int>(test_size, start_val + i));
    }
    std::vector<int> send_data;
    for (const auto& row : result) {
        send_data.insert(send_data.end(), row.begin(), row.end());
    }
    return send_data;
}

void create_parent_dir(std::filesystem::path file_path)
{
    // Create parent directory if does not exist
    std::filesystem::path dir_path = file_path.parent_path();
    if (!std::filesystem::exists(dir_path))
    {
        if (std::filesystem::create_directories(dir_path))
        {
            std::cout << "Directory created: " << dir_path << "\n";
        }
        else
        {
            throw std::runtime_error("Failed to create directory: " + dir_path.string());
        }
    }
}

std::pair<double,double> compute_moments(std::vector<double> data)
{
    // Compute mean
    double sum = 0.0;
    for (double x : data) {
        sum += x;
    }
    double mean = sum / data.size();

    // Compute variance (population variance)
    double varianceSum = 0.0;
    for (double x : data) {
        varianceSum += (x - mean) * (x - mean);
    }
    double variance = varianceSum / data.size();

    return std::make_pair(mean, variance);
}

void write_to_file(std::string collective, std::string modules, int algorithm, std::size_t num_ranks, int rpn, int iterations, int size, std::vector<double> result){
    // Compute mean and variance
    auto moments = compute_moments(result);
    // Compute nodes
    int nodes = num_ranks / rpn;
    // Print info
    std::cout
    << "\nCollective:        " << collective   << '\n'
    << "Module:            " << modules      << '\n'
    << "Algorithm:         " << algorithm    << '\n'
    << "Nodes:             " << nodes        << '\n'
    << "Ranks:             " << num_ranks    << '\n'
    << "Ranks/node         " << rpn          << '\n'
    << "Size/Rank:         " << size         << '\n'
    << "Iterations:        " << iterations   << '\n'
    << "Mean runtime:      " << moments.first  << '\n'
    << "Variance:          " << moments.second << '\n'
    << std::flush;

    // Create directory
    std::string runtime_file_path = "result/mpi/" + collective + "/runtimes_" + collective + "_mpi";
    create_parent_dir(runtime_file_path);

    // Add header if necessary
    const std::string header = "collective;module;algorithm;nodes;ranks;rpn;size;iterations;mean;variance;\n";
    // Read existing content
    std::ifstream infile(runtime_file_path);
    std::stringstream buffer;
    buffer << infile.rdbuf();
    std::string contents = buffer.str();
    infile.close();
    // Only append if header not present
    if (contents.find(header) == std::string::npos) {
        std::ofstream outfile(runtime_file_path, std::ios_base::app);
        outfile << header;
        outfile.close();
    }

    // Add runtimes
    std::ofstream outfile;
    outfile.open(runtime_file_path, std::ios_base::app);
    outfile << collective << ";"
            << modules << ";"
            << algorithm << ";"
            << nodes << ";"
            << num_ranks << ";"
            << rpn << ";"
            << size << ";"
            << iterations << ";"
            << moments.first << ";"
            << moments.second << ";\n";
    outfile.close();
}

//////////////////////////////////////////////////////////////////////////////////////
void test_scatter(int rpn, int iterations, int test_size, std::string modules, int algorithm)
{
    // Get parameters
    int this_rank, num_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &this_rank); // Get rank
    MPI_Comm_size(MPI_COMM_WORLD, &num_ranks); // Get size
    // Result vector 
    std::vector<double> result(iterations, 0.0);
    // Data
    std::vector<int> send_data;
    std::vector<int> recv_data(test_size,0.0);

    for (std::size_t i = 0; i != iterations; ++i)
    {
        double t_before, t_after;
        if (this_rank == 0)
        {
            send_data = generateMatrix(num_ranks, test_size, 42 + i);
        }

        // Time collective
        t_before = MPI_Wtime();
        MPI_Scatter(send_data.data(), test_size, MPI_INT, recv_data.data(), test_size, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        t_after = MPI_Wtime(); 
        // Write runtime into vector
        if(this_rank == 0)
        {
            result[i] = t_after - t_before;
        }

        // Check for correctness
        for (int check : recv_data)
        {
            if( 42 + i + this_rank != check)
            {
                std::cout << "ERROR with calculating Scatter with testsize " << test_size << " and " << num_ranks <<" ranks\n";
            }
        }
    }

    if (this_rank == 0)
    {
        write_to_file("scatter", modules, algorithm, num_ranks, rpn, iterations, test_size, result);
    }
}

void test_broadcast(int rpn, int iterations, int test_size, std::string modules, int algorithm)
{
    // Get parameters
    int this_rank, num_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &this_rank); // Get rank
    MPI_Comm_size(MPI_COMM_WORLD, &num_ranks); // Get size
    // Result vector
    std::vector<double> result(iterations, 0.0);
    // Data
    std::vector<int> send_data(test_size, 0.0);

    for (std::size_t i = 0; i != iterations; ++i)
    {
        double t_before, t_after;
        if (this_rank == 0)
        {
            send_data = std::vector<int>(test_size, i);
        }

        // Time collective
        t_before = MPI_Wtime();
        MPI_Bcast(send_data.data(), test_size, MPI_INT, 0, MPI_COMM_WORLD);
        t_after = MPI_Wtime(); 
        // Write runtime into vector
        if(this_rank == 0)
        {
            result[i] = t_after - t_before;
        }

        // Check for correctness
        if(i != send_data[0])
        {
            std::cout << "ERROR with calculating Broadcast with testsize " << test_size << " and " << num_ranks <<" ranks\n";
        }
    }

    if (this_rank == 0)
    {
        write_to_file("broadcast", modules, algorithm, num_ranks, rpn, iterations, test_size, result);
    }
}

void test_reduce(int rpn, int iterations, int test_size, std::string modules, int algorithm)
{
    // Get parameters
    int this_rank, num_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &this_rank); // Get rank
    MPI_Comm_size(MPI_COMM_WORLD, &num_ranks); // Get size
    // Result vector 
    std::vector<double> result(iterations, 0.0);
    // Data
    std::vector<int> send_data;
    std::vector<int> recv_data(test_size);

    for (std::size_t i = 0; i != iterations; ++i)
    {
        double t_before, t_after;
        send_data = std::vector<int>(test_size, i);

        // Time collective
        t_before = MPI_Wtime();
        MPI_Reduce(send_data.data(), recv_data.data(), test_size, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        t_after = MPI_Wtime(); 
        // Write runtime into vector
        if(this_rank == 0)
        {
            result[i] = t_after - t_before;
        }

        // Check for correctness
        if(this_rank == 0 && num_ranks * i != recv_data[0])
        {
            std::cout << "ERROR with calculating Reduce with testsize " << test_size << " and " << num_ranks <<" ranks\n";
        }
    }

    if (this_rank == 0)
    {
        write_to_file("reduce", modules, algorithm, num_ranks, rpn, iterations, test_size, result);
    }
}

void test_gather(int rpn, int iterations, int test_size, std::string modules, int algorithm)
{
    // Get parameters
    int this_rank, num_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &this_rank); // Get rank
    MPI_Comm_size(MPI_COMM_WORLD, &num_ranks); // Get size
    // Result vector
    std::vector<double> result(iterations, 0.0);
    // Data
    std::vector<int> send_data;
    std::vector<int> recv_data(test_size*num_ranks);

    for (std::size_t i = 0; i != iterations; ++i)
    {
        double t_before, t_after;
        send_data = std::vector<int>(test_size, this_rank);
        // Time collective
        t_before = MPI_Wtime();
        MPI_Gather(send_data.data(), test_size, MPI_INT, recv_data.data(),test_size, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        t_after = MPI_Wtime(); 
        // Check for correctness
        if(this_rank == 0)
        {
            for (int j  = 0; j < num_ranks; j++)
            {
                if (j != recv_data[j*test_size])
                {
                    std::cout << "ERROR with calculating Gather with testsize " << test_size << " and " << num_ranks <<" rank\n";
                }
            }
        }
        // Write runtime into vector
        result[i] = t_after - t_before;
    }

    if (this_rank == 0)
    {
        write_to_file("gather", modules, algorithm, num_ranks, rpn, iterations, test_size, result);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv) {
    MPI_Init(&argc, &argv); // Initialize MPI
    cxxopts::Options options("MPI Collective Benchmark");
    options.add_options()
        ("rpn", "Number of localities per Node", cxxopts::value<int>()->default_value("16"))
        ("algorithm", "Algorithm of the Operation", cxxopts::value<int>()->default_value("0"))
        ("m,module", "What collective module will be used.", cxxopts::value<std::string>()->default_value("tuned"))
        ("iterations", "Number of iterations", cxxopts::value<int>()->default_value("0"))
        ("t,test_size", "Test Size for each Message.", cxxopts::value<int>()->default_value("1"))
        ("operation", "What collective operation will be used.", cxxopts::value<std::string>()->default_value("scatter"));
    auto result = options.parse(argc, argv);
    int rpn = result["rpn"].as<int>();
    int algorithm = result["algorithm"].as<int>();
    std::string modules = result["module"].as<std::string>();
    int iterations = result["iterations"].as<int>();
    int test_size = result["test_size"].as<int>();
    std::string operation = result["operation"].as<std::string>();

    // Write header
    std::vector<double> data;
    // Run test
    if (operation == "scatter")
    {
        test_scatter(rpn, iterations, test_size, modules, algorithm);
    }
    else if (operation == "reduce")
    {
        test_reduce(rpn, iterations, test_size, modules, algorithm);
    }
    else if (operation == "broadcast")
    {
        test_broadcast(rpn, iterations, test_size, modules, algorithm);
    }
    else if (operation == "gather")
    {
        test_gather(rpn, iterations, test_size, modules, algorithm);
    }

    MPI_Finalize(); // Finalize MPI
    return 0;
}
