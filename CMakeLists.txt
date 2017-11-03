cmake_minimum_required (VERSION 3.7)
project (Vulkan-Test)

find_package (glfw3 REQUIRED)
find_package (Vulkan REQUIRED)

file (GLOB SOURCES *.c)
add_executable (vulkan-test ${SOURCES})

target_link_libraries (vulkan-test glfw vulkan m)
