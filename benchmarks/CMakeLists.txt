# Benchmarks for PSAC
# Run using Google Benchmark (https://github.com/google/benchmark)
set(NUMA_COMMAND numactl -i all)

function(add_benchmark benchmark)
  add_executable(${benchmark} ${benchmark}.cpp)
  target_link_libraries(${benchmark} psac benchmark_main)
  target_compile_options(${benchmark} PRIVATE -Wall -Wextra -Wfatal-errors)

  if(PSAC_BENCHMARK_NUMACTL_TARGETS)
    add_custom_target(numactl_${benchmark}
      COMMAND ${NUMA_COMMAND} ${CMAKE_CURRENT_BINARY_DIR}/${benchmark} --benchmark_counters_tabular=true
    )
    add_dependencies(numactl_${benchmark} ${benchmark})
  endif()
endfunction()

# List all benchmarks
add_benchmark(bench_mapreduce)
add_benchmark(bench_raytracing)
add_benchmark(bench_editdistance)
add_benchmark(bench_listcontraction)
add_benchmark(bench_treecontraction)
add_benchmark(bench_rabin_karp)
add_benchmark(bench_bst)

# Additional benchmarks. Currently works in progress
#add_benchmark(bench_convex_hull)
#add_benchmark(bench_bst_mapreduce)
#add_benchmark(bench_bst_compose)

# Need to use tree-based reader set for raytracing
target_compile_definitions(bench_raytracing PRIVATE -DPSAC_USE_TREE_READER_SET)
