CMAKE_MINIMUM_REQUIRED(VERSION 3.7)
PROJECT(guacservice)

SET(guacservice_HEADERS
        include/guacservice/GuacService.h
        include/guacservice/GuacClientProcessHandler.h
        include/guacservice/GuacClientProcessHandlerMap.h
        include/guacservice/GuacConnection.h
        include/guacservice/GuacConnectionHandler.h
        include/guacservice/GuacConfig.h
        include/guacservice/IGuacClientProcessHandlerMapObserver.h
        include/guacservice/GuacLogger.h
        include/guacservice/IGuacConnectionNotifier.h
        include/guacservice/GuacDefines.h
        include/guacservice/IGuacConnectionSocket.h
        include/guacservice/GuacConnectionTCPSocket.h
        include/guacservice/GuacConnectionSSLSocket.h
        include/guacservice/GuacConfigParser.h)

SET(guacservice_SRCS
        src/GuacService.cpp
        src/GuacConfig.cpp
        src/GuacConnectionHandler.cpp
        src/GuacConnection.cpp
        src/GuacClientProcessHandlerMap.cpp
        src/GuacClientProcessHandler.cpp
        src/GuacServiceRunner.cpp
        src/GuacLogger.cpp
        src/GuacConnectionTCPSocket.cpp
        src/GuacConnectionSSLSocket.cpp
        src/GuacConfigParser.cpp)

SET(guacservice_client_HEADERS
        include/guacservice/GuacClientProcess.h
        include/guacservice/GuacClientUser.h
        include/guacservice/GuacLogger.h
        include/guacservice/GuacDefines.h)

SET(guacervice_client_SRCS
        src/GuacClientProcess.cpp
        src/GuacClientUser.cpp
        src/GuacClientProcessRunner.cpp
        src/GuacLogger.cpp)

SET(guacservice_DEPENDED_DLLS
        ${Cairo_DYNAMIC_LIBRARIES}
        ${PNG_DYNAMIC_LIBRARIES}
        ${JPEG_DYNAMIC_LIBRARIES}
        ${FreeRDP_DYNAMIC_LIBRARIES}
        ${OpenSSL_DYNAMIC_LIBRARIES})

SET_SOURCE_FILES_PROPERTIES(${guacservice_SRCS} ${guacservice_HEADERS} ${guacservice_client_HEADERS} ${guacervice_client_SRCS} PROPERTIES LANGUAGE CXX)

SET(guacservice_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/guacservice/include
        ${common_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS})
SET(guacservice_INCLUDE_DIRS ${guacservice_INCLUDE_DIRS} PARENT_SCOPE)

INCLUDE_DIRECTORIES(${guacservice_INCLUDE_DIRS})

ADD_EXECUTABLE(guacservice ${guacservice_HEADERS} ${guacservice_SRCS})
TARGET_LINK_LIBRARIES(guacservice ${common_LIBRARIES} ${Boost_LIBRARIES})
SET_TARGET_PROPERTIES(guacservice PROPERTIES CXX_STANDARD 11)

FOREACH (DYN_LIB ${guacservice_DEPENDED_DLLS})
    ADD_CUSTOM_COMMAND(TARGET guacservice POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DYN_LIB}
            $<TARGET_FILE_DIR:guacservice>)
ENDFOREACH ()

ADD_EXECUTABLE(guacservice_client ${guacservice_client_HEADERS} ${guacervice_client_SRCS})
TARGET_LINK_LIBRARIES(guacservice_client ${common_LIBRARIES})
SET_TARGET_PROPERTIES(guacservice_client PROPERTIES CXX_STANDARD 11)

FOREACH (DYN_LIB ${guacservice_DEPENDED_DLLS})
    ADD_CUSTOM_COMMAND(TARGET guacservice_client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DYN_LIB}
            $<TARGET_FILE_DIR:guacservice_client>)
ENDFOREACH ()

SET(guacservice_LIBRARIES guacservice guacservice_client PARENT_SCOPE)
INSTALL(TARGETS guacservice guacservice_client
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
INSTALL(FILES ${guacservice_HEADERS}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/guacservice/)
INSTALL(FILES ${guacservice_client_HEADERS}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/guacservice_client/)
INSTALL(FILES config/guac_config.json config/dh512.pem
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/guacservice/)
INSTALL(FILES ${guacservice_DEPENDED_DLLS}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)