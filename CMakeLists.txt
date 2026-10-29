cmake_minimum_required(VERSION 3.25)
project(bwos_m4_report_benchmark LANGUAGES CXX)

# NVIDIA/stdexec main currently requires CMake 3.28; fail early with its precise requirement.
if(CMAKE_VERSION VERSION_LESS 3.28)
  message(FATAL_ERROR "NVIDIA/stdexec main requires CMake 3.28 or newer (found ${CMAKE_VERSION}).")
endif()

include(FetchContent)
# Keep already-fetched revisions stable across reruns; a fresh build still fetches
# stdexec main and the pinned RAPIDS helper once.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(STDEXEC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(STDEXEC_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(STDEXEC_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(STDEXEC_INSTALL OFF CACHE BOOL "" FORCE)

# stdexec fetches RAPIDS CMake as a helper; declare it here using Git so the
# complete dependency chain is handled by FetchContent without manual clones.
FetchContent_Declare(
  rapids-cmake
  GIT_REPOSITORY https://github.com/rapidsai/rapids-cmake.git
  GIT_TAG branch-24.02
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  stdexec
  GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
  GIT_TAG main
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stdexec)

add_executable(bwos_benchmark src/benchmark.cpp)
target_compile_features(bwos_benchmark PRIVATE cxx_std_20)
target_compile_options(bwos_benchmark PRIVATE -O2)
target_compile_definitions(bwos_benchmark PRIVATE STDEXEC_NO_DEBUG)
target_link_libraries(bwos_benchmark PRIVATE STDEXEC::stdexec)
