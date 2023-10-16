project(fota)

cmake_minimum_required (VERSION 2.8)
set(CMAKE_C_FLAGS "-g3 -pthread")
set(CMAKE_CXX_FLAGS "-g3 -pthread")
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	prebuilts/Doip/src/include/
	prebuilts/Doip/public/
)

add_definitions(
#	-D__DOIP_UDS_CONTROL_C__
)

link_directories(
)

#add_library (doip SHARED
#add_executable(doip_exe 
#	src/doip/DoipControlCommon.c
#	src/doip/DoIPUdsControl.c
#	src/doip/DoipControlMain.c
#)


add_executable(doip_client_server
	src/doip_client_server.c
)

add_executable(doip_cli
	src/doip_client.c
)

# add_executable(read_pkg
# 	src/doip_server.c
# )
# target_link_libraries(read_pkg
# 	doip
# )
