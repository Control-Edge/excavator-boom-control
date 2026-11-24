cmake_minimum_required(VERSION 3.21)

project(BoomControl LANGUAGES C)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/dependencies")
find_package(cppmodel REQUIRED)

enable_testing()

add_executable(BoomControlSimulation BoomControlSimulation.c BoomControl.c)
target_link_libraries(BoomControlSimulation PRIVATE cppmodel::cppmodel)
cppmodel_deploy_runtime(BoomControlSimulation)

add_test(NAME BoomControl COMMAND BoomControlSimulation)
