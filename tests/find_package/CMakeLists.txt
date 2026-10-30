# Programs built against an installed rcspp, as a user's project would be (check_install.cmake): one
# for the core library, one for each component in COMPONENTS ("|"-separated).

cmake_minimum_required(VERSION 3.26)

project(rcspp-install-check LANGUAGES CXX)

string(REPLACE "|" ";" components "${COMPONENTS}")
find_package(rcspp REQUIRED COMPONENTS ${components})

add_executable(rcspp-install-check main.cpp)
target_link_libraries(rcspp-install-check PRIVATE rcspp::rcspp)

foreach(component ${components})
  add_executable(rcspp-install-check-${component} ${component}.cpp)
  target_link_libraries(rcspp-install-check-${component} PRIVATE rcspp::${component})
endforeach()
