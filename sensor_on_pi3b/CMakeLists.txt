cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

cmake_policy(SET CMP0077 NEW)

project(bme280_reader
    VERSION 0.1.0
    DESCRIPTION "BME280 sensor reader for Raspberry Pi 3B"
    LANGUAGES CXX
)

enable_language(CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/bme280.c"
)

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    gpiod
)
