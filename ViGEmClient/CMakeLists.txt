get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_library(
	${PROJECT_NAME} STATIC
	../ThirdParty/ViGEmClient/include/ViGEm/Client.h
	../ThirdParty/ViGEmClient/include/ViGEm/Common.h
	../ThirdParty/ViGEmClient/include/ViGEm/Util.h
	../ThirdParty/ViGEmClient/include/ViGEm/km/BusShared.h
	../ThirdParty/ViGEmClient/src/Internal.h
	../ThirdParty/ViGEmClient/src/ViGEmClient.cpp
	ViGEmClientCpp.h
	ViGEmClientCpp.cpp
)

target_include_directories(
	${PROJECT_NAME} PRIVATE 
	../ThirdParty/ViGemClient/include
)
