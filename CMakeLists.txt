cmake_minimum_required(VERSION 2.8)
project(evelog)

#set(Boost_DEBUG ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost COMPONENTS system thread date_time regex filesystem REQUIRED)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-std=c++0x EVELOG_HAS_STDCXX0X_FLAG)
if (EVELOG_HAS_STDCXX0X_FLAG)
  add_definitions(-std=c++0x)
endif()

include_directories(include ${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

add_subdirectory(source)
add_subdirectory(tools)
