#include <mpi.h>
#include <cxxopts.hpp>
#include <string>
#include <iostream>
#include <fstream>

constexpr int ITERATIONS = 1;

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

void test_scatter(int lpn, int test_size, std::string modules, int algorithm)
{
    std::string operation = "scatter";
    int this_locality, num_localities;
    MPI_Comm_rank(MPI_COMM_WORLD, &this_locality); // Get rank
    MPI_Comm_size(MPI_COMM_WORLD, &num_localities); // Get size
    if (this_locality == 0)
    {
        write_to_file(lpn, num_localities, operation, test_size, modules, algorithm);
    }

    double t1, t2;
    t1 = MPI_Wtime(); 
    for (std::size_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> send_data;
        std::vector<int> recv_data(test_size);
        if (this_locality == 0)
        {
            send_data = generateMatrix(num_localities, test_size, 42 + i);
        }
        MPI_Scatter(send_data.data(), test_size, MPI_INT, recv_data.data(), test_size, MPI_INT, 0, MPI_COMM_WORLD);
        for (int check : recv_data)
        {
                if( 42+ i + this_locality != check)
                {
                    std::cout << "ERROR with calculating Scatter with testsize " << test_size << " and " << num_localities <<" localities\n";
                }
        }
    }

    t2 = MPI_Wtime(); 
    if (this_locality == 0)
    {
        write_to_file(lpn, (t2-t1)/ITERATIONS, operation, test_size, modules, algorithm);
    }
}

void test_broadcast(int lpn, int test_size, std::string modules, int algorithm)
{
    std::string operation = "broadcast";
    int this_locality, num_localities;
    MPI_Comm_rank(MPI_COMM_WORLD, &this_locality); // Get rank
    MPI_Comm_size(MPI_COMM_WORLD, &num_localities); // Get size
    if (this_locality == 0)
    {
        write_to_file(lpn, num_localities, operation, test_size, modules, algorithm);
    }

    double t1, t2;
    t1 = MPI_Wtime(); 
    for (std::size_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> send_data(test_size);
        if (this_locality == 0)
        {
            send_data = std::vector<int>(test_size, i);
        }
        MPI_Bcast(send_data.data(), test_size, MPI_INT, 0, MPI_COMM_WORLD);
        if(i != send_data[0])
        {
            std::cout << "ERROR with calculating Broadcast with testsize " << test_size << " and " << num_localities <<" localities\n";
        }
        MPI_Barrier(MPI_COMM_WORLD); 
    }

    t2 = MPI_Wtime();
    
    if (this_locality == 0)
    {
        write_to_file(lpn, (t2-t1)/ITERATIONS, operation, test_size, modules, algorithm);
    }
}

std::vector<double> test_reduce(int lpn, int test_size, std::string modules, int algorithm)
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
        // Timers
        double t_before, t_after;
        t_before = MPI_Wtime();
        MPI_Reduce(send_data.data(), recv_data.data(), test_size, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        t_after = MPI_Wtime(); 

        // if(this_rank == 0 && i*num_localities != recv_data[0])
        // {
        //     std::cout << "ERROR with calculating Reduce with testsize " << test_size << " and " << num_localities <<" localities\n";
        // }
        // Write runtime into vector
        if(this_rank == 0)
        {
            result[i] = t_before - t_after;
        }
    }

    return result;
    // if (this_locality == 0)
    // {
    //     write_to_file(lpn, (t2-t1)/ITERATIONS, operation, test_size, modules, algorithm);
    // }
}

void test_gather(int lpn, int test_size, std::string modules, int algorithm)
{
    /*
    std::string operation = "gather";
    int this_locality, num_localities;
    MPI_Comm_rank(MPI_COMM_WORLD, &this_locality); // Get rank
    MPI_Comm_size(MPI_COMM_WORLD, &num_localities); // Get size
                                                    //
    if (this_locality == 0)
    {
        write_to_file(lpn, num_localities, operation, test_size, modules, algorithm);
    }

    double t_before, t_after;
    t_before = MPI_Wtime();

    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
                    
        std::vector<int> send_data;
        std::vector<int> recv_data(test_size*num_localities);
        send_data = std::vector<int>(test_size, i+this_locality);
        MPI_Gather(send_data.data(), test_size, MPI_INT, recv_data.data(),test_size, MPI_INT, 0, MPI_COMM_WORLD);
        if(this_locality == 0)
        {
            for (int j  = 0; j < num_localities; j++)
            {
                if (i+j != recv_data[j*test_size])
                {
                    std::cout << "ERROR with calculating Gather with testsize " << test_size << " and " << num_localities <<" localities. Result: " << recv_data[j*num_localities] << ". Expected: "<< i+j <<"\n";
                }
            } 
        }
    }

    t2 = MPI_Wtime(); 
    if (this_locality == 0)
    {
        write_to_file(lpn, (t2-t1)/ITERATIONS, operation, test_size, modules, algorithm);
    }
    */
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
        //data = 
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
    //                 //
    //                 //
    // // Compute mean
    // double sum = 0.0;
    // for (double x : data) {
    //     sum += x;
    // }
    // double mean = sum / data.size();
    //
    // // Compute variance (population variance)
    // double varianceSum = 0.0;
    // for (double x : data) {
    //     varianceSum += (x - mean) * (x - mean);
    // }
    // double variance = varianceSum / data.size();
    //
    // std::cout << "Mean: " << mean << std::endl;
    // std::cout << "Variance: " << variance << std::endl;
    return 0;
}
