# One executable per example; each one focuses on a part of the public API.
set(AW_TPL_EXAMPLES
    01_tasks_basics
    02_task_run
    03_when_all
    04_when_any
    05_cancellation
    06_thread_pool
    07_timeout
)

foreach(example IN LISTS AW_TPL_EXAMPLES)
    add_executable(aw_tpl_example_${example} ${example}.cpp)
    target_link_libraries(aw_tpl_example_${example} PRIVATE aw::tpl)
endforeach()

# Runs every example in sequence (used by `ctest`/CI as a smoke test and handy for demos).
add_custom_target(aw_tpl_examples)
foreach(example IN LISTS AW_TPL_EXAMPLES)
    add_dependencies(aw_tpl_examples aw_tpl_example_${example})
endforeach()
