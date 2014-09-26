#ifndef _PDFTYPES_H_
#define _PDFTYPES_H_
#include "dpxutil.h"
typedef signed long spt_t;

typedef struct pdf_tmatrix
{
  double a, b, c, d, e, f;
} pdf_tmatrix;

typedef struct pdf_rect
{
  double llx, lly, urx, ury;
} pdf_rect;

typedef struct pdf_coord
{
  double x, y;
} pdf_coord;

#define PDF_COLOR_COMPONENT_MAX 4

typedef struct
{
  int    num_components;
  double values[PDF_COLOR_COMPONENT_MAX];
} pdf_color;


/* The name transform_info is misleading.
 * I'll put this here for a moment...
 */
typedef struct
{
  /* Physical dimensions
   *
   * If those values are given, images will be scaled
   * and/or shifted to fit within a box described by
   * those values.
   */
  double      width;
  double      height;
  double      depth;

  pdf_tmatrix matrix; /* transform matrix */
  pdf_rect    bbox;   /* user_bbox */

  int         flags;
} transform_info;


typedef struct pdf_page
{
  pdf_obj  *page_obj;
  pdf_obj  *page_ref;

  int       flags;

  double    ref_x, ref_y;
  pdf_rect  cropbox;

  pdf_obj  *resources;

  /* Contents */
  pdf_obj  *background;
  pdf_obj  *contents;

  /* global bop, background, contents, global eop */
  pdf_obj  *content_refs[4];

  pdf_obj  *annots;
  pdf_obj  *beads;
} pdf_page;

typedef struct pdf_olitem
{
  pdf_obj *dict;

  int      is_open;

  struct pdf_olitem *first;
  struct pdf_olitem *parent;

  struct pdf_olitem *next;
} pdf_olitem;

typedef struct pdf_bead
{
  char    *id;
  long     page_no;
  pdf_rect rect;
} pdf_bead;

typedef struct pdf_article
{
  char     *id;
  pdf_obj  *info;
  long      num_beads;
  long      max_beads;
  pdf_bead *beads;
} pdf_article;

typedef struct pdf_doc
{
  struct {
    pdf_obj *dict;

    pdf_obj *viewerpref;
    pdf_obj *pagelabels;
    pdf_obj *pages;
    pdf_obj *names;
    pdf_obj *threads;
  } root;

  pdf_obj *info;

  struct {
    pdf_rect mediabox;
    pdf_obj *bop, *eop;

    long      num_entries; /* This is not actually total number of pages. */
    long      max_entries;
    pdf_page *entries;
  } pages;

  struct {
    pdf_olitem *first;
    pdf_olitem *current;
    int         current_depth;
  } outlines;

  struct {
    long         num_entries;
    long         max_entries;
    pdf_article *entries;
  } articles;

  struct name_dict *names;

  int check_gotos;
  struct ht_table gotos;

  struct {
    int    outline_open_depth;
    double annot_grow;
  } opt;

  struct form_list_node *pending_forms;
  char  manual_thumb_enabled;
  char* doccreator;
  pdf_color bgcolor;
} pdf_doc;

#endif