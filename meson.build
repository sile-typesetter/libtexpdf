project('libtexpdf', 'c', version : '2.4.6', default_options : ['c_std=gnu11'])

pkg = import('pkgconfig')

compiler = meson.get_compiler('c')

conf = configuration_data()
conf.set_quoted('PACKAGE_VERSION', meson.project_version())

check_headers = ['inttypes.h', 'stdint.h', 'stdbool.h', 'stdlib.h', 'string.h', 'sys/stat.h', 'sys/types.h', 'sys/wait.h']
foreach header_name : check_headers
    if compiler.has_header(header_name)
        conf.set('HAVE_' + header_name.underscorify().to_upper(), 1)
    endif
endforeach

if compiler.has_function('timezone', prefix : '#include <time.h>')
    conf.set('HAVE_TIMEZONE', 1)
endif

if compiler.has_function('getenv', prefix : '#include <stdlib.h>')
    conf.set('HAVE_GETENV', 1)
endif

if compiler.has_function('mkstemp', prefix : '#include <stdlib.h>')
    conf.set('HAVE_MKSTEMP', 1)
endif

if compiler.has_member('struct tm', 'tm_gmtoff', prefix : '#include <time.h>')
    conf.set('HAVE_TM_GMTOFF', 1)
endif

configure_file(input: 'config.h.meson', output: 'config.h', configuration: conf)

libm = compiler.find_library('m', required : false)

zlib_dep = dependency('zlib')
libpng_dep = dependency('libpng')

if not compiler.has_function('compress2', prefix : '#include <zlib.h>', dependencies: zlib_dep)
	  error('zlib with compress2 is required')
endif

libtexpdf_deps = [libpng_dep, zlib_dep, libm]

libtexpdf_cargs = ['-DXETEX', '-DBUILDING_LIBTEXPDF', '-DHAVE_CONFIG_H', '-DHAVE_ZLIB', '-DHAVE_ZLIB_COMPRESS2', '-DHAVE_LIBPNG']

libtexpdf_sources = [
	  'agl.c',
	  'bmpimage.c',
	  'cff.c',
	  'cff_dict.c',
	  'cid.c',
	  'cidtype0.c',
	  'cidtype2.c',
	  'cmap.c',
	  'cmap_read.c',
	  'cmap_write.c',
	  'cs_type2.c',
	  'dpxcrypt.c',
	  'dpxfile.c',
	  'dpxutil.c',
	  'epdf.c',
	  'error.c',
	  'fontmap.c',
	  'jp2image.c',
	  'jpegimage.c',
	  'mem.c',
	  'mfileio.c',
	  'numbers.c',
	  'otl_conf.c',
	  'otl_opt.c',
	  'pdfcolor.c',
	  'pdfdev.c',
	  'pdfdoc.c',
	  'pdfdraw.c',
	  'pdfencrypt.c',
	  'pdfencoding.c',
	  'pdffont.c',
	  'pdfnames.c',
	  'pdfobj.c',
	  'pdfparse.c',
	  'pdfresource.c',
	  'pdfximage.c',
	  'pngimage.c',
	  'pst.c',
	  'pst_obj.c',
	  'sfnt.c',
	  'subfont.c',
	  't1_char.c',
	  't1_load.c',
	  'truetype.c',
	  'tt_aux.c',
	  'tt_cmap.c',
	  'tt_glyf.c',
	  'tt_gsub.c',
	  'tt_post.c',
	  'tt_table.c',
	  'type0.c',
	  'type1.c',
	  'type1c.c',
	  'unicode.c',
	  'pkfont.c',
	  'tfm.c',
]

libtexpdf = library('texpdf', libtexpdf_sources,
    dependencies: libtexpdf_deps,
    c_args: libtexpdf_cargs,
    install: true,
)

install_headers(
    'agl.h',
    'bmpimage.h',
    'cff.h',
    'cff_dict.h',
    'cff_limits.h',
    'cff_stdstr.h',
    'cff_types.h',
    'cid_basefont.h',
    'cid.h',
    'cid_p.h',
    'cidtype0.h',
    'cidtype2.h',
    'cmap.h',
    'cmap_p.h',
    'cmap_read.h',
    'cmap_write.h',
    'cs_type2.h',
    'dpxcrypt.h',
    'dpxfile.h',
    'dpxutil.h',
    'epdf.h',
    'error.h',
    'fontmap.h',
    'jp2image.h',
    'jpegimage.h',
    'libtexpdf.h',
    'mem.h',
    'mfileio.h',
    'numbers.h',
    'otl_conf.h',
    'otl_opt.h',
    'pdfcolor.h',
    'pdfdev.h',
    'pdfdoc.h',
    'pdfdraw.h',
    'pdfencrypt.h',
    'pdfencoding.h',
    'pdffont.h',
    'pdflimits.h',
    'pdfnames.h',
    'pdfobj.h',
    'pdfparse.h',
    'pdfresource.h',
    'pdftypes.h',
    'pdfximage.h',
    'pngimage.h',
    'pst.h',
    'pst_obj.h',
    'sfnt.h',
    'subfont.h',
    't1_char.h',
    't1_load.h',
    'truetype.h',
    'tt_aux.h',
    'tt_cmap.h',
    'tt_glyf.h',
    'tt_gsub.h',
    'tt_post.h',
    'tt_table.h',
    'type0.h',
    'type1.h',
    'type1c.h',
    'unicode.h',
    'pkfont.h',
    'tfm.h',
    subdir : 'libtexpdf'
)

libtexpdf_dep = declare_dependency(
    dependencies : libtexpdf_deps,
    link_with : libtexpdf,
)

subdir('tests')

pkg.generate(libtexpdf,
    filebase : 'libtexpdf',
    name : 'libtexpdf',
    description : 'A PDF library extracted from TeX\'s dvipdfmx',
    url : 'https://github.com/sile-typesetter/libtexpdf',
)

install_data('LICENSE', install_dir : 'share/libtexpdf')

