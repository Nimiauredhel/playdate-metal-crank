cmake_minimum_required(VERSION 3.14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)

option(PLATFORM_PLAYDATE "Build for playdate" ON)
option(PLAYDATE_BUILD_FOR_DEVICE "Build for playdate device")

#if (TOOLCHAIN STREQUAL "armgcc")
#     set(CMAKE_ASM_COMPILER "/usr/bin/arm-none-eabi-gcc-ar")
#     set(CMAKE_C_COMPILER "/usr/bin/arm-none-eabi-gcc")
#     set(CMAKE_CXX_COMPILER "/usr/bin/arm-none-eabi-c++")
#endif()

if (PLAYDATE_BUILD_FOR_DEVICE)
      ADD_DEFINITIONS(-DPLATFORM_IMPL_PLAYDATE)
      add_link_options("--specs=nosys.specs")
else()
endif()

set(ENVSDK $ENV{PLAYDATE_SDK_PATH})

if (NOT ${ENVSDK} STREQUAL "")
	# Convert path from Windows
	file(TO_CMAKE_PATH ${ENVSDK} SDK)
else()
	execute_process(
			COMMAND bash -c "egrep '^\\s*SDKRoot' $HOME/.Playdate/config"
			COMMAND head -n 1
			COMMAND cut -c9-
			OUTPUT_VARIABLE SDK
			OUTPUT_STRIP_TRAILING_WHITESPACE
	)
endif()

if (NOT EXISTS ${SDK})
	message(FATAL_ERROR "SDK Path not found; set ENV value PLAYDATE_SDK_PATH")
	return()
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_XCODE_GENERATE_SCHEME TRUE)

# Game Name Customization
set(PLAYDATE_GAME_NAME metal_crank)
set(PLAYDATE_GAME_DEVICE metal_crank_DEVICE)

project(${PLAYDATE_GAME_NAME} C ASM)

add_compile_options(--specs=/lib/arm-none-eabi/newlib/nosys.specs)

if (TOOLCHAIN STREQUAL "armgcc")
	add_executable(${PLAYDATE_GAME_DEVICE} src/main.c)
else()
	add_library(${PLAYDATE_GAME_NAME} SHARED src/main.c )
endif()

include(${SDK}/C_API/buildsupport/playdate_game.cmake)
