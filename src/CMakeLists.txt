cmake_minimum_required (VERSION 3.5)

project (nesemu)

# set(CMAKE_CXX_STANDARD 11)
# set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find the OpenCV package
find_package(OpenCV REQUIRED)

add_executable(nesemu utils.cpp lstdebugger.cpp ppu.cpp cpu.cpp cpumem.cpp main.cpp)

target_link_libraries(nesemu ${OpenCV_LIBS})

# Include the OpenCV headers
target_include_directories(nesemu PRIVATE ${OpenCV_INCLUDE_DIRS})
