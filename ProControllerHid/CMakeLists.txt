cmake_minimum_required (VERSION 3.12)
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJECT_NAME} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(
	${PROJECT_NAME} STATIC
	hidio.h
	hidio.cpp
	ProController.h
	ProController.cpp
)

add_executable(
	${PROJECT_NAME}.Test
	ProController.Test.cpp
)

target_link_libraries(
	${PROJECT_NAME}.Test
	${PROJECT_NAME}
)
