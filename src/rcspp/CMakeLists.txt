project(rcspp VERSION 0.1 LANGUAGES CXX)

# Gather all C++ sources/headers for the library -------------------------------
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE HEADERS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")

# Generate the umbrella header rcspp.hpp ---------------------------------------
set(RCSPP_UMBRELLA_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/rcspp.hpp")

# first lines of the umbrella header
file(WRITE "${RCSPP_UMBRELLA_HEADER}" "#pragma once\n\n// Automatically generated umbrella header\n\n")

foreach(header ${HEADERS_FILES})
    # Compute relative path inside
    file(RELATIVE_PATH rel_path "${CMAKE_SOURCE_DIR}/src" "${header}")
	
    # Add it to the umbrella header
    file(APPEND "${RCSPP_UMBRELLA_HEADER}" "#include \"${rel_path}\"\n")
endforeach()

# add the umbrella header to the list
set(${HEADERS_FILES} ${RCSPP_UMBRELLA_HEADER} ${HEADERS_FILES})

# Compile the dynamic library --------------------------------------------------
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(LIB ${PROJECT_NAME})
add_library(${LIB} SHARED ${SOURCE_FILES} ${HEADERS_FILES})

# Set library version
set_target_properties(${LIB} PROPERTIES VERSION ${PROJECT_VERSION})
target_compile_features(${LIB} PUBLIC cxx_std_23)

# Copy headers to build/include preserving folder structure --------------------
foreach(header ${HEADERS_FILES})
    # Compute relative path inside
    file(RELATIVE_PATH rel_path "${CMAKE_SOURCE_DIR}/src" "${header}")
    set(dest "${BUILD_INCLUDE_DIR}/${rel_path}")

    # Make sure destination folder exists
    get_filename_component(dest_dir "${dest}" DIRECTORY)
    file(MAKE_DIRECTORY "${dest_dir}")

    # Copy header
    configure_file("${header}" "${dest}" COPYONLY)
endforeach()

# Make the build include directory available to other targets
target_include_directories(${LIB}
    PUBLIC
        $<BUILD_INTERFACE:${BUILD_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
)
