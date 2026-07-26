cmake_minimum_required(VERSION 3.15)
project(FLPRecoveryTool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find or specify JUCE path
set(JUCE_PATH "../JUCE" CACHE PATH "Path to JUCE library")

add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)

juce_add_gui_app(FLPRecoveryTool
    COMPANY_NAME "YourCompany"
    PRODUCT_NAME "FLP Recovery Tool"
    VERSION_MAJOR 1
    VERSION_MINOR 0
    VERSION_PATCH 0
)

target_sources(FLPRecoveryTool PRIVATE
    FLPRecovery.cpp
    FLPRecoveryTool.cpp
)

target_include_directories(FLPRecoveryTool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})