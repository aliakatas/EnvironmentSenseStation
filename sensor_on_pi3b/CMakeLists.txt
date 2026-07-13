cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

cmake_policy(SET CMP0077 NEW)

project(bme280_reader
    VERSION 0.1.0
    DESCRIPTION "BME280 sensor reader for Raspberry Pi 3B"
    LANGUAGES CXX C
)

enable_language(CXX)
enable_language(C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} 
    "${CMAKE_CURRENT_SOURCE_DIR}/bme280.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
)

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    gpiod
)

add_library(mongoose STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/mongoose/mongoose.c"
)

target_include_directories(mongoose PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mongoose")

add_executable(bme280_server
    "${CMAKE_CURRENT_SOURCE_DIR}/bme280.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sensor_utilities.cpp"    
    "${CMAKE_CURRENT_SOURCE_DIR}/main_http_server.cpp"
)

target_include_directories(bme280_server PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/mongoose"
    "${CMAKE_CURRENT_SOURCE_DIR}/json"
)

target_link_libraries(bme280_server PRIVATE mongoose gpiod)
