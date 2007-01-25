INTERFACE=nacro.h
SRCS=nacro.c
OBJS=nacro.o
ISRCS=nacro_wrap.c
IOBJS=nacro_wrap.o
TARGET=nacro
LIBS= @LIBVNCSERVERLIBS@

nacro_CFLAGS= @LIBVNCSERVERCFLAGS@

SWIGOPT=

EXTRA_DIST=autogen.sh $(INTERFACE) $(SRCS) $(ISRCS) nacro.pm recorder.pl

all: $(LIBPREFIX)$(TARGET)$(SO)

# the following is borrowed from SWIG

SWIG= @SWIG@

##################################################################
#####                       PERL 5                          ######
##################################################################

# You need to set this variable to the Perl5 directory containing the
# files "perl.h", "EXTERN.h" and "XSUB.h".   With Perl5.003, it's
# usually something like /usr/local/lib/perl5/arch-osname/5.003/CORE.

PERL5_INCLUDE= @PERL5EXT@

# Extra Perl specific dynamic linking options
PERL5_DLNK   = @PERL5DYNAMICLINKING@
PERL5_CCFLAGS = @PERL5CCFLAGS@

# ----------------------------------------------------------------
# Build a Perl5 dynamically loadable module (C)
# ----------------------------------------------------------------

$(ISRCS): $(INTERFACE)
	@test -n "$(SWIG)" || (echo "Need SWIG" && exit 1)
	$(SWIG) -perl5 $(SWIGOPT) $(INTERFACE)

$(OBJS): $(SRCS) $(INTERFACE)
	$(CC) -c -Dbool=char $(CCSHARED) $(CFLAGS) -o $@ $< $(LIBVNCSERVERCFLAGS) $(INCLUDES) -I$(PERL5_INCLUDE)

$(IOBJS): $(ISRCS) $(INTERFACE)
	$(CC) -c -Dbool=char $(CCSHARED) $(CFLAGS) -o $@ $< $(INCLUDES) $(PERL5_CCFLAGS) -I$(PERL5_INCLUDE)

$(LIBPREFIX)$(TARGET)$(SO): $(OBJS) $(IOBJS)
	$(LDSHARED) $(OBJS) $(IOBJS) $(PERL5_DLNK) $(LIBS) -o $(LIBPREFIX)$(TARGET)$(SO)

