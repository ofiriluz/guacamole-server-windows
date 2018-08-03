/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

// Added by ofir
#define HAVE_PROTOTYPES 
#define HAVE_BOOST

/* Type compatibility */
#define CHANNEL_ENTRY_POINTS_FREERDP CHANNEL_ENTRY_POINTS_EX

/* Whether support for the common SSH core is enabled */
/* #undef ENABLE_COMMON_SSH */

/* Whether support for Ogg Vorbis is enabled */
/* #undef ENABLE_OGG */

/* Whether PulseAudio support is enabled */      
/* #undef ENABLE_PULSE */

/* Whether agent forwarding support for SSH is enabled */
/* #undef ENABLE_SSH_AGENT */

/* Whether SSL-related support is enabled */
// #define ENABLE_SSL /**/

/* Whether support for listen-mode VNC connections is enabled. */
/* #undef ENABLE_VNC_LISTEN */

/* Whether support for VNC repeaters is enabled. */
/* #undef ENABLE_VNC_REPEATER */

/* Whether WebP support is enabled */
#define ENABLE_WEBP

/* Whether library support for WinPR types was found */
#define ENABLE_WINPR 

/* Whether this version of FreeRDP requires _aligned_malloc() for bitmap data
   */
/* #undef FREERDP_BITMAP_REQUIRES_ALIGNED_MALLOC */

/* The full path to the guacd config file */
// #define GUACD_CONF_FILE "/etc/guacamole/guacd.conf"

/* Whether the ADDIN_ARGV type is available */
/* #undef HAVE_ADDIN_ARGV */

/* Define to 1 if you have the <cairo/cairo.h> header file. */
#define HAVE_CAIRO_CAIRO_H 1

/* Whether cairo_format_stride_for_width() is defined */
//#define HAVE_CAIRO_FORMAT_STRIDE_FOR_WIDTH 1

/* Define to 1 if you have the `clock_gettime' function. */
/* #undef HAVE_CLOCK_GETTIME */

/* Define to 1 if you have the <dlfcn.h> header file. */
// #define HAVE_DLFCN_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
// #define HAVE_FCNTL_H 1

/* Define to 1 if you have the <freerdp/addin.h> header file. */
#define HAVE_FREERDP_ADDIN_H 1

/* Whether freerdp_channels_global_init() is defined */
#define HAVE_FREERDP_CHANNELS_GLOBAL_INIT 1

/* Define to 1 if you have the <freerdp/client/channels.h> header file. */
#define HAVE_FREERDP_CLIENT_CHANNELS_H 1

/* Define to 1 if you have the <freerdp/client/cliprdr.h> header file. */
#define HAVE_FREERDP_CLIENT_CLIPRDR_H 1

/* Define to 1 if you have the <freerdp/client/disp.h> header file. */
#define HAVE_FREERDP_CLIENT_DISP_H 1

/* Whether freerdp_color_convert_drawing_order_color_to_gdi_color() is defined
   */
/* #undef HAVE_FREERDP_COLOR_CONVERT_DRAWING_ORDER_COLOR_TO_GDI_COLOR */

/* Define to 1 if `ContextSize' is a member of `freerdp'. */
#define HAVE_FREERDP_CONTEXTSIZE 1

/* Define to 1 if `context_size' is a member of `freerdp'. */
// #define HAVE_FREERDP_CONTEXT_SIZE 1

/* Whether freerdp_convert_gdi_order_color() is defined */
/* #undef HAVE_FREERDP_CONVERT_GDI_ORDER_COLOR */

/* Whether FreeRDP supports the display update channel */
#define HAVE_FREERDP_DISPLAY_UPDATE_SUPPORT 1

/* Whether this version of FreeRDP provides the PubSub event system */
#define HAVE_FREERDP_EVENT_PUBSUB 1

/* Define to 1 if you have the <freerdp/kbd/layouts.h> header file. */
#define HAVE_FREERDP_KBD_LAYOUTS_H 1

/* Define to 1 if you have the <freerdp/locale/keyboard.h> header file. */
#define HAVE_FREERDP_LOCALE_KEYBOARD_H 1

/* Define to 1 if you have the <freerdp/plugins/cliprdr.h> header file. */
#define HAVE_FREERDP_PLUGINS_CLIPRDR_H 1

/* Whether freerdp_register_addin_provider() is defined */
#define HAVE_FREERDP_REGISTER_ADDIN_PROVIDER 1

/* Define to 1 if you have the <freerdp/version.h> header file. */
#define HAVE_FREERDP_VERSION_H 1

/* Define to 1 if you have the `gettimeofday' function. */
//#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Whether libpthread was found */
// #define HAVE_LIBPTHREAD /**/

/* Define to 1 if you have the `wsock32' library (-lwsock32). */
#define HAVE_LIBWSOCK32 1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the `nanosleep' function. */
//#define HAVE_NANOSLEEP 1

/* Define to 1 if you have the <ossp/uuid.h> header file. */
/* #undef HAVE_OSSP_UUID_H */

/* Define to 1 if you have the <pngstruct.h> header file. */
/* #undef HAVE_PNGSTRUCT_H */

/* Whether png_get_io_ptr() is defined */
#define HAVE_PNG_GET_IO_PTR 1

/* Define to 1 if `codecs' is a member of `rdpContext'. */
/* #undef HAVE_RDPCONTEXT_CODECS */

/* Define to 1 if `SetDefault' is a member of `rdpPointer'. */
#define HAVE_RDPPOINTER_SETDEFAULT 1

/* Define to 1 if `SetNull' is a member of `rdpPointer'. */
#define HAVE_RDPPOINTER_SETNULL 1

/* Whether the rdpSettings structure has AudioCapture settings */
#define HAVE_RDPSETTINGS_AUDIOCAPTURE 1

/* Whether the rdpSettings structure has AudioPlayback settings */
#define HAVE_RDPSETTINGS_AUDIOPLAYBACK 1

/* Define to 1 if `audio_capture' is a member of `rdpSettings'. */
// #define HAVE_RDPSETTINGS_AUDIO_CAPTURE

/* Define to 1 if `audio_playback' is a member of `rdpSettings'. */
// #define HAVE_RDPSETTINGS_AUDIO_PLAYBACK 1

/* Whether the rdpSettings structure has DeviceRedirection settings */
#define HAVE_RDPSETTINGS_DEVICEREDIRECTION 1

/* Define to 1 if `device_redirection' is a member of `rdpSettings'. */
/* #undef HAVE_RDPSETTINGS_DEVICE_REDIRECTION */

/* Define to 1 if `FastPathInput' is a member of `rdpSettings'. */
#define HAVE_RDPSETTINGS_FASTPATHINPUT 1

/* Define to 1 if `FastPathOutput' is a member of `rdpSettings'. */
#define HAVE_RDPSETTINGS_FASTPATHOUTPUT 1

/* Define to 1 if `height' is a member of `rdpSettings'. */
#define HAVE_RDPSETTINGS_HEIGHT 1

/* Define to 1 if `OrderSupport' is a member of `rdpSettings'. */
#define HAVE_RDPSETTINGS_ORDERSUPPORT 1

/* Define to 1 if `order_support' is a member of `rdpSettings'. */
#define HAVE_RDPSETTINGS_ORDER_SUPPORT 1

/* Define to 1 if `SendPreconnectionPdu' is a member of `rdpSettings'. */
#define HAVE_RDPSETTINGS_SENDPRECONNECTIONPDU 1

/* Define to 1 if `SupportDisplayControl' is a member of `rdpSettings'. */
//#define HAVE_RDPSETTINGS_SUPPORTDISPLAYCONTROL 1

/* Define to 1 if `width' is a member of `rdpSettings'. */
// #define HAVE_RDPSETTINGS_WIDTH 1

/* Define to 1 if `interval_ms' is a member of `rdpSvcPlugin'. */
/* #undef HAVE_RDPSVCPLUGIN_INTERVAL_MS */

/* Define to 1 if `event_class' is a member of `RDP_EVENT'. */
#define HAVE_RDP_EVENT_EVENT_CLASS 1

/* Define to 1 if `destHost' is a member of `rfbClient'. */
/* #undef HAVE_RFBCLIENT_DESTHOST */

/* Define to 1 if `destPort' is a member of `rfbClient'. */
/* #undef HAVE_RFBCLIENT_DESTPORT */

/* Define to 1 if you have the `select' function. */
//#define HAVE_SELECT 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <syslog.h> header file. */
//#define HAVE_SYSLOG_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
//#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
//#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
//#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
//#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <time.h> header file. */
//#define HAVE_TIME_H 1

/* Define to 1 if you have the <unistd.h> header file. */
//#define HAVE_UNISTD_H 1

/* Define to 1 if `id' is a member of `wMessage'. */
/* #undef HAVE_WMESSAGE_ID */

/* Whether interleaved_decompress() accepts an additional palette parameter */
/* #undef INTERLEAVED_DECOMPRESS_TAKES_PALETTE */

/* Whether the legacy RDP_EVENT API was found */
// #define LEGACY_EVENT /**/

/* Whether the older version of the FreeRDP API was found */
// #define LEGACY_FREERDP /**/

/* Whether the legacy IWTSVirtualChannelCallback API was found */
#define LEGACY_IWTSVIRTUALCHANNELCALLBACK 1

/* Whether the legacy rdpBitmap API was found */
// #define LEGACY_RDPBITMAP /**/

/* Whether the legacy rdpPalette API was found */
// #define LEGACY_RDPPALETTE /**/

/* Whether the legacy version of the rdpSettings API was found */
// #define LEGACY_RDPSETTINGS /**/

/* Whether libssh2 was built against libgcrypt */
/* #undef LIBSSH2_USES_GCRYPT */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "guacamole-server"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "guacamole-server"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "guacamole-server 0.9.12-incubating"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "guacamole-server"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.9.12-incubating"

/* Whether planar_decompress() can flip */
/* #undef PLANAR_DECOMPRESS_CAN_FLIP */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "0.9.12-incubating"

/* Uses X/Open and POSIX APIs */
#define _XOPEN_SOURCE 700

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef ssize_t */
