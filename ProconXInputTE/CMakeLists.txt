get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_executable(
	${PROJECT_NAME}
	main.cpp
	ProconX360Bridge.cpp
	Tests/TestHelper.cpp
	
	Tests/X360BridgeTest.cpp
)

target_include_directories(
	${PROJECT_NAME} PRIVATE
	../
)

target_link_libraries(
	${PROJECT_NAME}
	ProControllerHid
	ViGEmClient
)
