CMAKE_MINIMUM_REQUIRED(VERSION 3.7)
PROJECT(rdp)

SET(rdp_SRCS
        src/compat/winpr-stream.c
        src/audio_input.c
        src/client.c
        src/dvc.c
        src/input.c
        src/keyboard.c
        src/ptr_string.c
        src/rdp.c
        src/rdp_bitmap.c
        src/rdp_cliprdr.c
        src/rdp_color.c
        src/rdp_disp.c
        src/rdp_fs.c
        src/rdp_gdi.c
        src/rdp_glyph.c
        src/rdp_keymap.c
        src/rdp_print_job.c
        src/rdp_pointer.c
        src/rdp_rail.c
        src/rdp_settings.c
        src/rdp_stream.c
        src/rdp_svc.c
        src/resolution.c
        src/unicode.c
        src/user.c)

SET(rdp_guac_ai_SRCS
        src/guac_ai/ai_messages.c
        src/guac_ai/ai_service.c
        src/audio_input.c
        src/ptr_string.c
        src/dvc.c)

SET(rdp_guac_rdpdr_SRCS
        src/guac_rdpdr/rdpdr_fs_messages.c
        src/guac_rdpdr/rdpdr_fs_messages_dir_info.c
        src/guac_rdpdr/rdpdr_fs_messages_file_info.c
        src/guac_rdpdr/rdpdr_fs_messages_vol_info.c
        src/guac_rdpdr/rdpdr_fs_service.c
        src/guac_rdpdr/rdpdr_messages.c
        src/guac_rdpdr/rdpdr_printer.c
        src/guac_rdpdr/rdpdr_service.c
        src/rdp_fs.c
        src/rdp_print_job.c
        src/rdp_stream.c
        src/unicode.c)

SET(rdp_guac_rdpsnd_SRCS
        src/guac_rdpsnd/rdpsnd_messages.c
        src/guac_rdpsnd/rdpsnd_service.c)

SET(rdp_guac_svc_SRCS
        src/guac_svc/svc_service.c
        src/rdp_svc.c)

SET(rdp_NOINSTALL_HEADERS
        include/rdp/compat/client-cliprdr.h
        include/rdp/compat/rail.h
        include/rdp/compat/winpr-stream.h
        include/rdp/compat/winpr-wtypes.h
        include/rdp/audio_input.h
        include/rdp/client.h
        include/rdp/dvc.h
        include/rdp/input.h
        include/rdp/keyboard.h
        include/rdp/ptr_string.h
        include/rdp/rdp.h
        include/rdp/rdp_bitmap.h
        include/rdp/rdp_cliprdr.h
        include/rdp/rdp_color.h
        include/rdp/rdp_disp.h
        include/rdp/rdp_fs.h
        include/rdp/rdp_gdi.h
        include/rdp/rdp_glyph.h
        include/rdp/rdp_keymap.h
        include/rdp/rdp_pointer.h
        include/rdp/rdp_print_job.h
        include/rdp/rdp_rail.h
        include/rdp/rdp_settings.h
        include/rdp/rdp_status.h
        include/rdp/rdp_stream.h
        include/rdp/rdp_svc.h
        include/rdp/resolution.h
        include/rdp/unicode.h
        include/rdp/user.h)

SET(rdp_guac_ai_NOINSTALL_HEADERS
        include/rdp/guac_ai/ai_messages.h
        include/rdp/guac_ai/ai_service.h
        include/rdp/audio_input.h
        include/rdp/ptr_string.h
        include/rdp/dvc.h)

SET(rdp_guac_rdpdr_NOINSTALL_HEADERS
        include/rdp/guac_rdpdr/rdpdr_fs_messages.h
        include/rdp/guac_rdpdr/rdpdr_fs_messages_dir_info.h
        include/rdp/guac_rdpdr/rdpdr_fs_messages_file_info.h
        include/rdp/guac_rdpdr/rdpdr_fs_messages_vol_info.h
        include/rdp/guac_rdpdr/rdpdr_messages.h
        include/rdp/guac_rdpdr/rdpdr_fs_service.h
        include/rdp/guac_rdpdr/rdpdr_printer.h
        include/rdp/guac_rdpdr/rdpdr_service.h
        include/rdp/rdp_fs.h
        include/rdp/rdp_print_job.h
        include/rdp/rdp_stream.h
        include/rdp/unicode.h)

SET(rdp_guac_rdpsnd_NOINSTALL_HEADERS
        include/rdp/guac_rdpsnd/rdpsnd_messages.h
        include/rdp/guac_rdpsnd/rdpsnd_service.h)

SET(rdp_guac_svc_NOINSTALL_HEADERS
        include/rdp/guac_svc/svc_service.h)

# Keymaps
SET(rdp_KEYMAPS
        ${CMAKE_CURRENT_SOURCE_DIR}/keymaps/base.keymap
        ${CMAKE_CURRENT_SOURCE_DIR}/keymaps/failsafe.keymap
        ${CMAKE_CURRENT_SOURCE_DIR}/keymaps/de_de_qwertz.keymap
        ${CMAKE_CURRENT_SOURCE_DIR}/keymaps/en_us_qwerty.keymap
        ${CMAKE_CURRENT_SOURCE_DIR}/keymaps/fr_fr_azerty.keymap
        ${CMAKE_CURRENT_SOURCE_DIR}/keymaps/fr_ch_qwertz.keymap
        ${CMAKE_CURRENT_SOURCE_DIR}/keymaps/it_it_qwerty.keymap
        ${CMAKE_CURRENT_SOURCE_DIR}/keymaps/ja_jp_qwerty.keymap
        ${CMAKE_CURRENT_SOURCE_DIR}/keymaps/sv_se_qwerty.keymap)

ADD_CUSTOM_COMMAND(OUTPUT _generated_keymaps.c
        COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/keymaps/generate.pl ${rdp_KEYMAPS})

SET_SOURCE_FILES_PROPERTIES(${rdp_SRCS}
        ${rdp_guac_ai_SRCS}
        ${rdp_guac_rdpdr_SRCS}
        ${rdp_guac_rdpsnd_SRCS}
        ${rdp_guac_svc_SRCS}
        ${rdp_NOINSTALL_HEADERS}
        ${rdp_guac_ai_NOINSTALL_HEADERS}
        ${rdp_guac_rdpdr_NOINSTALL_HEADERS}
        ${rdp_guac_rdpsnd_NOINSTALL_HEADERS}
        ${rdp_guac_svc_NOINSTALL_HEADERS}
        _generated_keymaps.c PROPERTIES LANGUAGE CXX)

SET(rdp_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/protocols/rdp/include
        ${common_INCLUDE_DIRS}
        ${libguac_INCLUDE_DIRS}
        ${FreeRDP_INCLUDE_DIRS}
        ${WinPR_INCLUDE_DIRS})
SET(rdp_INCLUDE_DIRS ${rdp_INCLUDE_DIRS} PARENT_SCOPE)

INCLUDE_DIRECTORIES(${rdp_INCLUDE_DIRS})

# RDP
ADD_LIBRARY(rdp SHARED ${rdp_NOINSTALL_HEADERS} _generated_keymaps.c ${rdp_SRCS})
TARGET_LINK_LIBRARIES(rdp ${common_LIBRARIES} ${libguac_LIBRARIES} ${FreeRDP_LIBRARIES})
SET_TARGET_PROPERTIES(rdp PROPERTIES CXX_STANDARD 11)

# GuacRDPSVC
ADD_LIBRARY(guacsvc ${rdp_guac_svc_NOINSTALL_HEADERS} ${rdp_guac_svc_SRCS})
TARGET_LINK_LIBRARIES(guacsvc ${common_LIBRARIES} ${libguac_LIBRARIES} ${FreeRDP_LIBRARIES})
SET_TARGET_PROPERTIES(guacsvc PROPERTIES CXX_STANDARD 11)

# GuacAI
ADD_LIBRARY(guacai-client SHARED ${rdp_guac_ai_NOINSTALL_HEADERS} ${rdp_guac_ai_SRCS})
TARGET_LINK_LIBRARIES(guacai-client ${common_LIBRARIES} ${libguac_LIBRARIES} ${FreeRDP_LIBRARIES} guacsvc)
SET_TARGET_PROPERTIES(guacai-client PROPERTIES CXX_STANDARD 11)

# GuacRDPDR
ADD_LIBRARY(guacdr-client SHARED ${rdp_guac_rdpdr_NOINSTALL_HEADERS} ${rdp_guac_rdpdr_SRCS})
TARGET_LINK_LIBRARIES(guacdr-client ${common_LIBRARIES} ${libguac_LIBRARIES} ${FreeRDP_LIBRARIES} guacsvc)
SET_TARGET_PROPERTIES(guacdr-client PROPERTIES CXX_STANDARD 11)

# GuacRDPSND
ADD_LIBRARY(guacsnd-client SHARED ${rdp_guac_rdpsnd_NOINSTALL_HEADERS} ${rdp_guac_rdpsnd_SRCS})
TARGET_LINK_LIBRARIES(guacsnd-client ${common_LIBRARIES} ${libguac_LIBRARIES} ${FreeRDP_LIBRARIES} guacsvc)
SET_TARGET_PROPERTIES(guacsnd-client PROPERTIES CXX_STANDARD 11)

SET(rdp_LIBRARIES rdp PARENT_SCOPE)

INSTALL(TARGETS rdp guacsnd-client guacdr-client guacai-client
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
INSTALL(TARGETS guacsvc
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)