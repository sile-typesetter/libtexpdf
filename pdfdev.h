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

#ifndef _PDFDEV_H_
#define _PDFDEV_H_

#include "numbers.h"
#include "pdfobj.h"
#include "pdfcolor.h"

#define INFO_HAS_USER_BBOX (1 << 0)
#define INFO_HAS_WIDTH     (1 << 1)
#define INFO_HAS_HEIGHT    (1 << 2)
#define INFO_DO_CLIP       (1 << 3)
#define INFO_DO_HIDE       (1 << 4)

#include "pdftypes.h"
#include "pdfdoc.h"

extern void   texpdf_transform_info_clear (transform_info *info);


extern void   texpdf_dev_set_verbose (void);

/* Not in spt_t. */
extern int    texpdf_sprint_matrix (char *buf, const pdf_tmatrix *p);
extern int    pdf_sprint_rect   (char *buf, const pdf_rect    *p);
extern int    pdf_sprint_coord  (char *buf, const pdf_coord   *p);
extern int    pdf_sprint_length (char *buf, double value);
extern int    pdf_sprint_number (char *buf, double value);

/* unit_conv: multiplier for input unit (spt_t) to bp conversion.
 * precision: How many fractional digits preserved in output (not real
 *            accuracy control).
 * is_bw:     Ignore color related special instructions.
 */
extern void   texpdf_init_device   (pdf_doc *p, double unit_conv, int precision, int is_bw);
extern void   texpdf_close_device  (void);

/* returns 1.0/unit_conv */
extern double dev_unit_dviunit  (void);

#if 0
/* DVI interpreter knows text positioning in relative motion.
 * However, texpdf_dev_set_string() recieves text string with placement
 * in absolute position in user space, and it convert absolute
 * positioning back to relative positioning. It is quite wasteful.
 *
 * TeX using DVI register stack operation to do CR and then use down
 * command for LF. DVI interpreter knows hint for current leading
 * and others (raised or lowered), but they are mostly lost in
 * texpdf_dev_set_string().
 */

typedef struct
{
  int      argc;

  struct {
    int    is_kern; /* kern or string */

    spt_t  kern;    /* negative kern means space */

    int    offset;  /* offset to sbuf   */
    int    length;  /* length of string */
  } args[];

  unsigned char sbuf[PDF_STRING_LEN_MAX];

} pdf_text_string;

/* Something for handling raise, leading, etc. here. */

#endif

/* Draw texts and rules:
 *
 * xpos, ypos, width, and height are all fixed-point numbers
 * converted to big-points by multiplying unit_conv (dvi2pts).
 * They must be position in the user space.
 *
 * ctype:
 *   0 - input string is in multi-byte encoding.
 *   1 - input string is in 8-bit encoding.
 *   2 - input string is in 16-bit encoding.
 */
extern void   texpdf_dev_set_string (pdf_doc *p, spt_t xpos, spt_t ypos,
				  const void *instr_ptr, int instr_len,
				  spt_t text_width,
				  int   font_id, int ctype);
extern void   texpdf_dev_set_rule   (pdf_doc *p, spt_t xpos, spt_t ypos,
				  spt_t width, spt_t height);

/* Place XObject */
extern int    texpdf_dev_put_image  (pdf_doc *doc, int xobj_id,
				  transform_info *p, double ref_x, double ref_y, int track_boxes);

/* The design_size and ptsize required by PK font support...
 */
extern int    texpdf_dev_load_native_font(const char *filename, uint32_t index,
                        spt_t ptsize, int layout_dir, int extend, int slant, int embolden);

extern int    texpdf_dev_locate_font (const char *font_name, spt_t ptsize);

extern int    texpdf_dev_setfont     (const char *font_name, spt_t ptsize);

/* The following two routines are NOT WORKING.
 * Dvipdfmx doesn't manage gstate well..
 */
#if 0
/* texpdf_dev_translate() or texpdf_dev_concat() should be used. */
extern void   texpdf_dev_set_origin (double orig_x, double orig_y);
#endif
/* Always returns 1.0, please rename this. */
extern double texpdf_dev_scale      (void);

/* Access text state parameters. */
#if 0
extern int    texpdf_dev_currentfont     (void); /* returns font_id */
extern double texpdf_dev_get_font_ptsize (int font_id);
#endif
extern int    texpdf_dev_get_font_wmode  (int font_id); /* ps: special support want this (pTeX). */

/* Text composition (direction) mode
 * This affects only when auto_rotate is enabled.
 */
extern int    texpdf_dev_get_dirmode     (void);
extern void   texpdf_dev_set_dirmode     (int dir_mode);

/* Set rect to rectangle in device space.
 * Unit conversion spt_t to bp and transformation applied within it.
 */
extern void   texpdf_dev_set_rect   (pdf_rect *rect,
				  spt_t x_pos, spt_t y_pos,
				  spt_t width, spt_t height, spt_t depth);

/* Accessor to various device parameters.
 */
#define PDF_DEV_PARAM_AUTOROTATE  1
#define PDF_DEV_PARAM_COLORMODE   2

extern int    texpdf_dev_get_param (int param_type);
extern void   texpdf_dev_set_param (int param_type, int value);

/* Text composition mode is ignored (always same as font's
 * writing mode) and glyph rotation is not enabled if
 * auto_rotate is unset.
 */
#define texpdf_dev_set_autorotate(v) texpdf_dev_set_param(PDF_DEV_PARAM_AUTOROTATE, (v))
#define texpdf_dev_set_colormode(v)  texpdf_dev_set_param(PDF_DEV_PARAM_COLORMODE,  (v))

/*
 * For pdf_doc, pdf_draw and others.
 */

/* Force reselecting font and color:
 * XFrom (content grabbing) and Metapost support want them.
 */
extern void   texpdf_dev_reset_fonts (void);
extern void   texpdf_dev_reset_color (pdf_doc *p, int force);

/* Initialization of transformation matrix with M and others.
 * They are called within pdf_doc_begin_page() and pdf_doc_end_page().
 */
extern void   texpdf_dev_bop (pdf_doc *p, const pdf_tmatrix *M);
extern void   texpdf_dev_eop (pdf_doc *p);

/* Text is normal and line art is not normal in dvipdfmx. So we don't have
 * begin_text (BT in PDF) and end_text (ET), but instead we have texpdf_graphics_mode()
 * to terminate text section. texpdf_dev_flushpath() and others call this.
 */
extern void   texpdf_graphics_mode (pdf_doc *p);

extern void   texpdf_dev_get_coord(double *xpos, double *ypos);
extern void   texpdf_dev_push_coord(double xpos, double ypos);
extern void   texpdf_dev_pop_coord(void);

#endif /* _PDFDEV_H_ */
