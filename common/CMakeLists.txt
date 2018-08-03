CMAKE_MINIMUM_REQUIRED(VERSION 3.7)
PROJECT(guacamole)

SET(common_HEADERS
        include/common/io.h
        include/common/blank_cursor.h
        include/common/clipboard.h
        include/common/cursor.h
        include/common/display.h
        include/common/dot_cursor.h
        include/common/ibar_cursor.h
        include/common/iconv.h
        include/common/json.h
        include/common/list.h
        include/common/pointer_cursor.h
        include/common/recording.h
        include/common/rect.h
        include/common/string.h
        include/common/surface.h)

SET(common_SRCS
        src/io.c
        src/blank_cursor.c
        src/clipboard.c
        src/cursor.c
        src/display.c
        src/dot_cursor.c
        src/ibar_cursor.c
        src/iconv.c
        src/json.c
        src/list.c
        src/pointer_cursor.c
        src/recording.c
        src/rect.c
        src/string.c
        src/surface.c)

SET_SOURCE_FILES_PROPERTIES(${common_SRCS} ${common_HEADERS} PROPERTIES LANGUAGE CXX)

SET(common_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/common/include
        ${libguac_INCLUDE_DIRS})
SET(common_INCLUDE_DIRS ${common_INCLUDE_DIRS} PARENT_SCOPE)

INCLUDE_DIRECTORIES(${common_INCLUDE_DIRS})

ADD_LIBRARY(common ${common_HEADERS} ${common_SRCS})
TARGET_LINK_LIBRARIES(common ${libguac_LIBRARIES})
SET_TARGET_PROPERTIES(common PROPERTIES CXX_STANDARD 11)

SET(common_LIBRARIES common PARENT_SCOPE)
INSTALL(TARGETS common
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
INSTALL(FILES ${common_HEADERS}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/common/)