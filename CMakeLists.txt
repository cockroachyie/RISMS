cmake_minimum_required(VERSION 3.10)
project(RISMS CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

include_directories(include)

add_executable(risms
    src/main.cpp
    src/ConsoleUI.cpp
    src/ConfigStore.cpp
    src/FileStore.cpp
    src/UserRepository.cpp
    src/AuthService.cpp
    src/Validator.cpp
    src/ProductRepository.cpp
    src/ProductService.cpp
    src/MenuController.cpp
)
