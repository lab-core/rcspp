# Python CMakeLists.txt for rcspp

project(rcsppy LANGUAGES CXX)

# Gather all C++ sources for the Python module
file(GLOB_RECURSE MODULE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rcspp/*.cpp")

# Add pybind11 module
pybind11_add_module(${PROJECT_NAME} ${MODULE_SOURCES})

# Add include directories for project sources
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Link the core C++ library
target_link_libraries(${PROJECT_NAME} PRIVATE rcspp)

# Find all .py files in rcspp (recursively)
file(GLOB_RECURSE PYTHON_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rcspp/*.py")

foreach(PY_FILE IN LISTS PYTHON_SOURCES)
  file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/rcspp" "${PY_FILE}")

  # Get directory of the relative path (using CMake, not shell)
  get_filename_component(REL_DIR "${REL_PATH}" DIRECTORY)

  add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/rcspp/${REL_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PY_FILE}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/rcspp/${REL_PATH}"
  )
endforeach()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -f
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/rcspp/graphpy.py")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rcspp>"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
