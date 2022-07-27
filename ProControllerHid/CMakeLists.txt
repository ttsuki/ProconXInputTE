get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_library(
	${PROJECT_NAME} STATIC
	hidio.h
	hidio.cpp
	ProController.h
	ProController.cpp
)
