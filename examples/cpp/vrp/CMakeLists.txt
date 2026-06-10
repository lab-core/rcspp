# VRP CMakeLists.txt for rcspp

project(rcspp-vrp LANGUAGES CXX)

# Use the canonical FindGUROBI.cmake from examples/cpp/vrp/cmake
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Boost (optional) ------------------------------------------------------------
find_package(Boost CONFIG QUIET)
if(NOT Boost_FOUND)
  find_package(Boost)  # fall back to module mode (FindBoost.cmake)
endif()
if(Boost_FOUND)
  message(STATUS "Boost found: ${Boost_VERSION}")
  # Boost::headers is only defined in CONFIG mode; create a shim in module mode
  # so downstream link/include lines below can use Boost::headers uniformly.
  if(NOT TARGET Boost::headers)
    add_library(Boost::headers INTERFACE IMPORTED)
    target_include_directories(Boost::headers INTERFACE ${Boost_INCLUDE_DIRS})
  endif()
else()
  message(STATUS "Boost not found; building without Boost subproblem.")
endif()

# Gurobi ----------------------------------------------------------------------
set(GUROBI_REQUIRE_CXX ON)
find_package(GUROBI)

if(NOT GUROBI_FOUND)
  message(STATUS "Gurobi not found; skipping VRP targets.")
  return()
endif()

if(TARGET GUROBI::gurobi_cxx)
  message(STATUS "Linking against Gurobi C++ API target: GUROBI::gurobi_cxx")
else()
  message(STATUS "Gurobi C++ target not found; linking only GUROBI::gurobi.")
endif()

# Sources ---------------------------------------------------------------------
# Shared VRP sources (everything except the CLI entry point). The benchmark
# programs now live under examples/cpp/ and link the library defined here.
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
if(NOT Boost_FOUND)
  file(GLOB BOOST_CPP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/cg/subproblem/boost/*.cpp")
  list(REMOVE_ITEM SOURCE_FILES ${BOOST_CPP_FILES})
endif()

set(LIBS rcspp)
if(Boost_FOUND)
  set(LIBS ${LIBS} Boost::headers)
endif()
if(TARGET GUROBI::gurobi_cxx)
  set(LIBS ${LIBS} GUROBI::gurobi_cxx)
else()
  set(LIBS ${LIBS} GUROBI::gurobi)
endif()

# Reusable VRP library ---------------------------------------------------------
# Built once and shared by the CLI (below) and the benchmark/demo executables in
# examples/cpp/. Include dirs, link libs and the Boost feature flag are PUBLIC so
# consumers inherit them transitively.
set(VRP_LIB "rcspp-vrp-lib")
add_library(${VRP_LIB} STATIC ${SOURCE_FILES})
set_target_properties(${VRP_LIB} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(${VRP_LIB} PUBLIC
    ${CMAKE_SOURCE_DIR}/cpp                # rcspp core headers ("rcspp/...")
    ${CMAKE_CURRENT_SOURCE_DIR}/..         # for "vrp/..."-prefixed includes
    ${CMAKE_CURRENT_SOURCE_DIR}            # for bare VRP header names ("instance.hpp")
    ${GUROBI_INCLUDE_DIRS}
)
target_link_libraries(${VRP_LIB} PUBLIC ${LIBS})
if(Boost_FOUND)
  target_compile_definitions(${VRP_LIB} PUBLIC RCSPP_VRP_HAS_BOOST)
endif()

# build CLI executable ---------------------------------------------------------
add_executable(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE ${VRP_LIB})
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rcspp>"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
