cmake_minimum_required(VERSION 3.16.3)
project(client CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(sourceCode
        server.cpp
        worker.cpp
)


find_package(spdlog CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS charconv system)
find_package(OpenSSL CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_executable(client.exe
        ${sourceCode}
        main.cpp)

target_link_libraries(client.exe PRIVATE
        spdlog::spdlog
        Boost::charconv
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
)