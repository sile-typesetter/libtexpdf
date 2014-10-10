#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "system.h"
#include "mem.h"
#include "error.h"
#include "mfileio.h"
#include "numbers.h"
#include "dpxfile.h"

#include <libtexpdf/libtexpdf.h>

fontmap_t *dvi_fontmap = NULL;

/* CIDFont */
static char *strip_options (const char *map_name, fontmap_opt *opt);

static char *
readline (char *buf, int buf_len, FILE *fp)
{
  char  *p, *q;
  ASSERT( buf && buf_len > 0 && fp );
  p = mfgets(buf, buf_len, fp);
  if (!p)
    return  NULL;
  q = strchr(p, '%'); /* we don't have quoted string */
  if (q)
    *q = '\0';
  return  p;
}

/* strdup: just returns NULL for NULL */
static char *
mstrdup (const char *s)
{
  char  *r;
  if (!s)
    return  NULL;
  r = NEW(strlen(s) + 1, char);
  strcpy(r, s);
  return  r;
}

#ifndef ISBLANK
#  define ISBLANK(c) ((c) == ' ' || (c) == '\t')
#endif
static void
skip_blank (const char **pp, const char *endptr)
{
  const char  *p = *pp;
  if (!p || p >= endptr)
    return;
  for ( ; p < endptr && ISBLANK(*p); p++);
  *pp = p;
}

static char *
texpdf_parse_string_value (const char **pp, const char *endptr)
{
  char  *q = NULL;
  const char *p = *pp;
  int    n;

  if (!p || p >= endptr)
    return  NULL;
  if (*p == '"')
    q = texpdf_parse_c_string(&p, endptr);
  else {
    for (n = 0; p < endptr && !isspace((unsigned char)*p); p++, n++);
    if (n == 0)
      return  NULL;
    q = NEW(n + 1, char);
    memcpy(q, *pp, n); q[n] = '\0';
  }

  *pp = p;
  return  q;
}

/* no preceeding spaces allowed */
static char *
texpdf_parse_integer_value (const char **pp, const char *endptr, int base)
{
  char  *q;
  const char *p = *pp;
  int    has_sign = 0, has_prefix = 0, n;

  ASSERT( base == 0 || (base >= 2 && base <= 36) );

  if (!p || p >= endptr)
    return  NULL;

  if (*p == '-' || *p == '+') {
    p++; has_sign = 1;
  }
  if ((base == 0 || base == 16) &&
      p + 2 <= endptr &&
      p[0] == '0' && p[1] == 'x') {
    p += 2; has_prefix = 1;
  }
  if (base == 0) {
    if (has_prefix)
      base = 16;
    else if (p < endptr && *p == '0')
      base = 8;
    else {
      base = 10;
    }
  }
#define ISDIGIT_WB(c,b) ( \
  ((b) <= 10 && (c) >= '0' && (c) < '0' + (b)) || \
  ((b) >  10 && ( \
      ((c) >= '0' && (c) <= '9') || \
      ((c) >= 'a' && (c) < 'a' + ((b) - 10)) || \
      ((c) >= 'A' && (c) < 'A' + ((b) - 10)) \
    ) \
  ) \
)
  for (n = 0; p < endptr && ISDIGIT_WB(*p, base); p++, n++);
  if (n == 0)
    return  NULL;
  if (has_sign)
    n += 1;
  if (has_prefix)
    n += 2;

  q = NEW(n + 1, char);
  memcpy(q, *pp, n); q[n] = '\0';

  *pp = p;
  return  q;
}

static void
hval_free (void *vp)
{
  fontmap_rec *mrec = (fontmap_rec *) vp;
  texpdf_clear_fontmap_record(mrec);
  RELEASE(mrec);
}

void
dvipdf_init_fontmaps (void)
{
  dvi_fontmap = NEW(1, struct ht_table);
  texpdf_ht_init_table(dvi_fontmap, hval_free);
}

static int
fontmap_parse_mapdef_dpm (fontmap_rec *mrec,
                          const char *mapdef, const char *endptr)
{
  const char  *p = mapdef;

  /*
   * Parse record line in map file.  First two fields (after TeX font
   * name) are position specific.  Arguments start at the first token
   * beginning with a  '-'.
   *
   * NOTE:
   *   Dvipdfm basically uses texpdf_parse_ident() for parsing enc_name,
   *   font_name, and other string values which assumes PostScript-like
   *   syntax.
   *   texpdf_skip_white() skips '\r' and '\n' but they should terminate
   *   fontmap line here.
   */

  skip_blank(&p, endptr);
  /* encoding field */
  if (p < endptr && *p != '-') { /* May be NULL */
    mrec->enc_name = texpdf_parse_string_value(&p, endptr);
    skip_blank(&p, endptr);
  }

  /* fontname or font filename field */
  if (p < endptr && *p != '-') { /* May be NULL */
    mrec->font_name = texpdf_parse_string_value(&p, endptr);
    skip_blank(&p, endptr);
  }
  if (mrec->font_name) {
    char  *tmp;
    /* Several options are encoded in font_name for
     * compatibility with dvipdfm.
     */
    tmp = strip_options(mrec->font_name, &mrec->opt);
    if (tmp) {
      RELEASE(mrec->font_name);
      mrec->font_name = tmp;
    }
  }

  skip_blank(&p, endptr);
  /* Parse any remaining arguments */
  while (p + 1 < endptr &&
         *p != '\r' && *p != '\n' && *p == '-') {
    char  *q, mopt = p[1];
    long   v;

    p += 2; skip_blank(&p, endptr);
    switch (mopt) {

    case  's': /* Slant option */
      q = texpdf_parse_float_decimal(&p, endptr);
      if (!q) {
        WARN("Missing a number value for 's' option.");
        return  -1;
      }
      mrec->opt.slant = atof(q);
      RELEASE(q);
      break;

    case  'e': /* Extend option */
      q = texpdf_parse_float_decimal(&p, endptr);
      if (!q) {
        WARN("Missing a number value for 'e' option.");
        return  -1;
      }
      mrec->opt.extend = atof(q);
      if (mrec->opt.extend <= 0.0) {
        WARN("Invalid value for 'e' option: %s", q);
        return  -1;
      }
      RELEASE(q);
      break;

    case  'b': /* Fake-bold option */
      q = texpdf_parse_float_decimal(&p, endptr);
      if (!q) {
        WARN("Missing a number value for 'b' option.");
        return  -1;
      }
      mrec->opt.bold = atof(q);
      if (mrec->opt.bold <= 0.0) {
        WARN("Invalid value for 'b' option: %s", q);
        return  -1;
      }
      RELEASE(q);
      break;

    case  'r': /* Remap option; obsolete; just ignore */
      break;

    case  'i':  /* TTC index */
      q = texpdf_parse_integer_value(&p, endptr, 10);
      if (!q) {
        WARN("Missing TTC index number...");
        return  -1;
      }
      mrec->opt.index = atoi(q);
      if (mrec->opt.index < 0) {
        WARN("Invalid TTC index number: %s", q);
        return  -1;
      }
      RELEASE(q);
      break;

    case  'p': /* UCS plane: just for testing */
      q = texpdf_parse_integer_value(&p, endptr, 0);
      if (!q) {
        WARN("Missing a number for 'p' option.");
        return  -1;
      }
      v = strtol(q, NULL, 0);
      if (v < 0 || v > 16)
        WARN("Invalid value for option 'p': %s", q);
      else {
        mrec->opt.mapc = v << 16;
      }
      RELEASE(q);
      break;

    case  'u': /* ToUnicode */
      q = texpdf_parse_string_value(&p, endptr);
      if (q)
        mrec->opt.tounicode = q;
      else {
        WARN("Missing string value for option 'u'.");
        return  -1;
      }
      break;

    case  'v': /* StemV */
      q = texpdf_parse_integer_value(&p, endptr, 10);
      if (!q) {
        WARN("Missing a number for 'v' option.");
        return  -1;
      }
      mrec->opt.stemv = strtol(q, NULL, 0);
      RELEASE(q);
      break;

    /* Omega uses both single-byte and double-byte set_char command
     * even for double-byte OFMs. This confuses CMap decoder.
     */
    case  'm':
      /* Map single bytes char 0xab to double byte char 0xcdab  */
      if (p + 4 <= endptr &&
          p[0] == '<' && p[3] == '>') {
        p++;
        q = texpdf_parse_integer_value(&p, endptr, 16);
        if (!q) {
          WARN("Invalid value for option 'm'.");
          return  -1;
        } else if (p < endptr && *p != '>') {
          WARN("Invalid value for option 'm': %s", q);
          RELEASE(q);
          return  -1;
        }
        v = strtol(q, NULL, 16);
        mrec->opt.mapc = ((v << 8) & 0x0000ff00L);
        RELEASE(q); p++;
      } else if (p + 4 <= endptr &&
                 !memcmp(p, "sfd:", strlen("sfd:"))) {
        char  *r;
        const char  *rr;
        /* SFD mapping: sfd:Big5,00 */
        p += 4; skip_blank(&p, endptr);
        q  = texpdf_parse_string_value(&p, endptr);
        if (!q) {
          WARN("Missing value for option 'm'.");
          return  -1;
        }
        r  = strchr(q, ',');
        if (!r) {
          WARN("Invalid value for option 'm': %s", q);
          RELEASE(q);
          return  -1;
        }
        *r = 0; rr = ++r; skip_blank(&rr, r + strlen(r));
        if (*rr == '\0') {
          WARN("Invalid value for option 'm': %s,", q);
          RELEASE(q);
          return  -1;
        }
        mrec->charmap.sfd_name   = mstrdup(q);
        mrec->charmap.subfont_id = mstrdup(rr);
        RELEASE(q);
      } else if (p + 4 < endptr &&
                 !memcmp(p, "pad:", strlen("pad:"))) {
        p += 4; skip_blank(&p, endptr);
        q  = texpdf_parse_integer_value(&p, endptr, 16);
        if (!q) {
          WARN("Invalid value for option 'm'.");
          return  -1;
        } else if (p < endptr && !isspace((unsigned char)*p)) {
          WARN("Invalid value for option 'm': %s", q);
          RELEASE(q);
          return  -1;
        }
        v = strtol(q, NULL, 16);
        mrec->opt.mapc = ((v << 8) & 0x0000ff00L);
        RELEASE(q);
      } else {
        WARN("Invalid value for option 'm'.");
        return  -1;
      }
      break;

    case 'w': /* Writing mode (for unicode encoding) */
      if (!mrec->enc_name ||
           strcmp(mrec->enc_name, "unicode")) {
        WARN("Fontmap option 'w' meaningless for encoding other than \"unicode\".");
        return  -1;
      }
      q  = texpdf_parse_integer_value(&p, endptr, 10);
      if (!q) {
        WARN("Missing wmode value...");
        return  -1;
      }
      if (atoi(q) == 1)
        mrec->opt.flags |= FONTMAP_OPT_VERT;
      else if (atoi(q) == 0)
        mrec->opt.flags &= ~FONTMAP_OPT_VERT;
      else {
        WARN("Invalid value for option 'w': %s", q);
      }
      RELEASE(q);
      break;

    default:
      WARN("Unrecognized font map option: '%c'", mopt);
      return  -1;
    }
    skip_blank(&p, endptr);
  }

  if (p < endptr && *p != '\r' && *p != '\n') {
    WARN("Invalid char in fontmap line: %c", *p);
    return  -1;
  }

  return  0;
}


/* Parse record line in map file of DVIPS/pdfTeX format. */
static int
fontmap_parse_mapdef_dps (fontmap_rec *mrec,
                          const char *mapdef, const char *endptr)
{
  const char *p = mapdef;
  char *q;

  skip_blank(&p, endptr);

  /* The first field (after TFM name) must be PostScript name. */
  /* However, pdftex.map allows a line without PostScript name. */

  if (*p != '"' && *p != '<') {
    if (p < endptr) {
      q = texpdf_parse_string_value(&p, endptr);
      if (q) RELEASE(q);
      skip_blank(&p, endptr);
    } else {
      WARN("Missing a PostScript font name.");
      return -1;
    }
  }

  if (p >= endptr) return 0;

  /* Parse any remaining arguments */
  while (p < endptr && *p != '\r' && *p != '\n' && (*p == '<' || *p == '"')) {
    switch (*p) {
    case '<': /* encoding or fontfile field */
      /* If we see <[ or <<, just ignore the second char instead
         of doing as directed (define encoding file, fully embed); sorry.  */
      if (++p < endptr && (*p == '[' || *p == '<')) p++; /*skip */
      skip_blank(&p, endptr);
      if ((q = texpdf_parse_string_value(&p, endptr))) {
        int n = strlen(q);
        if (n > 4 && strncmp(q+n-4, ".enc", 4) == 0)
          mrec->enc_name = q;
        else
          mrec->font_name = q;
      }
      skip_blank(&p, endptr);
      break;

    case '"': /* Options */
      if ((q = texpdf_parse_string_value(&p, endptr))) {
        const char *r = q, *e = q+strlen(q);
        char *s, *t;
        skip_blank(&r, e);
        while (r < e) {
          if ((s = texpdf_parse_float_decimal(&r, e))) {
            skip_blank(&r, e);
            if ((t = texpdf_parse_string_value(&r, e))) {
              if (strcmp(t, "SlantFont") == 0)
                mrec->opt.slant = atof(s);
              else if (strcmp(t, "ExtendFont") == 0)
                mrec->opt.extend = atof(s);
              RELEASE(t);
            }
            RELEASE(s);
          } else if ((s = texpdf_parse_string_value(&r, e))) { /* skip */
            RELEASE(s);
          }
          skip_blank(&r, e);
        }
        RELEASE(q);
      }
      skip_blank(&p, endptr);
      break;
    
    default:
      WARN("Found an invalid entry: %s", p);
      return -1;
    }
    skip_blank(&p, endptr);
  }

  if (p < endptr && *p != '\r' && *p != '\n') {
    WARN("Invalid char in fontmap line: %c", *p);
    return -1;
  }

  return  0;
}

#define fontmap_invalid(m) (!(m) || !(m)->map_name || !(m)->font_name)

/* "foo@A@ ..." is expanded to
 *   fooab ... -m sfd:A,ab
 *   ...
 *   fooyz ... -m sfd:A,yz
 * where 'ab' ... 'yz' is subfont IDs in SFD 'A'.
 */
int
texpdf_append_fontmap_record (const char *kp, const fontmap_rec *vp)
{
  fontmap_rec *mrec;
  char        *fnt_name, *sfd_name = NULL;

  if (!kp || fontmap_invalid(vp)) {
    WARN("Invalid fontmap record...");
    return -1;
  }

  if (texpdf_fontmap_get_verbose() > 3)
    MESG("fontmap>> append key=\"%s\"...", kp);

  fnt_name = texpdf_chop_sfd_name(kp, &sfd_name);
  if (fnt_name && sfd_name) {
    char  *tfm_name;
    char **subfont_ids;
    int    n = 0;
    subfont_ids = sfd_get_subfont_ids(sfd_name, &n);
    if (!subfont_ids)
      return  -1;
    while (n-- > 0) {
      tfm_name = texpdf_make_subfont_name(kp, sfd_name, subfont_ids[n]);
      if (!tfm_name)
        continue;
      mrec = texpdf_ht_lookup_table(dvi_fontmap, tfm_name, strlen(tfm_name));
      if (!mrec) {
        mrec = NEW(1, fontmap_rec);
        texpdf_init_fontmap_record(mrec);
        mrec->map_name = mstrdup(kp); /* link */
        mrec->charmap.sfd_name   = mstrdup(sfd_name);
        mrec->charmap.subfont_id = mstrdup(subfont_ids[n]);
        ht_insert_table(dvi_fontmap, tfm_name, strlen(tfm_name), mrec);
      }
      RELEASE(tfm_name);
    }
    RELEASE(fnt_name);
    RELEASE(sfd_name);
  }

  mrec = texpdf_ht_lookup_table(dvi_fontmap, kp, strlen(kp));
  if (!mrec) {
    mrec = NEW(1, fontmap_rec);
    texpdf_copy_fontmap_record(mrec, vp);
    if (mrec->map_name && !strcmp(kp, mrec->map_name)) {
      RELEASE(mrec->map_name);
      mrec->map_name = NULL;
    }
    ht_insert_table(dvi_fontmap, kp, strlen(kp), mrec);
  }
  if (texpdf_fontmap_get_verbose() > 3)
    MESG("\n");

  return  0;
}

int
texpdf_remove_fontmap_record (const char *kp)
{
  char  *fnt_name, *sfd_name = NULL;

  if (!kp)
    return  -1;

  if (texpdf_fontmap_get_verbose() > 3)
    MESG("fontmap>> remove key=\"%s\"...", kp);

  fnt_name = texpdf_chop_sfd_name(kp, &sfd_name);
  if (fnt_name && sfd_name) {
    char  *tfm_name;
    char **subfont_ids;
    int    n = 0;
    subfont_ids = sfd_get_subfont_ids(sfd_name, &n);
    if (!subfont_ids)
      return  -1;
    if (texpdf_fontmap_get_verbose() > 3)
      MESG("\nfontmap>> Expand @%s@:", sfd_name);
    while (n-- > 0) {
      tfm_name = texpdf_make_subfont_name(kp, sfd_name, subfont_ids[n]);
      if (!tfm_name)
        continue;
      if (texpdf_fontmap_get_verbose() > 3)
        MESG(" %s", tfm_name);
      ht_remove_table(dvi_fontmap, tfm_name, strlen(tfm_name));
      RELEASE(tfm_name);
    }
    RELEASE(fnt_name);
    RELEASE(sfd_name);
  }

  ht_remove_table(dvi_fontmap, kp, strlen(kp));

  if (texpdf_fontmap_get_verbose() > 3)
    MESG("\n");

  return  0;
}

int
texpdf_read_fontmap_line (fontmap_rec *mrec, const char *mline, long mline_len, int format)
{
  int    error;
  char  *q;
  const char *p, *endptr;

  ASSERT(mrec);

  p      = mline;
  endptr = p + mline_len;

  skip_blank(&p, endptr);
  if (p >= endptr)
    return -1;

  q = texpdf_parse_string_value(&p, endptr);
  if (!q)
    return -1;

  if (format > 0) /* DVIPDFM format */
    error = fontmap_parse_mapdef_dpm(mrec, p, endptr);
  else /* DVIPS/pdfTeX format */
    error = fontmap_parse_mapdef_dps(mrec, p, endptr);
  if (!error) {
    char  *fnt_name, *sfd_name = NULL;
    fnt_name = texpdf_chop_sfd_name(q, &sfd_name);
    if (fnt_name && sfd_name) {
      if (!mrec->font_name) {
      /* In the case of subfonts, the base name (before the character '@')
       * will be used as a font_name by default.
       * Otherwise tex_name will be used as a font_name by default.
       */
        mrec->font_name = fnt_name;
      } else {
        RELEASE(fnt_name);
      }
      if (mrec->charmap.sfd_name)
        RELEASE(mrec->charmap.sfd_name);
      mrec->charmap.sfd_name = sfd_name ;
    }
    pdftex_fill_in_defaults(mrec, q);
  }
  RELEASE(q);

  return  error;
}

/* DVIPS/pdfTeX fontmap line if one of the following three cases found:
 *
 * (1) any line including the character '"'
 * (2) any line including the character '<'
 * (3) if the line consists of two entries (tfmname and psname)
 *
 * DVIPDFM fontmap line otherwise.
 */
int
texpdf_is_pdfm_mapline (const char *mline) /* NULL terminated. */
{
  int   n = 0;
  const char *p, *endptr;

  if (strchr(mline, '"') || strchr(mline, '<'))
    return -1; /* DVIPS/pdfTeX format */

  p      = mline;
  endptr = p + strlen(mline);

  skip_blank(&p, endptr);

  while (p < endptr) {
    /* Break if '-' preceeded by blanks is found. (DVIPDFM format) */
    if (*p == '-') return 1;
    for (n++; p < endptr && !ISBLANK(*p); p++);
    skip_blank(&p, endptr);
  }

  /* Two entries: TFM_NAME PS_NAME only (DVIPS format)
   * Otherwise (DVIPDFM format) */
  return (n == 2 ? 0 : 1);
}

int
texpdf_load_fontmap_file (const char *filename, int mode)
{
  fontmap_rec *mrec;
  FILE        *fp;
  const char  *p = NULL, *endptr;
  long         llen, lpos  = 0;
  int          error = 0, format = 0;

  ASSERT(filename);
  ASSERT(dvi_fontmap) ;

  if (texpdf_fontmap_get_verbose())
    MESG("<FONTMAP:");
  fp = DPXFOPEN(filename, DPX_RES_TYPE_FONTMAP); /* outputs path if texpdf_fontmap_get_verbose() */
  if (!fp) {
    WARN("Couldn't open font map file \"%s\".", filename);
    return  -1;
  }
  
  while (!error &&
         (p = readline(work_buffer, WORK_BUFFER_SIZE, fp)) != NULL) {
    int m;

    lpos++;
    llen   = strlen(work_buffer);
    endptr = p + llen;

    skip_blank(&p, endptr);
    if (p == endptr)
      continue;

    m = texpdf_is_pdfm_mapline(p);

    if (format * m < 0) { /* mismatch */
      WARN("Found a mismatched fontmap line %d from %s.", lpos, filename);
      WARN("-- Ignore the current input buffer: %s", p);
      continue;
    } else
      format += m;

    mrec  = NEW(1, fontmap_rec);
    texpdf_init_fontmap_record(mrec);

    /* format > 0: DVIPDFM, format <= 0: DVIPS/pdfTeX */
    error = texpdf_read_fontmap_line(mrec, p, llen, format);
    if (error) {
      WARN("Invalid map record in fontmap line %d from %s.", lpos, filename);
      WARN("-- Ignore the current input buffer: %s", p);
      texpdf_clear_fontmap_record(mrec);
      RELEASE(mrec);
      continue;
    } else {
      switch (mode) {
      case FONTMAP_RMODE_REPLACE:
        texpdf_insert_fontmap_record(dvi_fontmap, mrec->map_name, mrec);
        break;
      case FONTMAP_RMODE_APPEND:
        texpdf_append_fontmap_record(mrec->map_name, mrec);
        break;
      case FONTMAP_RMODE_REMOVE:
        texpdf_remove_fontmap_record(mrec->map_name);
        break;
      }
    }
    texpdf_clear_fontmap_record(mrec);
    RELEASE(mrec);
  }
  DPXFCLOSE(fp);

  if (texpdf_fontmap_get_verbose())
    MESG(">");

  return  error;
}

static char *
substr (const char **str, char stop)
{
  char *sstr;
  const char *endptr;

  endptr = strchr(*str, stop);
  if (!endptr || endptr == *str)
    return NULL;
  sstr = NEW(endptr-(*str)+1, char);
  memcpy(sstr, *str, endptr-(*str));
  sstr[endptr-(*str)] = '\0';

  *str = endptr+1;
  return sstr;
}

#include <ctype.h>
#define CID_MAPREC_CSI_DELIM '/'

static char *
strip_options (const char *map_name, fontmap_opt *opt)
{
  char *font_name;
  const char *p;
  char *next = NULL;
  int   have_csi = 0, have_style = 0;

  ASSERT(opt);

  p = map_name;
  font_name      = NULL;
  opt->charcoll  = NULL;
  opt->index     = 0;
  opt->style     = FONTMAP_STYLE_NONE;
  opt->flags     = 0;

  if (*p == ':' && isdigit((unsigned char)*(p+1))) {
    opt->index = (int) strtoul(p+1, &next, 10);
    if (*next == ':')
      p = next + 1;
    else {
      opt->index = 0;
    }
  }
  if (*p == '!') { /* no-embedding */
    if (*(++p) == '\0')
      ERROR("Invalid map record: %s (--> %s)", map_name, p);
    opt->flags |= FONTMAP_OPT_NOEMBED;
  }

  if ((next = strchr(p, CID_MAPREC_CSI_DELIM)) != NULL) {
    if (next == p)
      ERROR("Invalid map record: %s (--> %s)", map_name, p);
    font_name = substr(&p, CID_MAPREC_CSI_DELIM);
    have_csi  = 1;
  } else if ((next = strchr(p, ',')) != NULL) {
    if (next == p)
      ERROR("Invalid map record: %s (--> %s)", map_name, p);
    font_name = substr(&p, ',');
    have_style = 1;
  } else {
    font_name = NEW(strlen(p)+1, char);
    strcpy(font_name, p);
  }

  if (have_csi) {
    if ((next = strchr(p, ',')) != NULL) {
      opt->charcoll = substr(&p, ',');
      have_style = 1;
    } else if (p[0] == '\0') {
      ERROR("Invalid map record: %s.", map_name);
    } else {
      opt->charcoll = NEW(strlen(p)+1, char);
      strcpy(opt->charcoll, p);
    }
  }

  if (have_style) {
    if (!strncmp(p, "BoldItalic", 10)) {
      if (*(p+10))
        ERROR("Invalid map record: %s (--> %s)", map_name, p);
      opt->style = FONTMAP_STYLE_BOLDITALIC;
    } else if (!strncmp(p, "Bold", 4)) {
      if (*(p+4))
        ERROR("Invalid map record: %s (--> %s)", map_name, p);
      opt->style = FONTMAP_STYLE_BOLD;
    } else if (!strncmp(p, "Italic", 6)) {
      if (*(p+6))
        ERROR("Invalid map record: %s (--> %s)", map_name, p);
      opt->style = FONTMAP_STYLE_ITALIC;
    }
  }

  return font_name;
}

#if 0
void
pdf_clear_fontmaps (void)
{
  texpdf_close_fontmaps();
  texpdf_init_fontmaps();
}
#endif

/* CIDFont options
 *
 * FORMAT:
 *
 *   (:int:)?!?string(/string)?(,string)?
 */

#if  DPXTEST
static void
texpdf_dump_fontmap_rec (const char *key, const fontmap_rec *mrec)
{
  fontmap_opt *opt = (fontmap_opt *) &mrec->opt;

  if (mrec->map_name)
    fprintf(stdout, "  <!-- subfont");
  else
    fprintf(stdout, "  <insert");
  fprintf(stdout, " id=\"%s\"", key);
  if (mrec->map_name)
    fprintf(stdout, " map-name=\"%s\"", mrec->map_name);
  if (mrec->enc_name)
    fprintf(stdout, " enc-name=\"%s\"",  mrec->enc_name);
  if (mrec->font_name)
    fprintf(stdout, " font-name=\"%s\"", mrec->font_name);
  if (mrec->charmap.sfd_name && mrec->charmap.subfont_id) {
    fprintf(stdout, " charmap=\"sfd:%s,%s\"",
            mrec->charmap.sfd_name, mrec->charmap.subfont_id);
  }
  if (opt->slant != 0.0)
    fprintf(stdout, " font-slant=\"%g\"", opt->slant);
  if (opt->extend != 1.0)
    fprintf(stdout, " font-extend=\"%g\"", opt->extend);
  if (opt->charcoll)
    fprintf(stdout, " glyph-order=\"%s\"", opt->charcoll);
  if (opt->tounicode)
    fprintf(stdout, " tounicode=\"%s\"", opt->tounicode);
  if (opt->index != 0)
    fprintf(stdout, " ttc-index=\"%d\"", opt->index);
  if (opt->flags & FONTMAP_OPT_NOEMBED)
    fprintf(stdout, " embedding=\"no\"");
  if (opt->mapc >= 0) {
    fprintf(stdout, " charmap=\"pad:");
    if (opt->mapc > 0xffff)
      fprintf(stdout, "%02x %02x", (opt->mapc >> 16) & 0xff, (opt->mapc >> 8) & 0xff);
    else
      fprintf(stdout, "%02x", (opt->mapc >> 8) & 0xff);
    fprintf(stdout, "\"");
  }
  if (opt->flags & FONTMAP_OPT_VERT)
    fprintf(stdout, " writing-mode=\"vertical\"");
  if (opt->style != FONTMAP_STYLE_NONE) {
    fprintf(stdout, " font-style=\"");
    switch (opt->style) {
    case FONTMAP_STYLE_BOLD:
      fprintf(stdout, "bold");
      break;
    case FONTMAP_STYLE_ITALIC:
      fprintf(stdout, "italic");
      break;
    case FONTMAP_STYLE_BOLDITALIC:
      fprintf(stdout, "bolditalic");
      break;
    }
    fprintf(stdout, "\"");
  }
  if (mrec->map_name)
    fprintf(stdout, " / -->\n");
  else
    fprintf(stdout, " />\n");
}

void
texpdf_dump_fontmaps (void)
{
  struct ht_iter iter;
  fontmap_rec   *mrec;
  char           key[128], *kp;
  int            kl;

  if (!dvi_fontmap)
    return;

  fprintf(stdout, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(stdout, "<!DOCTYPE fontmap SYSTEM \"fontmap.dtd\">\n");
  fprintf(stdout, "<fontmap id=\"%s\">\n", "foo");
  if (ht_set_iter(dvi_fontmap, &iter) == 0) {
    do {
      kp   = ht_iter_getkey(&iter, &kl);
      mrec = ht_iter_getval(&iter);
      if (kl > 127)
        continue;
      memcpy(key, kp, kl); key[kl] = 0;
      texpdf_dump_fontmap_rec(key, mrec);
    } while (!ht_iter_next(&iter));
  }
  ht_clear_iter(&iter);
  fprintf(stdout, "</fontmap>\n");

  return;
}

void
test_fontmap_help (void)
{
  fprintf(stdout, "usage: fontmap [options] [mapfile...]\n");
  fprintf(stdout, "-l, --lookup string\n");
  fprintf(stdout, "  Lookup fontmap entry for 'string' after loading mapfile(s).\n");
}

int
test_fontmap_main (int argc, char *argv[])
{
  int    i;
  char  *key = NULL;

  for (;;) {
    int  c, optidx = 0;
    static struct option long_options[] = {
      {"lookup", 1, 0, 'l'},
      {"help",   0, 0, 'h'},
      {0, 0, 0, 0}
    };
    c = getopt_long(argc, argv, "l:h", long_options, &optidx);
    if (c == -1)
      break;

    switch (c) {
    case  'l':
      key = optarg;
      break;
    case  'h':
      test_fontmap_help();
      return  0;
      break;
    default:
      test_fontmap_help();
      return  -1;
      break;
    }
  }

  texpdf_init_fontmaps();
  for (i = optind; i < argc; i++)
    texpdf_load_fontmap_file(argv[i], FONTMAP_RMODE_REPLACE);

  if (key == NULL)
    texpdf_dump_fontmaps();
  else {
    fontmap_rec *mrec;
    mrec = texpdf_lookup_fontmap_record(key);
    if (mrec)
      texpdf_dump_fontmap_rec(key, mrec);
    else {
      WARN("Fontmap entry \"%s\" not found.", key);
    }
  }
  texpdf_close_fontmaps();

  return  0;
}
#endif /* DPXTEST */
