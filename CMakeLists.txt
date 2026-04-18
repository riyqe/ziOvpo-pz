cmake_minimum_required(VERSION 3.20)
project(TrayApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(TrayApp WIN32
    ziOvpo-pz/ziOvpo-pz.cpp
    ziOvpo-pz/ziOvpo-pz.rc
    ziOvpo-pz/ziOvpo-pz.ico
    ziOvpo-pz/small.ico
)

target_include_directories(TrayApp PRIVATE ziOvpo-pz)
target_compile_definitions(TrayApp PRIVATE UNICODE _UNICODE)
target_link_libraries(TrayApp PRIVATE shell32)
