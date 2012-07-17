CC=cc

# You only need to uncomment one section below
# Hopefully, in a future release I will include gnu configure

# For Linux Systems with Redhat Motif 2.1
LIBS=-lXm -lXt -lX11 -lXext -lXp
LIBPATH=/usr/X11R6/lib
MOREARGS=-Wall


# Static Link version for Linux Systems with Redhat Motif 2.1
LIBS=-lXm -lXt -lX11 -lXext -lXp -lSM -lICE
LIBPATH=/usr/X11R6/lib
MOREARGS=-static


# For Linux Systems with Lesstif (or other versions of Motif)
# LIBS=-lXm -lXt -lX11 -lXext
# LIBPATH=/usr/X11R6/lib
# MOREARGS=

# For SCO UNIX
# LIBS=-lXm -lXt -lX11 -lXext -lsocket -lmalloc
# LIBPATH=/usr/lib/X11
# MOREARGS=

# For Solaris
# LIBS=-lXm -lXt -lX11 -lXext
# LIBPATH=/usr/dt/lib
# MOREARGS=-I/usr/openwin/include -I/usr/dt/include


xsol: xsol.c xsol.h
	$(CC) -o xsol xsol.c $(MOREARGS) -L$(LIBPATH) $(LIBS)