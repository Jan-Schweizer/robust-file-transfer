find_package(Boost 1.78 COMPONENTS program_options REQUIRED)

find_path(BOOST_ASIO_INCLUDE_DIR NAMES boost/asio.hpp REQUIRED)

find_path(BOOST_BIND_INCLUDE_DIR NAMES boost/bind.hpp REQUIRED)

find_path(BOOST_MULTIPRECISION_INCLUDE_DIR NAMES boost/multiprecision/cpp_int.hpp REQUIRED)
