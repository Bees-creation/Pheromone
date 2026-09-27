cmake_minimum_required(VERSION 3.10)
project(Pheromone)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(OS_NAME "Windows")
    add_compile_definitions(PLATFORM_WINDOWS)
else()
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} platform is not support")
endif()
if(CMAKE_VS_PLATFORM_NAME)
    set(ARCH_NAME ${CMAKE_VS_PLATFORM_NAME})
else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH_NAME "x64")
    else()
        set(ARCH_NAME "x86")
    endif()
endif()
set(PLATFORM_TAG "${OS_NAME}-${ARCH_NAME}")
message(STATUS "Building for platform: ${PLATFORM_TAG}")

add_compile_definitions("${CMAKE_BUILD_TYPE}")
message(STATUS "Building config: ${CMAKE_BUILD_TYPE}")

set(BASE_DIR "${CMAKE_SOURCE_DIR}")
set(CONFIG_PLATFORM_DIR "$<CONFIG>-${PLATFORM_TAG}")
set(TARGET_NAME_DIR "$<TARGET_PROPERTY:PROJECT_NAME>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BASE_DIR}/bin/${CONFIG_PLATFORM_DIR}/${TARGET_NAME_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BASE_DIR}/bin/${CONFIG_PLATFORM_DIR}/${TARGET_NAME_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${BASE_DIR}/bin/${CONFIG_PLATFORM_DIR}/${TARGET_NAME_DIR}")

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(FLTK_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_FLUID OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_GL OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_FORMS OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(${PROJECT_NAME}/third_party/fltk EXCLUDE_FROM_ALL)

add_sources(CORE_SRC "${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/src")
add_executable(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/main.cpp" ${CORE_SRC})
target_include_directories(${PROJECT_NAME}
    PUBLIC
        "${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/inc"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        fltk
        setupapi # windows 设备管理库
)
target_link_options(${PROJECT_NAME}
    PRIVATE
        -mwindows
)
