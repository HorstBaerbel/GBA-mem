cmake_minimum_required(VERSION 3.1.0)

# Note that you must copy the cmake folder and the DevkitArmGBA.cmake file in this directory
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/3ds-cmake/DevkitArmGBA.cmake)
# Add the cmake folder to the modules paths, so that we can use the tools
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/3ds-cmake/cmake)
# ASM must be enabled to support .S files
enable_language(ASM)
# Include all the macros and tools needed for GBA development.
include(ToolsGBA)

project(MemTestGBA)

# List all the source files in out directory
LIST(APPEND SOURCE_FILES
	./main.cpp
	./memcpy.s
)

LIST(APPEND EXTRA_SOURCE_FILES
)

LIST(APPEND EXTRA_DATA_FILES
	./data/dkp_logo.c
)

LIST(APPEND INCLUDE_DIRECTORIES
	${DEVKITPRO}/libgba/include
	${DEVKITARM}/arm-none-eabi/include/
)

LIST(APPEND TARGET_LIBRARIES
	${DEVKITPRO}/libgba/lib
)

link_directories(${TARGET_LIBRARIES})
include_directories(${INCLUDE_DIRECTORIES})
add_executable(${CMAKE_PROJECT_NAME}.elf ${SOURCE_FILES} ${INCLUDE_FILES} ${EXTRA_SOURCE_FILES} ${EXTRA_DATA_FILES}) # Create the elf file
add_gba_executable(${CMAKE_PROJECT_NAME}.elf) # Generate the .gba from the elf
target_link_libraries(${CMAKE_PROJECT_NAME}.elf gba mm) # Link the application, libgba and maxmod

# List all the binary files
file(GLOB_RECURSE DATA_FILES
	./data/*.bin
)
