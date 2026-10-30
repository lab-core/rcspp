# Installs the built rcspp into PREFIX, then configures and builds the programs of this folder
# against it, with find_package(rcspp). Run by CTest (install_and_find_package), after a build.
#
# Expects: BUILD_DIR, CONFIG (the build configuration, for multi-configuration generators), PREFIX,
# CONSUMER_SOURCE_DIR, CONSUMER_BUILD_DIR, GENERATOR, and optionally COMPONENTS (the components
# built, "|"-separated: a program each), GENERATOR_PLATFORM and CXX_COMPILER, so that the programs
# are built as rcspp was.

foreach(variable BUILD_DIR PREFIX CONSUMER_SOURCE_DIR CONSUMER_BUILD_DIR GENERATOR)
  if(NOT ${variable})
    message(FATAL_ERROR "check_install.cmake requires -D${variable}")
  endif()
endforeach()

# Start from nothing, so that files of an earlier install cannot make the check pass.
file(REMOVE_RECURSE "${PREFIX}" "${CONSUMER_BUILD_DIR}")

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

set(configure_arguments -S "${CONSUMER_SOURCE_DIR}" -B "${CONSUMER_BUILD_DIR}" -G "${GENERATOR}"
    "-DCMAKE_PREFIX_PATH=${PREFIX}" "-DCOMPONENTS=${COMPONENTS}")
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
run("${CMAKE_COMMAND}" ${configure_arguments})
run("${CMAKE_COMMAND}" --build "${CONSUMER_BUILD_DIR}" ${config_arguments})
