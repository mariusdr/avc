cmake_minimum_required(VERSION 2.8)

include(FindPkgConfig)

pkg_check_modules(ALSA REQUIRED alsa)

find_package(Boost REQUIRED COMPONENTS program_options)

add_executable(avc avc.cpp)
target_link_libraries(avc ${ALSA_LIBRARIES} ${Boost_LIBRARIES})
