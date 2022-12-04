cmake_minimum_required(VERSION 3.8)

option(LOG_LEVEL "Log level" 4)
option(HOST "Host address" localhost)

project(DIRECTORY_MONITOR)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

FILE(GLOB TARGET_H "${CMAKE_SOURCE_DIR}/*.h")
FILE(GLOB TARGET_C "${CMAKE_SOURCE_DIR}/*.c")

SET(TARGET_SRC ${TARGET_C} ${TARGET_H})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb")

if (LOG_LEVEL)
        add_definitions(-DCONFIG_LOG_LEVEL=${LOG_LEVEL})
endif()

if (HOST)
        add_definitions(-DCONFIG_HAVE_HOST_ADDRESS=${HOST})
endif()

# 'pthread' library
find_package(Threads REQUIRED)

# 'mosquitto' library
find_path(MOSQUITTO_INCLUDE_DIR mosquitto.h)
find_library(MOSQUITTO_LIBRARIES NAMES mosquitto)

# json-c' library
find_path(JSONC_INCLUDE_DIR json-c/json.h)
find_library(JSONC_LIBRARIES NAMES json-c)

add_executable(dir_mon ${TARGET_SRC})
target_link_libraries(dir_mon ${MOSQUITTO_LIBRARIES} ${JSONC_LIBRARIES} Threads::Threads)

# Run application
add_custom_target(run
        COMMAND ../bin/dir_mon ../data/users ../data/tenants ../data/assets ../data/procedures ../data/results
        WORKING_DIRECTORY ${CMAKE_PROJECT_DIR})

# Run directory change simulator
add_custom_target(run-sim
        COMMAND ../simulate_dir_changes.bash ../data/users ../data/tenants ../data/assets ../data/procedures ../data/results
        WORKING_DIRECTORY ${CMAKE_PROJECT_DIR})

# Run directory change simulator
add_custom_target(check
        COMMAND valgrind --leak-check=full --show-leak-kinds=all ../bin/dir_mon ../data/users ../data/tenants ../data/assets ../data/procedures ../data/results
        WORKING_DIRECTORY ${CMAKE_PROJECT_DIR})
