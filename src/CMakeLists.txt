cmake_minimum_required(VERSION 3.13)

set(CMAKE_BUILD_TYPE "Release")

include(pico_sdk_import.cmake)

project(myproject_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_SDK_FETCH_FROM_GIT)

pico_sdk_init()

add_executable(i3cblaster
	main.c
	i3c_hl.c
	ucli.c
	XiaoNeoPixel.c
	)

pico_generate_pio_header(i3cblaster ${CMAKE_CURRENT_LIST_DIR}/i3c.pio)


pico_enable_stdio_usb(i3cblaster 1)
pico_enable_stdio_uart(i3cblaster 0)


target_link_libraries(i3cblaster pico_stdlib hardware_pio  hardware_adc)

pico_add_extra_outputs(i3cblaster)


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

