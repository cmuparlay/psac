find_package (Python3 COMPONENTS Interpreter)

set(REPORT_NUM_ITERATIONS 10 CACHE STRING "The number of times to run each benchmark for reporting")
set(NUMA_COMMAND numactl -i all)
set(DATE_COMMAND "$$(date" "+%Y-%m-%d)")

# A target that generates all of the reports
add_custom_target(reports)

# Add a target for producing the benchmark output
function(add_benchmark benchmark)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/results)
  foreach(i RANGE 1 ${REPORT_NUM_ITERATIONS})
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/results/${benchmark}-output${i}.json
      COMMAND ${NUMA_COMMAND} ${CMAKE_BINARY_DIR}/benchmarks/bench_${benchmark}
      --benchmark_counters_tabular=true --benchmark_out_format=json --benchmark_out=${CMAKE_CURRENT_BINARY_DIR}/results/${benchmark}-output${i}.json
      DEPENDS bench_${benchmark}
    )
  endforeach(i)
endfunction()

# Add a target for generating a benchmark report
function(add_report benchmark test)
  file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
  cmake_host_system_information(RESULT _host_name QUERY HOSTNAME)
  set(report_inputs "")
  foreach(i RANGE 1 ${REPORT_NUM_ITERATIONS})
    set(report_inputs "${report_inputs}" "${CMAKE_CURRENT_BINARY_DIR}/results/${benchmark}-output${i}.json")
  endforeach(i)
  add_custom_target(
    report-${test}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/make_report.py ${report_inputs}
    --output=${CMAKE_CURRENT_BINARY_DIR}/reports/${test} --test=${test} 
    COMMAND tar -C ${CMAKE_CURRENT_BINARY_DIR}/reports/ -zcvf ${CMAKE_SOURCE_DIR}/output/${test}-${_host_name}-${DATE_COMMAND}.tar.gz ${test}
    DEPENDS ${report_inputs}
  )
  add_dependencies(reports report-${test})
endfunction()

# List all benchmarks and reports
add_benchmark(raytracing)
add_report(raytracing raytrace)

add_benchmark(editdistance)
add_report(editdistance edit_distance)

add_benchmark(listcontraction)
add_report(listcontraction list_contraction)

add_benchmark(rabin_karp)
add_report(rabin_karp rabin_karp)

add_benchmark(treecontraction)
add_report(treecontraction tree_contraction)

add_benchmark(bst)
add_report(bst filter)
