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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include "dvi.h"
#include "system.h"
#include "mem.h"
#include "error.h"
#include "mfileio.h"

#include "numbers.h"

#include "libtexpdf/libtexpdf.h"
#include "dvipdfmx.h"

#include "specials.h"

#include "spc_util.h"
#include "spc_pdfm.h"

#include "dvipdfmx.h"


/* PLEASE REMOVE THIS */
struct resource_map {
  int   type;
  int   res_id;
};

struct tounicode {
  int       cmap_id;
  int       unescape_backslash;
  pdf_obj  *taintkeys; /* An array of PDF names. */
};

struct spc_pdf_
{
   pdf_obj          *annot_dict;   /* pending annotation dict       */
   int               lowest_level; /* current min level of outlines */
   struct ht_table  *resourcemap;  /* see remark below (somewhere)  */
   struct tounicode  cd;           /* For to-UTF16-BE conversion :( */
};

static struct spc_pdf_  _pdf_stat = {
  NULL,
  255,
  NULL,
  { -1, 0, NULL }
};

/* PLEASE REMOVE THIS */
static void
hval_free (void *vp)
{
  RELEASE(vp);
}


static int
addresource (struct spc_pdf_ *sd, const char *ident, int res_id)
{
  struct resource_map *r;

  if (!ident || res_id < 0)
    return  -1;

  r = NEW(1, struct resource_map);
  r->type   = 0; /* unused */
  r->res_id = res_id;

  texpdf_ht_append_table(sd->resourcemap, ident, strlen(ident), r);
  spc_push_object(ident, texpdf_ximage_get_reference(res_id));

  return 0;
}

static int
findresource (struct spc_pdf_ *sd, const char *ident)
{
  struct resource_map *r;

  if (!ident)
    return  -1;

  r = texpdf_ht_lookup_table(sd->resourcemap, ident, strlen(ident));

  return (r ? r->res_id : -1);
}


static int
spc_handler_pdfm__init (void *dp)
{
  struct spc_pdf_ *sd = dp;
  static const char *default_taintkeys[] = {
    "Title",   "Author",   "Subject", "Keywords",
    "Creator", "Producer", "Contents", "Subj",
    "TU",      "T",        "TM",        NULL /* EOD */
  };
  int  i;

  sd->annot_dict   = NULL;
  sd->lowest_level = 255;
  sd->resourcemap  = NEW(1, struct ht_table);
  texpdf_ht_init_table(sd->resourcemap, hval_free);

  sd->cd.taintkeys = texpdf_new_array();
  for (i = 0; default_taintkeys[i] != NULL; i++) {
    texpdf_add_array(sd->cd.taintkeys,
		  texpdf_new_name(default_taintkeys[i]));
  }

  return 0;
}

static int
spc_handler_pdfm__clean (void *dp)
{
  struct spc_pdf_ *sd = dp;

  if (sd->annot_dict) {
    WARN("Unbalanced bann and eann found.");
    texpdf_release_obj(sd->annot_dict);
  }
  sd->lowest_level = 255;
  sd->annot_dict   = NULL;
  if (sd->resourcemap) {
    texpdf_ht_clear_table(sd->resourcemap);
    RELEASE(sd->resourcemap);
  }
  sd->resourcemap = NULL;

  if (sd->cd.taintkeys)
    texpdf_release_obj(sd->cd.taintkeys);
  sd->cd.taintkeys = NULL;

  return 0;
}


int
spc_pdfm_at_begin_document (void)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  return  spc_handler_pdfm__init(sd);
}

int
spc_pdfm_at_end_document (void)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  return  spc_handler_pdfm__clean(sd);
}


/* Dvipdfm specials */
static int
spc_handler_pdfm_bop (struct spc_env *spe, struct spc_arg *args)
{
  if (args->curptr < args->endptr) {
    texpdf_doc_set_bop_content(pdf, args->curptr,
			    (long) (args->endptr - args->curptr));
  }

  args->curptr = args->endptr;

  return 0;
}

static int
spc_handler_pdfm_eop (struct spc_env *spe, struct spc_arg *args)
{
  if (args->curptr < args->endptr) {
    texpdf_doc_set_eop_content(pdf, args->curptr,
			    (long) (args->endptr - args->curptr));
  }

  args->curptr = args->endptr;

  return 0;
}

#define streamfiltered(o) \
  (texpdf_lookup_dict(texpdf_stream_dict((o)), "Filter") ? 1 : 0)

/* Why should we have this kind of things? */
static int
safeputresdent (pdf_obj *kp, pdf_obj *vp, void *dp)
{
  char  *key;

  ASSERT(kp && vp && dp);

  key = texpdf_name_value(kp);
  if (texpdf_lookup_dict(dp, key))
    WARN("Object \"%s\" already defined in dict! (ignored)", key);
  else {
    texpdf_add_dict(dp,
                 texpdf_link_obj(kp), texpdf_link_obj(vp));
  }
  return 0;
}

#ifndef pdf_obj_isaref
#define pdf_obj_isaref(o) (texpdf_obj_typeof((o)) == PDF_INDIRECT)
#endif

static int
safeputresdict (pdf_obj *kp, pdf_obj *vp, void *dp)
{
  char    *key;
  pdf_obj *dict;

  ASSERT(kp && vp && dp);

  key  = texpdf_name_value(kp);
  dict = texpdf_lookup_dict(dp, key);

  if (pdf_obj_isaref(vp)) {
    texpdf_add_dict(dp, texpdf_new_name(key), texpdf_link_obj(vp));
  } else if (texpdf_obj_typeof(vp) == PDF_DICT) {
    if (dict)
      texpdf_foreach_dict(vp, safeputresdent, dict);
    else {
      texpdf_add_dict(dp, texpdf_new_name(key), texpdf_link_obj(vp));
    }
  } else {
    WARN("Invalid type (not DICT) for page/form resource dict entry: key=\"%s\"", key);
    return  -1;
  }

  return 0;
}


/* Think what happens if you do
 *
 *  pdf:put @resources << /Font << >> >>
 * 
 */
static int
spc_handler_pdfm_put (struct spc_env *spe, struct spc_arg *ap)
{
  pdf_obj  *obj1, *obj2; /* put obj2 into obj1 */
  char     *ident;
  int       error = 0;

  texpdf_skip_white(&ap->curptr, ap->endptr);

  ident = texpdf_parse_opt_ident(&ap->curptr, ap->endptr);
  if (!ident) {
    spc_warn(spe, "Missing object identifier.");
    return  -1;
  }
  obj1 = spc_lookup_object(ident);
  if (!obj1) {
    spc_warn(spe, "Specified object not exist: %s", ident);
    RELEASE(ident);
    return  -1;
  }
  texpdf_skip_white(&ap->curptr, ap->endptr);

  obj2 = texpdf_parse_pdf_object(&ap->curptr, ap->endptr, NULL);
  if (!obj2) {
    spc_warn(spe, "Missing (an) object(s) to put into \"%s\"!", ident);
    RELEASE(ident);
    return  -1;
  }

  switch (texpdf_obj_typeof(obj1)) {
  case  PDF_DICT:
    if (texpdf_obj_typeof(obj2) != PDF_DICT) {
      spc_warn(spe, "Inconsistent object type for \"put\" (expecting DICT): %s", ident);
      error = -1;
    } else {
      if (!strcmp(ident, "resources"))
        error = texpdf_foreach_dict(obj2, safeputresdict, obj1);
      else {
        texpdf_merge_dict(obj1, obj2);
      }
    }
    break;

  case  PDF_STREAM:
    if (texpdf_obj_typeof(obj2) == PDF_DICT)
      texpdf_merge_dict(texpdf_stream_dict(obj1), obj2);
    else if (texpdf_obj_typeof(obj2) == PDF_STREAM)
#if  0
    {
      texpdf_merge_dict(texpdf_stream_dict(obj1), texpdf_stream_dict(obj2));
      texpdf_add_stream(obj1, pdf_stream_dataptr(obj2), pdf_stream_length(obj2));
    }
#else
    {
      spc_warn(spe, "\"put\" operation not supported for STREAM <- STREAM: %s", ident);
      error = -1;
    }
#endif
    else {
      spc_warn(spe, "Invalid type: expecting a DICT or STREAM: %s", ident);
      error = -1;
    }
    break;

  case PDF_ARRAY:
    /* dvipdfm */
    texpdf_add_array(obj1, texpdf_link_obj(obj2));
    while (ap->curptr < ap->endptr) {
      pdf_obj *obj3 = texpdf_parse_pdf_object(&ap->curptr, ap->endptr, NULL);
      if (!obj3)
	break;
      texpdf_add_array(obj1, obj3);
      texpdf_skip_white(&ap->curptr, ap->endptr);
    }
    break;

  default:
    spc_warn(spe, "Can't \"put\" object into non-DICT/STREAM/ARRAY type object: %s", ident);
    error = -1;
    break;
  }
  texpdf_release_obj(obj2);
  RELEASE(ident);

  return  error;
}


/* For pdf:tounicode support
 * This feature is provided for convenience. TeX can't do
 * input encoding conversion.
 */

static int
reencodestring (CMap *cmap, pdf_obj *instring)
{
#define WBUF_SIZE 4096
  unsigned char  wbuf[WBUF_SIZE];
  unsigned char *obufcur;
  const unsigned char *inbufcur;
  long inbufleft, obufleft;

  if (!cmap || !instring)
    return 0;

  inbufleft = texpdf_string_length(instring);
  inbufcur  = texpdf_string_value (instring);

  wbuf[0]  = 0xfe;
  wbuf[1]  = 0xff;
  obufcur  = wbuf + 2;
  obufleft = WBUF_SIZE - 2;

  texpdf_CMap_decode(cmap,
	      &inbufcur, &inbufleft,
	      &obufcur, &obufleft);

  if (inbufleft > 0) {
    return  -1;
  }

  texpdf_set_string(instring, wbuf, WBUF_SIZE - obufleft);

  return 0;
}

/* tables/values used in UTF-8 interpretation -
   code is based on ConvertUTF.[ch] sample code
   published by the Unicode consortium */
static unsigned long
offsetsFromUTF8[6] =    {
        0x00000000UL,
        0x00003080UL,
        0x000E2080UL,
        0x03C82080UL,
        0xFA082080UL,
        0x82082080UL
};

static unsigned char
bytesFromUTF8[256] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

static int
maybe_reencode_utf8(pdf_obj *instring)
{
  unsigned char* inbuf;
  int            inlen;
  int            non_ascii = 0;
  unsigned char* cp;
  unsigned char* op;
  unsigned char  wbuf[WBUF_SIZE];

  if (!instring)
    return 0;

  inlen = texpdf_string_length(instring);
  inbuf = texpdf_string_value(instring);

  /* check if the input string is strictly ASCII */
  for (cp = inbuf; cp < inbuf + inlen; ++cp) {
    if (*cp > 127) {
      non_ascii = 1;
    }
  }
  if (non_ascii == 0)
    return 0; /* no need to reencode ASCII strings */

  cp = inbuf;
  if (inbuf[0] == 0xfe && inbuf[1] == 0xff)
    return 0; /* no need to reencode UTF16BE with BOM */
  op = wbuf;
  *op++ = 0xfe;
  *op++ = 0xff;
  while (cp < inbuf + inlen) {
    unsigned long usv = *cp++;
    int extraBytes = bytesFromUTF8[usv];
    if (cp + extraBytes > inbuf + inlen)
      return -1; /* ill-formed, so give up reencoding */
    switch (extraBytes) {   /* note: code falls through cases! */
      case 5: usv <<= 6; usv += *cp++;
      case 4: usv <<= 6; usv += *cp++;
      case 3: usv <<= 6; usv += *cp++;
      case 2: usv <<= 6; usv += *cp++;
      case 1: usv <<= 6; usv += *cp++;
      case 0: ;
    };
    usv -= offsetsFromUTF8[extraBytes];
    if (usv > 0x10FFFF)
      return -1; /* out of valid Unicode range, give up */
    if (usv > 0xFFFF) {
      /* supplementary-plane character: generate high surrogate */
      unsigned long hi = 0xdc00 + (usv - 0x10000) % 0x0400;
      if (op > wbuf + WBUF_SIZE - 2)
        return -1; /* out of space */
      *op++ = hi / 256;
      *op++ = hi % 256;
      usv = 0xd800 + (usv - 0x10000) / 0x0400;
      /* remaining value in usv is the low surrogate */
    }
    if (op > wbuf + WBUF_SIZE - 2)
      return -1; /* out of space */
    *op++ = usv / 256;
    *op++ = usv % 256;
  }

  texpdf_set_string(instring, wbuf, op - wbuf);

  return 0;
}

static int
needreencode (pdf_obj *kp, pdf_obj *vp, struct tounicode *cd)
{
  int      r = 0, i;
  pdf_obj *tk;

  ASSERT( cd && cd->taintkeys );
  ASSERT( texpdf_obj_typeof(kp) == PDF_NAME );
  ASSERT( texpdf_obj_typeof(vp) == PDF_STRING );

  for (i = 0; i < texpdf_array_length(cd->taintkeys); i++) {
    tk = texpdf_get_array(cd->taintkeys, i);
    ASSERT( tk && texpdf_obj_typeof(tk) == PDF_NAME );
    if (!strcmp(texpdf_name_value(kp), texpdf_name_value(tk))) {
      r = 1;
      break;
    }
  }
  if (r) {
    /* Check UTF-16BE BOM. */
    if (texpdf_string_length(vp) >= 2 &&
        !memcmp(texpdf_string_value(vp), "\xfe\xff", 2))
      r = 0;
  }

  return  r;
}

static int
modstrings (pdf_obj *kp, pdf_obj *vp, void *dp)
{
  int               r = 0; /* continue */
  struct tounicode *cd = dp;

  ASSERT( texpdf_obj_typeof(kp) == PDF_NAME );

  switch (texpdf_obj_typeof(vp)) {
  case  PDF_STRING:
    if (cd && cd->cmap_id >= 0 && cd->taintkeys) {
      CMap *cmap = texpdf_CMap_cache_get(cd->cmap_id);
      if (needreencode(kp, vp, cd))
        r = reencodestring(cmap, vp);
    } else if (is_xdv)
      r = maybe_reencode_utf8(vp);
    if (r < 0) /* error occured... */
      WARN("Failed to convert input string to UTF16...");
    break;
  case  PDF_DICT:
    r = texpdf_foreach_dict(vp, modstrings, dp);
    break;
  case  PDF_STREAM:
    r = texpdf_foreach_dict(texpdf_stream_dict(vp), modstrings, dp);
    break;
  }

  return  r;
}

static pdf_obj *
texpdf_parse_pdf_dict_with_tounicode (const char **pp, const char *endptr, struct tounicode *cd)
{
  pdf_obj  *dict;

  /* disable this test for XDV files, as we do UTF8 reencoding with no cmap */
  if (!is_xdv && cd->cmap_id < 0)
    return  texpdf_parse_pdf_dict(pp, endptr, NULL);

  /* :( */
  if (cd && cd->unescape_backslash) 
    dict = texpdf_parse_texpdf_tainted_dict(pp, endptr);
  else {
    dict = texpdf_parse_pdf_dict(pp, endptr, NULL);
  }
  if (dict)
    texpdf_foreach_dict(dict, modstrings, cd);

  return  dict;
}

static int
spc_handler_pdfm_annot (struct spc_env *spe, struct spc_arg *args)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  pdf_obj       *annot_dict;
  pdf_rect       rect;
  char          *ident = NULL;
  pdf_coord      cp;
  transform_info ti;

  texpdf_skip_white(&args->curptr, args->endptr);
  if (args->curptr[0] == '@') {
    ident = texpdf_parse_opt_ident(&args->curptr, args->endptr);
    texpdf_skip_white(&args->curptr, args->endptr);
  }

  texpdf_transform_info_clear(&ti);
  if (spc_util_read_dimtrns(spe, &ti, args, NULL, 0) < 0) {
    if (ident)
      RELEASE(ident);
    return  -1;
  }

  if ((ti.flags & INFO_HAS_USER_BBOX) &&
      ((ti.flags & INFO_HAS_WIDTH) || (ti.flags & INFO_HAS_HEIGHT))) {
    spc_warn(spe, "You can't specify both bbox and width/height.");
    if (ident)
      RELEASE(ident);
    return  -1;
  }

  annot_dict = texpdf_parse_pdf_dict_with_tounicode(&args->curptr, args->endptr, &sd->cd);
  if (!annot_dict) {
    spc_warn(spe, "Could not find dictionary object.");
    if (ident)
      RELEASE(ident);
    return  -1;
  } else if (!PDF_OBJ_DICTTYPE(annot_dict)) {
    spc_warn(spe, "Invalid type: not dictionary object.");
    if (ident)
      RELEASE(ident);
    texpdf_release_obj(annot_dict);
    return  -1;
  }

  cp.x = spe->x_user; cp.y = spe->y_user;
  texpdf_dev_transform(&cp, NULL);
  if (ti.flags & INFO_HAS_USER_BBOX) {
    rect.llx = ti.bbox.llx + cp.x;
    rect.lly = ti.bbox.lly + cp.y;
    rect.urx = ti.bbox.urx + cp.x;
    rect.ury = ti.bbox.ury + cp.y;
  } else {
    rect.llx = cp.x;
    rect.lly = cp.y - spe->mag * ti.depth;
    rect.urx = cp.x + spe->mag * ti.width;
    rect.ury = cp.y + spe->mag * ti.height;
  }

  /* Order is important... */
  if (ident)
    spc_push_object(ident, texpdf_link_obj(annot_dict));
  /* Add this reference. */
  texpdf_doc_add_annot(pdf, texpdf_doc_current_page_number(pdf), &rect, annot_dict, 1);

  if (ident) {
    spc_flush_object(ident);
    RELEASE(ident);
  }
  texpdf_release_obj(annot_dict);

  return 0;
}

/* NOTE: This can't have ident. See "Dvipdfm User's Manual". */
static int
spc_handler_pdfm_bann (struct spc_env *spe, struct spc_arg *args)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  int    error = 0;

  if (sd->annot_dict) {
    spc_warn(spe, "Can't begin an annotation when one is pending.");
    return  -1;
  }

  texpdf_skip_white(&args->curptr, args->endptr);

  sd->annot_dict = texpdf_parse_pdf_dict_with_tounicode(&args->curptr, args->endptr, &sd->cd);
  if (!sd->annot_dict) {
    spc_warn(spe, "Ignoring annotation with invalid dictionary.");
    return  -1;
  } else if (!PDF_OBJ_DICTTYPE(sd->annot_dict)) {
    spc_warn(spe, "Invalid type: not a dictionary object.");
    texpdf_release_obj(sd->annot_dict);
    sd->annot_dict = NULL;
    return  -1;
  }

  error = spc_begin_annot(spe, sd->annot_dict);

  return  error;
}

static int
spc_handler_pdfm_eann (struct spc_env *spe, struct spc_arg *args)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  int    error = 0;

  if (!sd->annot_dict) {
    spc_warn(spe, "Tried to end an annotation without starting one!");
    return  -1;
  }

  error = spc_end_annot(spe);

  texpdf_release_obj(sd->annot_dict);
  sd->annot_dict = NULL;

  return  error;
}


/* Color:.... */
static int
spc_handler_pdfm_bcolor (struct spc_env *spe, struct spc_arg *ap)
{
  int       error;
  pdf_color fc, sc;
  pdf_color *pfc, *psc;

  texpdf_color_get_current(&psc, &pfc);
  error = spc_util_read_pdfcolor(spe, &fc, ap, pfc);
  if (!error) {
    if (ap->curptr < ap->endptr) {
      error = spc_util_read_pdfcolor(spe, &sc, ap, psc);
    } else {
      texpdf_color_copycolor(&sc, &fc);
    }
  }

  if (error)
    spc_warn(spe, "Invalid color specification?");
  else {
    texpdf_color_push(pdf, &sc, &fc); /* save currentcolor */
  }

  return  error;
}

/*
 * This special changes the current color without clearing the color stack.
 * It therefore differs from "color rgb 1 0 0".
 */
static int
spc_handler_pdfm_scolor (struct spc_env *spe, struct spc_arg *ap)
{
  int       error;
  pdf_color fc, sc;
  pdf_color *pfc, *psc;

  texpdf_color_get_current(&psc, &pfc);
  error = spc_util_read_pdfcolor(spe, &fc, ap, pfc);
  if (!error) {
    if (ap->curptr < ap->endptr) {
      error = spc_util_read_pdfcolor(spe, &sc, ap, psc);
    } else {
      texpdf_color_copycolor(&sc, &fc);
    }
  }

  if (error)
    spc_warn(spe, "Invalid color specification?");
  else
    texpdf_color_set(pdf, &sc, &fc);

  return  error;
}

static int
spc_handler_pdfm_ecolor (struct spc_env *spe, struct spc_arg *args)
{
  texpdf_color_pop(pdf);
  return 0;
}


static int
spc_handler_pdfm_btrans (struct spc_env *spe, struct spc_arg *args)
{
  pdf_tmatrix     M;
  transform_info  ti;

  texpdf_transform_info_clear(&ti);
  if (spc_util_read_dimtrns(spe, &ti, args, NULL, 0) < 0) {
    return -1;
  }

  /* Create transformation matrix */
  pdf_copymatrix(&M, &(ti.matrix));
  M.e += ((1.0 - M.a) * spe->x_user - M.c * spe->y_user);
  M.f += ((1.0 - M.d) * spe->y_user - M.b * spe->x_user);

  texpdf_dev_gsave(pdf);
  texpdf_dev_concat(pdf, &M);

  return 0;
}

static int
spc_handler_pdfm_etrans (struct spc_env *spe, struct spc_arg *args)
{
  texpdf_dev_grestore(pdf);

  /*
   * Unfortunately, the following line is necessary in case
   * of a color change inside of the save/restore pair.
   * (Font changes are automatically corrected by texpdf_dev_grestore().)
   * Anything that was done there must be redone, so in effect,
   * we make no assumptions about what fonts. We act like we are
   * starting a new page.
   */
  texpdf_dev_reset_color(pdf, 0);

  return 0;
}

static int
spc_handler_pdfm_outline (struct spc_env *spe, struct spc_arg *args)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  pdf_obj   *item_dict, *tmp;
  int        level, is_open = -1;
  int        current_depth;

  texpdf_skip_white(&args->curptr, args->endptr);

  /*
   * pdf:outline is extended to support open/close feature
   *
   * pdf:outline 1 ... (as DVIPDFM)
   * pdf:outline [] 1 ... (open bookmark)
   * pdf:outline [-] 1 ... (closed bookmark)
   */
  if (args->curptr+3 < args->endptr && *args->curptr == '[') {
    args->curptr++;
    if (*args->curptr == '-') {
      args->curptr++;
    } else {
      is_open = 1;
    }
    args->curptr++;
  }
  texpdf_skip_white(&args->curptr, args->endptr);

  tmp = texpdf_parse_pdf_object(&args->curptr, args->endptr, NULL);
  if (!tmp) {
    spc_warn(spe, "Missing number for outline item depth.");
    return  -1;
  } else if (!PDF_OBJ_NUMBERTYPE(tmp)) {
    texpdf_release_obj(tmp);
    spc_warn(spe, "Expecting number for outline item depth.");
    return  -1;
  }

  item_dict = NULL;

  level = (int) texpdf_number_value(tmp);
  texpdf_release_obj(tmp);

  /* What is this? Starting at level 3 and can go down to level 1?
   *
   * Here is the original comment:
   *  Make sure we know where the starting level is
   *
   * NOTE: added
   *  We need this for converting pages from 3rd to... :(
   */
  sd->lowest_level = MIN(sd->lowest_level, level);

  level  +=  1 - sd->lowest_level;

  item_dict = texpdf_parse_pdf_dict_with_tounicode(&args->curptr, args->endptr, &sd->cd);
  if (!item_dict) {
    spc_warn(spe, "Ignoring invalid dictionary.");
    return  -1;
  }
  current_depth = texpdf_doc_bookmarks_depth(pdf);
  if (current_depth > level) {
    while (current_depth-- > level)
      texpdf_doc_bookmarks_up(pdf);
  } else if (current_depth < level) {
    while (current_depth++ < level)
      texpdf_doc_bookmarks_down(pdf);
  }

  texpdf_doc_bookmarks_add(pdf, item_dict, is_open);

  return 0;
}

static int
spc_handler_pdfm_article (struct spc_env *spe, struct spc_arg *args)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  char    *ident;
  pdf_obj *info_dict;

  texpdf_skip_white (&args->curptr, args->endptr);

  ident = texpdf_parse_opt_ident(&args->curptr, args->endptr);
  if (!ident) {
    spc_warn(spe,  "Article name expected but not found.");
    return -1;
  }

  info_dict = texpdf_parse_pdf_dict_with_tounicode(&args->curptr, args->endptr, &sd->cd);
  if (!info_dict) {
    spc_warn(spe, "Ignoring article with invalid info dictionary.");
    RELEASE(ident);
    return  -1;
  }

  texpdf_doc_begin_article(pdf, ident, texpdf_link_obj(info_dict));
  spc_push_object(ident, info_dict);
  RELEASE(ident);

  return 0;
}

static int
spc_handler_pdfm_bead (struct spc_env *spe, struct spc_arg *args)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  pdf_obj         *article;
  pdf_obj         *article_info;
  char            *article_name;
  pdf_rect         rect;
  long             page_no;
  transform_info   ti;
  pdf_coord        cp;

  texpdf_skip_white(&args->curptr, args->endptr);

  if (args->curptr[0] != '@') {
    spc_warn(spe, "Article identifier expected but not found.");
    return  -1;
  }

  article_name = texpdf_parse_opt_ident(&args->curptr, args->endptr);
  if (!article_name) {
    spc_warn(spe, "Article reference expected but not found.");
    return  -1;
  }

  /* If okay so far, try to get a bounding box */
  texpdf_transform_info_clear(&ti);
  if (spc_util_read_dimtrns(spe, &ti, args, NULL, 0) < 0) {
    RELEASE(article_name);
    return  -1;
  }

  if ((ti.flags & INFO_HAS_USER_BBOX) &&
      ((ti.flags & INFO_HAS_WIDTH) || (ti.flags & INFO_HAS_HEIGHT))) {
    spc_warn(spe, "You can't specify both bbox and width/height.");
    RELEASE(article_name);
    return -1;
  }

  cp.x = spe->x_user; cp.y = spe->y_user;
  texpdf_dev_transform(&cp, NULL);
  if (ti.flags & INFO_HAS_USER_BBOX) {
    rect.llx = ti.bbox.llx + cp.x;
    rect.lly = ti.bbox.lly + cp.y;
    rect.urx = ti.bbox.urx + cp.x;
    rect.ury = ti.bbox.ury + cp.y;
  } else {
    rect.llx = cp.x;
    rect.lly = cp.y - spe->mag * ti.depth;
    rect.urx = cp.x + spe->mag * ti.width;
    rect.ury = cp.y + spe->mag * ti.height;
  }

  texpdf_skip_white(&args->curptr, args->endptr);
  if (args->curptr[0] != '<') {
    article_info = texpdf_new_dict();
  } else {
    article_info = texpdf_parse_pdf_dict_with_tounicode(&args->curptr, args->endptr, &sd->cd);
    if (!article_info) {
      spc_warn(spe, "Error in reading dictionary.");
      RELEASE(article_name);
      return -1;
    }
  }

  /* Does this article exist yet */
  article = spc_lookup_object(article_name);
  if (article) {
    texpdf_merge_dict (article, article_info);
    texpdf_release_obj(article_info);
  } else {
    texpdf_doc_begin_article(pdf, article_name, texpdf_link_obj(article_info));
    spc_push_object(article_name, article_info);
  }
  page_no = texpdf_doc_current_page_number(pdf);
  texpdf_doc_add_bead(pdf, article_name, NULL, page_no, &rect);

  RELEASE(article_name);
  return 0;
}

static int
spc_handler_pdfm_image (struct spc_env *spe, struct spc_arg *args)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  int              xobj_id;
  char            *ident = NULL;
  pdf_obj         *fspec, *attr = NULL;
  transform_info   ti;
  long             page_no;

  texpdf_skip_white(&args->curptr, args->endptr);
  if (args->curptr[0] == '@') {
    ident = texpdf_parse_opt_ident(&args->curptr, args->endptr);
    xobj_id = findresource(sd, ident);
    if (xobj_id >= 0) {
      spc_warn(spe, "Object reference name for image \"%s\" already used.", ident);
      RELEASE(ident);
      return  -1;
    }
  }

  texpdf_transform_info_clear(&ti);
  page_no = 1;
  if (spc_util_read_dimtrns(spe, &ti, args, &page_no, 0) < 0) {
    if (ident)
      RELEASE(ident);
    return  -1;
  }

  texpdf_skip_white(&args->curptr, args->endptr);
  fspec = texpdf_parse_pdf_object(&args->curptr, args->endptr, NULL);
  if (!fspec) {
    spc_warn(spe, "Missing filename string for pdf:image.");
    if (ident)
      RELEASE(ident);
    return  -1;
  } else if (!PDF_OBJ_STRINGTYPE(fspec)) {
    spc_warn(spe, "Missing filename string for pdf:image.");
    texpdf_release_obj(fspec);
    if (ident)
      RELEASE(ident);
    return  -1;
  }

  texpdf_skip_white(&args->curptr, args->endptr);
  if (args->curptr < args->endptr) {
    attr = texpdf_parse_pdf_object(&args->curptr, args->endptr, NULL);
    if (!attr || !PDF_OBJ_DICTTYPE(attr)) {
      spc_warn(spe, "Ignore invalid attribute dictionary.");
      if (attr) texpdf_release_obj(attr);
    }
  }

  xobj_id = texpdf_ximage_findresource(pdf, texpdf_string_value(fspec), page_no, attr);
  if (xobj_id < 0) {
    spc_warn(spe, "Could not find image resource...");
    texpdf_release_obj(fspec);
    if (ident)
      RELEASE(ident);
    return  -1;
  }

  if (!(ti.flags & INFO_DO_HIDE))
    texpdf_dev_put_image(pdf, xobj_id, &ti, spe->x_user, spe->y_user, dvi_is_tracking_boxes());

  if (ident) {
    if (compat_mode &&
        texpdf_ximage_get_subtype(xobj_id) == PDF_XOBJECT_TYPE_IMAGE)
      texpdf_ximage_set_attr(xobj_id, 1, 1, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0);
    addresource(sd, ident, xobj_id);
    RELEASE(ident);
  }

  texpdf_release_obj(fspec);

  return 0;
}

/* Use do_names instead. */
static int
spc_handler_pdfm_dest (struct spc_env *spe, struct spc_arg *args)
{
  pdf_obj  *name, *array;

  texpdf_skip_white(&args->curptr, args->endptr);

  name = texpdf_parse_pdf_object(&args->curptr, args->endptr, NULL);
  if (!name) {
    spc_warn(spe, "PDF string expected for destination name but not found.");
    return  -1;
  } else if (!PDF_OBJ_STRINGTYPE(name)) {
    spc_warn(spe, "PDF string expected for destination name but invalid type.");
    texpdf_release_obj(name);
    return  -1;
  }

  if (is_xdv && maybe_reencode_utf8(name) < 0)
    WARN("Failed to convert input string to UTF16...");

  array = texpdf_parse_pdf_object(&args->curptr, args->endptr, NULL);
  if (!array) {
    spc_warn(spe, "No destination specified for pdf:dest.");
    texpdf_release_obj(name);
    return  -1;
  } else if (!PDF_OBJ_ARRAYTYPE(array)) {
    spc_warn(spe, "Destination not specified as an array object!");
    texpdf_release_obj(name);
    texpdf_release_obj(array);
    return  -1;
  }

  texpdf_doc_add_names(pdf, "Dests",
                    texpdf_string_value (name),
                    texpdf_string_length(name),
                    array);
  texpdf_release_obj(name);

  return 0;
}

static int
spc_handler_pdfm_names (struct spc_env *spe, struct spc_arg *args)
{
  pdf_obj *category, *key, *value, *tmp;
  int      i, size;

  category = texpdf_parse_pdf_object(&args->curptr, args->endptr, NULL);
  if (!category) {
    spc_warn(spe, "PDF name expected but not found.");
    return  -1;
  } else if (!PDF_OBJ_NAMETYPE(category)) {
    spc_warn(spe, "PDF name expected but not found.");
    texpdf_release_obj(category);
    return  -1;
  }

  tmp = texpdf_parse_pdf_object(&args->curptr, args->endptr, NULL);
  if (!tmp) {
    spc_warn(spe, "PDF object expected but not found.");
    texpdf_release_obj(category);
    return  -1;
  } else if (PDF_OBJ_ARRAYTYPE(tmp)) {
    size = texpdf_array_length(tmp);
    if (size % 2 != 0) {
      spc_warn(spe, "Array size not multiple of 2 for pdf:names.");
      texpdf_release_obj(category);
      texpdf_release_obj(tmp);
      return  -1;
    }

    for (i = 0; i < size / 2; i++) {
      key   = texpdf_get_array(tmp, 2 * i);
      value = texpdf_get_array(tmp, 2 * i + 1);
      if (!PDF_OBJ_STRINGTYPE(key)) {
        spc_warn(spe, "Name tree key must be string.");
        texpdf_release_obj(category);
        texpdf_release_obj(tmp);
        return -1;
      } else if (texpdf_doc_add_names(pdf, texpdf_name_value(category),
                                   texpdf_string_value (key),
                                   texpdf_string_length(key),
                                   texpdf_link_obj(value)) < 0) {
        spc_warn(spe, "Failed to add Name tree entry...");
        texpdf_release_obj(category);
        texpdf_release_obj(tmp);
        return -1;
      }
    }
    texpdf_release_obj(tmp);
  } else if (PDF_OBJ_STRINGTYPE(tmp)) {
    key   = tmp;
    value = texpdf_parse_pdf_object(&args->curptr, args->endptr, NULL);
    if (!value) {
      texpdf_release_obj(category);
      texpdf_release_obj(key);
      spc_warn(spe, "PDF object expected but not found.");
      return -1;
    }
    if (texpdf_doc_add_names(pdf, texpdf_name_value(category),
                          texpdf_string_value (key),
                          texpdf_string_length(key),
                          value) < 0) {
      spc_warn(spe, "Failed to add Name tree entry...");
      texpdf_release_obj(category);
      texpdf_release_obj(key);
      return -1;
    }
    texpdf_release_obj(key);
  } else {
    texpdf_release_obj(tmp);
    texpdf_release_obj(category);
    spc_warn(spe, "Invalid object type for pdf:names.");
    return  -1;
  }
  texpdf_release_obj(category);

  return 0;
}

static int
spc_handler_pdfm_docinfo (struct spc_env *spe, struct spc_arg *args)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  pdf_obj *docinfo, *dict;

  dict = texpdf_parse_pdf_dict_with_tounicode(&args->curptr, args->endptr, &sd->cd);
  if (!dict) {
    spc_warn(spe, "Dictionary object expected but not found.");
    return  -1;
  }

  docinfo = texpdf_doc_docinfo(pdf);
  texpdf_merge_dict(docinfo, dict);
  texpdf_release_obj(dict);

  return 0;
}

static int
spc_handler_pdfm_docview (struct spc_env *spe, struct spc_arg *args)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  pdf_obj   *catalog,  *dict;
  pdf_obj   *pref_old, *pref_add;

  dict = texpdf_parse_pdf_dict_with_tounicode(&args->curptr, args->endptr, &sd->cd);
  if (!dict) {
    spc_warn(spe, "Dictionary object expected but not found.");
    return  -1;
  }

  catalog  = texpdf_doc_catalog(pdf);
  /* Avoid overriding whole ViewerPreferences */
  pref_old = texpdf_lookup_dict(catalog, "ViewerPreferences");
  pref_add = texpdf_lookup_dict(dict,    "ViewerPreferences");
  if (pref_old && pref_add) {
    texpdf_merge_dict (pref_old, pref_add);
    texpdf_remove_dict(dict, "ViewerPreferences");
  }
  texpdf_merge_dict (catalog, dict);
  texpdf_release_obj(dict);

  return 0;
}

static int
spc_handler_pdfm_close (struct spc_env *spe, struct spc_arg *args)
{
  char *ident;

  texpdf_skip_white(&args->curptr, args->endptr);
  ident = texpdf_parse_opt_ident(&args->curptr, args->endptr);
  if (ident) {
    spc_flush_object(ident);
    RELEASE(ident);
  } else { /* Close all? */
    spc_clear_objects();
  }

  return 0;
}

static int
spc_handler_pdfm_object (struct spc_env *spe, struct spc_arg *args)
{
  char    *ident;
  pdf_obj *object;

  texpdf_skip_white(&args->curptr, args->endptr);
  ident = texpdf_parse_opt_ident(&args->curptr, args->endptr);
  if (!ident) {
    spc_warn(spe, "Could not find a object identifier.");
    return  -1;
  }

  object = texpdf_parse_pdf_object(&args->curptr, args->endptr, NULL);
  if (!object) {
    spc_warn(spe, "Could not find an object definition for \"%s\".", ident);
    RELEASE(ident);
    return  -1;
  } else {
    spc_push_object(ident, object);
  }
  RELEASE(ident);

  return 0;
}

static int
spc_handler_pdfm_content (struct spc_env *spe, struct spc_arg *args)
{
  long  len = 0;

  texpdf_skip_white(&args->curptr, args->endptr);
  if (args->curptr < args->endptr) {
    pdf_tmatrix M;

    pdf_setmatrix(&M, 1.0, 0.0, 0.0, 1.0, spe->x_user, spe->y_user);
    work_buffer[len++] = ' ';
    work_buffer[len++] = 'q';
    work_buffer[len++] = ' ';
    len += texpdf_sprint_matrix(work_buffer + len, &M);
    work_buffer[len++] = ' ';
    work_buffer[len++] = 'c';
    work_buffer[len++] = 'm';
    work_buffer[len++] = ' ';

    texpdf_doc_add_page_content(pdf, work_buffer, len);  /* op: q cm */
    len = (long) (args->endptr - args->curptr);
    texpdf_doc_add_page_content(pdf, args->curptr, len);  /* op: ANY */
    texpdf_doc_add_page_content(pdf, " Q", 2);  /* op: Q */
  }
  args->curptr = args->endptr;

  return 0;
}

static int
spc_handler_pdfm_literal (struct spc_env *spe, struct spc_arg *args)
{
  int       direct = 0;

  texpdf_skip_white(&args->curptr, args->endptr);
  while (args->curptr < args->endptr) {
    if (args->curptr + 7 <= args->endptr &&
	!strncmp(args->curptr, "reverse", 7)) {
      args->curptr += 7;
      WARN("The special \"pdf:literal reverse ...\" is no longer supported.\nIgnore the \"reverse\" option.");
    } else if (args->curptr + 6 <= args->endptr &&
	       !strncmp(args->curptr, "direct", 6)) {
      direct      = 1;
      args->curptr += 6;
    } else {
      break;
    }
    texpdf_skip_white(&args->curptr, args->endptr);
  }

  if (args->curptr < args->endptr) {
    pdf_tmatrix M;
    if (!direct) {
      M.a = M.d = 1.0; M.b = M.c = 0.0;
      M.e = spe->x_user; M.f = spe->y_user;
      texpdf_dev_concat(pdf, &M);
    }
    texpdf_doc_add_page_content(pdf, " ", 1);  /* op: */
    texpdf_doc_add_page_content(pdf, args->curptr, (long) (args->endptr - args->curptr));  /* op: ANY */
    if (!direct) {
      M.e = -spe->x_user; M.f = -spe->y_user;
      texpdf_dev_concat(pdf, &M);
    }
  }

  args->curptr = args->endptr;

  return 0;
}

static int
spc_handler_pdfm_bcontent (struct spc_env *spe, struct spc_arg *args)
{
  pdf_tmatrix M;
  double xpos, ypos;

  texpdf_dev_gsave(pdf);
  texpdf_dev_get_coord(&xpos, &ypos);
  pdf_setmatrix(&M, 1.0, 0.0, 0.0, 1.0, spe->x_user - xpos, spe->y_user - ypos);
  texpdf_dev_concat(pdf, &M);
  texpdf_dev_push_coord(spe->x_user, spe->y_user);
  return 0;
}

static int
spc_handler_pdfm_econtent (struct spc_env *spe, struct spc_arg *args)
{
  texpdf_dev_pop_coord();
  texpdf_dev_grestore(pdf);
  texpdf_dev_reset_color(pdf, 0);

  return 0;
}

static int
spc_handler_pdfm_code (struct spc_env *spe, struct spc_arg *args)
{
  texpdf_skip_white(&args->curptr, args->endptr);

  if (args->curptr < args->endptr) {
    texpdf_doc_add_page_content(pdf, " ", 1);  /* op: */
    texpdf_doc_add_page_content(pdf, args->curptr, (long) (args->endptr - args->curptr));  /* op: ANY */
    args->curptr = args->endptr;
  }

  return 0;
}

static int
spc_handler_pdfm_do_nothing (struct spc_env *spe, struct spc_arg *args)
{
  args->curptr = args->endptr;
  return 0;
}

#define STRING_STREAM 0
#define FILE_STREAM   1

static int
spc_handler_pdfm_stream_with_type (struct spc_env *spe, struct spc_arg *args, int type)
{
  pdf_obj *fstream;
  long     nb_read;
  char    *ident, *instring, *fullname;
  pdf_obj *tmp;
  FILE    *fp;

  texpdf_skip_white(&args->curptr, args->endptr);

  ident = texpdf_parse_opt_ident(&args->curptr, args->endptr);
  if (!ident) {
    spc_warn(spe, "Missing objname for pdf:(f)stream.");
    return  -1;
  }

  texpdf_skip_white(&args->curptr, args->endptr);

  tmp = texpdf_parse_pdf_object(&args->curptr, args->endptr, NULL);
  if (!tmp) {
    spc_warn(spe, "Missing input string for pdf:(f)stream.");
    RELEASE(ident);
    return  -1;
  } else if (!PDF_OBJ_STRINGTYPE(tmp)) {
    spc_warn(spe, "Invalid type of input string for pdf:(f)stream.");
    texpdf_release_obj(tmp);
    RELEASE(ident);
    return  -1;
  }

  instring = texpdf_string_value(tmp);

  switch (type) {
  case FILE_STREAM:
    if (!instring) {
      spc_warn(spe, "Missing filename for pdf:fstream.");
      texpdf_release_obj(tmp);
      RELEASE(ident);
      return  -1;
    }
    fullname = kpse_find_pict(instring);
    if (!fullname) {
      spc_warn(spe, "File \"%s\" not found.", instring);
      texpdf_release_obj(tmp);
      RELEASE(ident);
      return  -1;
    }
    fp = DPXFOPEN(fullname, DPX_RES_TYPE_BINARY);
    if (!fp) {
      spc_warn(spe, "Could not open file: %s", instring);
      texpdf_release_obj(tmp);
      RELEASE(ident);
      RELEASE(fullname);
      return -1;
    }
    fstream = texpdf_new_stream(STREAM_COMPRESS);
    while ((nb_read =
	    fread(work_buffer, sizeof(char), WORK_BUFFER_SIZE, fp)) > 0)
      texpdf_add_stream(fstream, work_buffer, nb_read);
    MFCLOSE(fp);
    RELEASE(fullname);
    break;
  case STRING_STREAM:
    fstream = texpdf_new_stream(STREAM_COMPRESS);
    if (instring)
      texpdf_add_stream(fstream, instring, strlen(instring));
    break;
  default:
    texpdf_release_obj(tmp);
    RELEASE(ident);
    return -1;
  }
  texpdf_release_obj(tmp);

  /*
   * Optional dict.
   *
   *  TODO: check Length, Filter...
   */
  texpdf_skip_white(&args->curptr, args->endptr);

  if (args->curptr[0] == '<') {
    pdf_obj *stream_dict;

    stream_dict = texpdf_stream_dict(fstream);

    tmp = texpdf_parse_pdf_dict(&args->curptr, args->endptr, NULL);
    if (!tmp) {
      spc_warn(spe, "Parsing dictionary failed.");
      texpdf_release_obj(fstream);
      RELEASE(ident);
      return -1;
    }
    if (texpdf_lookup_dict(tmp, "Length")) {
      texpdf_remove_dict(tmp, "Length");
    } else if (texpdf_lookup_dict(tmp, "Filter")) {
      texpdf_remove_dict(tmp, "Filter");
    }
    texpdf_merge_dict(stream_dict, tmp);
    texpdf_release_obj(tmp);
  }

  /* Users should explicitly close this. */
  spc_push_object(ident, fstream);
  RELEASE(ident);

  return 0;
}

/*
 * STREAM: Create a PDF stream object from an input string.
 *
 *  pdf: stream @objname (input_string) [PDF_DICT]
 */
static int
spc_handler_pdfm_stream (struct spc_env *spe, struct spc_arg *args)
{
  return spc_handler_pdfm_stream_with_type (spe, args, STRING_STREAM);
}

/*
 * FSTREAM: Create a PDF stream object from an existing file.
 *
 *  pdf: fstream @objname (filename) [PDF_DICT]
 */
static int
spc_handler_pdfm_fstream (struct spc_env *spe, struct spc_arg *args)
{
  return spc_handler_pdfm_stream_with_type (spe, args, FILE_STREAM);
}

/* Grab page content as follows:
 *
 * Reference point = (x_user, y_user)
 *
 * Case 1. \special{pdf:bxobj @obj width WD height HT depth DP}
 *
 *     Grab the box with the lower-left corner (x_user, y_user-DP)
 *     and the upper right corner (x_user+WD, y_user+HT).
 *
 * Case 2. \special{pdf:bxobj @obj bbox LLX LLY URX, URY}
 *
 *     Grab the box with the lower-left corner (x_user+LLX, y_user+LLY)
 *     and the upper right corner (x_user+URX, y_user+URY).
 *
 * Note that scale, xscale, yscale, xoffset, yoffset options are ignored.
 */
static int
spc_handler_pdfm_bform (struct spc_env *spe, struct spc_arg *args)
{
  int             xobj_id;
  char           *ident;
  pdf_rect        cropbox;
  transform_info  ti;

  texpdf_skip_white(&args->curptr, args->endptr);

  ident = texpdf_parse_opt_ident(&args->curptr, args->endptr);
  if (!ident) {
    spc_warn(spe, "A form XObject must have name.");
    return  -1;
  }

  texpdf_transform_info_clear(&ti);
  if (spc_util_read_dimtrns(spe, &ti, args, NULL, 0) < 0) {
    RELEASE(ident);
    return  -1;
  }

  /* A XForm with zero dimension results in a non-invertible transformation
   * matrix. And it may result in unpredictable behaviour. It might be an
   * error in Acrobat. Bounding box with zero dimension may cause division
   * by zero.
   */
  if (ti.flags & INFO_HAS_USER_BBOX) {
    if (ti.bbox.urx - ti.bbox.llx == 0.0 ||
        ti.bbox.ury - ti.bbox.lly == 0.0) {
      spc_warn(spe, "Bounding box has a zero dimension.");
      RELEASE(ident);
      return -1;
    }
    cropbox.llx = ti.bbox.llx;
    cropbox.lly = ti.bbox.lly;
    cropbox.urx = ti.bbox.urx;
    cropbox.ury = ti.bbox.ury;
  } else {
    if (ti.width == 0.0 ||
        ti.depth + ti.height == 0.0) {
      spc_warn(spe, "Bounding box has a zero dimension.");
      RELEASE(ident);
      return -1;
    }
    cropbox.llx = 0.0;
    cropbox.lly = -ti.depth;
    cropbox.urx = ti.width;
    cropbox.ury = ti.height;
  }

  xobj_id = texpdf_doc_begin_grabbing(pdf, ident, spe->x_user, spe->y_user, &cropbox);

  if (xobj_id < 0) {
    RELEASE(ident);
    spc_warn(spe, "Couldn't start form object.");
    return -1;
  }

  spc_push_object(ident, texpdf_ximage_get_reference(xobj_id));
  RELEASE(ident);

  return 0;
}

/* An extra dictionary after exobj must be merged to the form dictionary,
 * not resource dictionary.
 * Please use pdf:put @resources (before pdf:exobj) instead.
 */
static int
spc_handler_pdfm_eform (struct spc_env *spe, struct spc_arg *args)
{
  pdf_obj   *attrib = NULL;

  texpdf_skip_white(&args->curptr, args->endptr);

  if (args->curptr < args->endptr) {
    attrib = texpdf_parse_pdf_dict(&args->curptr, args->endptr, NULL);
    if (attrib && !PDF_OBJ_DICTTYPE(attrib)) {
      texpdf_release_obj(attrib);
      attrib = NULL;
    }
  }
  texpdf_doc_end_grabbing(pdf, attrib);

  return 0;
}

/* Saved XObjects can be used as follows:
 *
 * Reference point = (x_user, y_user)
 *
 * Case 1. \special{pdf:uxobj @obj width WD height HT depth DP}
 *
 *     Scale the XObject to fit in the box
 *     [x_user, y_user-DP, x_user+WD, y_user+HT].
 *
 * Case 2. \special{pdf:uxobj @obj xscale XS yscale YS}
 *
 *     Scale the XObject with XS and YS. Note that width and xscale
 *     or height and yscale cannot be used together.
 *
 * Case 3. \special{pdf:bxobj @obj bbox LLX LLY URX, URY}
 *
 *     Scale the XObject to fit in the box
 *     [x_user+LLX, y_user+LLY, x_user+URX, y_user+URY].
 *
 * Note that xoffset and yoffset moves the reference point where the
 * lower-left corner of the XObject will be put.
 */
static int
spc_handler_pdfm_uxobj (struct spc_env *spe, struct spc_arg *args)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  int              xobj_id;
  char            *ident;
  transform_info   ti;

  texpdf_skip_white(&args->curptr, args->endptr);

  ident = texpdf_parse_opt_ident(&args->curptr, args->endptr);
  if (!ident) {
    spc_warn(spe, "No object identifier given.");
    return  -1;
  }

  texpdf_transform_info_clear(&ti);
  if (args->curptr < args->endptr) {
    if (spc_util_read_dimtrns(spe, &ti, args, NULL, 0) < 0) {
      RELEASE(ident);
      return  -1;
    }
  }

  /* Dvipdfmx was suddenly changed to use file name to identify
   * external images. We can't use ident to find image resource
   * here.
   */
  xobj_id = findresource(sd, ident);
  if (xobj_id < 0) {
    xobj_id = texpdf_ximage_findresource(pdf, ident, 0, NULL);
    if (xobj_id < 0) {
      spc_warn(spe, "Specified (image) object doesn't exist: %s", ident);
      RELEASE(ident);
      return  -1;
    }
  }

  texpdf_dev_put_image(pdf, xobj_id, &ti, spe->x_user, spe->y_user,dvi_is_tracking_boxes());
  RELEASE(ident);

  return 0;
}

static int
spc_handler_pdfm_link (struct spc_env *spe, struct spc_arg *args)
{
  return  spc_resume_annot(spe);
}

static int
spc_handler_pdfm_nolink (struct spc_env *spe, struct spc_arg *args)
{
  return  spc_suspend_annot(spe);
}



/* Handled at BOP */
static int
spc_handler_pdfm_pagesize (struct spc_env *spe, struct spc_arg *args)
{
  args->curptr = args->endptr;

  return 0;
}

/* Please remove this.
 * This should be handled before processing pages!
 */
static int
spc_handler_pdfm_bgcolor (struct spc_env *spe, struct spc_arg *args)
{
  int       error;
  pdf_color colorspec;

  error = spc_util_read_pdfcolor(spe, &colorspec, args, NULL);
  if (error)
    spc_warn(spe, "No valid color specified?");
  else {
    texpdf_doc_set_bgcolor(pdf, &colorspec);
  }

  return  error;
}

static int
spc_handler_pdfm_mapline (struct spc_env *spe, struct spc_arg *ap)
{
  fontmap_rec *mrec;
  char        *map_name, opchr;
  int          error = 0;
  static char  buffer[1024];
  const char  *p;
  char        *q;

  texpdf_skip_white(&ap->curptr, ap->endptr);
  if (ap->curptr >= ap->endptr) {
    spc_warn(spe, "Empty mapline special?");
    return  -1;
  }

  opchr = ap->curptr[0];
  if (opchr == '-' || opchr == '+')
    ap->curptr++;

  texpdf_skip_white(&ap->curptr, ap->endptr);

  switch (opchr) {
  case  '-':
    map_name = texpdf_parse_ident(&ap->curptr, ap->endptr);
    if (map_name) {
      texpdf_remove_fontmap_record(map_name);
      RELEASE(map_name);
    } else {
      spc_warn(spe, "Invalid fontmap line: Missing TFM name.");
      error = -1;
    }
    break;
  default:
    p = ap->curptr;
    q = buffer;
    while (p < ap->endptr)
      *q++ = *p++;
    *q = '\0';
    mrec = NEW(1, fontmap_rec);
    texpdf_init_fontmap_record(mrec);
    error = texpdf_read_fontmap_line(mrec, buffer, (long) (ap->endptr - ap->curptr), texpdf_is_pdfm_mapline(buffer));
    if (error)
      spc_warn(spe, "Invalid fontmap line.");
    else if (opchr == '+')
      texpdf_append_fontmap_record(mrec->map_name, mrec);
    else
      texpdf_insert_fontmap_record(mrec->map_name, mrec);
    texpdf_clear_fontmap_record(mrec);
    RELEASE(mrec);
    break;
  }
  if (!error)
    ap->curptr = ap->endptr;

  return 0;
}

static int
spc_handler_pdfm_mapfile (struct spc_env *spe, struct spc_arg *args)
{
  char  *mapfile;
  int    mode, error = 0;

  texpdf_skip_white(&args->curptr, args->endptr);
  if (args->curptr >= args->endptr)
    return 0;

  switch (args->curptr[0]) {
  case  '-':
    mode = FONTMAP_RMODE_REMOVE;
    args->curptr++;
    break;
  case  '+':
    mode = FONTMAP_RMODE_APPEND;
    args->curptr++;
    break;
  default:
    mode = FONTMAP_RMODE_REPLACE;
    break;
  }

  mapfile = texpdf_parse_val_ident(&args->curptr, args->endptr);
  if (!mapfile) {
    spc_warn(spe, "No fontmap file specified.");
    return  -1;
  } else {
    error = texpdf_load_fontmap_file(mapfile, mode);
  }
  RELEASE(mapfile);

  return  error;
}


static int
spc_handler_pdfm_tounicode (struct spc_env *spe, struct spc_arg *args)
{
  struct spc_pdf_ *sd = &_pdf_stat;
  char *cmap_name;

  /* First clear */
  sd->cd.cmap_id = -1;
  sd->cd.unescape_backslash = 0;

  texpdf_skip_white(&args->curptr, args->endptr);
  if (args->curptr >= args->endptr) {
    spc_warn(spe, "Missing CMap name for pdf:tounicode.");
    return  -1;
  }

  /* _FIXME_
   * Any valid char allowed for PDF name object should be allowed here.
   * The argument to this special should be a PDF name obejct.
   * But it's too late to change this special.
   */
  cmap_name = texpdf_parse_ident(&args->curptr, args->endptr);
  if (!cmap_name) {
    spc_warn(spe, "Missing ToUnicode mapping name...");
    return -1;
  }

  sd->cd.cmap_id = texpdf_CMap_cache_find(cmap_name);
  if (sd->cd.cmap_id < 0) {
    spc_warn(spe, "Failed to load ToUnicode mapping: %s", cmap_name);
    RELEASE(cmap_name);
    return -1;
  }

  /* Shift-JIS like encoding may contain backslash in 2nd byte.
   * WARNING: This will add nasty extension to PDF parser.
   */
  if (sd->cd.cmap_id >= 0) {
    if (strstr(cmap_name, "RKSJ") ||
        strstr(cmap_name, "B5")   ||
        strstr(cmap_name, "GBK")  ||
        strstr(cmap_name, "KSC"))
      sd->cd.unescape_backslash = 1;
  }
  RELEASE(cmap_name);
  return 0;
}


static struct spc_handler pdfm_handlers[] = {
  {"annotation", spc_handler_pdfm_annot},
  {"annotate",   spc_handler_pdfm_annot},
  {"annot",      spc_handler_pdfm_annot},
  {"ann",        spc_handler_pdfm_annot},

  {"outline",    spc_handler_pdfm_outline},
  {"out",        spc_handler_pdfm_outline},

  {"article",    spc_handler_pdfm_article},
  {"art",        spc_handler_pdfm_article},

  {"bead",       spc_handler_pdfm_bead},
  {"thread",     spc_handler_pdfm_bead},

  {"destination", spc_handler_pdfm_dest}, 
  {"dest",        spc_handler_pdfm_dest},


  {"object",      spc_handler_pdfm_object},
  {"obj",         spc_handler_pdfm_object},


  {"docinfo",     spc_handler_pdfm_docinfo},
  {"docview",     spc_handler_pdfm_docview},

  {"content",     spc_handler_pdfm_content},
  {"put",         spc_handler_pdfm_put},
  {"close",       spc_handler_pdfm_close},
  {"bop",         spc_handler_pdfm_bop},
  {"eop",         spc_handler_pdfm_eop},

  {"image",       spc_handler_pdfm_image},
  {"img",         spc_handler_pdfm_image},
  {"epdf",        spc_handler_pdfm_image},

  {"link",        spc_handler_pdfm_link},
  {"nolink",      spc_handler_pdfm_nolink},

  {"begincolor",  spc_handler_pdfm_bcolor},
  {"bcolor",      spc_handler_pdfm_bcolor},
  {"bc",          spc_handler_pdfm_bcolor},

  {"setcolor",    spc_handler_pdfm_scolor},
  {"scolor",      spc_handler_pdfm_scolor},
  {"sc",          spc_handler_pdfm_scolor},

  {"endcolor",    spc_handler_pdfm_ecolor},
  {"ecolor",      spc_handler_pdfm_ecolor},
  {"ec",          spc_handler_pdfm_ecolor},

  {"begingray",   spc_handler_pdfm_bcolor},
  {"bgray",       spc_handler_pdfm_bcolor},
  {"bg",          spc_handler_pdfm_bcolor},

  {"endgray",     spc_handler_pdfm_ecolor},
  {"egray",       spc_handler_pdfm_ecolor},
  {"eg",          spc_handler_pdfm_ecolor},

  {"bgcolor",     spc_handler_pdfm_bgcolor},
  {"bgc",         spc_handler_pdfm_bgcolor},
  {"bbc",         spc_handler_pdfm_bgcolor},
  {"bbg",         spc_handler_pdfm_bgcolor},

  {"pagesize",    spc_handler_pdfm_pagesize},

  {"bannot",      spc_handler_pdfm_bann},
  {"beginann",    spc_handler_pdfm_bann},
  {"bann",        spc_handler_pdfm_bann},

  {"eannot",      spc_handler_pdfm_eann},
  {"endann",      spc_handler_pdfm_eann},
  {"eann",        spc_handler_pdfm_eann},

  {"btrans",         spc_handler_pdfm_btrans},
  {"begintransform", spc_handler_pdfm_btrans},
  {"begintrans",     spc_handler_pdfm_btrans},
  {"bt",             spc_handler_pdfm_btrans},

  {"etrans",         spc_handler_pdfm_etrans},
  {"endtransform",   spc_handler_pdfm_etrans},
  {"endtrans",       spc_handler_pdfm_etrans},
  {"et",             spc_handler_pdfm_etrans},

  {"bform",          spc_handler_pdfm_bform},
  {"beginxobj",      spc_handler_pdfm_bform},
  {"bxobj",          spc_handler_pdfm_bform},

  {"eform",          spc_handler_pdfm_eform},
  {"endxobj",        spc_handler_pdfm_eform},
  {"exobj",          spc_handler_pdfm_eform},

  {"usexobj",        spc_handler_pdfm_uxobj},
  {"uxobj",          spc_handler_pdfm_uxobj},

  {"tounicode",  spc_handler_pdfm_tounicode},
  {"literal",    spc_handler_pdfm_literal},
  {"stream",     spc_handler_pdfm_stream},
  {"fstream",    spc_handler_pdfm_fstream},
  {"names",      spc_handler_pdfm_names},
  {"mapline",    spc_handler_pdfm_mapline},
  {"mapfile",    spc_handler_pdfm_mapfile},

  {"bcontent",   spc_handler_pdfm_bcontent},
  {"econtent",   spc_handler_pdfm_econtent},
  {"code",       spc_handler_pdfm_code},

  {"minorversion", spc_handler_pdfm_do_nothing},
  {"encrypt",      spc_handler_pdfm_do_nothing},
};

int
spc_pdfm_check_special (const char *buf, long len)
{
  int    r = 0;
  const char *p, *endptr;

  p      = buf;
  endptr = p + len;

  texpdf_skip_white(&p, endptr);
  if (p + strlen("pdf:") <= endptr &&
      !memcmp(p, "pdf:", strlen("pdf:"))) {
    r = 1;
  }

  return  r;
}

int
spc_pdfm_setup_handler (struct spc_handler *sph,
                        struct spc_env *spe, struct spc_arg *ap)
{
  int    error = -1, i;
  char  *q;

  ASSERT(sph && spe && ap);

  texpdf_skip_white(&ap->curptr, ap->endptr);
  if (ap->curptr + strlen("pdf:") >= ap->endptr ||
      memcmp(ap->curptr, "pdf:", strlen("pdf:"))) {
    spc_warn(spe, "Not pdf: special???");
    return  -1;
  }
  ap->curptr += strlen("pdf:");

  texpdf_skip_white(&ap->curptr, ap->endptr);
  q = texpdf_parse_c_ident(&ap->curptr, ap->endptr);
  if (q) {
    for (i = 0;
         i < sizeof(pdfm_handlers) / sizeof(struct spc_handler); i++) {
      if (!strcmp(q, pdfm_handlers[i].key)) {
        ap->command = pdfm_handlers[i].key;
        sph->key   = "pdf:";
        sph->exec  = pdfm_handlers[i].exec;
        texpdf_skip_white(&ap->curptr, ap->endptr);
        error = 0;
        break;
      }
    }
    RELEASE(q);
  }

  return  error;
}

