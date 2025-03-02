cmake_minimum_required(VERSION 3.16.3)
project(my_webServer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

find_package(Boost REQUIRED COMPONENTS regex)
include_directories(${Boost_INCLUDE_DIRS})
if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

include_directories(/usr/include/mysql)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/buffer BUFFER_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/http HTTP_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/log LOG_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/pool POLL_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/server SERVER_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/timer TIMER_SRC)

add_executable(${PROJECT_NAME}
                main.cpp
                ${BUFFER_SRC}
                ${HTTP_SRC}
                ${LOG_SRC}
                ${POLL_SRC}
                ${SERVER_SRC}
                ${TIMER_SRC})

target_link_libraries(${PROJECT_NAME} mysqlclient pthread Boost::regex)