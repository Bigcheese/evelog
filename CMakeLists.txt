cmake_minimum_required(VERSION 2.8)
project(evelog)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-std=c++0x EVELOG_HAS_STDCXX0X_FLAG)
if (EVELOG_HAS_STDCXX0X_FLAG)
  add_definitions(-std=c++0x)
endif()

include_directories(include)

# add_subdirectory(source)
add_subdirectory(tools)
