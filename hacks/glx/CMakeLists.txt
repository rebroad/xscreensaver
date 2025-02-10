cmake_minimum_required(VERSION 3.10)
project(HexTrail)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(hextrail 
    hextrail.c 
    normals.c 
    # other source files
)

target_link_libraries(hextrail 
    SDL2::SDL2 
    OpenGL::GL
)
