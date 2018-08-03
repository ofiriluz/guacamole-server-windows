CMAKE_MINIMUM_REQUIRED(VERSION 3.7)
PROJECT(libguac)

SET(libguac_HEADERS
        include/guacamole/audio.h
        include/guacamole/audio-fntypes.h
        include/guacamole/audio-types.h
        include/guacamole/client-constants.h
        include/guacamole/client.h
        include/guacamole/client-fntypes.h
        include/guacamole/client-types.h
        include/guacamole/error.h
        include/guacamole/error-types.h
        include/guacamole/hash.h
        include/guacamole/layer.h
        include/guacamole/layer-types.h
        include/guacamole/object.h
        include/guacamole/object-types.h
        include/guacamole/parser-constants.h
        include/guacamole/parser.h
        include/guacamole/parser-types.h
        include/guacamole/plugin-constants.h
        include/guacamole/plugin.h
        include/guacamole/pool.h
        include/guacamole/pool-types.h
        include/guacamole/protocol.h
        include/guacamole/protocol-types.h
        include/guacamole/socket-constants.h
        include/guacamole/socket.h
        include/guacamole/socket-fntypes.h
        include/guacamole/socket-types.h
        include/guacamole/stream.h
        include/guacamole/stream-types.h
        include/guacamole/timestamp.h
        include/guacamole/timestamp-types.h
        include/guacamole/unicode.h
        include/guacamole/user.h
        include/guacamole/user-constants.h
        include/guacamole/user-fntypes.h
        include/guacamole/user-types.h
        include/guacamole/config.h
		include/guacamole/atomicops.h
		include/guacamole/readerwriterqueue.h
		include/guacamole/rwlock.h
		include/guacamole/rwlockimpl.h)

SET(libguac_NOINSTALL_HEADERS
        include/guacamole/encode-jpeg.h
        include/guacamole/encode-webp.h
        include/guacamole/raw_encoder.h
        include/guacamole/encode-png.h
        include/guacamole/id.h
        include/guacamole/palette.h
        include/guacamole/user-handlers.h)

SET(libguac_SRCS
        src/audio.c
        src/client.c
        src/encode-jpeg.c
        src/encode-png.c
        src/encode-webp.c
        src/error.c
        src/hash.c
        src/id.c
        src/palette.c
        src/parser.c
        src/pool.c
        src/protocol.c
        src/raw_encoder.c
        src/socket.c
        src/socket-fd.c
        src/socket-broadcast.c
        src/socket-nest.c
        src/socket-tee.c
        src/socket-boost-file.c
        src/socket-boost-tcp.c
        src/socket-iostream.c
        src/socket-named-pipe.c
        src/socket-shared-memory.c
        src/timestamp.c
        src/unicode.c
        src/user.c
        src/user-handlers.c)

SET_SOURCE_FILES_PROPERTIES(${libguac_SRCS}
        ${libguac_HEADERS}
        ${libguac_NOINSTALL_HEADERS} PROPERTIES LANGUAGE CXX)

SET(libguac_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/libguac/include
        ${Boost_INCLUDE_DIRS}
        ${OpenSSL_INCLUDE_DIRS}
        ${Cairo_INCLUDE_DIRS}
        ${PNG_INCLUDE_DIRS}
        ${JPEG_INCLUDE_DIRS}
        ${WebP_INCLUDE_DIRS})
SET(libguac_INCLUDE_DIRS ${libguac_INCLUDE_DIRS} PARENT_SCOPE)

INCLUDE_DIRECTORIES(${libguac_INCLUDE_DIRS})

ADD_LIBRARY(libguac ${libguac_HEADERS} ${libguac_NOINSTALL_HEADERS} ${libguac_SRCS})
TARGET_LINK_LIBRARIES(libguac ${Boost_LIBRARIES}
        ${OpenSSL_LIBRARIES}
        ${Cairo_LIBRARIES}
        ${PNG_LIBRARIES}
        ${JPEG_LIBRARIES}
        ${WebP_LIBRARIES})
SET_TARGET_PROPERTIES(libguac PROPERTIES CXX_STANDARD 11)

SET(libguac_LIBRARIES libguac PARENT_SCOPE)

INSTALL(TARGETS libguac
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
INSTALL(FILES ${libguac_HEADERS}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/guacamole/)