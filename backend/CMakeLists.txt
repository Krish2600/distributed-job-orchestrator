cmake_minimum_required(VERSION 3.22)
project(DistributedJobPlatform LANGUAGES CXX)

# Require C++17 for Crow and modern libpqxx features
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Threading library (required for worker pool and Crow web server)
find_package(Threads REQUIRED)

# Find dependencies using PkgConfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)
pkg_check_modules(HIREDIS REQUIRED hiredis)

# Fetch Crow HTTP Framework from GitHub
include(FetchContent)
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.2.0
)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(crow)

# Include paths
include_directories(
    include
    ${PQXX_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
)

# Source files
file(GLOB SOURCES src/*.cpp)

# Define backend executable target
add_executable(job_platform_backend ${SOURCES})

# Link libraries
target_link_libraries(job_platform_backend PRIVATE
    Crow::Crow
    ${PQXX_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    Threads::Threads
)
