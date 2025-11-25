cmake_minimum_required(VERSION 3.14)
project(custom_git_shared)

# Define the shared library
add_library(custom_git_shared STATIC
    ast.cpp
    https_api.cpp
    openai_api.cpp
    utils.cpp
)

# Set C++ standard
target_compile_features(custom_git_shared PUBLIC cxx_std_20)

# Download CPM.cmake if not already available
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

# Add dependencies that the shared library needs
CPMAddPackage(
  NAME cpp-tree-sitter
  GIT_REPOSITORY https://github.com/nsumner/cpp-tree-sitter.git
  GIT_TAG v0.0.2
)

# Add the grammar parsers
add_grammar_from_repo(tree-sitter-python
  https://github.com/tree-sitter/tree-sitter-python.git
  0.20.4
)

add_grammar_from_repo(tree-sitter-cpp
  https://github.com/tree-sitter/tree-sitter-cpp.git
  0.23.4
)

add_grammar_from_repo(tree-sitter-java
  https://github.com/tree-sitter/tree-sitter-java.git
  0.23.0
)

add_grammar_from_repo(tree-sitter-javascript
  https://github.com/tree-sitter/tree-sitter-javascript.git
  0.23.0
)

add_grammar_from_repo(tree-sitter-go
  https://github.com/tree-sitter/tree-sitter-go.git
  0.23.0
)

# Add nlohmann/json
CPMAddPackage(
  NAME nlohmann_json
  VERSION 3.11.3
  GITHUB_REPOSITORY nlohmann/json
)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Include directories for the shared library
target_include_directories(custom_git_shared 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${cpp-tree-sitter_SOURCE_DIR}/include
)

# Link libraries that the shared library depends on
target_link_libraries(custom_git_shared
    PUBLIC
        cpp-tree-sitter
        tree-sitter-python
        tree-sitter-cpp
        tree-sitter-java
        tree-sitter-javascript
        tree-sitter-go
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Add tests subdirectory if BUILD_TESTING is enabled
option(BUILD_TESTING "Build the testing tree." OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
