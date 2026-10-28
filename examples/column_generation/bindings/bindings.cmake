# The example's bindings, added to rcspp's Python module (_core) by python/CMakeLists.txt with
# USE_COLUMN_GENERATION_EXAMPLE: one module, so the graph types and the Ctrl-C state are shared.
# The submodule is rcspp._core.cpp_column_generation; its HiGHS master solver comes with
# USE_COLUMN_GENERATION_EXAMPLE_HIGHS.

set(example_bindings_dir "${CMAKE_CURRENT_LIST_DIR}")
target_sources(_core PRIVATE
    "${example_bindings_dir}/cpp_column_generation.cpp"
    "${example_bindings_dir}/column_generation.cpp"
)
# graph_pricer_factory.hpp, included by rcspp's graph bindings; and rcspp's binding headers
# (active_call.hpp) for the example's sources.
target_include_directories(_core PRIVATE "${example_bindings_dir}" "${PY_BINDINGS_DIR}")
target_link_libraries(_core PRIVATE rcspp_column_generation_objects)
target_compile_definitions(_core PRIVATE RCSPP_USE_COLUMN_GENERATION_EXAMPLE)

# The HiGHS master solver, and HiGHS itself (static: it ends up in the module).
if(USE_COLUMN_GENERATION_EXAMPLE_HIGHS)
  target_sources(_core PRIVATE "${example_bindings_dir}/column_generation_highs.cpp")
  target_link_libraries(_core PRIVATE rcspp_column_generation_highs)
  target_compile_definitions(_core PRIVATE RCSPP_USE_COLUMN_GENERATION_EXAMPLE_HIGHS)
endif()
