cmake_minimum_required(VERSION 3.10)

project(ufs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSONS OFF)

option(UFS_ENABLE_UNICODE "Enable unicode builds" ON)

add_library(ufs INTERFACE
  include/ufs/ufs.h
)
target_include_directories(ufs INTERFACE
  include
)
