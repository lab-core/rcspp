# Python CMakeLists.txt for rcspp

project(rcspp_python LANGUAGES CXX)

set(PYTHON_PACKAGE_NAME rcspp)
set(PYTHON_EXTENSION_NAME _core)

# Gather all C++ sources for the Python module
file(GLOB_RECURSE MODULE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rcspp/*.cpp")

# Add pybind11 module
pybind11_add_module(${PYTHON_EXTENSION_NAME} ${MODULE_SOURCES})
set_target_properties(${PYTHON_EXTENSION_NAME} PROPERTIES
  OUTPUT_NAME ${PYTHON_EXTENSION_NAME}
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_PACKAGE_NAME}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_PACKAGE_NAME}"
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
target_link_libraries(${PYTHON_EXTENSION_NAME} PRIVATE rcspp)

# Find all .py files in rcspp (recursively)
file(GLOB_RECURSE PYTHON_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rcspp/*.py")

foreach(PY_FILE IN LISTS PYTHON_SOURCES)
  file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/rcspp" "${PY_FILE}")
  get_filename_component(REL_DIR "${REL_PATH}" DIRECTORY)

  add_custom_command(
    TARGET ${PYTHON_EXTENSION_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PYTHON_EXTENSION_NAME}>/${REL_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PY_FILE}" "$<TARGET_FILE_DIR:${PYTHON_EXTENSION_NAME}>/${REL_PATH}"
  )
endforeach()

# Copy the core shared library next to the extension for local runs on platforms
# where the Python extension depends on the rcspp shared library.
add_custom_command(TARGET ${PYTHON_EXTENSION_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rcspp>"
            "$<TARGET_FILE_DIR:${PYTHON_EXTENSION_NAME}>"
)

install(TARGETS ${PYTHON_EXTENSION_NAME}
    LIBRARY DESTINATION ${PYTHON_PACKAGE_NAME}
    RUNTIME DESTINATION ${PYTHON_PACKAGE_NAME}
)

install(TARGETS rcspp
    LIBRARY DESTINATION ${PYTHON_PACKAGE_NAME}
    RUNTIME DESTINATION ${PYTHON_PACKAGE_NAME}
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/rcspp/"
    DESTINATION ${PYTHON_PACKAGE_NAME}
    FILES_MATCHING
    PATTERN "*.py"
)
