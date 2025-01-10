cmake_minimum_required(VERSION 3.20)

set(PICO_BOARD pico_w)

include(pico_sdk_import.cmake)
include(FreeRTOS_Kernel_import.cmake)

project(FreeRTOS_tcp_httpd_demo C CXX ASM)

pico_sdk_init()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Set paths
set(HTML_SOURCE_DIR ${CMAKE_SOURCE_DIR}/fs)
set(GENERATED_FS_FILE ${CMAKE_BINARY_DIR}/fsdata.c)

# Find the htmlgen tool
find_program(HTMLGEN_EXECUTABLE html_gen PATHS ../html_gen/build/)
if(NOT HTMLGEN_EXECUTABLE)
    message(FATAL_ERROR "htmlgen executable not found!")
endif()

# Add a custom command to generate fsdata.c
add_custom_command(
    OUTPUT ${GENERATED_FS_FILE}
    COMMAND ${HTMLGEN_EXECUTABLE} ${HTML_SOURCE_DIR} 
    DEPENDS ${HTML_SOURCE_DIR}
    COMMENT "Generating fsdata.c from HTML files"
)
add_executable(tcp_httpd_demo
        tcp_demo.cpp
        tcp_server.cpp
        tcp_server_command.cpp
        cgi_example.c
        ssi_example.c
        post_example.c
        httpd.c
        fs.c
        )

target_compile_definitions(tcp_httpd_demo PRIVATE
        WIFI_SSID="C14017750 7261"
        WIFI_PASSWORD="malamala"
        NO_SYS=0)

target_include_directories(tcp_httpd_demo PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        inc/
        )

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
        pico_lwip_mqtt
        )

# Ensure fsdata.c is generated before the build
add_custom_target(generate_fsdata DEPENDS ${GENERATED_FS_FILE})
add_dependencies(tcp_httpd_demo generate_fsdata)

pico_enable_stdio_usb(tcp_httpd_demo 1)
pico_enable_stdio_uart(tcp_httpd_demo 0)
    
pico_add_extra_outputs(tcp_httpd_demo)
