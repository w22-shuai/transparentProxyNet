cmake_minimum_required(VERSION 3.16.3)
project(client CXX)


set(sourceCode memoryPool.cpp)

add_executable(client.exe
        ${sourceCode}
        main.cpp)

