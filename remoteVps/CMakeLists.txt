cmake_minimum_required(VERSION 3.16.3)
project(remoteServer CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(sourceCode
    ../memoryPool/memoryPool.cpp
    AES.cpp
    session.cpp
    server.cpp
    worker.cpp
)





find_package(absl  CONFIG REQUIRED )
find_package(spdlog CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS charconv system)
find_package(OpenSSL CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(kcp CONFIG REQUIRED)



add_executable(remoteServer
        ${sourceCode}
        main.cpp)


target_link_libraries(remoteServer PRIVATE
        kcp::kcp
        absl::flat_hash_map
        spdlog::spdlog
        Boost::charconv
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
)