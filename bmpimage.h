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
@brief BMP file support.    
*/
	
#ifndef _BMPIMAGE_H_
#define _BMPIMAGE_H_

#include "mfileio.h"
#include "pdfximage.h"

extern int bmp_include_image (pdf_ximage *ximage, FILE *file);

/**
* Check that a file handler refers to a BMP.
* Examines the start of the file pointed to by the parameter and checks
* to see if the first bytes indicate BMP file format.
*/
extern int texpdf_check_for_bmp     (FILE *file);

/**
* Retrieve the bounding box of the BMP file.
* Examines the file pointer for the size and density of the BMP file.
*/
extern int texpdf_bmp_get_bbox      (FILE *file, int *width, int *height,
                              double *xdensity, double *ydensity);

#endif /* _BMPIMAGE_H_ */
