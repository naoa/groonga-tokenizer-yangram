AM_CFLAGS =					\
	$(GROONGA_CFLAGS)

AM_LDFLAGS =					\
	-avoid-version				\
	-module					\
	-no-undefined

LIBS =						\
	$(GROONGA_LIBS)

tokenizers_plugins_LTLIBRARIES =
tokenizers_plugins_LTLIBRARIES += yangram.la 

tokenizers_plugins_SOURCE =			\
	yangram.c

yangram_la_CFLAGS = $(AM_CFLAGS) $(SNOWBALL_CFLAGS)
yangram_la_LIBADD = $(LIBS) $(SNOWBALL_LIBS)
