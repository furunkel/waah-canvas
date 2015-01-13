#pragma once

#include <cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H

typedef struct yeah_canvas_s {
  cairo_t *cr;
  cairo_surface_t *surface;
  int width;
  int height;
  void (*free_func)(mrb_state *, void *ptr);
} yeah_canvas_t;

typedef struct yeah_image_s {
  unsigned char *data;
  cairo_surface_t *surface;
} yeah_image_t;

typedef struct yeah_font_s {
  FT_Face ft_face;
  cairo_font_face_t *cr_face;
} yeah_font_t;

typedef struct yeah_path_s {
  cairo_path_t *cr_path;
} yeah_path_t;

typedef struct yeah_pattern_s {
  cairo_pattern_t *cr_pattern;
} yeah_pattern_t;


struct yeah_img_buf {
  unsigned char *data;
  size_t off;
  size_t len;
};

mrb_value
_yeah_image_load(mrb_state *mrb, mrb_value self, int (*png)(mrb_state *, yeah_image_t *, const char *),
                                                int (*jpeg)(mrb_state *, yeah_image_t *, const char *));

int
_yeah_load_jpeg_from_file(mrb_state *mrb, yeah_image_t *image, FILE *file);

int
_yeah_load_png_from_buffer(mrb_state *mrb, yeah_image_t *image, unsigned char *data, size_t len);
