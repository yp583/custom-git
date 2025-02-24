cmake_minimum_required(VERSION 3.14)
project(YourProjectName)

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
  GIT_TAG v0.0.1
)

# Add the Python grammar (fix the version specification)
add_grammar_from_repo(tree-sitter-python
  https://github.com/tree-sitter/tree-sitter-python.git
  0.20.4  # Remove the 'v' prefix entirely
)

# Link these to your target
add_executable(git_aicommit.o 
    ast.cpp
    main.cpp
    https_api.cpp
    openai_api.cpp
)
target_link_libraries(git_aicommit.o
    cpp-tree-sitter
    tree-sitter-python
    ssl
    crypto
) 