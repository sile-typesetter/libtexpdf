AM_CPPFLAGS = $(FREETYPE_CFLAGS) $(LIBPNG_INCLUDES) $(ZLIB_INCLUDES) $(LIBPAPER_INCLUDES)

lib_LTLIBRARIES = libtexpdf.la

libtexpdf_la_CPPFLAGS = -DXETEX $(AM_CPPFLAGS)
AM_CFLAGS = $(WARNING_CFLAGS)

libtexpdf_la_SOURCES = \
	agl.c \
	agl.h \
	bmpimage.c \
	bmpimage.h \
	cff.c \
	cff.h \
	cff_dict.c \
	cff_dict.h \
	cff_limits.h \
	cff_stdstr.h \
	cff_types.h \
	cid_basefont.h \
	cid.c \
	cid.h \
	cid_p.h \
	cidtype0.c \
	cidtype0.h \
	cidtype2.c \
	cidtype2.h \
	cmap.c \
	cmap.h \
	cmap_p.h \
	cmap_read.c \
	cmap_read.h \
	cmap_write.c \
	cmap_write.h \
	cs_type2.c \
	cs_type2.h \
	dpxcrypt.c \
	dpxcrypt.h \
	dpxutil.c \
	dpxutil.h \
	epdf.c \
	epdf.h \
	error.c \
	error.h \
	fontmap.c \
	fontmap.h \
	jp2image.c \
	jp2image.h \
	jpegimage.c \
	jpegimage.h \
	mem.c \
	mem.h \
	mfileio.c \
	mfileio.h \
	mpost.c \
	mpost.h \
	numbers.c \
	numbers.h \
	otl_conf.c \
	otl_conf.h \
	otl_opt.c \
	otl_opt.h \
	pdfcolor.c \
	pdfcolor.h \
	pdfdev.c \
	pdfdev.h \
	pdfdoc.c \
	pdfdoc.h \
	pdfdraw.c \
	pdfdraw.h \
	pdfencrypt.c \
	pdfencrypt.h \
	pdfencoding.c \
	pdfencoding.h \
	pdffont.c \
	pdffont.h \
	pdflimits.h \
	pdfnames.c \
	pdfnames.h \
	pdfobj.c \
	pdfobj.h \
	pdfparse.c \
	pdfparse.h \
	pdfresource.c \
	pdfresource.h \
	pdfximage.c \
	pdfximage.h \
	pngimage.c \
	pngimage.h \
	pst.c \
	pst.h \
	pst_obj.c \
	pst_obj.h \
	sfnt.c \
	sfnt.h \
	subfont.c \
	subfont.h \
	system.h \
	t1_char.c \
	t1_char.h \
	t1_load.c \
	t1_load.h \
	truetype.c \
	truetype.h \
	tt_aux.c \
	tt_aux.h \
	tt_cmap.c \
	tt_cmap.h \
	tt_glyf.c \
	tt_glyf.h \
	tt_gsub.c \
	tt_gsub.h \
	tt_post.c \
	tt_post.h \
	tt_table.c \
	tt_table.h \
	type0.c \
	type0.h \
	type1.c \
	type1.h \
	type1c.c \
	type1c.h \
	unicode.c \
	unicode.h

if TEXLIVE_INTERNAL
libtexpdf_la_SOURCES += \
	pkfont.c \
	pkfont.h \
	tfm.c \
	tfm.h
endif

libtexpdf_la_LIBADD = $(FREETYPE_LIBS) $(LIBPNG_LIBS) $(ZLIB_LIBS) $(LIBPAPER_LIBS)
