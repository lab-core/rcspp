# Python CMakeLists.txt for rcspp
#
# Assembles the importable `rcspp` Python package from two source roots:
#   - bindings/      : pybind11 C++ glue, compiled into the `_core` extension
#   - src/rcspp/     : pure-Python modules, type stubs and py.typed marker
# Both are installed/copied into a single `rcspp` package directory so the
# wheel layout is unchanged from before the restructure.

project(rcspp_python LANGUAGES CXX)

set(PYTHON_PACKAGE_NAME rcspp)
set(PYTHON_EXTENSION_NAME _core)

# Source roots
set(PY_BINDINGS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bindings")
set(PY_PACKAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/${PYTHON_PACKAGE_NAME}")

# Gather all C++ sources for the Python module
file(GLOB_RECURSE MODULE_SOURCES "${PY_BINDINGS_DIR}/*.cpp")

# Add pybind11 module. Under coverage, pass NO_EXTRAS to disable pybind11's LTO:
# LTO is very slow to link and its -O3 codegen fuses lines, ruining gcov line
# attribution for the binding TUs.
if(USE_COVERAGE)
  pybind11_add_module(${PYTHON_EXTENSION_NAME} NO_EXTRAS ${MODULE_SOURCES})
else()
  pybind11_add_module(${PYTHON_EXTENSION_NAME} ${MODULE_SOURCES})
endif()
# Flatten output to the same directory for all configurations.
# Setting every per-config variant prevents MSVC's multi-config generators
# from appending a Debug/ or Release/ subdirectory.
set(_py_out_dir "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_PACKAGE_NAME}")
set_target_properties(${PYTHON_EXTENSION_NAME} PROPERTIES
  OUTPUT_NAME                               ${PYTHON_EXTENSION_NAME}
  LIBRARY_OUTPUT_DIRECTORY                  "${_py_out_dir}"
  LIBRARY_OUTPUT_DIRECTORY_DEBUG            "${_py_out_dir}"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE          "${_py_out_dir}"
  LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO   "${_py_out_dir}"
  LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL       "${_py_out_dir}"
  RUNTIME_OUTPUT_DIRECTORY                  "${_py_out_dir}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG            "${_py_out_dir}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE          "${_py_out_dir}"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO   "${_py_out_dir}"
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL       "${_py_out_dir}"
  BUILD_RPATH "$ORIGIN"
  INSTALL_RPATH "$ORIGIN"
)

if(APPLE)
  set_target_properties(${PYTHON_EXTENSION_NAME} PROPERTIES
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
  )
endif()

# Add include directories for project sources
target_include_directories(${PYTHON_EXTENSION_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/cpp)

# Link the core C++ library
target_link_libraries(${PYTHON_EXTENSION_NAME} PRIVATE rcspp_objects)

# Instrument the binding TUs themselves for coverage (rcspp_objects already
# propagates the flags for the core, but the bindings/*.cpp are _core's own sources).
if(USE_COVERAGE)
  target_link_libraries(${PYTHON_EXTENSION_NAME} PRIVATE rcspp_coverage)
endif()

# Find all .py, .pyi, and py.typed files in the pure-Python package (recursively)
file(GLOB_RECURSE PYTHON_SOURCES
  "${PY_PACKAGE_DIR}/*.py"
  "${PY_PACKAGE_DIR}/*.pyi"
  "${PY_PACKAGE_DIR}/py.typed"
)

# Build per-file copy rules with explicit OUTPUT so CMake re-copies whenever a
# .py file changes, without requiring the C++ extension to rebuild.
set(PYTHON_COPY_OUTPUTS "")

# Helper macro to register one copy rule.
macro(add_py_copy_rule PY_FILE BASE_DIR)
  file(RELATIVE_PATH REL_PATH "${BASE_DIR}" "${PY_FILE}")
  get_filename_component(REL_DIR "${REL_PATH}" DIRECTORY)
  set(OUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_PACKAGE_NAME}/${REL_PATH}")
  list(APPEND PYTHON_COPY_OUTPUTS "${OUT_FILE}")
  add_custom_command(
    OUTPUT "${OUT_FILE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_PACKAGE_NAME}/${REL_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PY_FILE}" "${OUT_FILE}"
    DEPENDS "${PY_FILE}"
    COMMENT "Copying ${REL_PATH} to build tree"
  )
endmacro()

foreach(PY_FILE IN LISTS PYTHON_SOURCES)
  add_py_copy_rule("${PY_FILE}" "${PY_PACKAGE_DIR}")
endforeach()

# Dedicated target that drives all .py copies; rebuilds only when sources change.
add_custom_target(copy_python_sources ALL DEPENDS ${PYTHON_COPY_OUTPUTS})
add_dependencies(${PYTHON_EXTENSION_NAME} copy_python_sources)

install(TARGETS ${PYTHON_EXTENSION_NAME}
    LIBRARY DESTINATION ${PYTHON_PACKAGE_NAME}
    RUNTIME DESTINATION ${PYTHON_PACKAGE_NAME}
)

install(DIRECTORY "${PY_PACKAGE_DIR}/"
    DESTINATION ${PYTHON_PACKAGE_NAME}
    FILES_MATCHING
    PATTERN "*.py"
    PATTERN "*.pyi"
    PATTERN "py.typed"
)
