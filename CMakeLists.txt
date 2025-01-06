cmake_minimum_required(VERSION 3.20)

set(PICO_BOARD pico_w)

include(pico_sdk_import.cmake)
include(FreeRTOS_Kernel_import.cmake)

project(FreeRTOS_tcp_httpd_demo C CXX ASM)

pico_sdk_init()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_executable(tcp_httpd_demo
        tcp_demo.cpp
        tcp_server.cpp
        tcp_server_command.cpp        
        ${PICO_SDK_PATH}/lib/lwip/src/apps/http/httpd.c
        ${PICO_SDK_PATH}/lib/lwip/src/apps/http/fs.c        
        )

target_compile_definitions(tcp_httpd_demo PRIVATE
        WIFI_SSID="C14017750 7261"
        WIFI_PASSWORD="malamala"
        NO_SYS=0)

target_include_directories(tcp_httpd_demo PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../..)

include(FetchContent)
FetchContent_Declare(ArduinoJson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG        v7.0.4
)
FetchContent_MakeAvailable(ArduinoJson)

target_link_libraries(tcp_httpd_demo
        pico_cyw43_arch_lwip_sys_freertos
        pico_stdlib
        pico_lwip_iperf
        FreeRTOS-Kernel-Heap4
        ArduinoJson
        )

pico_enable_stdio_usb(tcp_httpd_demo 1)
pico_enable_stdio_uart(tcp_httpd_demo 0)
    
pico_add_extra_outputs(tcp_httpd_demo)
