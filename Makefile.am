## Makefile.am for the TeX Live subdirectory texk/dvipdfm-x/
##
## Copyright (C) 2009-2014 Peter Breitenlohner <tex-live@tug.org>
## You may freely use, modify and/or distribute this file.
##
bin_PROGRAMS =
if DVIPDFMX
bin_PROGRAMS += dvipdfmx
if !WIN32
dist_bin_SCRIPTS = dvipdft
endif !WIN32
endif DVIPDFMX
if XDVIPDFMX
bin_PROGRAMS += xdvipdfmx
endif XDVIPDFMX
EXTRA_PROGRAMS = dvipdfmx xdvipdfmx

AM_CPPFLAGS = $(KPATHSEA_INCLUDES) $(LIBPNG_INCLUDES) $(ZLIB_INCLUDES) $(LIBPAPER_INCLUDES)
xdvipdfmx_CPPFLAGS = -DXETEX $(FREETYPE2_INCLUDES) $(AM_CPPFLAGS)
AM_CFLAGS = $(WARNING_CFLAGS)

SUBDIRS = libtexpdf

dvipdfmx_SOURCES = \
	dpxfile.c \
	dpxfile.h \
	dvi.c \
	dvi.h \
	dvicodes.h \
	dvipdfmx.c \
	dvipdfmx.h \
	error.c \
	error.h \
	mem.c \
	mem.h \
	mfileio.c \
	mfileio.h \
	numbers.c \
	numbers.h \
	spc_color.c \
	spc_color.h \
	spc_dvips.c \
	spc_dvips.h \
	spc_html.c \
	spc_html.h \
	spc_misc.c \
	spc_misc.h \
	spc_pdfm.c \
	spc_pdfm.h \
	spc_tpic.c \
	spc_tpic.h \
	spc_util.h \
	spc_util.c \
	spc_xtx.c \
	spc_xtx.h \
	specials.c \
	specials.h \
	system.h \
	vf.c \
	vf.h \
	xbb.c

nodist_xdvipdfmx_SOURCES = $(dvipdfmx_SOURCES)

dvipdfmx_LDADD = $(KPATHSEA_LIBS) $(LIBPNG_LIBS) $(ZLIB_LIBS) $(LIBPAPER_LIBS) libtexpdf/libtexpdf.la
xdvipdfmx_LDADD = $(FREETYPE2_LIBS) $(dvipdfmx_LDADD)

$(dvipdfmx_OBJECTS) $(xdvipdfmx_OBJECTS): config.force

## Links
##
bin_links =

if DVIPDFMX
if WIN32
noinst_PROGRAMS = call_dvipdfmx
endif WIN32
bin_links += \
	dvipdfmx$(EXEEXT):dvipdfm \
	dvipdfmx$(EXEEXT):ebb \
	dvipdfmx$(EXEEXT):extractbb
endif DVIPDFMX

call_dvipdfmx_CPPFLAGS = -DEXEPROG=\"dvipdfmx.exe\"
nodist_call_dvipdfmx_SOURCES = callexe.c

include $(top_srcdir)/am/bin_links.am

## Manpages

man1_links =
if DVIPDFMX
dist_man1_MANS = man/dvipdfmx.1 man/dvipdft.1 man/extractbb.1
man1_links += extractbb:ebb
man1_links += dvipdfmx:dvipdfm
endif DVIPDFMX

## doesn't seem worth worrying about the broken man page link
## if only xdvipdfmx is being built.  not like there is x*x-specific
## documentation anywhere anyway.
if XDVIPDFMX
man1_links += dvipdfmx:xdvipdfmx
endif XDVIPDFMX

include $(top_srcdir)/am/man1_links.am

## Data
##
configdatadir = $(datarootdir)/texmf-dist/dvipdfmx
dist_configdata_DATA = data/dvipdfmx.cfg
 
glyphlistdatadir = $(datarootdir)/texmf-dist/fonts/map/glyphlist
dist_glyphlistdata_DATA = data/glyphlist.txt data/pdfglyphlist.txt

mapdatadir = $(datarootdir)/texmf-dist/fonts/map/dvipdfmx
dist_mapdata_DATA = data/cid-x.map

cmapdatadir = $(datarootdir)/texmf-dist/fonts/cmap/dvipdfmx
dist_cmapdata_DATA = data/EUC-UCS2

install-exec-hook: install-bin-links
if DVIPDFMX
if WIN32
	$(INSTALL_PROGRAM) call_dvipdfmx$(EXEEXT) $(DESTDIR)$(bindir)/dvipdfm$(EXEEXT)
	$(INSTALL_PROGRAM) call_dvipdfmx$(EXEEXT) $(DESTDIR)$(bindir)/ebb$(EXEEXT)
	$(INSTALL_PROGRAM) call_dvipdfmx$(EXEEXT) $(DESTDIR)$(bindir)/extractbb$(EXEEXT)
endif WIN32
endif DVIPDFMX

install-data-hook: install-man1-links

uninstall-hook: uninstall-bin-links uninstall-man1-links
if DVIPDFMX
if WIN32
	rm -f $(DESTDIR)$(bindir)/dvipdfm$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/ebb$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/extractbb$(EXEEXT)
endif WIN32
endif DVIPDFMX

all-local: config.force

config.force: $(ZLIB_DEPEND) $(LIBPNG_DEPEND) $(KPATHSEA_DEPEND) $(FREETYPE2_DEPEND) $(LIBPAPER_DEPEND)
	echo timestamp >config.force
	$(SHELL) ./config.status --recheck

DISTCLEANFILES = config.force

## Rebuild libkpathsea
@KPATHSEA_RULE@
## Rebuild libfreetype
@FREETYPE2_RULE@
## Rebuild libpng
@LIBPNG_RULE@
## Rebuild libz
@ZLIB_RULE@
## Rebuild libpaper
@LIBPAPER_RULE@

## Tests
##
TESTS =
dvipdfmx_tests = dvipdfmx.test dvipdfm-ann.test dvipdfm-bb.test dvipdfm-bkm.test dvipdfm-psz.test
dvipdfmx_tests += dvipdfm-res.test
if DVIPDFMX
TESTS += $(dvipdfmx_tests)
endif DVIPDFMX
xdvipdfmx_tests = xdvipdfmx.test xdvipdfm-ann.test xdvipdfm-bb.test xdvipdfm-bkm.test xdvipdfm-psz.test
xdvipdfmx_tests += xdvipdfm-res.test
if XDVIPDFMX
TESTS += $(xdvipdfmx_tests)
endif XDVIPDFMX
EXTRA_DIST = $(dvipdfmx_tests) $(xdvipdfmx_tests)
## Test Data
EXTRA_DIST += tests/dvipdfmx.cfg tests/psfonts.map
EXTRA_DIST += tests/fullmap.dvi tests/fullmap.tex
EXTRA_DIST += tests/image.bmp tests/imgbmp.bb tests/imgbmp.xbb tests/picbmp.bb tests/picbmp.xbb
EXTRA_DIST += tests/image.jp2 tests/imgjp2.bb tests/imgjp2.xbb tests/picjp2.bb tests/picjp2.xbb
EXTRA_DIST += tests/image.jpeg tests/imgjpeg.bb tests/imgjpeg.xbb tests/picjpeg.bb tests/picjpeg.xbb
EXTRA_DIST += tests/image.png tests/imgpng.bb tests/imgpng.xbb tests/picpng.bb tests/picpng.xbb
EXTRA_DIST += tests/image.pdf tests/imgpdf.bb tests/imgpdf.xbb tests/picpdf.bb tests/picpdf.xbb
EXTRA_DIST += tests/image.dvi tests/image.tex
EXTRA_DIST += tests/cmr10.pfb tests/cmr10.tfm tests/annot.dvi tests/annot.tex
EXTRA_DIST += tests/bookm.dvi tests/bookm.tex tests/paper.dvi tests/paper.tex
EXTRA_DIST += tests/resrc.dvi tests/resrc.tex
EXTRA_DIST += tests/xbmc.dvi tests/xbmc.tex tests/xbmc10.600pk tests/xbmc10.tfm
DISTCLEANFILES += img*.* pic*.* image*.pdf annot*.pdf bookm*.pdf paper*.pdf resrc*.pdf xbmc*.pdf
