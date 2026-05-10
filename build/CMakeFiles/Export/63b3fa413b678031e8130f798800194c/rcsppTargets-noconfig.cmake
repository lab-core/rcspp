#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "rcspp::rcspp" for configuration ""
set_property(TARGET rcspp::rcspp APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(rcspp::rcspp PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/librcspp.so.0.1"
  IMPORTED_SONAME_NOCONFIG "librcspp.so.0.1"
  )

list(APPEND _cmake_import_check_targets rcspp::rcspp )
list(APPEND _cmake_import_check_files_for_rcspp::rcspp "${_IMPORT_PREFIX}/lib/librcspp.so.0.1" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
