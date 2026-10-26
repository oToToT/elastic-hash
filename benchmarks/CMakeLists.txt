cmake_minimum_required(VERSION 3.24)
project(elastic_hash_comparison LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
include(FetchContent)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(abseil
  URL https://codeload.github.com/abseil/abseil-cpp/tar.gz/refs/tags/20260817.0
  URL_HASH SHA256=f7e05179df39c45434cad433f5783840bb3788ef322976f9138bc6b72b3a107d
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(unordered_dense
  URL https://codeload.github.com/martinus/unordered_dense/tar.gz/refs/tags/v4.8.1
  URL_HASH SHA256=9f7202ec6d8353932ef865d33f5872e4b7a1356e9032da7cd09c3a0c5bb2b7de
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(abseil unordered_dense)
add_executable(hash_comparison comparison.cc)
target_include_directories(hash_comparison PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_include_directories(hash_comparison SYSTEM PRIVATE "${abseil_SOURCE_DIR}" "${unordered_dense_SOURCE_DIR}/include")
target_link_libraries(hash_comparison PRIVATE absl::flat_hash_map unordered_dense::unordered_dense)
target_compile_options(hash_comparison PRIVATE -Wall -Wextra -Wpedantic)

set(ELASTIC_CANDIDATE_HEADER "" CACHE FILEPATH "Optional alternate header for a second benchmark binary")
if(ELASTIC_CANDIDATE_HEADER)
  add_executable(hash_comparison_candidate comparison.cc)
  target_include_directories(hash_comparison_candidate PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
  target_include_directories(hash_comparison_candidate SYSTEM PRIVATE "${abseil_SOURCE_DIR}" "${unordered_dense_SOURCE_DIR}/include")
  target_link_libraries(hash_comparison_candidate PRIVATE absl::flat_hash_map unordered_dense::unordered_dense)
  target_compile_definitions(hash_comparison_candidate PRIVATE ELASTIC_HASH_HEADER="${ELASTIC_CANDIDATE_HEADER}")
  target_compile_options(hash_comparison_candidate PRIVATE -Wall -Wextra -Wpedantic)
endif()
