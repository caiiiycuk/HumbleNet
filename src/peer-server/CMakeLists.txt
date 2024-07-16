CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

CreateTool(peer-server
DIRS
	.
FEATURES
	cxx_auto_type cxx_range_for
INCLUDES
	${BoringSSL_INCLUDE_DIR}
LINK
	humblepeer
	sha1
	websockets
	${CMAKE_THREAD_LIBS_INIT}
	${LWS_BORING_LIBS}
	${LWS_BORING_INTERFACE}
PROPERTIES
	FOLDER HumbleNet/Server
)

source_group("Source" REGULAR_EXPRESSION ".")
source_group("" FILES CMakeLists.txt)