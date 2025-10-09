# Enhanced Gurobi finder for Windows (handles suffixed C++ libs)
include(FindPackageHandleStandardArgs)

option(GUROBI_REQUIRE_CXX "Fail if Gurobi C++ library not found" ON)

# Allow -DGUROBI_HOME or environment variable
set(_GUROBI_HINTS
    ${GUROBI_DIR}
    $ENV{GUROBI_HOME}
)

# Include directories (both headers live in same include dir)
find_path(GUROBI_INCLUDE_DIRS
    NAMES gurobi_c.h
    HINTS ${_GUROBI_HINTS}
    PATH_SUFFIXES include
)

# Core (C) library (names differ by version)
find_library(GUROBI_LIBRARY
    NAMES gurobi gurobi120 gurobi110 gurobi100
    HINTS ${_GUROBI_HINTS}
    PATH_SUFFIXES lib
)

# C++ header (presence indicates you likely use the C++ API)
find_path(GUROBI_CXX_INCLUDE_DIR
    NAMES gurobi_c++.h
    HINTS ${_GUROBI_HINTS}
    PATH_SUFFIXES include
)

# If user manually supplies GUROBI_CXX_LIBRARY, respect it; otherwise search.
if(NOT GUROBI_CXX_LIBRARY AND NOT GUROBI_CXX_LIBRARY_RELEASE)
    # Try explicit variant names Gurobi ships on Windows
    # Dynamic runtime (/MD)
    find_library(GUROBI_CXX_LIBRARY_RELEASE_MD
        NAMES gurobi_c++md2017
        HINTS ${_GUROBI_HINTS}
        PATH_SUFFIXES lib
    )
    find_library(GUROBI_CXX_LIBRARY_DEBUG_MD
        NAMES gurobi_c++mdd2017
        HINTS ${_GUROBI_HINTS}
        PATH_SUFFIXES lib
    )
    # Static runtime (/MT)
    find_library(GUROBI_CXX_LIBRARY_RELEASE_MT
        NAMES gurobi_c++mt2017
        HINTS ${_GUROBI_HINTS}
        PATH_SUFFIXES lib
    )
    find_library(GUROBI_CXX_LIBRARY_DEBUG_MT
        NAMES gurobi_c++mtd2017
        HINTS ${_GUROBI_HINTS}
        PATH_SUFFIXES lib
    )

    # Fallback generic (older naming) if none of the above found
    find_library(GUROBI_CXX_LIBRARY
        NAMES gurobi_c++
        HINTS ${_GUROBI_HINTS}
        PATH_SUFFIXES lib
    )
endif()

# Determine which runtime we are using to prioritize appropriate libs
# CMake 3.15+ may define CMAKE_MSVC_RUNTIME_LIBRARY; otherwise inspect flags.
set(_RUNTIME_KIND "")
if(CMAKE_MSVC_RUNTIME_LIBRARY)
    if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "MultithreadedDLL")
        set(_RUNTIME_KIND "MD")
    elseif(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "Multithreaded")
        set(_RUNTIME_KIND "MT")
    endif()
else()
    # Heuristic
    string(JOIN " " _ALL_CXX_FLAGS "${CMAKE_CXX_FLAGS}" "${CMAKE_CXX_FLAGS_RELEASE}")
    if(_ALL_CXX_FLAGS MATCHES "/MT")
        if(_ALL_CXX_FLAGS MATCHES "/MTd")
            set(_RUNTIME_KIND "MTd")
        else()
            set(_RUNTIME_KIND "MT")
        endif()
    elseif(_ALL_CXX_FLAGS MATCHES "/MD")
        if(_ALL_CXX_FLAGS MATCHES "/MDd")
            set(_RUNTIME_KIND "MDd")
        else()
            set(_RUNTIME_KIND "MD")
        endif()
    endif()
endif()

# Choose candidate libs for Release/Debug
set(GUROBI_CXX_LIBRARY_RELEASE "")
set(GUROBI_CXX_LIBRARY_DEBUG "")

# Prefer dynamic runtime set
if(_RUNTIME_KIND MATCHES "MD" OR _RUNTIME_KIND STREQUAL "")
    if(GUROBI_CXX_LIBRARY_RELEASE_MD)
        set(GUROBI_CXX_LIBRARY_RELEASE "${GUROBI_CXX_LIBRARY_RELEASE_MD}")
    endif()
    if(GUROBI_CXX_LIBRARY_DEBUG_MD)
        set(GUROBI_CXX_LIBRARY_DEBUG "${GUROBI_CXX_LIBRARY_DEBUG_MD}")
    endif()
endif()

# If not found and static available, use static
if(NOT GUROBI_CXX_LIBRARY_RELEASE AND GUROBI_CXX_LIBRARY_RELEASE_MT)
    set(GUROBI_CXX_LIBRARY_RELEASE "${GUROBI_CXX_LIBRARY_RELEASE_MT}")
endif()
if(NOT GUROBI_CXX_LIBRARY_DEBUG AND GUROBI_CXX_LIBRARY_DEBUG_MT)
    set(GUROBI_CXX_LIBRARY_DEBUG "${GUROBI_CXX_LIBRARY_DEBUG_MT}")
endif()

# If user provided a generic GUROBI_CXX_LIBRARY (non-suffixed), use as fallback for both configs
if(GUROBI_CXX_LIBRARY AND NOT GUROBI_CXX_LIBRARY_RELEASE)
    set(GUROBI_CXX_LIBRARY_RELEASE "${GUROBI_CXX_LIBRARY}")
endif()
if(GUROBI_CXX_LIBRARY AND NOT GUROBI_CXX_LIBRARY_DEBUG)
    set(GUROBI_CXX_LIBRARY_DEBUG "${GUROBI_CXX_LIBRARY}")
endif()

# Core is required
find_package_handle_standard_args(GUROBI
    REQUIRED_VARS GUROBI_LIBRARY GUROBI_INCLUDE_DIRS
)

if(NOT GUROBI_FOUND)
    message(FATAL_ERROR "Failed to locate Gurobi core library.")
endif()

# If C++ header present and required, enforce at least a release lib
if(GUROBI_REQUIRE_CXX AND GUROBI_CXX_INCLUDE_DIR AND NOT GUROBI_CXX_LIBRARY_RELEASE)
    message(FATAL_ERROR
        "Gurobi C++ header found at ${GUROBI_CXX_INCLUDE_DIR} but no matching C++ library was found.\n"
        "Searched variants: gurobi_c++md2017 / mdd / mt / mtd.\n"
        "Check that these files exist in <GUROBI_HOME>/lib or pass -DGUROBI_CXX_LIBRARY=<path>."
    )
endif()

# Imported core target
if(NOT TARGET GUROBI::gurobi)
    add_library(GUROBI::gurobi UNKNOWN IMPORTED)
    set_target_properties(GUROBI::gurobi PROPERTIES
        IMPORTED_LOCATION "${GUROBI_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GUROBI_INCLUDE_DIRS}"
    )
endif()

# Imported C++ target
if(GUROBI_CXX_LIBRARY_RELEASE)
    if(NOT TARGET GUROBI::gurobi_cxx)
        add_library(GUROBI::gurobi_cxx UNKNOWN IMPORTED)
        # If we have a debug variant use configuration-specific properties
        if(GUROBI_CXX_LIBRARY_DEBUG)
            set_target_properties(GUROBI::gurobi_cxx PROPERTIES
                IMPORTED_LOCATION "${GUROBI_CXX_LIBRARY_RELEASE}"
                IMPORTED_LOCATION_RELEASE "${GUROBI_CXX_LIBRARY_RELEASE}"
                IMPORTED_LOCATION_RELWITHDEBINFO "${GUROBI_CXX_LIBRARY_RELEASE}"
                IMPORTED_LOCATION_MINSIZEREL "${GUROBI_CXX_LIBRARY_RELEASE}"
                IMPORTED_LOCATION_DEBUG "${GUROBI_CXX_LIBRARY_DEBUG}"
                INTERFACE_INCLUDE_DIRECTORIES "${GUROBI_INCLUDE_DIRS};${GUROBI_CXX_INCLUDE_DIR}"
                INTERFACE_LINK_LIBRARIES "GUROBI::gurobi"
            )
        else()
            set_target_properties(GUROBI::gurobi_cxx PROPERTIES
                IMPORTED_LOCATION "${GUROBI_CXX_LIBRARY_RELEASE}"
                INTERFACE_INCLUDE_DIRECTORIES "${GUROBI_INCLUDE_DIRS};${GUROBI_CXX_INCLUDE_DIR}"
                INTERFACE_LINK_LIBRARIES "GUROBI::gurobi"
            )
        endif()
    endif()
endif()

# Diagnostics
message(STATUS "---- Gurobi Detection ----")
message(STATUS "GUROBI_INCLUDE_DIRS      = ${GUROBI_INCLUDE_DIRS}")
message(STATUS "GUROBI_LIBRARY           = ${GUROBI_LIBRARY}")
message(STATUS "GUROBI_CXX_INCLUDE_DIR   = ${GUROBI_CXX_INCLUDE_DIR}")
message(STATUS "GUROBI_CXX_LIBRARY_RELEASE = ${GUROBI_CXX_LIBRARY_RELEASE}")
message(STATUS "GUROBI_CXX_LIBRARY_DEBUG   = ${GUROBI_CXX_LIBRARY_DEBUG}")
message(STATUS "Runtime heuristic (_RUNTIME_KIND) = ${_RUNTIME_KIND}")
message(STATUS "GUROBI_REQUIRE_CXX       = ${GUROBI_REQUIRE_CXX}")
message(STATUS "--------------------------")
