# Root CMakeLists.txt for rcspp

include(GNUInstallDirs)

cmake_minimum_required(VERSION 3.26)

project(rcspp-lib LANGUAGES CXX)

option(CXX "enable C++ compilation" ON)
option(USE_VRP "Enable vrp testing" OFF)
option(USE_PYTHON "Enable Python bindings via pybind11" OFF)
option(USE_TESTS "Enable tests for RCSPP" ON)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
endif()

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Clang-Tidy & compile_commands ------------------------------------------------
set(CMAKE_CXX_CLANG_TIDY
    "clang-tidy;-config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set global output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")   # .a, .lib
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")   # .so, .dylib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")   # executables
set(CMAKE_PDB_OUTPUT_DIRECTORY     "${CMAKE_BINARY_DIR}/bin")   # .pdb (Windows)
set(BUILD_INCLUDE_DIR          "${CMAKE_BINARY_DIR}/include")   # for headers
file(MAKE_DIRECTORY ${BUILD_INCLUDE_DIR})

# Build the core librairy ------------------------------------------------------
add_subdirectory(src/rcspp)

# VRP column generation tests --------------------------------------------------
if(USE_VRP)
  message("Building vrp column generation tests")
  add_subdirectory(src/vrp)
endif()

# Python bindings --------------------------------------------------------------
if(USE_PYTHON)
  message("Building python wrapper")
  add_subdirectory(extern/pybind11)
  add_subdirectory(src/python_interface)
endif()

# RCSPP tests --------------------------------------------------
if(USE_TESTS)
  message("Building tests for RCSPP")
  add_subdirectory(tests/rcspp)
endif()

# (Optional) If you are seeing -nostartfiles / -nostdlib unexpectedly, check:
#   - CMAKE_EXE_LINKER_FLAGS
#   - Toolchain file overrides
# and remove those flags unless you *really* intend to supply your own CRT startup.

# target_compile_options(rcspp PRIVATE /W4 /WX)  # Re-enable if desired

# To install the library -------------------------------------------------------
# to be used with: cmake --install build --prefix /usr/local (or .)
install(TARGETS rcspp
    EXPORT rcsppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${BUILD_INCLUDE_DIR}/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# set cmake file for package
# to be used with:
# find_package(rcspp)
# target_link_libraries(app PRIVATE rcspp::rcspp)
install(EXPORT rcsppTargets
    FILE rcsppTargets.cmake
    NAMESPACE rcspp::
    DESTINATION lib/cmake/rcspp
)
