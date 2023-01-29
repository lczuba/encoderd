cmake_minimum_required (VERSION 3.1)
project(encoderd VERSION 1.0 LANGUAGES CXX)

add_executable(encoderd main.cpp)

find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)
find_package(pigpio REQUIRED)

target_link_libraries(encoderd PUBLIC
    ALSA::ALSA
    Threads::Threads
    pigpio::pigpiod_if2
)

install(TARGETS encoderd DESTINATION bin)
