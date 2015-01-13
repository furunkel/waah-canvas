#pragma once

#include <cairo.h>
#include <cairo/cairo-ft.h>


typedef struct waah_canvas_s {
  cairo_t *cr;
  cairo_surface_t *surface;
  int width;
  int height;
  void (*free_func)(mrb_state *, void *ptr);
} waah_canvas_t;

typedef struct waah_image_s {
  unsigned char *data;
  cairo_surface_t *surface;
} waah_image_t;

typedef struct waah_font_s {
  FT_Face ft_face;
  cairo_font_face_t *cr_face;
#ifdef CAIRO_HAS_FC_FONT
  FcPattern *fc_pattern;
#endif
} waah_font_t;

typedef struct waah_path_s {
  cairo_path_t *cr_path;
} waah_path_t;

typedef struct waah_pattern_s {
  cairo_pattern_t *cr_pattern;
} waah_pattern_t;


struct waah_img_buf {
  unsigned char *data;
  size_t off;
  size_t len;
};

mrb_value
_waah_image_load(mrb_state *mrb, mrb_value self, int (*png)(mrb_state *, waah_image_t *, const char *),
                                                int (*jpeg)(mrb_state *, waah_image_t *, const char *));

int
_waah_load_jpeg_from_file(mrb_state *mrb, waah_image_t *image, FILE *file);

int
_waah_load_png_from_buffer(mrb_state *mrb, waah_image_t *image, unsigned char *data, size_t len);
