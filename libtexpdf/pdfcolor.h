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
/**
@file
@brief PDF Color handling functions

`libtexpdf` provides functionality for setting various colors within the
generated PDF. Most typically when generating PDFs, you will want to create
a color object using one of the initializing functions like so:

    pdf_color white, black, grey;
    texpdf_color_rgbcolor(&white, 1.0, 1.0, 1.0);
    texpdf_color_rgbcolor(&black, 0,   0,   0);
    texpdf_color_rgbcolor(&grey,  0.5, 0.5, 0.5)

You will then probably want to either set the drawing pens for
stroke and fill to your chosen color permanently:

    texpdf_color_set(pdf, black, grey); // Black stroke, grey fill

Or temporarily:

    texpdf_color_push(pdf, black, grey);
    ... Do some work here ...
    texpdf_color_pop(pdf);

*/

#ifndef _PDF_COLOR_H_
#define _PDF_COLOR_H_

#include "pdfobj.h"
#include "pdftypes.h"

#define PDF_COLORSPACE_TYPE_DEVICECMYK -4
#define PDF_COLORSPACE_TYPE_DEVICERGB  -3
#define PDF_COLORSPACE_TYPE_DEVICEGRAY -1
#define PDF_COLORSPACE_TYPE_INVALID     0
#define PDF_COLORSPACE_TYPE_CALGRAY     1
#define PDF_COLORSPACE_TYPE_CIELAB      2
#define PDF_COLORSPACE_TYPE_CALRGB      3
#define PDF_COLORSPACE_TYPE_ICCBASED    4

/** Type macro for CMYK color */
#define PDF_COLORSPACE_TYPE_CMYK  PDF_COLORSPACE_TYPE_DEVICECMYK
/** Type macro for RGB color */
#define PDF_COLORSPACE_TYPE_RGB   PDF_COLORSPACE_TYPE_DEVICERGB
/** Type macro for gray color */
#define PDF_COLORSPACE_TYPE_GRAY  PDF_COLORSPACE_TYPE_DEVICEGRAY

#include "pdfdoc.h"

extern void       texpdf_color_set_verbose   (void);

/** Initialize an RGB color.
Initialize a color from red, green and blue values. Caller provides allocated struct. */
extern int        texpdf_color_rgbcolor      (pdf_color *color,
                                           double r, double g, double b);

/** Initialize an CMYK color.
Initialize a color from CMYK values. Caller provides allocated struct. */
extern int        texpdf_color_cmykcolor     (pdf_color *color,
                                           double c, double m, double y, double k);
/** Initialize a gray.
Initialize a color from a single gray value. Caller provides allocated struct. */
extern int        texpdf_color_graycolor     (pdf_color *color, double g);
/** Copy color objects.
Copies color information from source to destination, assuming both structs are allocated. */
extern void       texpdf_color_copycolor     (pdf_color *color1, const pdf_color *color2);

/** Set color to black. */
#define texpdf_color_black(c)   texpdf_color_graycolor(c, 0.0);
/** Set color to white. */
#define texpdf_color_white(c)   texpdf_color_graycolor(c, 1.0);

/** Brighten a color.
Increases the luminosity of the color value by `f`, where 0 means no change and 1 means white. */
extern void       texpdf_color_brighten_color (pdf_color *dst, const pdf_color *src, double f);

/** Return type of color.
Returns either `PDF_COLORSPACE_TYPE_GRAY`, `PDF_COLORSPACE_TYPE_RGB` or `PDF_COLORSPACE_TYPE_CMYK`. */
extern int        texpdf_color_type          (const pdf_color *color);
/** Compares two colors (badly).
Returns -1 if two colors differ, 0 if they have the same color space and values.
Note that checking for same color space means that CMYK red (0,1,1,0) and
RGB red (1,0,0) will not compare as equal.*/
extern int        texpdf_color_compare       (const pdf_color *color1, const pdf_color *color2);
/** Convert color to a (PDF) color string.
In practice this means rounding each component and joining them together with spaces.
Caller is responsible for providing a long-enough buffer. */
extern int        texpdf_color_to_string     (const pdf_color *color, char *buffer);
/** Compares colors against white, in whatever color space. */
extern int        texpdf_color_is_white      (const pdf_color *color);
/** Checks all color values are within the range 0-1. */
extern int        texpdf_color_is_valid      (const pdf_color *color);

/* Not check size */
extern pdf_obj *iccp_get_rendering_intent (const void *profile, long proflen);
extern int      iccp_check_colorspace     (int colortype,
					   const void *profile, long proflen);

/* returns colorspace ID */
extern int      iccp_load_profile (const char *ident,
				   const void *profile, long proflen);

extern void     texpdf_init_colors  (void);
extern void     texpdf_close_colors (void);

/** XXX I don't know. */
extern pdf_obj *texpdf_get_colorspace_reference      (int cspc_id);
#if 0
extern int      texpdf_get_colorspace_num_components (int cspc_id);
extern int      texpdf_get_colorspace_subtype        (int cspc_id);

/* Not working */
extern int      pdf_colorspace_load_ICCBased      (const char *ident,
						   const char *profile_filename);
#endif

/* Color special
 * See remark in spc_color.c.
 */
/** Set current stroke and fill color. */
extern void     texpdf_color_set   (pdf_doc *p, pdf_color *sc, pdf_color *fc);
extern void     texpdf_color_set_default (const pdf_color *color);
/** Push stroke and fill colors onto the color stack.
Sets the current stroke color to `sc` and fill color to `fc` while maintaining
the stack of previously used colors. */
extern void     texpdf_color_push  (pdf_doc *p, pdf_color *sc, pdf_color *fc);
/** Pop the top color off the color stack.
Restores the current drawing color to the previously used color. */
extern void     texpdf_color_pop   (pdf_doc *p);

/* Color stack
 */
/** Empties the color stack */
extern void     texpdf_color_clear_stack (void);
/** Copy current top of color stack.
Fills the (already allocated) `sc` and `fc` colors with the stroke and fill color
at the top of the color stack. */
extern void     texpdf_color_get_current (pdf_color **sc, pdf_color **fc);

#if 0
/* Reinstall color */
extern void     texpdf_dev_preserve_color(void);
#endif

#endif /* _PDF_COLOR_H_ */
