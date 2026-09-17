cmake_minimum_required(VERSION 3.16.3)
project(client CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include_directories("/home/download/vcpkg/installed/x64-linux/include")

set(sourceCode
    priorityHeap.cpp
)




find_package(spdlog CONFIG REQUIRED)




add_executable(client.exe
        ${sourceCode}
        main.cpp)


target_link_libraries(
        client.exe PRIVATE
        spdlog::spdlog
)