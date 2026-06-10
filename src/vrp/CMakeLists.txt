# VRP CMakeLists.txt for rcspp

project(rcspp-vrp LANGUAGES CXX)

# Use the canonical FindGUROBI.cmake from src/cmake
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
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/benchmark_main.cpp")
list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/benchmark_large_main.cpp")
list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/benchmark_ng_main.cpp")
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

# build executable -------------------------------------------------------------
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(${PROJECT_NAME} PRIVATE ${GUROBI_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
if(Boost_FOUND)
  target_compile_definitions(${PROJECT_NAME} PRIVATE RCSPP_VRP_HAS_BOOST)
endif()
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rcspp>"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

# build benchmark (C1/R1/RC1 families) ----------------------------------------
list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
set(BENCHMARK_NAME "rcspp-vrp-benchmark")
add_executable(${BENCHMARK_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/benchmark_main.cpp" ${SOURCE_FILES})
target_include_directories(${BENCHMARK_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(${BENCHMARK_NAME} PRIVATE ${GUROBI_INCLUDE_DIRS})
target_link_libraries(${BENCHMARK_NAME} PRIVATE ${LIBS})
if(Boost_FOUND)
  target_compile_definitions(${BENCHMARK_NAME} PRIVATE RCSPP_VRP_HAS_BOOST)
endif()
add_custom_command(TARGET ${BENCHMARK_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:rcspp>"
        $<TARGET_FILE_DIR:${BENCHMARK_NAME}>
)

# build ng benchmark (ng-route unreachable augmentation sweep) ----------------
set(BENCHMARK_NG_NAME "rcspp-vrp-benchmark-ng")
add_executable(${BENCHMARK_NG_NAME}
               "${CMAKE_CURRENT_SOURCE_DIR}/benchmark_ng_main.cpp" ${SOURCE_FILES})
target_include_directories(${BENCHMARK_NG_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(${BENCHMARK_NG_NAME} PRIVATE ${GUROBI_INCLUDE_DIRS})
target_link_libraries(${BENCHMARK_NG_NAME} PRIVATE ${LIBS})
if(Boost_FOUND)
  target_compile_definitions(${BENCHMARK_NG_NAME} PRIVATE RCSPP_VRP_HAS_BOOST)
endif()
add_custom_command(TARGET ${BENCHMARK_NG_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:rcspp>"
        $<TARGET_FILE_DIR:${BENCHMARK_NG_NAME}>
)

# build large benchmark (C2/R2/RC2 + Gehring & Homberger) --------------------
set(BENCHMARK_LARGE_NAME "rcspp-vrp-benchmark-large")
add_executable(${BENCHMARK_LARGE_NAME}
               "${CMAKE_CURRENT_SOURCE_DIR}/benchmark_large_main.cpp" ${SOURCE_FILES})
target_include_directories(${BENCHMARK_LARGE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(${BENCHMARK_LARGE_NAME} PRIVATE ${GUROBI_INCLUDE_DIRS})
target_link_libraries(${BENCHMARK_LARGE_NAME} PRIVATE ${LIBS})
if(Boost_FOUND)
  target_compile_definitions(${BENCHMARK_LARGE_NAME} PRIVATE RCSPP_VRP_HAS_BOOST)
endif()
add_custom_command(TARGET ${BENCHMARK_LARGE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:rcspp>"
        $<TARGET_FILE_DIR:${BENCHMARK_LARGE_NAME}>
)
