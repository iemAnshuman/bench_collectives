#include <mpi.h>
#include <cxxopts.hpp>
#include <string>
#include <iostream>
#include <fstream>

constexpr int ITERATIONS = 10;

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

void write_to_file(int localities_per_node, double elapsed_average, std::string operation, int test_size, std::string modules, int algorithm)
{
    std::string filename = "";
    if (modules == "tuned")
    {
        filename = operation +"/arity_" + std::to_string(algorithm) + "_lpm_" + std::to_string(localities_per_node) + "_size_" + std::to_string(test_size)+".csv";
    }
    
    // Open file in append mode
    std::ofstream file(filename, std::ios::app);


    if (!file) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    std::ifstream checkFile(filename);
    checkFile.seekg(0, std::ios::end);
    if (checkFile.tellg() > 0) {
        file << ", "; // Add a comma before appending if the file is not empty
    }
    file << elapsed_average;
    file.close();
}

void compute_moments(std::vector<double> data)
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

    std::cout << "Mean: " << mean << std::endl;
    std::cout << "Variance: " << variance << std::endl;
}

void test_scatter(int lpn, int test_size, std::string modules, int algorithm)
{
    // Get parameters
    int this_rank, num_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &this_rank); // Get rank
    MPI_Comm_size(MPI_COMM_WORLD, &num_ranks); // Get size
    // Result vector 
    std::vector<double> result(ITERATIONS, 0.0);
    // Data
    std::vector<int> send_data;
    std::vector<int> recv_data(test_size,0.0);
    if (this_rank == 0)
    {
        send_data = generateMatrix(num_ranks, test_size, 42);
    }

    for (std::size_t i = 0; i != ITERATIONS; ++i)
    {
        double t_before, t_after;
        // Time collective
        t_before = MPI_Wtime();
        MPI_Scatter(send_data.data(), test_size, MPI_INT, recv_data.data(), test_size, MPI_INT, 0, MPI_COMM_WORLD);
        t_after = MPI_Wtime(); 
        // Check for correctness
        for (int check : recv_data)
        {
            if( 42 + this_rank != check)
            {
                std::cout << "ERROR with calculating Scatter with testsize " << test_size << " and " << num_ranks <<" ranks\n";
            }
        }
        // Write runtime into vector
        if(this_rank == 0)
        {
            result[i] = t_after - t_before;
        }
    }

    if (this_rank == 0)
    {
        compute_moments(result);
    }
    
    if (this_rank == 0)
    {
        // Compute mean
        double sum = 0.0;
        for (double x : result) 
        {
            sum += x;
        }
        double mean = sum / result.size();

        std::string operation = "scatter";
        write_to_file(lpn, num_ranks, operation, test_size, modules, algorithm);
        write_to_file(lpn, mean/ITERATIONS, operation, test_size, modules, algorithm);
    }
}

void test_broadcast(int lpn, int test_size, std::string modules, int algorithm)
{
    // Get parameters
    int this_rank, num_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &this_rank); // Get rank
    MPI_Comm_size(MPI_COMM_WORLD, &num_ranks); // Get size
    // Result vector 
    std::vector<double> result(ITERATIONS, 0.0);
    // Data
    std::vector<int> send_data(test_size, 0.0);
    if (this_rank == 0)
    {
        send_data = std::vector<int>(test_size, 1.0);
    }

    for (std::size_t i = 0; i != ITERATIONS; ++i)
    {
        double t_before, t_after;
        // Time collective
        t_before = MPI_Wtime();
        MPI_Bcast(send_data.data(), test_size, MPI_INT, 0, MPI_COMM_WORLD);
        t_after = MPI_Wtime(); 
        // Check for correctness
        if(1.0 != send_data[0])
        {
            std::cout << "ERROR with calculating Broadcast with testsize " << test_size << " and " << num_ranks <<" ranks\n";
        }
        // Write runtime into vector
        if(this_rank == 0)
        {
            result[i] = t_after - t_before;
        }
    }

    if (this_rank == 0)
    {
        compute_moments(result);
    }
    
    if (this_rank == 0)
    {
        // Compute mean
        double sum = 0.0;
        for (double x : result) 
        {
            sum += x;
        }
        double mean = sum / result.size();

        std::string operation = "broadcast";
        write_to_file(lpn, num_ranks, operation, test_size, modules, algorithm);
        write_to_file(lpn, mean/ITERATIONS, operation, test_size, modules, algorithm);
    }
}

void test_reduce(int lpn, int test_size, std::string modules, int algorithm)
{
    // Get parameters
    int this_rank, num_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &this_rank); // Get rank
    MPI_Comm_size(MPI_COMM_WORLD, &num_ranks); // Get size
    // Result vector 
    std::vector<double> result(ITERATIONS, 0.0);
    // Data
    std::vector<int> send_data;
    std::vector<int> recv_data(test_size);
    send_data = std::vector<int>(test_size, 1.0);

    for (std::size_t i = 0; i != ITERATIONS; ++i)
    {
        double t_before, t_after;
        // Time collective
        t_before = MPI_Wtime();
        MPI_Reduce(send_data.data(), recv_data.data(), test_size, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        t_after = MPI_Wtime(); 
        // Check for correctness
        if(this_rank == 0 && num_ranks != recv_data[0])
        {
            std::cout << "ERROR with calculating Reduce with testsize " << test_size << " and " << num_ranks <<" ranks\n";
        }
        // Write runtime into vector
        if(this_rank == 0)
        {
            result[i] = t_after - t_before;
        }
    }

    if (this_rank == 0)
    {
        compute_moments(result);
    }
    
    if (this_rank == 0)
    {
        // Compute mean
        double sum = 0.0;
        for (double x : result) 
        {
            sum += x;
        }
        double mean = sum / result.size();

        std::string operation = "reduce";
        write_to_file(lpn, num_ranks, operation, test_size, modules, algorithm);
        write_to_file(lpn, mean/ITERATIONS, operation, test_size, modules, algorithm);
    }
}

void test_gather(int lpn, int test_size, std::string modules, int algorithm)
{
    // Get parameters
    int this_rank, num_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &this_rank); // Get rank
    MPI_Comm_size(MPI_COMM_WORLD, &num_ranks); // Get size
    // Result vector 
    std::vector<double> result(ITERATIONS, 0.0);
    // Data
    std::vector<int> send_data;
    std::vector<int> recv_data(test_size*num_ranks);
    send_data = std::vector<int>(test_size, this_rank);

    for (std::size_t i = 0; i != ITERATIONS; ++i)
    {
        double t_before, t_after;
        // Time collective
        t_before = MPI_Wtime();
        MPI_Gather(send_data.data(), test_size, MPI_INT, recv_data.data(),test_size, MPI_INT, 0, MPI_COMM_WORLD);
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
        if(this_rank == 0)
        {
            result[i] = t_after - t_before;
        }
    }

    if (this_rank == 0)
    {
        compute_moments(result);
    }
    
    if (this_rank == 0)
    {
        // Compute mean
        double sum = 0.0;
        for (double x : result) 
        {
            sum += x;
        }
        double mean = sum / result.size();

        std::string operation = "gather";
        write_to_file(lpn, num_ranks, operation, test_size, modules, algorithm);
        write_to_file(lpn, mean/ITERATIONS, operation, test_size, modules, algorithm);
    }
}



int main(int argc, char** argv) {
    MPI_Init(&argc, &argv); // Initialize MPI
    cxxopts::Options options("MyApp", "Example of command-line parsing");
    options.add_options()
        ("lpn", "Number of localities per Node", cxxopts::value<int>()->default_value("16"))
        ("algorithm", "Algorithm of the Operation", cxxopts::value<int>()->default_value("0"))
        ("m,module", "What collective operation module will be used.", cxxopts::value<std::string>()->default_value("tuned"))
        ("t,test_size", "Test Size for each Message.", cxxopts::value<int>()->default_value("1"))
        ("operation", "What collective operation module will be used.", cxxopts::value<std::string>()->default_value("scatter"));
    auto result = options.parse(argc, argv);
    int lpn = result["lpn"].as<int>();
    int algorithm = result["algorithm"].as<int>();
    std::string modules = result["module"].as<std::string>();
    int test_size = result["test_size"].as<int>();
    std::string operation = result["operation"].as<std::string>();

    // Write header
    std::vector<double> data;
    // Run test
    if (operation == "scatter")
    {
        test_scatter(lpn, test_size, modules, algorithm);
    }
    else if (operation == "reduce")
    {
        test_reduce(lpn, test_size, modules, algorithm);
    }
    else if (operation == "bcast")
    {
        test_broadcast(lpn, test_size, modules, algorithm);
    }
    else if (operation == "gather")
    {
        test_gather(lpn, test_size, modules, algorithm);
    }

    MPI_Finalize(); // Finalize MPI
    return 0;
}
