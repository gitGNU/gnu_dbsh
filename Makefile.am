ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = -D_XOPEN_SOURCE=600 -DLOCALEDIR=\"$(localedir)\" @DEFS@
AM_CFLAGS = -Wall

bin_PROGRAMS = dbsh

dbsh_SOURCES = main.c common.h \
               action.h action.c \
               buffer.h buffer.c \
               command.h command.c \
               db.h db.c \
               err.h err.c \
               gettext.h \
               gplv3.h \
               help.h \
               output.h output.c \
               parser.h parser.c \
               prompt.h prompt.c \
               rc.h rc.c \
               rl.h rl.c \
               results.h results.c \
               sig.h sig.c \
               stream.h stream.c

dbsh_LDADD = @LIBINTL@

EXTRA_DIST = config.rpath dbsh.test/*.exp dbsh.test/test.db

SUBDIRS = po
