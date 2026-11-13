# Installs the built rcspp into PREFIX, then configures and builds the program of this folder
# against it, with find_package(rcspp). Run by CTest (install_and_find_package), after a build.
#
# Expects: BUILD_DIR, CONFIG (the build configuration, for multi-configuration generators), PREFIX,
# CONSUMER_SOURCE_DIR, CONSUMER_BUILD_DIR, GENERATOR, and optionally GENERATOR_PLATFORM and
# CXX_COMPILER, so that the program is built as rcspp was. With EXAMPLE_SOURCE_DIR and
# EXAMPLE_BUILD_DIR, the column generation example is then built on its own against the same
# install and its tests run (EXAMPLE_ARGUMENTS: more configure arguments, "|"-separated).

foreach(variable BUILD_DIR PREFIX CONSUMER_SOURCE_DIR CONSUMER_BUILD_DIR GENERATOR)
  if(NOT ${variable})
    message(FATAL_ERROR "check_install.cmake requires -D${variable}")
  endif()
endforeach()

# Start from nothing, so that files of an earlier install cannot make the check pass.
file(REMOVE_RECURSE "${PREFIX}" "${CONSUMER_BUILD_DIR}" "${EXAMPLE_BUILD_DIR}")

function(run)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    string(JOIN " " command ${ARGN})
    message(FATAL_ERROR "Failed (${result}): ${command}")
  endif()
endfunction()

set(config_arguments)
if(CONFIG)
  set(config_arguments --config "${CONFIG}")
endif()
run("${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${PREFIX}" ${config_arguments})

# The arguments every project built against the install shares.
set(configure_arguments -G "${GENERATOR}" "-DCMAKE_PREFIX_PATH=${PREFIX}")
if(GENERATOR_PLATFORM)
  list(APPEND configure_arguments -A "${GENERATOR_PLATFORM}")
endif()
# Visual Studio generators pick their compiler themselves.
if(CXX_COMPILER AND NOT GENERATOR MATCHES "^Visual Studio")
  list(APPEND configure_arguments "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
endif()
if(CONFIG)
  list(APPEND configure_arguments "-DCMAKE_BUILD_TYPE=${CONFIG}")
endif()
run("${CMAKE_COMMAND}" -S "${CONSUMER_SOURCE_DIR}" -B "${CONSUMER_BUILD_DIR}" ${configure_arguments})
run("${CMAKE_COMMAND}" --build "${CONSUMER_BUILD_DIR}" ${config_arguments})

if(EXAMPLE_SOURCE_DIR AND EXAMPLE_BUILD_DIR)
  string(REPLACE "|" ";" example_arguments "${EXAMPLE_ARGUMENTS}")
  run("${CMAKE_COMMAND}" -S "${EXAMPLE_SOURCE_DIR}" -B "${EXAMPLE_BUILD_DIR}" ${configure_arguments}
      ${example_arguments})
  run("${CMAKE_COMMAND}" --build "${EXAMPLE_BUILD_DIR}" ${config_arguments})
  set(ctest_arguments)
  if(CONFIG)
    set(ctest_arguments -C "${CONFIG}")
  endif()
  run("${CMAKE_CTEST_COMMAND}" --test-dir "${EXAMPLE_BUILD_DIR}" ${ctest_arguments}
      --output-on-failure)
endif()
