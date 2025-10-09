# VRP CMakeLists.txt for rcspp

project(rcspp-vrp LANGUAGES CXX)

# Make sure our custom FindGUROBI.cmake is found
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Boost -----------------------------------------------------------------------
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
# Use only header-only Boost components to avoid linker errors
find_package(Boost REQUIRED)
if(Boost_FOUND)
  message(STATUS "Boost found: ${Boost_VERSION}")
endif()

# Gurobi ----------------------------------------------------------------------
# We require the C++ API since the code uses GRBModel / GRBEnv.
set(GUROBI_REQUIRE_CXX ON)
find_package(GUROBI REQUIRED)

# Diagnostics (from the enhanced finder these should already appear,
# but we add a short summary here too)
if(TARGET GUROBI::gurobi_cxx)
  message(STATUS "Linking against Gurobi C++ API target: GUROBI::gurobi_cxx")
else()
  message(STATUS "Gurobi C++ target not found; will link only GUROBI::gurobi (this should NOT happen with GUROBI_REQUIRE_CXX=ON).")
endif()

# Sources ---------------------------------------------------------------------
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Include paths for project sources (kept as you had them)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(${PROJECT_NAME} PRIVATE ${GUROBI_INCLUDE_DIRS})
if(Boost_FOUND)
  target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})
endif()

set(LIBS rcspp)
# Link Gurobi (C++ if available)
if(TARGET GUROBI::gurobi_cxx)
  set(LIBS ${LIBS} GUROBI::gurobi_cxx)
else()
  set(LIBS ${LIBS} GUROBI::gurobi)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rcspp>"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
