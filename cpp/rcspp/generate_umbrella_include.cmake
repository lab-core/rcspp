# filepath: ${CMAKE_CURRENT_SOURCE_DIR}/generate_umbrella_include.cmake
# Helper cmake script invoked by add_custom_command to generate the umbrella header
# Expects two variables passed on the command line:
#  -DREPO_SRC_DIR: absolute path to the repository "src" folder
#  -DOUT_FILE: absolute path to the output umbrella header to write

if(NOT REPO_SRC_DIR)
  message(FATAL_ERROR "generate_umbrella_include.cmake requires -DREPO_SRC_DIR to be set")
endif()
if(NOT REPO_REL_DIR)
  message(FATAL_ERROR "generate_umbrella_include.cmake requires -DREPO_REL_DIR to be set")
endif()
if(NOT REPO_BUILD_DIR)
  message(FATAL_ERROR "generate_umbrella_include.cmake requires -DREPO_BUILD_DIR to be set")
endif()
if(NOT OUT_FILE)
  message(FATAL_ERROR "generate_umbrella_include.cmake requires -DOUT_FILE to be set")
endif()

message("Generating umbrella header at: ${OUT_FILE} from headers under: ${REPO_SRC_DIR}/*.hpp relative to: ${REPO_REL_DIR}")

# Collect headers under REPO_SRC_DIR (non-recursive safe approach: use file(GLOB_RECURSE))
file(GLOB_RECURSE ALL_HEADERS "${REPO_SRC_DIR}/*.hpp")

# Remove the umbrella header itself if present to avoid self-include
list(REMOVE_ITEM ALL_HEADERS "${OUT_FILE}")

# Sort the list for deterministic output
list(SORT ALL_HEADERS)

# Write the file header and pragma once
file(WRITE "${OUT_FILE}" "// Automatically generated umbrella header clang-format off NOLINT(legal/copyright)\n#pragma once\n\n")

# Append includes relative to REPO_SRC_DIR
foreach(h ${ALL_HEADERS})
  file(RELATIVE_PATH rel_path "${REPO_REL_DIR}" "${h}")
  # Normalize: replace backslashes with forward slashes (Windows)
  string(REPLACE "\\" "/" rel_path "${rel_path}")
  file(APPEND "${OUT_FILE}" "#include \"${rel_path}\"\n")
  message(STATUS "Including header: ${rel_path} in umbrella header: ${OUT_FILE}")
endforeach()

message(STATUS "Umbrella header generation complete: ${OUT_FILE}")

# Copy headers to build/include preserving folder structure --------------------
# add the umbrella header to the list
foreach(h ${ALL_HEADERS} ${OUT_FILE})
  # Compute relative path inside
  file(RELATIVE_PATH rel_path "${REPO_REL_DIR}" "${h}")
  set(dest "${REPO_REL_DIR}/${rel_path}")

  # Make sure destination folder exists
  get_filename_component(dest_dir "${dest}" DIRECTORY)
  file(MAKE_DIRECTORY "${dest_dir}")

  # Copy header
  configure_file("${h}" "${dest}" COPYONLY)
  message(STATUS "Copied header: ${rel_path} to build include directory: ${dest}")
endforeach()
