cmake_minimum_required(VERSION 3.14)
project(git_gcommit)

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

# Add cpp-tree-sitter
CPMAddPackage(
  NAME cpp-tree-sitter
  GIT_REPOSITORY https://github.com/nsumner/cpp-tree-sitter.git
  GIT_TAG v0.0.2
)

# Add the Python grammar (fix the version specification)
add_grammar_from_repo(tree-sitter-python
  https://github.com/tree-sitter/tree-sitter-python.git
  0.20.4  # Remove the 'v' prefix entirely
)

add_grammar_from_repo(tree-sitter-cpp
  https://github.com/tree-sitter/tree-sitter-cpp.git
  0.23.4  # Remove the 'v' prefix entirely
)

add_grammar_from_repo(tree-sitter-java
  https://github.com/tree-sitter/tree-sitter-java.git
  0.23.0
)

add_grammar_from_repo(tree-sitter-javascript
  https://github.com/tree-sitter/tree-sitter-javascript.git
  0.23.0
)

# TypeScript support temporarily disabled due to build issues
# add_grammar_from_repo(tree-sitter-typescript
#   https://github.com/tree-sitter/tree-sitter-typescript.git
#   0.20.4
# )

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

# Link these to your target
add_executable(git_gcommit.o
    src/ast.cpp
    src/main.cpp
    src/https_api.cpp
    src/openai_api.cpp
    src/hierarchal.cpp
    src/utils.cpp
)
target_include_directories(git_gcommit.o PRIVATE ${cpp-tree-sitter_SOURCE_DIR}/include)
target_link_libraries(git_gcommit.o
    cpp-tree-sitter
    tree-sitter-python
    tree-sitter-cpp
    tree-sitter-java
    tree-sitter-javascript
    # tree-sitter-typescript  # Temporarily disabled
    tree-sitter-go
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
) 