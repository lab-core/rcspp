# A program built against an installed rcspp, as a user's project would be (check_install.cmake).

cmake_minimum_required(VERSION 3.26)

project(rcspp-install-check LANGUAGES CXX)

find_package(rcspp REQUIRED)

add_executable(rcspp-install-check main.cpp)
target_link_libraries(rcspp-install-check PRIVATE rcspp::rcspp)
