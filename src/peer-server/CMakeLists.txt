CMAKE_MINIMUM_REQUIRED(VERSION 3.5)


CreateTool(peer-server
DIRS
	.
	"${CMAKE_CURRENT_LIST_DIR}/../../3rdparty/sqlite-3500400"
FEATURES
	cxx_auto_type cxx_range_for
INCLUDES
	${BoringSSL_INCLUDE_DIR}
LINK
	humblepeer
	sha1
	websockets
	${CAP_LIB}
	${CMAKE_THREAD_LIBS_INIT}
	${LWS_BORING_LIBS}
	${LWS_BORING_INTERFACE}
	${LWS_SSL_LIBS}
PROPERTIES
	FOLDER HumbleNet/Server
	CXX_STANDARD 14
)


source_group("Source" REGULAR_EXPRESSION ".")
source_group("" FILES CMakeLists.txt)