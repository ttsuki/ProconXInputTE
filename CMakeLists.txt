cmake_minimum_required (VERSION 3.12)
project(ProconXInputTE LANGUAGES CXX)
cmake_policy(SET CMP0091 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (	"${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" 
	OR	"${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel"
	OR	"${CMAKE_CXX_COMPILER_ID}" STREQUAL "IntelLLVM")
	add_compile_options(/W4)
	string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}") # Warning compile option
	string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG          ${CMAKE_CXX_FLAGS_DEBUG}          ) # Use static CRT
	string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_MINSIZEREL     ${CMAKE_CXX_FLAGS_MINSIZEREL}     ) # Use static CRT
	string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE        ${CMAKE_CXX_FLAGS_RELEASE}        ) # Use static CRT
	string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ) # Use static CRT
else()
	add_compile_options(-Wall -Wextra -Wpedantic) # Warning compile option
endif()

add_subdirectory(ProControllerHid)
add_subdirectory(ViGemClient)
add_subdirectory(ProconXInputTE)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ProconXInputTE)


