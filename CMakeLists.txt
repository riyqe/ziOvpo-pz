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

target_include_directories(TrayApp PRIVATE
    ziOvpo-pz
    ziOvpo-pz/common
    ziOvpo-pz/rpc
)
target_compile_definitions(TrayApp PRIVATE UNICODE _UNICODE)
target_link_libraries(TrayApp PRIVATE shell32 rpcrt4 advapi32)

add_executable(ZiOvpoPzService
    ziOvpo-pz/service/ZiOvpoPzService.cpp
)

target_include_directories(ZiOvpoPzService PRIVATE
    ziOvpo-pz
    ziOvpo-pz/common
    ziOvpo-pz/rpc
)
target_compile_definitions(ZiOvpoPzService PRIVATE UNICODE _UNICODE)
target_link_libraries(ZiOvpoPzService PRIVATE rpcrt4 wtsapi32 userenv advapi32 winhttp iphlpapi crypt32)

set(ZIOVPO_DATA_DIR ${CMAKE_SOURCE_DIR}/ziOvpo-pz/data)
foreach(target_name IN ITEMS TrayApp ZiOvpoPzService)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${target_name}>/data
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ZIOVPO_DATA_DIR}/default.avdb
            $<TARGET_FILE_DIR:${target_name}>/data/default.avdb
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ZIOVPO_DATA_DIR}/public_cert.pem
            $<TARGET_FILE_DIR:${target_name}>/data/public_cert.pem
    )
endforeach()
