#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libtexpdf.h>

double precision = 65536.0;
char* producer = "SILE";

pdf_doc *pdf_init (const char *fn, double w, double height) {
  pdf_doc *p = texpdf_open_document(fn, 0, w, height, 0,0,0);
  texpdf_init_device(p, 1 / precision, 2, 0);

  pdf_rect mediabox;
  mediabox.llx = 0.0;
  mediabox.lly = 0.0;
  mediabox.urx = w;
  mediabox.ury = height;
  texpdf_files_init();
  texpdf_init_fontmaps();
  texpdf_tt_aux_set_always_embed();
  texpdf_doc_set_mediabox(p, 0, &mediabox);
  texpdf_add_dict(p->info,
               texpdf_new_name("Producer"),
               texpdf_new_string(producer, strlen(producer)));
  return p;
}

int pdf_finish(pdf_doc *p) {
  texpdf_files_close();
  texpdf_close_device();
  texpdf_close_document(p);
  texpdf_close_fontmaps();
  return 0;
}

int pdf_add_content(pdf_doc *p, const char *input) {
  int input_l = strlen(input);
  texpdf_graphics_mode(p); /* Don't be mid-string! */
  texpdf_doc_add_page_content(p, " ", 1);
  texpdf_doc_add_page_content(p, input, input_l);
  texpdf_doc_add_page_content(p, " ", 1);
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <output-file>\n", argv[0]);
    return 1;
  }
  const char *filename = argv[1];
  const double width = 400.0, height = 600.0;
  pdf_doc *p = pdf_init(filename, width, height);
  texpdf_doc_begin_page(p, 1, 0, height);
  pdf_add_content(p, "Hello world!");
  texpdf_doc_end_page(p);
  pdf_finish(p);
  return 0;
}
