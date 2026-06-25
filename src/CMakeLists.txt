# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)
find_package(Git)

#set(CMAKE_BUILD_TYPE "Release")
#set(CMAKE_BUILD_TYPE "Debug")
#set(CMAKE_BUILD_TYPE "RelWithDebInfo")
set(CMAKE_BUILD_TYPE "RelWithDebInfo")

include(pico_sdk_import.cmake)

project(i3cblaster C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(GIT_EXECUTABLE)
  # Generate a git-describe version string from Git repository tags
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --dirty
    OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
    RESULT_VARIABLE GIT_DESCRIBE_ERROR_CODE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  if(NOT GIT_DESCRIBE_ERROR_CODE)
    set(I3CBLASTER_VERSION \"${GIT_DESCRIBE_VERSION}\")
  endif()
endif()

if(NOT DEFINED I3CBLASTER_VERSION)
  set(I3CBLASTER_VERSION \"unknown\")
endif()

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


target_link_libraries(i3cblaster pico_stdlib hardware_pio  hardware_adc hardware_i2c)

pico_add_extra_outputs(i3cblaster)


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -D VERSION='${I3CBLASTER_VERSION}'")

