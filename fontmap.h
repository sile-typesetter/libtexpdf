extern fontmap_t *dvi_fontmap;
void dvipdf_init_fontmaps (void);

extern int          texpdf_load_fontmap_file     (const char  *filename, int mode);
extern int          texpdf_read_fontmap_line     (fontmap_rec *mrec, const char *mline, long mline_strlen, int format);

extern int          texpdf_append_fontmap_record (const char  *kp, const fontmap_rec *mrec);
extern int          texpdf_remove_fontmap_record (const char  *kp);
extern int          texpdf_is_pdfm_mapline           (const char  *mline);

