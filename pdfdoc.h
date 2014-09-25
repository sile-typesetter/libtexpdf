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

#ifndef _PDFDOC_H_
#define _PDFDOC_H_

#include "pdfobj.h"
#include "pdftypes.h"
#include "pdfcolor.h"
    
#define PDF_DOC_GRABBING_NEST_MAX 4

#include "pdfdev.h"


extern void     texpdf_doc_set_verbose (void);

extern void     texpdf_open_document (pdf_doc *p,
                    const char *filename,
				    int do_encryption,
				    double media_width, double media_height,
				    double annot_grow_amount, int bookmark_open_depth,
				    int check_gotos);
extern void     texpdf_close_document (pdf_doc *p);


/* PDF document metadata */
extern void     texpdf_doc_set_creator   (pdf_doc *p, const char *creator);


/* They just return PDF dictionary object.
 * Callers are completely responsible for doing right thing...
 */
extern pdf_obj *texpdf_doc_get_dictionary (pdf_doc *p, const char *category);
extern pdf_obj *texpdf_doc_get_reference  (pdf_doc *p, const char *category);

#define texpdf_doc_page_tree(p) texpdf_doc_get_dictionary(p,"Pages")
#define texpdf_doc_catalog(p)   texpdf_doc_get_dictionary(p,"Catalog")
#define texpdf_doc_docinfo(p)   texpdf_doc_get_dictionary(p,"Info")
#define texpdf_doc_names(p)     texpdf_doc_get_dictionary(p,"Names")
#define texpdf_doc_this_page(p) texpdf_doc_get_dictionary(p,"@THISPAGE")

extern pdf_obj *texpdf_doc_get_page (pdf_file *pf, long page_no, long *count_p,
				  pdf_rect *bbox, pdf_obj **resources_p);

extern long     texpdf_doc_current_page_number    (pdf_doc *p);
extern pdf_obj *texpdf_doc_current_page_resources (pdf_doc *p);

extern pdf_obj *texpdf_doc_ref_page (pdf_doc *p, unsigned long page_no);
#define texpdf_doc_this_page_ref(p) pdf_doc_get_reference(p,"@THISPAGE")
#define texpdf_doc_next_page_ref(p) pdf_doc_get_reference(p,"@NEXTPAGE")
#define texpdf_doc_prev_page_ref(p) pdf_doc_get_reference(p,"@PREVPAGE")

/* Not really managing tree...
 * There should be something for number tree.
 */
extern int      texpdf_doc_add_names       (pdf_doc *p, const char *category,
					 const void *key, int keylen, pdf_obj *value);

extern void     texpdf_doc_set_bop_content (pdf_doc *p, const char *str, unsigned length);
extern void     texpdf_doc_set_eop_content (pdf_doc *p, const char *str, unsigned length);

/* Page */
extern void     texpdf_doc_begin_page   (pdf_doc *p, double scale, double x_origin, double y_origin);
extern void     texpdf_doc_end_page     (pdf_doc *p);

extern void     texpdf_doc_set_mediabox (pdf_doc *p, unsigned page_no, const pdf_rect *mediabox);
extern void     texpdf_doc_get_mediabox (pdf_doc *p, unsigned page_no, pdf_rect *mediabox);

extern void     texpdf_doc_add_page_content  (pdf_doc *p, const char *buffer, unsigned length);
extern void     texpdf_doc_add_page_resource (pdf_doc *p, const char *category,
					   const char *resource_name, pdf_obj *resources);

/* Article thread */
extern void     texpdf_doc_begin_article (pdf_doc *p, const char *article_id, pdf_obj *info);
#if 0
extern void     texpdf_doc_end_article   (pdf_doc *p, const char *article_id);  /* Do nothing... */
#endif
extern void     texpdf_doc_make_article  (pdf_doc *p, const char *article_id,
				       const char **bead_order, int num_beads);
extern void     texpdf_doc_add_bead      (pdf_doc *p, const char *article_id,
				       const char *bead_id,
				       long page_no, const pdf_rect *rect);

/* Bookmarks */
extern int      texpdf_doc_bookmarks_up    (pdf_doc *p);
extern int      texpdf_doc_bookmarks_down  (pdf_doc *p);
extern void     texpdf_doc_bookmarks_add   (pdf_doc *p, pdf_obj *dict, int is_open);
extern int      texpdf_doc_bookmarks_depth (pdf_doc *p);


/* Returns xobj_id of started xform. */
extern int      texpdf_doc_begin_grabbing (pdf_doc *p, const char *ident,
					double ref_x, double ref_y,
					const pdf_rect *cropbox);
extern void     texpdf_doc_end_grabbing   (pdf_doc *p, pdf_obj *attrib);


/* Annotation */
extern void     texpdf_doc_add_annot   (pdf_doc *p, unsigned page_no,
				     const pdf_rect *rect,
				     pdf_obj *annot_dict,
				     int new_annot);

/* Annotation with auto- clip and line (or page) break */
extern void     texpdf_doc_begin_annot (pdf_doc *p, pdf_obj *dict);
extern void     texpdf_doc_end_annot   (pdf_doc *p);

extern void     texpdf_doc_break_annot (pdf_doc *p);
extern void     texpdf_doc_expand_box  (pdf_doc *p, const pdf_rect *rect);

/* Manual thumbnail */
extern void     texpdf_doc_enable_manual_thumbnails (pdf_doc *p);

#if 0
/* PageLabels - */
extern void     pdf_doc_set_pagelabel (long  page_start,
                                       const char *type,
                                       const void *prefix, int pfrx_len,
                                       long  counter_start);
#endif

/* Similar to bop_content */
#include "pdfcolor.h"
extern void     texpdf_doc_set_bgcolor   (pdf_doc *p, const pdf_color *color);

#endif /* _PDFDOC_H_ */
