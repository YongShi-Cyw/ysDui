# Compare repeated ysDui performance outputs by median on the same runner.
if(NOT DEFINED BASELINE_DIR OR NOT IS_DIRECTORY "${BASELINE_DIR}")
  message(FATAL_ERROR "BASELINE_DIR must name an existing directory")
endif()
if(NOT DEFINED CURRENT_DIR OR NOT IS_DIRECTORY "${CURRENT_DIR}")
  message(FATAL_ERROR "CURRENT_DIR must name an existing directory")
endif()
if(NOT DEFINED THRESHOLD_PERCENT)
  set(THRESHOLD_PERCENT 15)
endif()
if(NOT THRESHOLD_PERCENT MATCHES "^[0-9]+$")
  message(FATAL_ERROR "THRESHOLD_PERCENT must be a non-negative integer")
endif()

file(GLOB baseline_files "${BASELINE_DIR}/*.txt")
file(GLOB current_files "${CURRENT_DIR}/*.txt")
list(SORT baseline_files)
list(SORT current_files)

list(LENGTH baseline_files baseline_count)
list(LENGTH current_files current_count)
if(baseline_count LESS 5 OR current_count LESS 5)
  message(FATAL_ERROR "At least five baseline and current samples are required")
endif()
if(NOT baseline_count EQUAL current_count)
  message(FATAL_ERROR "Baseline and current sample counts differ: ${baseline_count} != ${current_count}")
endif()

set(metrics
  "layout_1000_controls_20_iterations_us"
  "virtual_list_10000_rows_1000_frames_us"
  "spreadsheet_100000x1000_1000_frames_us"
  "control_tree_1111_nodes_10000_hit_tests_us"
  "directwrite_100_draws_us"
  "peak_working_set_bytes"
  "private_bytes")

set(optional_metrics
  "spreadsheet_sparse_bind_1000_reloads_us"
  "spreadsheet_sparse_bind_allocations"
  "spreadsheet_row_resize_1000_moves_us"
  "spreadsheet_frozen_100000x1000_1000_frames_us"
  "spreadsheet_batch_write_1000_cells_us"
  "spreadsheet_100000x1000_1000_frames_allocations"
  "spreadsheet_frozen_100000x1000_1000_frames_allocations"
  "spreadsheet_batch_write_1000_cells_allocations")

function(metric_is_reported sample_files metric output_variable)
  set(reported_count 0)
  foreach(sample_file IN LISTS sample_files)
    file(STRINGS "${sample_file}" matches REGEX "^${metric}=[0-9]+$")
    list(LENGTH matches match_count)
    if(match_count GREATER 1)
      message(FATAL_ERROR "Expected at most one ${metric} entry in ${sample_file}, found ${match_count}")
    endif()
    if(match_count EQUAL 1)
      math(EXPR reported_count "${reported_count} + 1")
    endif()
  endforeach()
  list(LENGTH sample_files sample_count)
  if(reported_count GREATER 0 AND NOT reported_count EQUAL sample_count)
    message(FATAL_ERROR "Optional metric ${metric} is missing from some samples")
  endif()
  set(${output_variable} ${reported_count} PARENT_SCOPE)
endfunction()

function(read_metric_median sample_files metric output_variable)
  set(values)
  foreach(sample_file IN LISTS sample_files)
    file(STRINGS "${sample_file}" matches REGEX "^${metric}=[0-9]+$")
    list(LENGTH matches match_count)
    if(NOT match_count EQUAL 1)
      message(FATAL_ERROR "Expected one ${metric} entry in ${sample_file}, found ${match_count}")
    endif()
    list(GET matches 0 metric_line)
    string(REGEX REPLACE "^[^=]+=" "" value "${metric_line}")
    list(APPEND values "${value}")
  endforeach()

  list(SORT values COMPARE NATURAL ORDER ASCENDING)
  list(LENGTH values value_count)
  math(EXPR median_index "${value_count} / 2")
  list(GET values ${median_index} median)
  set(${output_variable} "${median}" PARENT_SCOPE)
endfunction()

set(regressions)
foreach(metric IN LISTS metrics)
  read_metric_median("${baseline_files}" "${metric}" baseline_median)
  read_metric_median("${current_files}" "${metric}" current_median)
  math(EXPR allowed_value "${baseline_median} * (100 + ${THRESHOLD_PERCENT})")
  math(EXPR current_value "${current_median} * 100")
  message(STATUS "${metric}: baseline=${baseline_median}, current=${current_median}")
  if(current_value GREATER allowed_value)
    list(APPEND regressions "${metric}: ${baseline_median} -> ${current_median}")
  endif()
endforeach()

foreach(metric IN LISTS optional_metrics)
  metric_is_reported("${baseline_files}" "${metric}" baseline_reported)
  metric_is_reported("${current_files}" "${metric}" current_reported)
  if(NOT baseline_reported OR NOT current_reported)
    message(STATUS "${metric}: awaiting a baseline on both revisions")
    continue()
  endif()
  read_metric_median("${baseline_files}" "${metric}" baseline_median)
  read_metric_median("${current_files}" "${metric}" current_median)
  math(EXPR allowed_value "${baseline_median} * (100 + ${THRESHOLD_PERCENT})")
  math(EXPR current_value "${current_median} * 100")
  message(STATUS "${metric}: baseline=${baseline_median}, current=${current_median}")
  if(current_value GREATER allowed_value)
    list(APPEND regressions "${metric}: ${baseline_median} -> ${current_median}")
  endif()
endforeach()

if(regressions)
  list(JOIN regressions "\n  " regression_message)
  message(FATAL_ERROR
    "Performance regression exceeded ${THRESHOLD_PERCENT}%:\n  ${regression_message}")
endif()

message(STATUS "All performance metrics are within the ${THRESHOLD_PERCENT}% regression limit")
