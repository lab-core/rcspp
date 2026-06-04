# Python CMakeLists.txt for rcspp

project(rcspp_python LANGUAGES CXX)

set(PYTHON_PACKAGE_NAME rcspp)
set(PYTHON_EXTENSION_NAME _core)

# Gather all C++ sources for the Python module
file(GLOB_RECURSE MODULE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rcspp/*.cpp")

# Add pybind11 module
pybind11_add_module(${PYTHON_EXTENSION_NAME} ${MODULE_SOURCES})
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
target_include_directories(${PYTHON_EXTENSION_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Link the core C++ library
target_link_libraries(${PYTHON_EXTENSION_NAME} PRIVATE rcspp_objects)

# Collect .py files from the C++/pybind11 interface directory.
file(GLOB_RECURSE PYTHON_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rcspp/*.py")

# Also collect pure-Python helpers from src/python/rcspp/ (e.g. pricing_pool.py).
file(GLOB_RECURSE PYTHON_EXTRA_SOURCES "${CMAKE_SOURCE_DIR}/src/python/rcspp/*.py")

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
  add_py_copy_rule("${PY_FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/rcspp")
endforeach()

foreach(PY_FILE IN LISTS PYTHON_EXTRA_SOURCES)
  # Skip files that would produce an output already covered by PYTHON_SOURCES
  # (e.g. __init__.py exists in both directories; the interface version wins).
  file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}/src/python/rcspp" "${PY_FILE}")
  set(_out "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_PACKAGE_NAME}/${_rel}")
  if(NOT "${_out}" IN_LIST PYTHON_COPY_OUTPUTS)
    add_py_copy_rule("${PY_FILE}" "${CMAKE_SOURCE_DIR}/src/python/rcspp")
  endif()
endforeach()

# Dedicated target that drives all .py copies; rebuilds only when sources change.
add_custom_target(copy_python_sources ALL DEPENDS ${PYTHON_COPY_OUTPUTS})
add_dependencies(${PYTHON_EXTENSION_NAME} copy_python_sources)

install(TARGETS ${PYTHON_EXTENSION_NAME}
    LIBRARY DESTINATION ${PYTHON_PACKAGE_NAME}
    RUNTIME DESTINATION ${PYTHON_PACKAGE_NAME}
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/rcspp/"
    DESTINATION ${PYTHON_PACKAGE_NAME}
    FILES_MATCHING
    PATTERN "*.py"
)

# Install pure-Python helpers from src/python/rcspp/ (e.g. pricing_pool.py).
install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/python/rcspp/"
    DESTINATION ${PYTHON_PACKAGE_NAME}
    FILES_MATCHING
    PATTERN "*.py"
)
