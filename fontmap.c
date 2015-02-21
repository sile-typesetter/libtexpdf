/* This is dvipdfmx, an eXtended version of dvipdfm by Mark A. Wicks.

    Copyright (C) 2002-2014 by Jin-Hwan Cho and Shunsaku Hirata,
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "mem.h"
#include "error.h"

#include "dpxfile.h"
#include "dpxutil.h"

#include "subfont.h"

#include "fontmap.h"

static int verbose = 0;
void
texpdf_fontmap_set_verbose (void)
{
  verbose++;
}

int
texpdf_fontmap_get_verbose (void)
{
  return verbose;
}

void
texpdf_init_fontmap_record (fontmap_rec *mrec) 
{
  ASSERT(mrec);

  mrec->map_name   = NULL;

  /* SFD char mapping */
  mrec->charmap.sfd_name   = NULL;
  mrec->charmap.subfont_id = NULL;
  /* for OFM */
  mrec->opt.mapc   = -1; /* compatibility */

  mrec->font_name  = NULL;
  mrec->enc_name   = NULL;

  mrec->opt.slant  = 0.0;
  mrec->opt.extend = 1.0;
  mrec->opt.bold   = 0.0;

  mrec->opt.flags  = 0;

  mrec->opt.design_size = -1.0;

  mrec->opt.tounicode = NULL;
  mrec->opt.otl_tags  = NULL; /* deactivated */
  mrec->opt.index     = 0;
  mrec->opt.charcoll  = NULL;
  mrec->opt.style     = FONTMAP_STYLE_NONE;
  mrec->opt.stemv     = -1; /* not given explicitly by an option */

  mrec->opt.cff_charsets = NULL;
}

void
texpdf_clear_fontmap_record (fontmap_rec *mrec)
{
  ASSERT(mrec);

  if (mrec->map_name)
    RELEASE(mrec->map_name);
  if (mrec->charmap.sfd_name)
    RELEASE(mrec->charmap.sfd_name);
  if (mrec->charmap.subfont_id)
    RELEASE(mrec->charmap.subfont_id);
  if (mrec->enc_name)
    RELEASE(mrec->enc_name);
  if (mrec->font_name)
    RELEASE(mrec->font_name);

  if (mrec->opt.tounicode)
    RELEASE(mrec->opt.tounicode);
  if (mrec->opt.otl_tags)
    RELEASE(mrec->opt.otl_tags);
  if (mrec->opt.charcoll)
    RELEASE(mrec->opt.charcoll);
  texpdf_init_fontmap_record(mrec);
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

void
texpdf_copy_fontmap_record (fontmap_rec *dst, const fontmap_rec *src)
{
  ASSERT( dst && src );

  dst->map_name   = mstrdup(src->map_name);

  dst->charmap.sfd_name   = mstrdup(src->charmap.sfd_name);
  dst->charmap.subfont_id = mstrdup(src->charmap.subfont_id);

  dst->font_name  = mstrdup(src->font_name);
  dst->enc_name   = mstrdup(src->enc_name);

  dst->opt.slant  = src->opt.slant;
  dst->opt.extend = src->opt.extend;
  dst->opt.bold   = src->opt.bold;

  dst->opt.flags  = src->opt.flags;
  dst->opt.mapc   = src->opt.mapc;

  dst->opt.tounicode = mstrdup(src->opt.tounicode);
  dst->opt.otl_tags  = mstrdup(src->opt.otl_tags);
  dst->opt.index     = src->opt.index;
  dst->opt.charcoll  = mstrdup(src->opt.charcoll);
  dst->opt.style     = src->opt.style;
  dst->opt.stemv     = src->opt.stemv;

  dst->opt.cff_charsets = src->opt.cff_charsets;
}


static void
hval_free (void *vp)
{
  fontmap_rec *mrec = (fontmap_rec *) vp;
  texpdf_clear_fontmap_record(mrec);
  RELEASE(mrec);
}

void
pdftex_fill_in_defaults (fontmap_rec *mrec, const char *tex_name)
{
  if (mrec->enc_name &&
      (!strcmp(mrec->enc_name, "default") ||
       !strcmp(mrec->enc_name, "none"))) {
    RELEASE(mrec->enc_name);
    mrec->enc_name = NULL;
  }
  if (mrec->font_name && 
      (!strcmp(mrec->font_name, "default") ||
       !strcmp(mrec->font_name, "none"))) {
    RELEASE(mrec->font_name);
    mrec->font_name = NULL;
  }
  /* We *must* fill font_name either explicitly or by default */
  if (!mrec->font_name) {
    mrec->font_name = NEW(strlen(tex_name)+1, char);
    strcpy(mrec->font_name, tex_name);
  }

  mrec->map_name = NEW(strlen(tex_name)+1, char);
  strcpy(mrec->map_name, tex_name);

#ifndef WITHOUT_COMPAT
  /* Use "UCS" character collection for Unicode SFD
   * and Identity CMap combination. For backward
   * compatibility.
   */
  if (mrec->charmap.sfd_name && mrec->enc_name &&
      !mrec->opt.charcoll) {
    if ((!strcmp(mrec->enc_name, "Identity-H") ||
         !strcmp(mrec->enc_name, "Identity-V"))
          &&
         (strstr(mrec->charmap.sfd_name, "Uni")  ||
          strstr(mrec->charmap.sfd_name, "UBig") ||
          strstr(mrec->charmap.sfd_name, "UBg")  ||
          strstr(mrec->charmap.sfd_name, "UGB")  ||
          strstr(mrec->charmap.sfd_name, "UKS")  ||
          strstr(mrec->charmap.sfd_name, "UJIS"))) {
      mrec->opt.charcoll = NEW(strlen("UCS")+1, char);
      strcpy(mrec->opt.charcoll, "UCS");
    }
  }
#endif /* WITHOUT_COMPAT */

  return;
}

fontmap_t *native_fontmap = NULL;

#define fontmap_invalid(m) (!(m) || !(m)->map_name || !(m)->font_name)
char *
texpdf_chop_sfd_name (const char *tex_name, char **sfd_name)
{
  char  *fontname;
  char  *p, *q;
  int    m, n, len;

  *sfd_name = NULL;

  p = strchr(tex_name, '@');
  if (!p ||
      p[1] == '\0' || p == tex_name) {
    return  NULL;
  }
  m = (int) (p - tex_name);
  p++;
  q = strchr(p, '@');
  if (!q || q == p) {
    return NULL;
  }
  n = (int) (q - p);
  q++;

  len = strlen(tex_name) - n;
  fontname = NEW(len+1, char);
  memcpy(fontname, tex_name, m);
  fontname[m] = '\0';
  if (*q)
    strcat(fontname, q);

  *sfd_name = NEW(n+1, char);
  memcpy(*sfd_name, p, n);
  (*sfd_name)[n] = '\0';

  return  fontname;
}

char *
texpdf_make_subfont_name (const char *map_name, const char *sfd_name, const char *sub_id)
{
  char  *tfm_name;
  int    n, m;
  char  *p, *q;

  p = strchr(map_name, '@');
  if (!p || p == map_name)
    return  NULL;
  m = (int) (p - map_name);
  q = strchr(p + 1, '@');
  if (!q || q == p + 1)
    return  NULL;
  n = (int) (q - p) + 1; /* including two '@' */
  if (strlen(sfd_name) != n - 2 ||
      memcmp(p + 1, sfd_name, n - 2))
    return  NULL;
  tfm_name = NEW(strlen(map_name) - n + strlen(sub_id) + 1, char);
  memcpy(tfm_name, map_name, m);
  tfm_name[m] = '\0';
  strcat(tfm_name, sub_id);
  if (q[1]) /* not ending with '@' */
    strcat(tfm_name, q + 1);

  return  tfm_name;
}

int
texpdf_insert_fontmap_record (fontmap_t* map, const char *kp, const fontmap_rec *vp)
{
  fontmap_rec *mrec;
  char        *fnt_name, *sfd_name;

  if (!kp || fontmap_invalid(vp)) {
    WARN("Invalid fontmap record...");
    return -1;
  }

  if (verbose > 3)
    MESG("fontmap>> insert key=\"%s\"...", kp);

  fnt_name = texpdf_chop_sfd_name(kp, &sfd_name);
  if (fnt_name && sfd_name) {
    char  *tfm_name;
    char **subfont_ids;
    int    n = 0;
    subfont_ids = sfd_get_subfont_ids(sfd_name, &n);
    if (!subfont_ids) {
      RELEASE(fnt_name);
      RELEASE(sfd_name);
      return  -1;
    }
    if (verbose > 3)
      MESG("\nfontmap>> Expand @%s@:", sfd_name);
    while (n-- > 0) {
      tfm_name = texpdf_make_subfont_name(kp, sfd_name, subfont_ids[n]);
      if (!tfm_name)
        continue;
      if (verbose > 3)
        MESG(" %s", tfm_name);
      mrec = NEW(1, fontmap_rec);
      texpdf_init_fontmap_record(mrec);
      mrec->map_name = mstrdup(kp); /* link to this entry */
      mrec->charmap.sfd_name   = mstrdup(sfd_name);
      mrec->charmap.subfont_id = mstrdup(subfont_ids[n]);
      ht_insert_table(map, tfm_name, strlen(tfm_name), mrec);
      RELEASE(tfm_name);
    }
    RELEASE(fnt_name);
    RELEASE(sfd_name);
  }

  mrec = NEW(1, fontmap_rec);
  texpdf_copy_fontmap_record(mrec, vp);
  if (mrec->map_name && !strcmp(kp, mrec->map_name)) {
    RELEASE(mrec->map_name);
    mrec->map_name = NULL;
  }
  ht_insert_table(map, kp, strlen(kp), mrec);

  if (verbose > 3)
    MESG("\n");

  return  0;
}

#ifdef XETEX
int
texpdf_insert_native_fontmap_record (const char *path, uint32_t index,
                                  int layout_dir, int extend, int slant, int embolden)
{
  char        *fontmap_key;
  fontmap_rec *mrec;

  ASSERT(path);

  fontmap_key = malloc(strlen(path) + 40);	// CHECK
  sprintf(fontmap_key, "%s/%d/%c/%d/%d/%d", path, index, layout_dir == 0 ? 'H' : 'V', extend, slant, embolden);

  if (verbose)
    MESG("<NATIVE-FONTMAP:%s", fontmap_key);

  mrec  = NEW(1, fontmap_rec);
  texpdf_init_fontmap_record(mrec);

  mrec->map_name  = fontmap_key;
  mrec->enc_name  = mstrdup(layout_dir == 0 ? "Identity-H" : "Identity-V");
  mrec->font_name = (path != NULL) ? mstrdup(path) : NULL;
  mrec->opt.index = index;
  if (layout_dir != 0)
    mrec->opt.flags |= FONTMAP_OPT_VERT;

  pdftex_fill_in_defaults(mrec, fontmap_key);
  
  mrec->opt.extend = extend   / 65536.0;
  mrec->opt.slant  = slant    / 65536.0;
  mrec->opt.bold   = embolden / 65536.0;
  
  texpdf_insert_fontmap_record(native_fontmap, mrec->map_name, mrec);
  texpdf_clear_fontmap_record(mrec);
  RELEASE(mrec);

  if (verbose)
    MESG(">");

  return 0;
}

int
texpdf_load_native_font (const char *filename, unsigned long index,
                      int layout_dir, int extend, int slant, int embolden)
{
  return texpdf_insert_native_fontmap_record(filename, index,
                                          layout_dir, extend, slant, embolden);
}
#endif /* XETEX */

#if 0
/* tfm_name="dmjhira10", map_name="dmj@DNP@10", sfd_name="DNP"
 *  --> sub_id="hira"
 * Test if tfm_name can be really considered as subfont.
 */
static int
test_subfont (const char *tfm_name, const char *map_name, const char *sfd_name)
{
  int    r = 0;
  char **ids;
  int    n, m;
  char  *p = (char *) map_name;
  char  *q = (char *) tfm_name;

  ASSERT( tfm_name && map_name && sfd_name );

  /* until first occurence of '@' */
  for ( ; *p && *q && *p == *q && *p != '@'; p++, q++);
  if (*p != '@')
    return  0;
  p++;
  /* compare sfd_name (should be always true here) */
  if (strlen(p) <= strlen(sfd_name) ||
      memcmp(p, sfd_name, strlen(sfd_name)) ||
      p[strlen(sfd_name)] != '@')
    return  0;
  /* check tfm_name follows second '@' */
  p += strlen(sfd_name) + 1;
  if (*p) {
    char  *r = (char *) tfm_name;
    r += strlen(tfm_name) - strlen(p);
    if (strcmp(r, p))
      return  0;
  }
  /* Now 'p' is located at next to SFD name terminator
   * (second '@') in map_name and 'q' is at first char
   * of subfont_id substring in tfm_name.
   */
  n  = strlen(q) - strlen(p); /* length of subfont_id string */
  if (n <= 0)
    return  0;
  /* check if n-length substring 'q' is valid as subfont ID */
  ids = sfd_get_subfont_ids(sfd_name, &m);
  if (!ids)
    return  0;
  while (!r && m-- > 0) {
    if (strlen(ids[m]) == n &&
        !memcmp(q, ids[m], n)) {
      r = 1;
    }
  }

  return  r;
}
#endif  /* 0 */


fontmap_rec *
texpdf_lookup_fontmap_record (fontmap_t* map, const char *tfm_name)
{
  fontmap_rec *mrec = NULL;

  if (map && tfm_name)
    mrec = texpdf_ht_lookup_table(map, tfm_name, strlen(tfm_name));

  return  mrec;
}


void
texpdf_init_fontmaps (void)
{
  native_fontmap = NEW(1, struct ht_table);
  texpdf_ht_init_table(native_fontmap, hval_free);
}

void
texpdf_close_fontmaps (void)
{
  if (native_fontmap) {
    texpdf_ht_clear_table(native_fontmap);
    RELEASE(native_fontmap);
  }
  native_fontmap = NULL;

  release_sfd_record();
}

