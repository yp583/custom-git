cmake_minimum_required(VERSION 3.14)
project(git_mcommit)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download CPM.cmake
set(CPM_DOWNLOAD_VERSION 0.38.1)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake...")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
    )
endif()
include(${CPM_DOWNLOAD_LOCATION})

# Add nlohmann/json - only dependency needed for JSON parsing
CPMAddPackage(
  NAME nlohmann_json
  VERSION 3.11.3
  GITHUB_REPOSITORY nlohmann/json
)

# Find OpenSSL - needed for HTTPS connections
find_package(OpenSSL REQUIRED)

# Create minimal shared library with only the files mcommit actually uses
add_library(mcommit_shared STATIC
    ../../shared/https_api.cpp
    ../../shared/openai_api.cpp
    ../../shared/utils.cpp
)

# Set up include directories for shared library
target_include_directories(mcommit_shared 
    PUBLIC 
        ../../shared
)

# Link only the necessary dependencies
target_link_libraries(mcommit_shared
    PUBLIC
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Create the main executable
add_executable(git_mcommit.o
    src/main.cpp
)

# Set up include directories for executable
target_include_directories(git_mcommit.o PRIVATE 
    src
    ../../shared
)

# Link the executable to the minimal shared library
target_link_libraries(git_mcommit.o
    mcommit_shared
)