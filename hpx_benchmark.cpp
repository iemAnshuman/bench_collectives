//  Copyright (c) 2020-2023 Hartmut Kaiser
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>

#if !defined(HPX_COMPUTE_DEVICE_CODE)
#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>
#include <hpx/modules/collectives.hpp>
#include <hpx/modules/testing.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <fstream>

using namespace hpx::collectives;

constexpr char const* scatter_direct_basename = "/test/scatter_direct/";
constexpr char const* reduce_direct_basename = "/test/reduce_direct/";
constexpr char const* broadcast_direct_basename = "/test/broadcast_direct/";
constexpr char const* gather_direct_basename = "/test/gather_direct/";
constexpr char const* scatter_direct_basenames[] = {"/test/scatter_direct/0/", "/test/scatter_direct/1/", "/test/scatter_direct/2/", "/test/scatter_direct/3/"};

constexpr int ITERATIONS = 100;


struct VectorAdder {
    std::vector<int> operator()(const std::vector<int>& a,
                                          const std::vector<int>& b) const {
        if (a.size() != b.size()) {
            throw std::runtime_error("Vector sizes must match!");
        }

        std::vector<int> result(a.size());
        for (size_t i = 0; i < a.size(); ++i) {
            result[i] = a[i] + b[i];
        }
        return result;
    }
};

std::vector<std::vector<int>> generateMatrix(int localities, int test_size, int start_val) {
    std::vector<std::vector<int>> result;
    for (int i = 0; i < localities; ++i) {
        result.push_back(std::vector<int>(test_size, start_val + i));
    }
    return result;
}


void write_to_file(int arity, int localities_per_node, double elapsed_average, std::string operation, int test_size)
{
    std::string filename = operation +"/arity_" + std::to_string(arity) + "_lpm_" + std::to_string(localities_per_node) + "_size_" + std::to_string(test_size)+".csv";

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

void test_scatter_hierarchical(int arity, int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(arity, lpn, num_localities, operation, test_size);
    }
    if (num_localities <= arity)
       {
            if (this_locality == 0)
            {
                write_to_file(arity, lpn, 0, operation, test_size);
            } 
            return;
        }
    auto communicators = create_hierarchical_communicator(scatter_direct_basename, num_sites_arg(num_localities), this_site_arg(this_locality),generation_arg(1), root_site_arg(0), arity);
    char const* const barrier_test_name = "/test/barrier/hierarchical";

    hpx::distributed::barrier b(barrier_test_name);
    hpx::chrono::high_resolution_timer const t;
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        if (this_locality == 0)
        {
            std::vector<std::vector<int>> data = generateMatrix(num_localities, test_size, 42 + i);
            hpx::future<std::vector<int>> result = scatter_to_hierarchically(communicators, std::move(data), this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            for (int check : result.get())
            {
                HPX_TEST_EQ(42+ i +this_locality, check);
            }
        }
        else
        {
            hpx::future<std::vector<int>> result = scatter_from_hierarchically<std::vector<int>>(communicators, this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            for (int check : result.get())
            {
                HPX_TEST_EQ(42+ i +this_locality, check);
            }
        }
        //b.wait();
        
    }
    auto const elapsed = t.elapsed();
            if (this_locality == 0)
            {
                 write_to_file(arity, lpn, elapsed/ITERATIONS, operation, test_size);
            }
    
}

void test_reduce_hierarchical(int arity, int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(arity, lpn, num_localities, operation, test_size);
    }
    if (num_localities <= arity)
       {
            if (this_locality == 0)
            {
                write_to_file(arity, lpn, 0, operation, test_size);
            } 
            return;
        }
    auto communicators = create_hierarchical_communicator(reduce_direct_basename, num_sites_arg(num_localities), this_site_arg(this_locality),generation_arg(1), root_site_arg(0), arity);
    char const* const barrier_test_name = "/test/barrier/hierarchical";

    hpx::distributed::barrier b(barrier_test_name);
    hpx::chrono::high_resolution_timer const t;
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> data = std::vector<int>(test_size, i);
        //auto data = i;

        if (this_locality == 0)
        {
            hpx::future<std::vector<int>> result = reduce_hierarchically_here(communicators, std::move(data), VectorAdder{},this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            //hpx::future<std::uint32_t> result = reduce_hierarchically_here(communicators, std::move(data), std::plus<std::uint32_t>{},this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            HPX_TEST_EQ(i*num_localities, result.get()[0]);
        }
        else
        {
            hpx::future<void> overall_result = reduce_hierarchically_there(communicators, std::move(data), VectorAdder{},this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            //hpx::future<void> overall_result = reduce_hierarchically_there(communicators, std::move(data), std::plus<std::uint32_t>{},this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            overall_result.get();
        }
        //b.wait();
        
    }
    auto const elapsed = t.elapsed();
            if (this_locality == 0)
            {
                 write_to_file(arity, lpn, elapsed/ITERATIONS, operation, test_size);
            }
    
}

void test_broadcast_hierarchical(int arity, int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(arity, lpn, num_localities, operation, test_size);
    }
    if (num_localities <= arity)
       {
            if (this_locality == 0)
            {
                write_to_file(arity, lpn, 0, operation, test_size);
            } 
            return;
        }
    auto communicators = create_hierarchical_communicator(broadcast_direct_basename, num_sites_arg(num_localities), this_site_arg(this_locality),generation_arg(1), root_site_arg(0), arity);
    char const* const barrier_test_name = "/test/barrier/hierarchical";

    hpx::distributed::barrier b(barrier_test_name);
    hpx::chrono::high_resolution_timer const t;
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> data = std::vector<int>(test_size, i);
        //auto data = i;

        if (this_locality == 0)
        {
            hpx::future<std::vector<int>> result = broadcast_to_hierarchically(communicators, std::move(data), this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            //hpx::future<std::uint32_t> result = reduce_hierarchically_here(communicators, std::move(data), std::plus<std::uint32_t>{},this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            HPX_TEST_EQ(i, result.get()[0]);
        }
        else
        {
            hpx::future<std::vector<int>> result = broadcast_from_hierarchically<std::vector<int>>(communicators, this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            //hpx::future<void> overall_result = reduce_hierarchically_there(communicators, std::move(data), std::plus<std::uint32_t>{},this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            HPX_TEST_EQ(i, result.get()[0]);

        }
        //b.wait();
        
    }
    auto const elapsed = t.elapsed();
            if (this_locality == 0)
            {
                 write_to_file(arity, lpn, elapsed/ITERATIONS, operation, test_size);
            }
    
}

void test_gather_hierarchical(int arity, int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(arity, lpn, num_localities, operation, test_size);
    }
    if (num_localities <= arity)
       {
            if (this_locality == 0)
            {
                write_to_file(arity, lpn, 0, operation, test_size);
            } 
            return;
        }
    auto communicators = create_hierarchical_communicator(gather_direct_basename, num_sites_arg(num_localities), this_site_arg(this_locality),generation_arg(1), root_site_arg(0), arity);
    char const* const barrier_test_name = "/test/barrier/hierarchical";

    hpx::distributed::barrier b(barrier_test_name);
    hpx::chrono::high_resolution_timer const t;
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> data = std::vector<int>(test_size, i+this_locality);
        //auto data = i;

        if (this_locality == 0)
        {
            std::vector<std::vector<int>> result = gather_here_hierarchically(communicators, std::move(data), this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            //hpx::future<std::uint32_t> result = reduce_hierarchically_here(communicators, std::move(data), std::plus<std::uint32_t>{},this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            for (int j  = 0; j < num_localities; j++)
            {
                HPX_TEST_EQ(i+j, result[j][0]);
            }
        }
        else
        {
            hpx::future<void> result = gather_there_hierarchically(communicators, std::move(data), this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
            //hpx::future<void> overall_result = reduce_hierarchically_there(communicators, std::move(data), std::plus<std::uint32_t>{},this_site_arg(this_locality), generation_arg(i+1), root_site_arg(0), arity);
        }
        //b.wait();
        
    }
    auto const elapsed = t.elapsed();
            if (this_locality == 0)
            {
                 write_to_file(arity, lpn, elapsed/ITERATIONS, operation, test_size);
            }
    
}

void test_multiple_use_non_hierarchy()
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);

    std::uint32_t const this_locality = hpx::get_locality_id();

    //hpx::chrono::high_resolution_timer const t;
    auto const scatter_direct_client = create_communicator(scatter_direct_basename, num_sites_arg(num_localities), this_site_arg(this_locality));
    hpx::chrono::high_resolution_timer const t;
    // test functionality based on immediate local result value
    for (std::uint32_t i = 0; i != 100; ++i)
    {
        if (this_locality == 0)
        {
            std::vector<std::uint32_t> data(num_localities);
            std::iota(data.begin(), data.end(), 42 + i);

            hpx::future<std::uint32_t> result =
                scatter_to(scatter_direct_client, std::move(data), this_site_arg(this_locality), generation_arg(i+1));

            HPX_TEST_EQ(i + 42 + this_locality, result.get());
        }
        else
        {
            hpx::future<std::uint32_t> result =
                scatter_from<std::uint32_t>(scatter_direct_client, this_site_arg(this_locality), generation_arg(i+1));

            HPX_TEST_EQ(i + 42 + this_locality, result.get());
        }
    }
    auto const elapsed = t.elapsed();
            if (this_locality == 0)
            {
                std::cout << "non_hierarchical_timing: " << elapsed << "[s]\n";
            }
}

void test_scatter()
{
	std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);



    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
            {
                std::vector<std::uint32_t> data(num_localities-1);
                std::iota(data.begin(), data.end(), 42);

                scatter_to(scatter_direct_basename, std::move(data),
                        num_sites_arg(num_localities - 1), this_site_arg(this_locality),
                        generation_arg(1));
            }
            else
            {
                hpx::future<std::uint32_t> from_result =
                    scatter_from<std::uint32_t>(scatter_direct_basename,
                        this_site_arg(this_locality-1), generation_arg(1));

                HPX_TEST_EQ(42 + this_locality - 1, from_result.get());

            }

}

void test_hierarchical_better(int children)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);



    std::uint32_t const this_locality = hpx::get_locality_id();
    std::string scatter_sub_basename = "/test/scatter_direct/" + std::to_string(this_locality % children) + "/";
    int group_size = num_localities/children;

    hpx::chrono::high_resolution_timer const t;

    if (num_localities % children != 0)
    {
	    if (this_locality == 0)
	    {
		    std::cout << "Number of children doesn't properly divide  number of localities\n";
	    }
	    return;
    }

    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
            if (this_locality == 0)
            {
                std::vector<std::vector<std::uint32_t>> data(children, std::vector<std::uint32_t>(group_size));
                for (int j = 0; j < children; ++j)
		{
			std::iota(data[j].begin(), data[j].end(), 42 + i+j*(group_size));
		}

                hpx::future<std::vector<std::uint32_t>> from_result =
                    scatter_to(scatter_direct_basename, std::move(data),
                        num_sites_arg(children), this_site_arg(this_locality),
                        generation_arg(i + 1));

                hpx::future<std::uint32_t> result =
                    scatter_to(scatter_sub_basename.c_str(), from_result.get(),
                        num_sites_arg(group_size), this_site_arg(this_locality/children),
                        generation_arg(i + 1));

                HPX_TEST_EQ(i + 42 + (this_locality/children)+(group_size*(this_locality % children)), result.get());
            }
            else if (this_locality < children)
            {
                hpx::future<std::vector<std::uint32_t>> from_result =
                    scatter_from<std::vector<std::uint32_t>>(scatter_direct_basename,
                        this_site_arg(this_locality), generation_arg(i + 1));

                hpx::future<std::uint32_t> result =
                    scatter_to(scatter_sub_basename.c_str(), from_result.get(),
                        num_sites_arg(group_size), this_site_arg(this_locality/children),
                        generation_arg(i + 1));
                HPX_TEST_EQ(i + 42 + (this_locality/children)+(group_size*(this_locality % children)), result.get());

            }
            else
            {
                hpx::future<std::uint32_t> result =
                    scatter_from<std::uint32_t>(scatter_sub_basename.c_str(),
                        this_site_arg(this_locality/children), generation_arg(i + 1));

                HPX_TEST_EQ(i + 42 + (this_locality/children)+(group_size*(this_locality % children)), result.get());
            }
    }
    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
	    std::cout << "hierarchical_timing: " << elapsed/ITERATIONS;
	    std::cout << " child_nodes: " << children << "\n";
    }
}

void check_localities()
{
	const char* node_name = std::getenv("SLURMD_NODENAME");
	std::uint32_t const this_locality = hpx::get_locality_id();
	std::cout << "locality: " << this_locality << "; node_name: " << node_name << "\n";
}

void test_hierarchical()
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    

    std::uint32_t const this_locality = hpx::get_locality_id();
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    { 
	    if (this_locality == 0)
	    {
		std::vector<std::vector<std::uint32_t>> data(4, std::vector<std::uint32_t>(10));
		std::iota(data[0].begin(), data[0].end(), 42 + i);
		std::iota(data[1].begin(), data[1].end(), 42 + i);
		std::iota(data[2].begin(), data[2].end(), 42 + i);
		std::iota(data[3].begin(), data[3].end(), 42 + i);

		hpx::future<std::vector<std::uint32_t>> from_result =
		    scatter_to(scatter_direct_basename, std::move(data),
			num_sites_arg(4), this_site_arg(this_locality),
			generation_arg(i + 1));

		hpx::future<std::uint32_t> result =
		    scatter_to(scatter_direct_basenames[this_locality % 4], std::move(from_result.get()),
			num_sites_arg(num_localities/4), this_site_arg(this_locality/4),
			generation_arg(i + 1));

		HPX_TEST_EQ(i + 42 + (this_locality/4), result.get());
	    }
	    else if (this_locality < 4)
	    {
		hpx::future<std::vector<std::uint32_t>> from_result =
		    scatter_from<std::vector<std::uint32_t>>(scatter_direct_basename,
			this_site_arg(this_locality), generation_arg(i + 1));

		hpx::future<std::uint32_t> result =
		    scatter_to( scatter_direct_basenames[this_locality % 4], HPX_MOVE(from_result.get()),
			num_sites_arg(num_localities/4), this_site_arg(this_locality/4),
			generation_arg(i + 1));
		HPX_TEST_EQ(i + 42 + (this_locality/4), result.get());

	    }
	    else
	    {
		hpx::future<std::uint32_t> result =
		    scatter_from<std::uint32_t>(scatter_direct_basenames[this_locality % 4],
			this_site_arg(this_locality/4), generation_arg(i + 1));

		HPX_TEST_EQ(i + 42 + (this_locality/4), result.get());
	    }
    }
}

void test_one_shot_use_scatter(int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    

    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(-3, lpn, num_localities, operation, test_size);
    }
    char const* const barrier_test_name = "/test/barrier/single";

    hpx::distributed::barrier b(barrier_test_name);

    hpx::chrono::high_resolution_timer const t;
    // test functionality based on immediate local result value
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        if (this_locality == 0)
        {
            std::vector<std::vector<int>> data = generateMatrix(num_localities, test_size, 42 + i);
            hpx::future<std::vector<int>> result =
                scatter_to(scatter_direct_basename, std::move(data),
                    num_sites_arg(num_localities), this_site_arg(this_locality),
                    generation_arg(i + 1));
            for (int check : result.get())
            {
                HPX_TEST_EQ(42+ i +this_locality, check);
            }
        }
        else
        {
            hpx::future<std::vector<int>> result =
                scatter_from<std::vector<int>>(scatter_direct_basename,
                    this_site_arg(this_locality), generation_arg(i + 1));
            for (int check : result.get())
            {
                HPX_TEST_EQ(42+ i +this_locality, check);
            }
        }
        //b.wait();
    }
    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
        write_to_file(-3, lpn, elapsed/ITERATIONS, operation, test_size);
    }
}

void test_one_shot_use_reduce(int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    

    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(-3, lpn, num_localities, operation, test_size);
    }
    char const* const barrier_test_name = "/test/barrier/single";

    hpx::distributed::barrier b(barrier_test_name);

    hpx::chrono::high_resolution_timer const t;
    // test functionality based on immediate local result value
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> data = std::vector<int>(test_size, i);
        if (this_locality == 0)
        {
            hpx::future<std::vector<int>> result = reduce_here(reduce_direct_basename, std::move(data),
                    VectorAdder{}, num_sites_arg(num_localities),
                    this_site_arg(this_locality), generation_arg(i + 1));
            HPX_TEST_EQ(i*num_localities, result.get()[0]);
        }
        else
        {
            hpx::future<void> overall_result =
                reduce_there(reduce_direct_basename, std::move(data),
                    this_site_arg(this_locality), generation_arg(i + 1));
            overall_result.get();
        }
        //b.wait();
    }
    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
        write_to_file(-3, lpn, elapsed/ITERATIONS, operation, test_size);
    }
}

void test_one_shot_use_broadcast(int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    

    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(-3, lpn, num_localities, operation, test_size);
    }
    char const* const barrier_test_name = "/test/barrier/single";

    hpx::distributed::barrier b(barrier_test_name);

    hpx::chrono::high_resolution_timer const t;
    // test functionality based on immediate local result value
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> data = std::vector<int>(test_size, i);
        if (this_locality == 0)
        {
            hpx::future<std::vector<int>> result = broadcast_to(broadcast_direct_basename, std::move(data), num_sites_arg(num_localities),
                    this_site_arg(this_locality), generation_arg(i + 1));
            HPX_TEST_EQ(i, result.get()[0]);
        }
        else
        {
            hpx::future<std::vector<int>>  result =
                broadcast_from<std::vector<int>>(broadcast_direct_basename,
                    this_site_arg(this_locality), generation_arg(i + 1));
            HPX_TEST_EQ(i, result.get()[0]);
        }
        //b.wait();
    }
    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
        write_to_file(-3, lpn, elapsed/ITERATIONS, operation, test_size);
    }
}

void test_one_shot_use_gather(int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    

    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(-3, lpn, num_localities, operation, test_size);
    }
    char const* const barrier_test_name = "/test/barrier/single";

    hpx::distributed::barrier b(barrier_test_name);

    hpx::chrono::high_resolution_timer const t;
    // test functionality based on immediate local result value
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> data = std::vector<int>(test_size, i+this_locality);
        if (this_locality == 0)
        {
            hpx::future<std::vector<std::vector<int>>> result = gather_here(gather_direct_basename, std::move(data), num_sites_arg(num_localities),
                    this_site_arg(this_locality), generation_arg(i + 1));
            auto out_result = result.get();
            for (int j  = 0; j < num_localities; j++)
            {
                HPX_TEST_EQ(i+j, out_result[j][0]);
            }
        }
        else
        {
            hpx::future<void>  result =
                gather_there(gather_direct_basename, data,
                    this_site_arg(this_locality), generation_arg(i + 1));
            result.get();
    
        }
        //b.wait();
    }
    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
        write_to_file(-3, lpn, elapsed/ITERATIONS, operation, test_size);
    }
}

void test_multiple_use_scatter(int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    

    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(-2, lpn, num_localities, operation, test_size);
    }

    auto const scatter_direct_client =
        create_communicator(scatter_direct_basename,
            num_sites_arg(num_localities), this_site_arg(this_locality));
    char const* const barrier_test_name = "/test/barrier/multiple";

    hpx::distributed::barrier b(barrier_test_name);

    hpx::chrono::high_resolution_timer const t;

    // test functionality based on immediate local result value
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        if (this_locality == 0)
        {
            std::vector<std::vector<int>> data = generateMatrix(num_localities, test_size, 42 + i);
            hpx::future<std::vector<int>> result =
                scatter_to(scatter_direct_client, std::move(data));

            for (int check : result.get())
            {
                HPX_TEST_EQ(42+ i +this_locality, check);
            }
        }
        else
        {
            hpx::future<std::vector<int>> result =
                scatter_from<std::vector<int>>(scatter_direct_client);

            for (int check : result.get())
            {
                HPX_TEST_EQ(42+ i +this_locality, check);
            }
        }
        //b.wait();
    }
    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
        write_to_file(-2, lpn, elapsed/ITERATIONS, operation, test_size);
    }

}

void test_multiple_use_reduce(int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    

    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(-2, lpn, num_localities, operation, test_size);
    }

    auto const reduce_direct_client =
        create_communicator(reduce_direct_basename,
            num_sites_arg(num_localities), this_site_arg(this_locality));
    char const* const barrier_test_name = "/test/barrier/multiple";

    hpx::distributed::barrier b(barrier_test_name);

    hpx::chrono::high_resolution_timer const t;

    // test functionality based on immediate local result value
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> data = std::vector<int>(test_size, i);
        if (this_locality == 0)
        {
           
            hpx::future<std::vector<int>> result = reduce_here(reduce_direct_client, std::move(data),
                    VectorAdder{});

            HPX_TEST_EQ(i*num_localities, result.get()[0]);
        }
        else
        {
            hpx::future<void> overall_result =
                reduce_there(reduce_direct_client, std::move(data));
            overall_result.get();
        }
        //b.wait();
    }
    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
        write_to_file(-2, lpn, elapsed/ITERATIONS, operation, test_size);
    }

}

void test_multiple_use_broadcast(int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    

    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(-2, lpn, num_localities, operation, test_size);
    }

    auto const broadcast_direct_client =
        create_communicator(broadcast_direct_basename,
            num_sites_arg(num_localities), this_site_arg(this_locality));
    char const* const barrier_test_name = "/test/barrier/multiple";

    hpx::distributed::barrier b(barrier_test_name);

    hpx::chrono::high_resolution_timer const t;

    // test functionality based on immediate local result value
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> data = std::vector<int>(test_size, i);
        if (this_locality == 0)
        {
           
            hpx::future<std::vector<int>> result = broadcast_to(broadcast_direct_client, std::move(data));

            HPX_TEST_EQ(i, result.get()[0]);
        }
        else
        {
            hpx::future<std::vector<int>> result =
                broadcast_from<std::vector<int>>(broadcast_direct_client);
            HPX_TEST_EQ(i, result.get()[0]);
        }
        //b.wait();
    }
    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
        write_to_file(-2, lpn, elapsed/ITERATIONS, operation, test_size);
    }

}

void test_multiple_use_gather(int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);
    

    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(-2, lpn, num_localities, operation, test_size);
    }

    auto const gather_direct_client =
        create_communicator(gather_direct_basename,
            num_sites_arg(num_localities), this_site_arg(this_locality));
    char const* const barrier_test_name = "/test/barrier/multiple";

    hpx::distributed::barrier b(barrier_test_name);

    hpx::chrono::high_resolution_timer const t;

    // test functionality based on immediate local result value
    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> data = std::vector<int>(test_size, i+this_locality);
        if (this_locality == 0)
        {
           
            hpx::future<std::vector<std::vector<int>>> result = gather_here(gather_direct_client, std::move(data));

            auto out_result = result.get();
            for (int j  = 0; j < num_localities; j++)
            {
                HPX_TEST_EQ(i+j, out_result[j][0]);
            }
        }
        else
        {
            hpx::future<void> result =
                gather_there(gather_direct_client, std::move(data));
            result.get();
        }
        //b.wait();
    }
    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
        write_to_file(-2, lpn, elapsed/ITERATIONS, operation, test_size);
    }

}

void test_multiple_use_with_generation_scatter(int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);

    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(-1, lpn, num_localities, operation, test_size);
    }

    auto const scatter_direct_client =
        create_communicator(scatter_direct_basename,
            num_sites_arg(num_localities), this_site_arg(this_locality));
    //std::cout << "Num Sites: " << scatter_direct_client.num_sites_ << "\n";
    char const* const barrier_test_name = "/test/barrier/generation";

    hpx::distributed::barrier b(barrier_test_name);
    hpx::chrono::high_resolution_timer const t;

    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        if (this_locality == 0)
        {
            std::vector<std::vector<int>> data = generateMatrix(num_localities, test_size, 42 + i);
            hpx::future<std::vector<int>> result = scatter_to(
                scatter_direct_client, std::move(data), generation_arg(i + 1));

            for (int check : result.get())
            {
                HPX_TEST_EQ(42+ i +this_locality, check);
            }
        }
        else
        {
            hpx::future<std::vector<int>>result = scatter_from<std::vector<int>>(
                scatter_direct_client, generation_arg(i + 1));

            for (int check : result.get())
            {
                HPX_TEST_EQ(42+ i +this_locality, check);
            }
        }
        //b.wait();
    }

    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
        write_to_file(-1, lpn, elapsed/ITERATIONS, operation, test_size);
    }
}

void test_multiple_use_with_generation_reduce(int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);

    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(-1, lpn, num_localities, operation, test_size);
    }

    auto const reduce_direct_client =
        create_communicator(reduce_direct_basename,
            num_sites_arg(num_localities), this_site_arg(this_locality));
    //std::cout << "Num Sites: " << scatter_direct_client.num_sites_ << "\n";
    char const* const barrier_test_name = "/test/barrier/generation";

    hpx::distributed::barrier b(barrier_test_name);
    hpx::chrono::high_resolution_timer const t;

    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> data = std::vector<int>(test_size, i);
        if (this_locality == 0)
        {
            hpx::future<std::vector<int>> overall_result =
                reduce_here(reduce_direct_client, std::move(data),
                    VectorAdder{}, generation_arg(i + 1));
            HPX_TEST_EQ(i*num_localities, overall_result.get()[0]);
            
        }
        else
        {
            hpx::future<void> overall_result = reduce_there(
                reduce_direct_client, std::move(data), generation_arg(i + 1));
            overall_result.get();
        }
        //b.wait();
    }

    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
        write_to_file(-1, lpn, elapsed/ITERATIONS, operation, test_size);
    }
}

void test_multiple_use_with_generation_broadcast(int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);

    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(-1, lpn, num_localities, operation, test_size);
    }

    auto const broadcast_direct_client =
        create_communicator(broadcast_direct_basename,
            num_sites_arg(num_localities), this_site_arg(this_locality));
    //std::cout << "Num Sites: " << scatter_direct_client.num_sites_ << "\n";
    char const* const barrier_test_name = "/test/barrier/generation";

    hpx::distributed::barrier b(barrier_test_name);
    hpx::chrono::high_resolution_timer const t;

    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> data = std::vector<int>(test_size, i);
        if (this_locality == 0)
        {
            hpx::future<std::vector<int>> overall_result =
                broadcast_to(broadcast_direct_client, std::move(data), generation_arg(i + 1));
            HPX_TEST_EQ(i, overall_result.get()[0]);
            
        }
        else
        {
            hpx::future<std::vector<int>> overall_result = broadcast_from<std::vector<int>>(
                broadcast_direct_client, generation_arg(i + 1));
            HPX_TEST_EQ(i, overall_result.get()[0]);
        }
        //b.wait();
    }

    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
        write_to_file(-1, lpn, elapsed/ITERATIONS, operation, test_size);
    }
}

void test_multiple_use_with_generation_gather(int lpn, int test_size, std::string operation)
{
    std::uint32_t const num_localities =
        hpx::get_num_localities(hpx::launch::sync);
    
    HPX_TEST_LTE(static_cast<std::uint32_t>(2), num_localities);

    std::uint32_t const this_locality = hpx::get_locality_id();
    if (this_locality == 0)
    {
        write_to_file(-1, lpn, num_localities, operation, test_size);
    }

    auto const gather_direct_client =
        create_communicator(gather_direct_basename,
            num_sites_arg(num_localities), this_site_arg(this_locality));
    //std::cout << "Num Sites: " << scatter_direct_client.num_sites_ << "\n";
    char const* const barrier_test_name = "/test/barrier/generation";

    hpx::distributed::barrier b(barrier_test_name);
    hpx::chrono::high_resolution_timer const t;

    for (std::uint32_t i = 0; i != ITERATIONS; ++i)
    {
        std::vector<int> data = std::vector<int>(test_size, i+this_locality);
        if (this_locality == 0)
        {
            hpx::future<std::vector<std::vector<int>>> overall_result =
                gather_here(gather_direct_client, std::move(data), generation_arg(i + 1));
            auto out_result = overall_result.get();
            for (int j  = 0; j < num_localities; j++)
            {
                HPX_TEST_EQ(i+j, out_result[j][0]);
            }
            
        }
        else
        {
            hpx::future<void> overall_result = gather_there(
                gather_direct_client, std::move(data), generation_arg(i + 1));
            overall_result.get();
        }
        //b.wait();
    }

    auto const elapsed = t.elapsed();
    if (this_locality == 0)
    {
        write_to_file(-1, lpn, elapsed/ITERATIONS, operation, test_size);
    }
}

void test_local_use()
{
    constexpr std::uint32_t num_sites = 10;

    std::vector<hpx::future<void>> sites;
    sites.reserve(num_sites);

    // launch num_sites threads to represent different sites
    for (std::uint32_t site = 0; site != num_sites; ++site)
    {
        sites.push_back(hpx::async([=]() {
            auto const scatter_direct_client =
                create_communicator(scatter_direct_basename,
                    num_sites_arg(num_sites), this_site_arg(site));


            for (std::uint32_t i = 0; i != ITERATIONS; ++i)
            {
                if (site == 0)
                {
                    std::vector<std::uint32_t> data(num_sites);
                    std::iota(data.begin(), data.end(), 42 + i);

                    hpx::future<std::uint32_t> result =
                        scatter_to(scatter_direct_client, std::move(data),
                            generation_arg(i + 1), this_site_arg(site));

                    HPX_TEST_EQ(i + 42 + site, result.get());
                }
                else
                {
                    hpx::future<std::uint32_t> result =
                        scatter_from<std::uint32_t>(scatter_direct_client,
                            generation_arg(i + 1), this_site_arg(site));

                    HPX_TEST_EQ(i + 42 + site, result.get());
                }
            }
        }));
    }

    hpx::wait_all(std::move(sites));
}

int hpx_main(hpx::program_options::variables_map& vm)
{
    const int arity = vm["arity"].as<int>();
    const int lpn = vm["lpn"].as<int>();
    const int test_size = vm["test_size"].as<int>();
    const std::string operation = vm["operation"].as<std::string>();
    if (hpx::get_num_localities(hpx::launch::sync) > 1)
    {
        if (operation == "scatter")
        {
            if (arity == -1)
            {
                test_one_shot_use_scatter(lpn,test_size, operation);
                test_multiple_use_scatter(lpn,test_size, operation);
                test_multiple_use_with_generation_scatter(lpn,test_size, operation);
            }
	        else
            {
                test_scatter_hierarchical(arity, lpn, test_size, operation);
            }
        }
        else if (operation == "reduce")
        {
            if (arity == -1)
            {
                test_one_shot_use_reduce(lpn,test_size, operation);
                test_multiple_use_reduce(lpn,test_size, operation);
                test_multiple_use_with_generation_reduce(lpn,test_size, operation);
            }
            else
            {
                test_reduce_hierarchical(arity, lpn, test_size, operation);
            }
        }
        else if (operation == "broadcast")
        {
            if (arity == -1)
            {
                test_one_shot_use_broadcast(lpn,test_size, operation);
                test_multiple_use_broadcast(lpn,test_size, operation);
                test_multiple_use_with_generation_broadcast(lpn,test_size, operation);
            }
            else
            {
                test_broadcast_hierarchical(arity, lpn, test_size, operation);
            }
        }
        else if (operation == "gather")
        {
            if (arity == -1)
            {
                test_one_shot_use_gather(lpn,test_size, operation);
                test_multiple_use_gather(lpn,test_size, operation);
                test_multiple_use_with_generation_gather(lpn,test_size, operation);
            }
            else
            {
                test_gather_hierarchical(arity, lpn, test_size, operation);
            }
        }
    }

    return hpx::finalize();
}

int main(int argc, char* argv[])
{
    using namespace hpx::program_options;

    options_description desc_commandline;
    desc_commandline.add_options()
    ("arity", value<int>()->default_value(-1), 
    "Arity of the operation")
    ("lpn", value<int>()->default_value(1), 
    "Number of localities per Node")
    ("test_size", value<int>()->default_value(1),
    "Number of Integers One Locality receives")
    ("operation", value<std::string>()->default_value("scatter"),
    "Collective Operation (scatter, reduce, broadcast, gather)");

    std::vector<std::string> const cfg = {"hpx.run_hpx_main!=1"};

    hpx::init_params init_args;
    init_args.desc_cmdline = desc_commandline;
    init_args.cfg = cfg;

    HPX_TEST_EQ(hpx::init(argc, argv, init_args), 0);
    return hpx::util::report_errors();
}

#endif
