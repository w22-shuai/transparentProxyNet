cmake_minimum_required(VERSION 3.16.3)
project(timeWheel CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(sourceCode
    timeWheel.hpp
)






find_package(spdlog CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS charconv system)




add_executable(timeWheel
        ${sourceCode}
        main.cpp)


target_link_libraries(timeWheel PRIVATE
        spdlog::spdlog
        Boost::charconv
        Boost::system
)