cmake_minimum_required(VERSION 3.14)

# Enable testing
enable_testing()

# Download and configure Google Test using CPM
CPMAddPackage(
  NAME googletest
  GITHUB_REPOSITORY google/googletest
  GIT_TAG v1.15.2
  VERSION 1.15.2
  OPTIONS
    "INSTALL_GTEST OFF"
    "gtest_force_shared_crt ON"
)

# Create test executable for async HTTPS API
add_executable(async_https_api_test
    async_https_api_test.cpp
    ../async_https_api.cpp
)

# Set C++ standard
target_compile_features(async_https_api_test PRIVATE cxx_std_20)

# Include directories
target_include_directories(async_https_api_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${OPENSSL_INCLUDE_DIR}
)

# Link with Google Test and dependencies
target_link_libraries(async_https_api_test
    PRIVATE
        gtest
        gtest_main
        gmock
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Add as a test to CTest
add_test(NAME AsyncHTTPSAPITest COMMAND async_https_api_test)

# Set test properties for better output
set_tests_properties(AsyncHTTPSAPITest PROPERTIES
    TIMEOUT 60
    LABELS "integration"
)

# Print status
message(STATUS "Test build configured for async_https_api")

# Create test executable for async OpenAI API
add_executable(async_openai_api_test
    async_openai_api_test.cpp
    ../async_https_api.cpp
    ../async_openai_api.cpp
)

# Set C++ standard
target_compile_features(async_openai_api_test PRIVATE cxx_std_20)

# Include directories
target_include_directories(async_openai_api_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${OPENSSL_INCLUDE_DIR}
)

# Link with Google Test and dependencies
target_link_libraries(async_openai_api_test
    PRIVATE
        gtest
        gtest_main
        gmock
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Add as a test to CTest
add_test(NAME AsyncOpenAIAPITest COMMAND async_openai_api_test)

# Set test properties for better output
set_tests_properties(AsyncOpenAIAPITest PROPERTIES
    TIMEOUT 120
    LABELS "integration"
)

# Print status
message(STATUS "Test build configured for async_openai_api")

# Create test executable for diffreader
add_executable(diffreader_test
    diffreader_test.cpp
    ../diffreader.cpp
)

target_compile_features(diffreader_test PRIVATE cxx_std_20)

target_include_directories(diffreader_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(diffreader_test
    PRIVATE
        gtest
        gtest_main
)

add_test(NAME DiffReaderTest COMMAND diffreader_test)

set_tests_properties(DiffReaderTest PROPERTIES
    TIMEOUT 30
    LABELS "unit"
)

message(STATUS "Test build configured for diffreader")

# Create test executable for hierarchal clustering
add_executable(hierarchal_test
    hierarchal_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commands/gcommit/src/hierarchal.cpp
    ../utils.cpp
    ../openai_api.cpp
    ../https_api.cpp
)

target_compile_features(hierarchal_test PRIVATE cxx_std_20)

target_include_directories(hierarchal_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commands/gcommit/src
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(hierarchal_test
    PRIVATE
        gtest
        gtest_main
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)

add_test(NAME HierarchalClusteringTest COMMAND hierarchal_test)

set_tests_properties(HierarchalClusteringTest PROPERTIES
    TIMEOUT 30
    LABELS "unit"
)

message(STATUS "Test build configured for hierarchal clustering")
