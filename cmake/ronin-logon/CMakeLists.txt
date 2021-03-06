PROJECT(ronin-logonserver)

SET( prefix ${ROOT_PATH}/src/ronin-logonserver)

FILE(GLOB sources
"${prefix}/*.h"
"${prefix}/*.cpp")

FILE(GLOB resources
${ROOT_PATH}/src/ronin-shared/resources.rc
${ROOT_PATH}/src/ronin-shared/Icon.ico)
source_group("Resources" FILES ${resources})

SET( SRCS ${SRCS} ${sources} ${resources} )

include_directories( ${GLOBAL_INCLUDE_DIRS} )
link_directories( ${EXTRA_LIBS_PATH} ${DEPENDENCY_LIBS} )
include(PCHSupport)

ADD_EXECUTABLE( ${PROJECT_NAME} ${SRCS} ${HEADERS} )

IF( CMAKE_GENERATOR MATCHES Visual*)
	ADD_NATIVE_PRECOMPILED_HEADER(${PROJECT_NAME} LogonStdAfx.h ${prefix}/LogonStdAfx.cpp)
ENDIF()

#Link our ronin-shared lib.
add_dependencies( ${PROJECT_NAME} database ronin-shared zlib )
target_link_libraries( ${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} ${MYSQL_LIBRARY} ${OPENSSL_LIBRARIES} database ronin-shared zlib ${EXTRA_LIBS} )

#Set our resources
set_source_files_properties(Icon.ico resources.rc PROPERTIES LANGUAGE RC)
#Set our compile information
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_DEFINITIONS _LOGON)
