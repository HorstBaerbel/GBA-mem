cmake_minimum_required(VERSION 3.1.0)

# Use GBA-demo-framework toolchain
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/3ds-cmake/DevkitArmGBA.cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/3ds-cmake/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

project(MemTestGBA)

# ASM must be enabled to support .S files
enable_language(ASM)
# Include all the macros and tools needed for GBA development
include(ToolsGBA)
# Using some special compiler flags for GBA
include(CompilerFlagsGBA)

# List all the source files in our directory
LIST(APPEND SOURCE_FILES
	dma.cpp
	itoa.cpp
	main.cpp
	memcpy.s
	memory.cpp
	memset.s
	tui.cpp
)

LIST(APPEND EXTRA_SRC_FILES
)

LIST(APPEND EXTRA_DATA_FILES
	./data/font_8x8.c
	./data/romdata.c
)

LIST(APPEND TARGET_INCLUDE_DIRS
	${DEVKITPRO}/libgba/include
	${DEVKITARM}/arm-none-eabi/include
)

LIST(APPEND TARGET_LIB_DIRS
	${DEVKITPRO}/libgba/lib
)

LIST(APPEND TARGET_LIBS
	gba
)

link_directories(${TARGET_LIB_DIRS})
include_directories(${TARGET_INCLUDE_DIRS})
add_executable(${PROJECT_NAME}.elf ${SOURCE_FILES} ${INCLUDE_FILES} ${EXTRA_SRC_FILES} ${EXTRA_DATA_FILES}) # Create the elf file
add_gba_executable(${PROJECT_NAME}.elf) # Generate the .gba from the elf
target_link_libraries(${PROJECT_NAME}.elf ${TARGET_LIBS}) # Link the application and the demo framework

# List all the binary files
file(GLOB_RECURSE DATA_FILES
	./data/*.bin
)
