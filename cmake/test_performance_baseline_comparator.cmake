set(fixture_root "${CMAKE_CURRENT_BINARY_DIR}/performance-comparator-fixtures")
file(MAKE_DIRECTORY "${fixture_root}/baseline" "${fixture_root}/within-limit" "${fixture_root}/regressed"
                    "${fixture_root}/missing-metric" "${fixture_root}/with-optional"
                    "${fixture_root}/partial-optional")

set(metrics
  "layout_1000_controls_20_iterations_us"
  "virtual_list_10000_rows_1000_frames_us"
  "spreadsheet_100000x1000_1000_frames_us"
  "control_tree_1111_nodes_10000_hit_tests_us"
  "directwrite_100_draws_us"
  "peak_working_set_bytes"
  "private_bytes")

set(optional_metrics
  "spreadsheet_frozen_100000x1000_1000_frames_us"
  "spreadsheet_batch_write_1000_cells_us"
  "spreadsheet_100000x1000_1000_frames_allocations"
  "spreadsheet_frozen_100000x1000_1000_frames_allocations"
  "spreadsheet_batch_write_1000_cells_allocations")

function(write_samples directory values)
  set(sample_index 0)
  foreach(value IN LISTS values)
    math(EXPR sample_index "${sample_index} + 1")
    set(content "")
    foreach(metric IN LISTS metrics)
      string(APPEND content "${metric}=${value}\n")
    endforeach()
    file(WRITE "${directory}/run-${sample_index}.txt" "${content}")
  endforeach()
endfunction()

function(write_samples_with_optional directory values)
  set(sample_index 0)
  foreach(value IN LISTS values)
    math(EXPR sample_index "${sample_index} + 1")
    set(content "")
    foreach(metric IN LISTS metrics optional_metrics)
      string(APPEND content "${metric}=${value}\n")
    endforeach()
    file(WRITE "${directory}/run-${sample_index}.txt" "${content}")
  endforeach()
endfunction()

write_samples("${fixture_root}/baseline" "98;99;100;101;102")
write_samples("${fixture_root}/within-limit" "113;114;115;116;117")
write_samples("${fixture_root}/regressed" "114;115;116;117;118")
write_samples("${fixture_root}/missing-metric" "98;99;100;101;102")
write_samples_with_optional("${fixture_root}/with-optional" "98;99;100;101;102")
write_samples_with_optional("${fixture_root}/partial-optional" "98;99;100;101;102")
file(STRINGS "${fixture_root}/partial-optional/run-3.txt" partial_optional_lines)
list(FILTER partial_optional_lines EXCLUDE REGEX "^spreadsheet_frozen_100000x1000_1000_frames_us=")
file(WRITE "${fixture_root}/partial-optional/run-3.txt" "")
foreach(line IN LISTS partial_optional_lines)
  file(APPEND "${fixture_root}/partial-optional/run-3.txt" "${line}\n")
endforeach()
file(WRITE "${fixture_root}/missing-metric/run-3.txt"
  "layout_1000_controls_20_iterations_us=100\n")

function(run_comparison current_directory expected_result)
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DBASELINE_DIR=${fixture_root}/baseline"
      "-DCURRENT_DIR=${current_directory}"
      "-DTHRESHOLD_PERCENT=15"
      -P "${CMAKE_CURRENT_LIST_DIR}/compare_performance_baselines.cmake"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(expected_result STREQUAL "pass" AND NOT result EQUAL 0)
    message(FATAL_ERROR "Expected comparison to pass:\n${output}${error}")
  endif()
  if(expected_result STREQUAL "fail" AND result EQUAL 0)
    message(FATAL_ERROR "Expected comparison to fail:\n${output}${error}")
  endif()
endfunction()

run_comparison("${fixture_root}/within-limit" pass)
run_comparison("${fixture_root}/with-optional" pass)
run_comparison("${fixture_root}/regressed" fail)
run_comparison("${fixture_root}/missing-metric" fail)
run_comparison("${fixture_root}/partial-optional" fail)
