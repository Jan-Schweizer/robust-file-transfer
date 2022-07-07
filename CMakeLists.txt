cmake_minimum_required(VERSION 3.21)
project(robust_file_transfer)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

include("${CMAKE_SOURCE_DIR}/vendor/boost.cmake")
include("${CMAKE_SOURCE_DIR}/vendor/plog.cmake")
include("${CMAKE_SOURCE_DIR}/vendor/hash.cmake")

include_directories(${CMAKE_SOURCE_DIR}/include)

set(SRC_CC
    "${CMAKE_SOURCE_DIR}/rft.cpp"
    "${CMAKE_SOURCE_DIR}/src/Server.cpp"
    "${CMAKE_SOURCE_DIR}/src/Client.cpp"
    "${CMAKE_SOURCE_DIR}/src/Bitfield.cpp"
    "${CMAKE_SOURCE_DIR}/src/util.cpp"
    "${hash_SOURCE_DIR}/sha256.cpp"
    )

add_executable(rft ${SRC_CC})
target_link_libraries(rft Boost::program_options)
