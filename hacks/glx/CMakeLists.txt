cmake_minimum_required(VERSION 3.10)
project(HexTrail C)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(
	${SDL2_INCLUDE_DIRS}
	../../hacks
	../../utils
)

set(SOURCES
	hextrail.c
	normals.c
)

add_executable(hextrail.exe ${SOURCES})

target_link_libraries(hextrail.exe
	${SDL2_LIBRARIES}
	opengl32
	glu32
)

target_compile_definitions(hextrail.exe PRIVATE
	USE_SDL
	USE_GL
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -pedantic -Wall")
