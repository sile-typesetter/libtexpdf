/* This is dvipdfmx, an eXtended version of dvipdfm by Mark A. Wicks.

    Copyright (C) 2002-2015 by Jin-Hwan Cho and Shunsaku Hirata,
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

#ifndef _FONTMAP_H_
#define _FONTMAP_H_

#define FONTMAP_RMODE_REPLACE  0
#define FONTMAP_RMODE_APPEND  '+'
#define FONTMAP_RMODE_REMOVE  '-'

#define FONTMAP_OPT_NOEMBED (1 << 1)
#define FONTMAP_OPT_VERT    (1 << 2)

#define FONTMAP_STYLE_NONE       0
#define FONTMAP_STYLE_BOLD       1
#define FONTMAP_STYLE_ITALIC     2
#define FONTMAP_STYLE_BOLDITALIC 3

/* Options */
typedef struct fontmap_opt {
  /* Synthetic font */
  double slant, extend, bold;
  /* comaptibility and other flags */
  long   mapc,  flags;

  char  *otl_tags;    /* currently unused */
  char  *tounicode;   /* not implemented yet */

  void  *cff_charsets;

  double design_size; /* unused */

  char  *charcoll;    /* Adobe-Japan1-4, etc. */
  int    index;       /* TTC index */
  int    style;       /* ,Bold, etc. */
  int    stemv;       /* StemV value especially for CJK fonts */
} fontmap_opt;

typedef struct fontmap_rec {
  char  *map_name;

  char  *font_name;
  char  *enc_name;

  /* Subfont mapping: translate 8-bit charcode to 16-bit charcode
   * via SFD.
   */
  struct {
    char  *sfd_name;
    char  *subfont_id;
  } charmap;

  fontmap_opt opt;
} fontmap_rec;

typedef struct ht_table fontmap_t;

extern void         texpdf_fontmap_set_verbose   (void);
extern int          texpdf_fontmap_get_verbose (void);

extern void         texpdf_init_fontmaps         (void);
#if 0
extern void         pdf_clear_fontmaps        (void);
#endif
extern void         texpdf_close_fontmaps        (void);

extern void         texpdf_init_fontmap_record   (fontmap_rec *mrec);
extern void         texpdf_clear_fontmap_record  (fontmap_rec *mrec);
extern void         texpdf_copy_fontmap_record (fontmap_rec *dst, const fontmap_rec *src);

extern void         pdftex_fill_in_defaults (fontmap_rec *mrec, const char *tex_name);

extern int          texpdf_insert_fontmap_record (fontmap_t* map, const char  *kp, const fontmap_rec *mrec);
extern fontmap_rec *texpdf_lookup_fontmap_record (fontmap_t* map, const char  *kp);
extern char *       texpdf_make_subfont_name (const char *map_name, const char *sfd_name, const char *sub_id);
extern char *       texpdf_chop_sfd_name (const char *tex_name, char **sfd_name);

extern fontmap_t *native_fontmap;
extern int          texpdf_insert_native_fontmap_record (const char *filename, uint32_t index,
                                                      int layout_dir, int extend, int slant, int embolden);

#endif /* _FONTMAP_H_ */
