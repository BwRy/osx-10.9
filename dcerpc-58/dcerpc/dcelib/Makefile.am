## Process this file with automake to produce Makefile.in

include $(top_srcdir)/include.mk
include $(top_srcdir)/ncklib/ncklib.mk

# Build the shared library
lib_LTLIBRARIES = libdcerpc.la
libdcerpc_la_SOURCES = dummyfuncs.c strlcpy.c strlcat.c
libdcerpc_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_unistr)
libdcerpc_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_unistr) $(NAF_NP_LDFLAGS)

if TARGET_OS_LINUX
libdcerpc_la_LDFLAGS += -version-info 1:2:0 -all_load -Wl,--version-script,$(srcdir)/exports -Wl,-Bgroup -Wl,--allow-shlib-undefined
else
libdcerpc_la_LDFLAGS += -version-info 1:2:0 -all_load
endif

# uint16 converters not really needed now
#
# libdcerpc_la_SOURCES += mswrappers.c
# if TARGET_OS_DARWIN
# libdcerpc_la_LDFLAGS += -no-undefined
# libdcerpc_la_SOURCES += darwin/wc16str.c
# endif

libdcerpc_la_LIBADD =			\
	../ncklib/libnck.la		\
	../idl_lib/libidl.la		\
	../uuid/libuuid.la		\
	../libdcethread/libdcethread.la	\
	$(LIB_socket)			\
	$(LIB_nsl)			\
	$(LIB_dl)			\
	$(LIB_unistr)
