/* This is dvipdfmx, an eXtended version of dvipdfm by Mark A. Wicks.

    Copyright (C) 2007-2014 by Jin-Hwan Cho and Shunsaku Hirata,
    the dvipdfmx project team.
    
    Copyright (C) 1998, 1999 by Mark A. Wicks <mwicks@kettering.edu>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
*/

#include "libtexpdf/libtexpdf.h"

#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "error.h"
#include "mem.h"

#include "dpxutil.h"
#include "mfileio.h"

#include "dpxfile.h"
#include "dpxcrypt.h"
#define MAX_KEY_LEN 16

#include <string.h>
#ifdef WIN32
#include <io.h>
#include <process.h>
#include <wchar.h>
#else
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(val) ((unsigned)(val) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(val) (((val) & 255) == 0)
#endif
#endif

#define xcalloc calloc
#define xgetcwd getcwd
#define xstrdup strdup

static int verbose = 0;
int keep_cache = 0;

void
dpx_file_set_verbose (void)
{
  verbose++;
}


/* Kpathsea library does not check file type. */
static int qcheck_filetype (const char *fqpn, dpx_res_type type);

/* For testing MIKTEX enabled compilation */
#if defined(TESTCOMPILE) && !defined(MIKTEX)
#  define MIKTEX        1
#  define PATH_SEP_CHR  '/'
#  define _MAX_PATH     256

static int
miktex_get_acrobat_font_dir (char *buf)
{
  strcpy(buf, "/usr/share/ghostscript/Resource/Font/");
  return  1;
}

static int
miktex_find_file (const char *filename, const char *dirlist, char *buf)
{
  int    r = 0;
  char  *fqpn;

  fqpn = kpse_path_search(dirlist, filename, 0);
  if (!fqpn)
    return  0;
  if (strlen(fqpn) > _MAX_PATH)
    r = 0;
  else {
    strcpy(buf, fqpn);
    r = 1;
  }
  RELEASE(fqpn);

  return  r;
}

static int
miktex_find_app_input_file (const char *progname, const char *filename, char *buf)
{
  int    r = 0;
  char  *fqpn;

  kpse_reset_program_name(progname);
  fqpn = kpse_find_file  (filename, kpse_program_text_format, false);
  kpse_reset_program_name("dvipdfmx");

  if (!fqpn)
    return  0;
  if (strlen(fqpn) > _MAX_PATH)
    r = 0;
  else {
    strcpy(buf, fqpn);
    r = 1;
  }
  RELEASE(fqpn);

  return  r;
}

static int
miktex_find_psheader_file (const char *filename, char *buf)
{
  int    r;
  char  *fqpn;

  fqpn = kpse_find_file(filename, kpse_tex_ps_header_format, 0);

  if (!fqpn)
    return  0;
  if (strlen(fqpn) > _MAX_PATH)
    r = 0;
  else {
    strcpy(buf, fqpn);
    r = 1;
  }
  RELEASE(fqpn);

  return  r; 
}

#endif /* TESTCOMPILE */

#ifdef  MIKTEX
#ifndef PATH_SEP_CHR
#  define PATH_SEP_CHR '\\'
#endif
static char  _tmpbuf[_MAX_PATH+1];
#endif /* MIKTEX */

static int exec_spawn (char *cmd)
{
  char **cmdv, **qv;
  char *p, *pp;
  char buf[1024];
  int  i, ret = -1;
#ifdef WIN32
  wchar_t **cmdvw, **qvw;
#endif

  if (!cmd)
    return -1;
  while (*cmd == ' ' || *cmd == '\t')
    cmd++;
  if (*cmd == '\0')
    return -1;
  i = 0;
  p = cmd;
  while (*p) {
    if (*p == ' ' || *p == '\t')
      i++;
    p++;
  }
  cmdv = xcalloc (i + 2, sizeof (char *));
  p = cmd;
  qv = cmdv;
  while (*p) {
    pp = buf;
    if (*p == '"') {
      p++;
      while (*p != '"') {
        if (*p == '\0') {
          goto done;
        }
        *pp++ = *p++;
      }
      p++;
    } else if (*p == '\'') {
      p++;
      while (*p != '\'') {
        if (*p == '\0') {
          goto done;
        }
        *pp++ = *p++;
      }
      p++;
    } else {
      while (*p != ' ' && *p != '\t' && *p) {
        if (*p == '\'') {
          p++;
          while (*p != '\'') {
             if (*p == '\0') {
                 goto done;
             }
             *pp++ = *p++;
          }
          p++;
        } else {
          *pp++ = *p++;
        }
      }
    }
    *pp = '\0';
#ifdef WIN32
    if (strchr (buf, ' ') || strchr (buf, '\t'))
      *qv = concat3 ("\"", buf, "\"");
    else
#endif
      *qv = xstrdup (buf);
/*
    fprintf(stderr,"\n%s", *qv);
*/
    while (*p == ' ' || *p == '\t')
      p++;
    qv++;
  }
#ifdef WIN32
  cmdvw = xcalloc (i + 2, sizeof (wchar_t *));
  qv = cmdv;
  qvw = cmdvw;
  while (*qv) {
    *qvw = get_wstring_from_fsyscp(*qv, *qvw=NULL);
    qv++;
    qvw++;
  }
  *qvw = NULL;
  ret = _wspawnvp (_P_WAIT, *cmdvw, (const wchar_t* const*) cmdvw);
  if (cmdvw) {
    qvw = cmdvw;
    while (*qvw) {
      free (*qvw);
      qvw++;
    }
    free (cmdvw);
  }
#else
  i = fork ();
  if (i < 0)
    ret = -1;
  else if (i == 0) {
    if (execvp (*cmdv, cmdv))
      _exit (-1);
  } else {
    if (wait (&ret) == i) {
      ret = (WIFEXITED (ret) ? WEXITSTATUS (ret) : -1);
    } else {
      ret = -1;
    }
  }
#endif
done:
  qv = cmdv;
  while (*qv) {
    free (*qv);
    qv++;
  }
  free (cmdv);
  return ret;
}

#if 0
/* ensuresuffix() returns a copy of basename if sfx is "". */
static char *
ensuresuffix (const char *basename, const char *sfx)
{
  char  *q, *p;

  p = NEW(strlen(basename) + strlen(sfx) + 1, char);
  strcpy(p, basename);
  q = strrchr(p, '.');
  if (!q && sfx[0])
    strcat(p, sfx);

  return  p;
}

static int
is_absolute_path(const char *filename)
{
#ifdef WIN32
  if (isalpha(filename[0]) && filename[1] == ':')
    return 1;
  if (filename[0] == '\\' && filename[1] == '\\')
    return 1;
  if (filename[0] == '/' && filename[1] == '/')
    return 1;
#else
  if (filename[0] == '/')
    return 1;
#endif
  return 0;
}
#endif

static int
has_suffix(const char *str, const char *suffix)
{
  if (!str || !suffix)
      return 0;
  size_t lenstr = strlen(str);
  size_t lensuffix = strlen(suffix);
  if (lensuffix >  lenstr)
      return 0;
  return strncasecmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

static const char *
dpx_get_tmpdir (void)
{
#ifdef WIN32
#  define __TMPDIR     "."
#else /* WIN32 */
#  define __TMPDIR     "/tmp"
#endif /* WIN32 */
    const char *_tmpd;

#ifdef  HAVE_GETENV
    _tmpd = getenv("TMPDIR");
#  ifdef WIN32
    if (!_tmpd)
      _tmpd = getenv("TMP");
    if (!_tmpd)
      _tmpd = getenv("TEMP");
#  endif /* WIN32 */
    if (!_tmpd)
      _tmpd = __TMPDIR;
#else /* HAVE_GETENV */
    _tmpd = __TMPDIR;
#endif /* HAVE_GETENV */
    return _tmpd;
}

#ifdef  HAVE_MKSTEMP
#  include <stdlib.h>
#endif

char *
dpx_create_temp_file (void)
{
  char  *tmp = NULL;

#if defined(MIKTEX)
  {
    tmp = NEW(_MAX_PATH + 1, char);
    miktex_create_temp_file_name(tmp); /* FIXME_FIXME */
  }
#elif defined(HAVE_MKSTEMP)
#  define TEMPLATE     "/dvipdfmx.XXXXXX"
  {
    const char *_tmpd;
    int   _fd = -1;
    _tmpd = dpx_get_tmpdir();
    tmp = NEW(strlen(_tmpd) + strlen(TEMPLATE) + 1, char);
    strcpy(tmp, _tmpd);
    strcat(tmp, TEMPLATE);
    _fd  = mkstemp(tmp);
    if (_fd != -1)
#  ifdef WIN32
      _close(_fd);
#  else
      close(_fd);
#  endif /* WIN32 */
    else {
      RELEASE(tmp);
      tmp = NULL;
    }
  }
#else /* use _tempnam or tmpnam */
  {
#  ifdef WIN32
    const char *_tmpd;
    char *p;
    _tmpd = dpx_get_tmpdir();
    tmp = _tempnam (_tmpd, "dvipdfmx.");
    for (p = tmp; *p; p++) {
      if (IS_KANJI (p))
        p++;
      else if (*p == '\\')
        *p = '/';
    }
#  else /* WIN32 */
    char *_tmpa = NEW(L_tmpnam + 1, char);
    tmp = tmpnam(_tmpa);
    if (!tmp)
      RELEASE(_tmpa);
#  endif /* WIN32 */
  }
#endif /* MIKTEX */

  return  tmp;
}

char *
dpx_create_fix_temp_file (const char *filename)
{
#define PREFIX "dvipdfm-x."
  static const char *dir = NULL;
  static char *cwd = NULL;
  char *ret, *s;
  int i;
  MD5_CONTEXT state;
  unsigned char digest[MAX_KEY_LEN];
#ifdef WIN32
  char *p;
#endif

  if (!dir) {
      char wd[PATH_MAX];
      dir = dpx_get_tmpdir();
      getcwd(wd, PATH_MAX);
      cwd = strdup(wd);
  }

  texpdf_MD5_init(&state);
  texpdf_MD5_write(&state, (unsigned char *)cwd,      strlen(cwd));
  texpdf_MD5_write(&state, (unsigned const char *)filename, strlen(filename));
  texpdf_MD5_final(digest, &state);

  ret = NEW(strlen(dir)+1+strlen(PREFIX)+MAX_KEY_LEN*2 + 1, char);
  sprintf(ret, "%s/%s", dir, PREFIX);
  s = ret + strlen(ret);
  for (i=0; i<MAX_KEY_LEN; i++) {
      sprintf(s, "%02x", digest[i]);
      s += 2;
  }
#ifdef WIN32
  for (p = ret; *p; p++) {
    if (IS_KANJI (p))
      p++;
    else if (*p == '\\')
      *p = '/';
  }
#endif
  /* printf("dpx_create_fix_temp_file: %s\n", ret); */
  return ret;
}

static int
dpx_clear_cache_filter (const struct dirent *ent) {
    int plen = strlen(PREFIX);
    if (strlen(ent->d_name) != plen + MAX_KEY_LEN * 2) return 0;
#ifdef WIN32
    return strncasecmp(ent->d_name, PREFIX, plen) == 0;
#else
    return strncmp(ent->d_name, PREFIX, plen) == 0;
#endif
}

/* WARNING: If you're used to dpx_open_file in dvipdfkmx, this has
quite a different meaning within libtexpdf. Here the job is ensuring
that the filename is a fully-qualified name of the type that we are
expecting. */
FILE *
dpx_open_file (const char *filename, dpx_res_type type)
{
  FILE  *fp   = NULL;

  if (type == DPX_RES_TYPE_DFONT && !has_suffix(filename, ".dfont"))
    return fp;

  if (qcheck_filetype(filename, type)) {
    fp = MFOPEN(filename, FOPEN_RBIN_MODE);
  }
  return  fp;
}

void
dpx_delete_old_cache (int life)
{
  const char *dir;
  char *pathname;
  DIR *dp;
  struct dirent *de;
  time_t limit;

  if (life == -2) {
      keep_cache = -1;
      return;
  }

  dir = dpx_get_tmpdir();
  pathname = NEW(strlen(dir)+1+strlen(PREFIX)+MAX_KEY_LEN*2 + 1, char);
  limit = time(NULL) - life * 60 * 60;

  if (life >= 0) keep_cache = 1;
  if ((dp = opendir(dir)) != NULL) {
      while((de = readdir(dp)) != NULL) {
          if (dpx_clear_cache_filter(de)) {
              struct stat sb;
              sprintf(pathname, "%s/%s", dir, de->d_name);
              stat(pathname, &sb);
              if (sb.st_mtime < limit) {
                  remove(pathname);
                  /* printf("remove: %s\n", pathname); */
              }
          }
      }
      closedir(dp);
  }
  RELEASE(pathname);
}

void
dpx_delete_temp_file (char *tmp, int force)
{
  if (!tmp)
    return;
  if (force || keep_cache != 1) remove (tmp);
  RELEASE(tmp);

  return;
}

/* dpx_file_apply_filter() is used for converting unsupported graphics
 * format to one of the formats that dvipdfmx can natively handle.
 * 'input' is the filename of the original file and 'output' is actually
 * temporal files 'generated' by the above routine.   
 * This should be system dependent. (MiKTeX may want something different)
 * Please modify as appropriate (see also pdfximage.c and dvipdfmx.c).
 */
int
dpx_file_apply_filter (const char *cmdtmpl,
                      const char *input, const char *output,
                      unsigned char version)
{
  char   *cmd = NULL;
  const char   *p, *q;
  size_t  n, size;
  int     error = 0;

  if (!cmdtmpl)
    return -1;
  else if (!input || !output)
    return -1;

  size = strlen(cmdtmpl) + strlen(input) + strlen(output) + 3;
  cmd  = NEW(size, char);
  memset(cmd, 0, size);
  for (n = 0, p = cmdtmpl; *p != 0; p++) {
#define need(s,l,m,n) \
if ((l) + (n) >= (m)) { \
  (m) += (n) + 128; \
  (s)  = RENEW((s), (m), char); \
}
    if (p[0] == '%') {
      p++;
      switch (p[0]) {
      case 'o': /* Output file name */
        need(cmd, n, size, strlen(output));
        strcpy(cmd + n, output); n += strlen(output);
        break;
      case 'i': /* Input filename */
        need(cmd, n, size, strlen(input));
        strcpy(cmd + n, input);  n += strlen(input);
        break;
      case 'b':
        need(cmd, n, size, strlen(input));
        q = strrchr(input, '.'); /* wrong */
        if (q) {
          memcpy(cmd + n, input, (int) (q - input));
          n += (int) (q - input);
        } else {
          strcpy(cmd + n, input); n += strlen(input);
        }
        break;
      case  'v': /* Version number, e.g. 1.4 */ {
       char buf[6];
       sprintf(buf, "1.%hu", (unsigned short) version);
       need(cmd, n, size, strlen(buf));
       strcpy(cmd + n, buf);  n += strlen(buf);
       break;
      }
      case  0:
        break;
      case '%':
        need(cmd, n, size, 1);
        cmd[n] = '%'; n++;
        break;
      }
    } else {
      need(cmd, n, size, 1);
      cmd[n] = p[0]; n++;
    }
  }
  need(cmd, n, size, 1);
  cmd[n] = '\0';
  if (strlen(cmd) == 0) {
    RELEASE(cmd);
    return -1;
  }

  error = exec_spawn(cmd);
  if (error)
    WARN("Filtering file via command -->%s<-- failed.", cmd);
  RELEASE(cmd);

  return  error;
}

static char _sbuf[128];
/*
 * SFNT type sigs:
 *  `true' (0x74727565): TrueType (Mac)
 *  `typ1' (0x74797031) (Mac): PostScript font housed in a sfnt wrapper
 *  0x00010000: TrueType (Win)/OpenType
 *  `OTTO': PostScript CFF font with OpenType wrapper
 *  `ttcf': TrueType Collection
 */
static int
istruetype (FILE *fp)
{
  int   n;

  rewind(fp);
  n = fread(_sbuf, 1, 4, fp);
  rewind(fp);

  if (n != 4)
    return  0;
  else if (!memcmp(_sbuf, "true", 4) ||
           !memcmp(_sbuf, "\0\1\0\0", 4)) /* This doesn't help... */
    return  1;
  else if (!memcmp(_sbuf, "ttcf", 4))
    return  1;

  return  0;
}
      
/* "OpenType" is only for ".otf" here */
static int
isopentype (FILE *fp)
{
  int   n;

  rewind(fp);
  n = fread(_sbuf, 1, 4, fp);
  rewind(fp);

  if (n != 4)
    return  0;
  else if (!memcmp(_sbuf, "OTTO", 4))
    return  1;
  else
    return  0;
}

static int
ist1binary (FILE *fp)
{
  char *p;
  int   n;

  rewind(fp);
  n = fread(_sbuf, 1, 21, fp);
  rewind(fp);

  p = _sbuf;
  if (n != 21)
    return  0;
  else if (p[0] != (char) 0x80 || p[1] < 0 || p[1] > 3)
    return  0;
  else if (!memcmp(p + 6, "%!PS-AdobeFont", 14) ||
           !memcmp(p + 6, "%!FontType1", 11))
    return  1;
  else if (!memcmp(p + 6, "%!PS", 4)) {
#if  0
    p[20] = '\0'; p += 6;
    WARN("Ambiguous PostScript resource type: %s", (char *) p);
#endif
    return  1;
  }
  /* Otherwise ambiguious */
  return  0;
}

/* %!PS-Adobe-x.y Resource-CMap */
static int
ispscmap (FILE *fp)
{
  char  *p;
  p = mfgets(_sbuf, 128, fp); p[127] = '\0';
  if (!p || strlen(p) < 4 || memcmp(p, "%!PS", 4))
    return 0;
  for (p += 4; *p && !isspace((unsigned char)*p); p++);
  for ( ; *p && (*p == ' ' || *p == '\t'); p++);
  if (*p == '\0' || strlen(p) < strlen("Resource-CMap"))
    return  0;
  else if (!memcmp(p, "Resource-CMap", strlen("Resource-CMap")))
    return  1;
  /* Otherwise ambiguious */
  return  0;
}

static int
isdfont (FILE *fp)
{
  int i, n;
  unsigned long pos;

  rewind(fp);

  get_unsigned_quad(fp);
  seek_absolute(fp, (pos = get_unsigned_quad(fp)) + 0x18);
  seek_absolute(fp, pos + get_unsigned_pair(fp));
  n = get_unsigned_pair(fp);
  for (i = 0; i <= n; i++) {
    if (get_unsigned_quad(fp) == 0x73666e74UL) /* "sfnt" */
      return 1;
    get_unsigned_quad(fp);
  }
  return 0;
}
      
/* This actually opens files. */
static int
qcheck_filetype (const char *fqpn, dpx_res_type type)
{
  int    r = 1;
  FILE  *fp;
  struct stat sb;

  if (!fqpn)
    return  0;

  if (stat(fqpn, &sb) != 0)
    return 0;

  if (sb.st_size == 0)
    return 0;

  fp = MFOPEN(fqpn, FOPEN_RBIN_MODE);
  if (!fp) {
    WARN("File \"%s\" found but I could not open that...", fqpn);
    return  0;
  }
  switch (type) {
  case DPX_RES_TYPE_T1FONT:
    r = ist1binary(fp);
    break;
  case DPX_RES_TYPE_TTFONT:
    r = istruetype(fp);
    break;
  case DPX_RES_TYPE_OTFONT:
    r = isopentype(fp);
    break;
  case DPX_RES_TYPE_CMAP:
    r = ispscmap(fp);
    break;
  case DPX_RES_TYPE_DFONT:
    r = isdfont(fp);
    break;
  default:
    break;
  }
  MFCLOSE(fp);

  return  r;
}

