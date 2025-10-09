cmake_minimum_required(VERSION 3.15)  # Needs generator expressions & runtime detection convenience

project(gurobi-template LANGUAGES CXX C)

option(CXX "Enable C++ compilation" ON)
option(GUROBI_REQUIRE_CXX "Fail if Gurobi C++ library not found" ON)

if(CXX)
  enable_language(CXX)
endif()

# ---------------------------------------------------------------------------
# Build type (only if single-config generators; harmless otherwise)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "[gurobi-template] Build type: ${CMAKE_BUILD_TYPE}")

# ---------------------------------------------------------------------------
# (Optional) Force static runtime if MT option is set externally:
if(MSVC AND DEFINED MT)
  # Respect existing flags; user explicitly requested it.
  foreach(var CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_DEBUG)
    if(${var} MATCHES "/MDd")
      string(REPLACE "/MDd" "/MTd" ${var} "${${var}}")
    elseif(${var} MATCHES "/MD")
      string(REPLACE "/MD" "/MT" ${var} "${${var}}")
    endif()
  endforeach()
endif()

# ---------------------------------------------------------------------------
# Locate Gurobi core (C) library + includes via your existing FindGUROBI.cmake
# Make sure the directory containing FindGUROBI.cmake is in CMAKE_MODULE_PATH
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
find_package(GUROBI REQUIRED)  # Provides: GUROBI_LIBRARY, GUROBI_INCLUDE_DIRS (and maybe more)

include_directories(${GUROBI_INCLUDE_DIRS})

# ---------------------------------------------------------------------------
# Detect the MSVC runtime flavor actually in use so we can pick the matching
# Gurobi C++ library variant.
set(_RUNTIME "")  # One of: MD, MDd, MT, MTd
if(CMAKE_MSVC_RUNTIME_LIBRARY)  # CMake 3.15+
  if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "MultithreadedDLL")
    set(_RUNTIME "MD")
  elseif(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "Multithreaded")
    set(_RUNTIME "MT")
  endif()
  if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "Debug")
    set(_RUNTIME "${_RUNTIME}d")
  endif()
else()
  # Fallback heuristic
  string(JOIN " " _ALL_FLAGS "${CMAKE_CXX_FLAGS}" "${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
  if(_ALL_FLAGS MATCHES "/MTd")
    set(_RUNTIME "MTd")
  elseif(_ALL_FLAGS MATCHES "/MT")
    set(_RUNTIME "MT")
  elseif(_ALL_FLAGS MATCHES "/MDd")
    set(_RUNTIME "MDd")
  else()
    set(_RUNTIME "MD")  # Default guess
  endif()
endif()
message(STATUS "[gurobi-template] Detected runtime: ${_RUNTIME}")

# ---------------------------------------------------------------------------
# Candidates by runtime
set(_CXX_RELEASE_CANDIDATES "")
set(_CXX_DEBUG_CANDIDATES   "")

if(_RUNTIME MATCHES "^MD")  # Dynamic CRT
  list(APPEND _CXX_RELEASE_CANDIDATES gurobi_c++md2017 gurobi_c++)
  list(APPEND _CXX_DEBUG_CANDIDATES   gurobi_c++mdd2017 gurobi_c++)
elseif(_RUNTIME MATCHES "^MT")  # Static CRT
  list(APPEND _CXX_RELEASE_CANDIDATES gurobi_c++mt2017 gurobi_c++)
  list(APPEND _CXX_DEBUG_CANDIDATES   gurobi_c++mtd2017 gurobi_c++)
else()
  # Fallback: try all
  list(APPEND _CXX_RELEASE_CANDIDATES gurobi_c++md2017 gurobi_c++mt2017 gurobi_c++)
  list(APPEND _CXX_DEBUG_CANDIDATES   gurobi_c++mdd2017 gurobi_c++mtd2017 gurobi_c++)
endif()

# Allow manual override:
#   -DGUROBI_CXX_LIBRARY_RELEASE=...  -DGUROBI_CXX_LIBRARY_DEBUG=...
if(NOT GUROBI_CXX_LIBRARY_RELEASE)
  find_library(GUROBI_CXX_LIBRARY_RELEASE
    NAMES ${_CXX_RELEASE_CANDIDATES}
    HINTS ${GUROBI_DIR} $ENV{GUROBI_HOME}
    PATH_SUFFIXES lib
  )
endif()

if(NOT GUROBI_CXX_LIBRARY_DEBUG)
  find_library(GUROBI_CXX_LIBRARY_DEBUG
    NAMES ${_CXX_DEBUG_CANDIDATES}
    HINTS ${GUROBI_DIR} $ENV{GUROBI_HOME}
    PATH_SUFFIXES lib
  )
endif()

message(STATUS "[gurobi-template] GUROBI_CXX_LIBRARY_RELEASE = ${GUROBI_CXX_LIBRARY_RELEASE}")
message(STATUS "[gurobi-template] GUROBI_CXX_LIBRARY_DEBUG   = ${GUROBI_CXX_LIBRARY_DEBUG}")
message(STATUS "[gurobi-template] GUROBI_LIBRARY (core)      = ${GUROBI_LIBRARY}")

# If we require C++ API but found nothing, abort early.
if(GUROBI_REQUIRE_CXX AND NOT GUROBI_CXX_LIBRARY_RELEASE AND NOT GUROBI_CXX_LIBRARY_DEBUG)
  message(FATAL_ERROR
    "Gurobi C++ library not found for runtime '${_RUNTIME}'. "
    "Looked for variants: ${_CXX_RELEASE_CANDIDATES} (release) / ${_CXX_DEBUG_CANDIDATES} (debug). "
    "Install Gurobi fully or pass -DGUROBI_CXX_LIBRARY_RELEASE=<path>."
  )
endif()

# ---------------------------------------------------------------------------
# Sources
set(sources mip1_c++.cpp)

add_executable(${PROJECT_NAME} ${sources})

set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
)

# Link core C library always
target_link_libraries(${PROJECT_NAME} PRIVATE ${GUROBI_LIBRARY})

# Link a single-config generator (e.g. Ninja) just picks Release var; use Debug if building Debug and available.
if(GUROBI_CXX_LIBRARY_RELEASE OR GUROBI_CXX_LIBRARY_DEBUG)
  if(GUROBI_CXX_LIBRARY_DEBUG)
    # Multi-config aware linking via generator expressions
    target_link_libraries(${PROJECT_NAME} PRIVATE
      $<$<CONFIG:Debug>:${GUROBI_CXX_LIBRARY_DEBUG}>
      $<$<NOT:$<CONFIG:Debug>>:${GUROBI_CXX_LIBRARY_RELEASE}>
    )
  else()
    target_link_libraries(${PROJECT_NAME} PRIVATE ${GUROBI_CXX_LIBRARY_RELEASE})
  endif()
endif()

# ---------------------------------------------------------------------------
# Feature summary (optional)
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
  include(FeatureSummary)
  feature_summary(WHAT ALL)
endif()
