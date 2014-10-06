/* Proof of concept for using the library.

Compile and link:
gcc -g -o library-poc  -I.. ./.libs/libtexpdf.a library-poc.c -lpng -lz `pkg-config --cflags --libs freetype2`

./library-poc ./SomeFont.ttf

*/

#include <stdint.h>
#include "libtexpdf/libtexpdf.h"

int load_font (char* filename) {
  char fontmap_key[1024];
  uint32_t index = 0;
  int layout_dir = 0;
  int extend = 65536;
  int slant = 0;
  int embolden = 0;
  double ptsize = 12.0 * 1.5;
  fontmap_rec  *mrec;
  
  return texpdf_dev_load_native_font(filename, index, ptsize, layout_dir, extend, slant, embolden);
}

int main(int argc, char** argv) {
  pdf_rect mediabox;
  int font_id;
  pdf_doc *p = texpdf_open_document("test.pdf", 0, 595.275597, 841.8897728999999, 0,0,0);
  texpdf_init_device(p, 1, 2, 0);

  mediabox.llx = 0.0;
  mediabox.lly = 0.0;
  mediabox.urx = 595.275597;
  mediabox.ury = 841.8897728999999;
  texpdf_doc_set_verbose();
  texpdf_init_fontmaps();
  texpdf_doc_set_mediabox(p, 0, &mediabox);
  texpdf_doc_begin_page(p, 1.0,72.0,770.0);
  font_id = load_font(argv[1]);
  printf("ID: %i\n", font_id);
  texpdf_dev_set_string(p, 92.0,-10.0, "HIJKLMNO", 7, 0, font_id, 1);

    texpdf_doc_end_page(p);
    texpdf_close_document(p);

  texpdf_close_device  ();
  texpdf_close_fontmaps();    
}
