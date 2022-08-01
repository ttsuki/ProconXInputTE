get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

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
