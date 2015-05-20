#include <mruby.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/array.h>
#include <mruby/value.h>
#include <mruby/variable.h>
#include <mruby/string.h>

#include "waah-canvas.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <jpeglib.h>

#include <cairo.h>
#include <cairo/cairo-ft.h>

static mrb_sym id_instance_eval;
static mrb_sym id_normal;
static mrb_sym id_italic;
static mrb_sym id_oblique;
static mrb_sym id_bold;

struct RClass *mWaah;
struct RClass *cCanvas;
struct RClass *cImage;
struct RClass *cFont;
struct RClass *cPath;
struct RClass *cPattern;

static FT_Library ft_lib;

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static void
canvas_free(mrb_state *mrb, void *ptr) {
  waah_canvas_t *canvas = (waah_canvas_t *) ptr;

  if(canvas->cr != NULL) {
    cairo_destroy(canvas->cr);
  }

  if(canvas->surface != NULL) {
    cairo_surface_destroy(canvas->surface);
  }

  if(canvas->free_func != NULL) {
    (*canvas->free_func)(mrb, ptr);
  }

  mrb_free(mrb, ptr);

}

static void
image_free(mrb_state *mrb, void *ptr) {
  waah_image_t *image = (waah_image_t *) ptr;
  if(image->surface != NULL) {
    cairo_surface_destroy(image->surface);
  }
  if(image->data != NULL) {
    mrb_free(mrb, image->data);
  }
  mrb_free(mrb, ptr);
}

static void
font_free(mrb_state *mrb, void *ptr) {
  waah_font_t *font = (waah_font_t *) ptr;

  if(font->cr_face != NULL) {
    cairo_font_face_destroy(font->cr_face);
  }
  if(font->ft_face != NULL) {
    FT_Done_Face(font->ft_face);
  }

#ifdef CAIRO_HAS_FC_FONT
  if(font->fc_pattern != NULL) {
    FcPatternDestroy(font->fc_pattern);
  }
#endif
  mrb_free(mrb, ptr);
}

static void
pattern_free(mrb_state *mrb, void *ptr) {
  waah_pattern_t *pattern = (waah_pattern_t *) ptr;

  if(pattern->cr_pattern != NULL) {
    cairo_pattern_destroy(pattern->cr_pattern);
  }
  mrb_free(mrb, ptr);
}

static void
path_free(mrb_state *mrb, void *ptr) {
  waah_path_t *path = (waah_path_t *) ptr;

  if(path->cr_path != NULL) {
    cairo_path_destroy(path->cr_path);
  }
  mrb_free(mrb, ptr);
}


#define CANVAS_DEFAULT_DECLS \
  waah_canvas_t *canvas;\
  cairo_t *cr;\
  (void) cr;

#define CANVAS_DEFAULT_DECL_INITS \
  Data_Get_Struct(mrb, self, &_waah_canvas_type_info, canvas);\
  cr = canvas->cr;



struct mrb_data_type _waah_canvas_type_info = {"Canvas", canvas_free};
struct mrb_data_type _waah_image_type_info = {"Image", image_free};
struct mrb_data_type _waah_font_type_info = {"Font", font_free};
struct mrb_data_type _waah_pattern_type_info = {"Pattern", pattern_free};
struct mrb_data_type _waah_path_type_info = {"Path", path_free};

static int raise_cairo_status(mrb_state *mrb, cairo_status_t status) {
  switch(status) {
    case CAIRO_STATUS_NO_MEMORY:
      mrb_raise(mrb, E_RUNTIME_ERROR, "no memory");
      break;
    case CAIRO_STATUS_FILE_NOT_FOUND:
      mrb_raise(mrb, E_RUNTIME_ERROR, "file not found");
      break;
    case CAIRO_STATUS_READ_ERROR:
      mrb_raise(mrb, E_RUNTIME_ERROR, "read error");
      break;
    case CAIRO_STATUS_SURFACE_TYPE_MISMATCH:
      mrb_raise(mrb, E_RUNTIME_ERROR, "surface type mismatch");
      break;
    case CAIRO_STATUS_WRITE_ERROR:
      mrb_raise(mrb, E_RUNTIME_ERROR, "write error");
      break;
    default:
      return FALSE;
  }
  return TRUE;
}


static mrb_value
image_new(mrb_state *mrb, waah_image_t **rimage) {
  waah_image_t *image = (waah_image_t *) mrb_calloc(mrb, sizeof(waah_image_t), 1);
  mrb_value mrb_image = mrb_class_new_instance(mrb, 0, NULL, cImage);

  DATA_PTR(mrb_image) = image;
  DATA_TYPE(mrb_image) = &_waah_image_type_info;

  *rimage = image;

  return mrb_image;
}

mrb_value
font_new(mrb_state *mrb, waah_font_t **rfont) {
  waah_font_t *font = (waah_font_t *) mrb_calloc(mrb, sizeof(waah_font_t), 1);
  mrb_value mrb_font = mrb_class_new_instance(mrb, 0, NULL, cFont);

  DATA_PTR(mrb_font) = font;
  DATA_TYPE(mrb_font) = &_waah_font_type_info;

  *rfont = font;

  return mrb_font;
}

static mrb_value
pattern_new(mrb_state *mrb, waah_pattern_t **rpattern) {
  waah_pattern_t *pattern = (waah_pattern_t *) mrb_calloc(mrb, sizeof(waah_pattern_t), 1);
  mrb_value mrb_pattern = mrb_class_new_instance(mrb, 0, NULL, cPattern);

  DATA_PTR(mrb_pattern) = pattern;
  DATA_TYPE(mrb_pattern) = &_waah_pattern_type_info;

  *rpattern = pattern;

  return mrb_pattern;
}



static cairo_status_t
read_png_from_buffer (void *closure,
                      unsigned char *data,
                      unsigned int length) {
  struct waah_img_buf *buf = (struct waah_img_buf *) closure;

  if(buf->off < buf->len) {
    size_t l = MIN(length, buf->len - buf->off);
    memcpy(data, buf->data + buf->off, l);
    buf->off += l;
  }
  return CAIRO_STATUS_SUCCESS;
}

int
_waah_load_png_from_buffer(mrb_state *mrb, waah_image_t *image, unsigned char *data, size_t len) {
  struct waah_img_buf buf = {.data = data, .len = len, .off = 0};
  image->surface = cairo_image_surface_create_from_png_stream(read_png_from_buffer, &buf);
  cairo_status_t status = cairo_surface_status(image->surface);
  if(raise_cairo_status(mrb, status)) {
    return FALSE;
  }
  return TRUE;
}


static int
load_png(mrb_state *mrb, waah_image_t *image, const char *filename) {
  image->surface = cairo_image_surface_create_from_png(filename);

  cairo_status_t status = cairo_surface_status(image->surface);
  if(raise_cairo_status(mrb, status)) {
    return FALSE;
  }

  return TRUE;
}


int
_waah_load_jpeg_from_file(mrb_state *mrb, waah_image_t *image, FILE *file) {

  if(file == NULL) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "file not found");
    return FALSE;
  }

  struct jpeg_decompress_struct info;
  struct jpeg_error_mgr err;
  int w, h, n_channels, stride, offset = 0;
  JSAMPARRAY row_buffer;

  info.err = jpeg_std_error(&err);     
  jpeg_create_decompress(&info); 

  jpeg_stdio_src(&info, file);
  jpeg_read_header(&info, TRUE);

  jpeg_start_decompress(&info);

  w = info.output_width;
  h = info.output_height;
  n_channels = info.num_components;
  
  stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, w);
  image->data  = mrb_malloc(mrb, stride * h);

  row_buffer = (JSAMPARRAY)mrb_malloc(mrb, sizeof(JSAMPROW));
  row_buffer[0] = (JSAMPROW)mrb_malloc(mrb, sizeof(JSAMPLE) * info.output_width * n_channels);

  assert(info.output_width * n_channels <= stride);

  while(info.output_scanline < info.output_height) {
    int i;

    jpeg_read_scanlines(&info, row_buffer, 1);
    for(i = 0; i < w; i++) {
      image->data[offset + 4 * i + 2] = row_buffer[0][n_channels * i];
      image->data[offset + 4 * i + 1] = row_buffer[0][n_channels * i + MIN(n_channels - 1, 1)];
      image->data[offset + 4 * i + 0] = row_buffer[0][n_channels * i + MIN(n_channels - 1, 2)];
      image->data[offset + 4 * i + 3] = 255;
    }
    offset += stride;

  }

  mrb_free(mrb, row_buffer[0]);
  mrb_free(mrb, row_buffer);

  image->surface = cairo_image_surface_create_for_data(image->data, CAIRO_FORMAT_RGB24, w, h, stride);

  jpeg_finish_decompress(&info);
  jpeg_destroy_decompress(&info);
  fclose(file);

  return TRUE;
}

static int
load_jpeg(mrb_state *mrb, waah_image_t *image, const char *filename) {
  FILE *file = fopen(filename, "rb" );
  return _waah_load_jpeg_from_file(mrb, image, file);
}

mrb_value
_waah_image_load(mrb_state *mrb, mrb_value self, int (*png)(mrb_state *, waah_image_t *, const char *),
                                                int (*jpeg)(mrb_state *, waah_image_t *, const char *)) {

  waah_image_t *image;
  mrb_value mrb_image = image_new(mrb, &image);

  char *filename;
  mrb_int len;

  mrb_get_args(mrb, "s", &filename, &len);
  
  if(len > 4 &&
     filename[len - 4] == '.' &&
     filename[len - 3] == 'p' &&
     filename[len - 2] == 'n' &&
     filename[len - 1] == 'g') {

      if(!((*png)(mrb, image, filename))) {
        goto error;
      }
  } else if( (len > 4 &&
              filename[len - 4] == '.' &&
              filename[len - 3] == 'j' &&
              filename[len - 2] == 'p' &&
              filename[len - 1] == 'g') ||
              (len > 5 &&
              filename[len - 5] == '.' &&
              filename[len - 4] == 'j' &&
              filename[len - 3] == 'p' &&
              filename[len - 2] == 'e' &&
              filename[len - 1] == 'g'
             )) {
    if(!((*jpeg)(mrb, image, filename))) {
      goto error;
    }
  } else {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown image format");
    return mrb_nil_value();
  }

  return mrb_image;

error:
  return mrb_nil_value();

}

static mrb_value
image_load(mrb_state *mrb, mrb_value self) {
  return _waah_image_load(mrb, self, load_png, load_jpeg);
}

int
_waah_font_load_from_filename(mrb_state *mrb, waah_font_t *font, const char *filename) {
  if(FT_New_Face(ft_lib,
                 filename,
                 0,
                 &font->ft_face) != FT_Err_Ok) {
    return FALSE;
  }
  return TRUE;
}

int
_waah_font_load_from_buffer(mrb_state *mrb, waah_font_t *font, unsigned char *buf, size_t len) {
  FT_Open_Args args;

  args.flags = FT_OPEN_MEMORY;
  args.memory_base = buf;
  args.memory_size = len;
  if(FT_Open_Face(ft_lib,
                  &args,
                  0,
                  &font->ft_face) != FT_Err_Ok) {
    return FALSE;
  }
  return TRUE;
}

static mrb_value
font_load(mrb_state *mrb, mrb_value self) {
  char *filename;
  waah_font_t *font;
  mrb_value mrb_font = font_new(mrb, &font);
  mrb_int len;
  mrb_get_args(mrb, "s", &filename, &len);

  if(!_waah_font_load_from_filename(mrb, font, filename)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "font loading failed");
    return mrb_nil_value();
  } else {
    return mrb_font;
  }
}


static mrb_value
font_find(mrb_state *mrb, mrb_value self) {
#ifdef CAIRO_HAS_FC_FONT
  char *name;
  waah_font_t *font;
  mrb_value mrb_font = font_new(mrb, &font);
  mrb_int len;
  FcConfig* config;
  FcPattern* pat;
  FcResult result;

  mrb_get_args(mrb, "s", &name, &len);
  config = FcConfigGetCurrent();
  pat = FcNameParse((const FcChar8*)name);
  FcConfigSubstitute(config, pat, FcMatchPattern);
  FcDefaultSubstitute(pat);
  font->fc_pattern = FcFontMatch(config, pat, &result);
  FcPatternDestroy(pat);

  return mrb_font;

#else
  return mrb_nil_value();
#endif
}


static mrb_value
font_list(mrb_state *mrb, mrb_value self) {
  mrb_value ary = mrb_ary_new(mrb);
#ifdef CAIRO_HAS_FC_FONT
  FcPattern *pattern;
  FcFontSet *font_set;
  FcObjectSet *os;
  FcConfig *config;
  int i;

  config = FcConfigGetCurrent();
  FcConfigSetRescanInterval(config, 0);
  pattern = FcPatternCreate();
  os = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_LANG, (char *) 0);
  font_set = FcFontList(config, pattern, os);
  for(i = 0;font_set && i < font_set->nfont; i++) {
    waah_font_t *font;
    mrb_value mrb_font = font_new(mrb, &font);
    mrb_ary_push(mrb, ary, mrb_font);
    font->fc_pattern = font_set->fonts[i];//FcFontSetFont(font_set, i);
    FcPatternReference(font->fc_pattern);
  }
  if(font_set) FcFontSetDestroy(font_set);

#endif

  return ary;
}

static mrb_value
font_name(mrb_state *mrb, mrb_value self) {
  waah_font_t *font;
  mrb_value retval;
  char *name = NULL;
  int free_name = FALSE;

  Data_Get_Struct(mrb, self, &_waah_font_type_info, font);

  if(font->ft_face != NULL) {
    name = (char *) FT_Get_Postscript_Name(font->ft_face);
  }

#ifdef CAIRO_HAS_FC_FONT
  else if(font->fc_pattern != NULL) {
    name = (char *) FcNameUnparse(font->fc_pattern);
    free_name = TRUE;
  }
#endif

  if(name != NULL) {
    retval = mrb_str_new_cstr(mrb, name);
  } else {
    retval = mrb_nil_value();
  }

  if(free_name) {
    free(name);
  }

  return retval;
}




static mrb_value
font_get_prop(mrb_state *mrb, mrb_value self, const char *prop) {
  waah_font_t *font;
  mrb_value retval;
  char *name = NULL;
  int free_name = FALSE;

  Data_Get_Struct(mrb, self, &_waah_font_type_info, font);

#ifdef CAIRO_HAS_FC_FONT
  if(font->fc_pattern != NULL) {
    if(FcPatternGetString(font->fc_pattern,
          prop, 0, (FcChar8 **)&name) != FcResultMatch) {
      name = NULL;
    }
    free_name = FALSE;
  }
#endif

  if(name != NULL) {
    retval = mrb_str_new_cstr(mrb, name);
  } else {
    retval = mrb_nil_value();
  }

  if(free_name) {
    free(name);
  }

  return retval;
}

static mrb_value
font_family(mrb_state *mrb, mrb_value self) {
#ifdef CAIRO_HAS_FC_FONT
  return font_get_prop(mrb, self, FC_FAMILY);
#else
  return mrb_nil_value();
#endif
}

static mrb_value
font_style(mrb_state *mrb, mrb_value self) {
#ifdef CAIRO_HAS_FC_FONT
  return font_get_prop(mrb, self, FC_STYLE);
#else
  return mrb_nil_value();
#endif
}

static mrb_value
path_initialize(mrb_state *mrb, mrb_value self) {
  waah_path_t *path = (waah_path_t *) mrb_calloc(mrb, sizeof(waah_path_t), 1);

  DATA_PTR(self) = path;
  DATA_TYPE(self) = &_waah_path_type_info;

  return self;
}

static mrb_value
canvas_initialize(mrb_state *mrb, mrb_value self) {
  waah_canvas_t *canvas = (waah_canvas_t *) mrb_calloc(mrb, sizeof(waah_canvas_t), 1);

  DATA_PTR(self) = canvas;
  DATA_TYPE(self) = &_waah_canvas_type_info;

  mrb_int w, h;
  mrb_get_args(mrb, "ii", &w, &h);

  canvas->width = w;
  canvas->height = h;
  canvas->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, canvas->width, canvas->height);
  canvas->cr = cairo_create(canvas->surface);

  return self;
}

static mrb_value
canvas_color(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_int r, g, b;
  double a = 1.0;
  mrb_value _a;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_int n_args = mrb_get_args(mrb, "iii|o", &r, &g, &b, &_a);

  if(n_args > 3) {
    switch (mrb_type(_a)) {
        case MRB_TT_FIXNUM:
          a = (double)mrb_fixnum(_a) / 255.0;
          break;
        case MRB_TT_FLOAT:
          a = mrb_float(_a);
          break;
        default:
          mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid alpha argument");
          return self;
    }
  }


  cairo_set_source_rgba(cr, (double)r / 255.0, (double)g / 255.0, (double)b / 255.0, a);

  return self;
}


static mrb_value
pattern_linear(mrb_state *mrb, mrb_value self) {

  waah_pattern_t *pattern;
  mrb_float x0, y0, x1, y1;
  mrb_value mrb_pattern = pattern_new(mrb, &pattern);

  mrb_get_args(mrb, "ffff", &x0, &y0, &x1, &y1);

  pattern->cr_pattern  = cairo_pattern_create_linear(x0,
                                                     y0,
                                                     x1,
                                                     y1);

  return mrb_pattern;
}

static mrb_value
pattern_radial(mrb_state *mrb, mrb_value self) {

  waah_pattern_t *pattern;
  mrb_float cx0, cy0, radius0, cx1, cy1, radius1;

  mrb_value mrb_pattern = pattern_new(mrb, &pattern);

  mrb_get_args(mrb, "ffffff", &cx0, &cy0, &radius0, &cx1, &cy1, &radius1);

  pattern->cr_pattern  = cairo_pattern_create_radial(cx0,
                                                     cy0,
                                                     radius0,
                                                     cx1,
                                                     cy1,
                                                     radius1);

  return mrb_pattern;
}

static mrb_value
pattern_color_stop(mrb_state *mrb, mrb_value self) {
  mrb_int r, g, b;
  double a = 1.0;
  mrb_value _a;
  mrb_float off;
  waah_pattern_t *pattern;
  Data_Get_Struct(mrb, self, &_waah_pattern_type_info, pattern);

  mrb_int n_args = mrb_get_args(mrb, "fiii|o", &off, &r, &g, &b, &_a);

  if(n_args > 4) {
    switch (mrb_type(_a)) {
        case MRB_TT_FIXNUM:
          a = (double)mrb_fixnum(_a) / 255.0;
          break;
        case MRB_TT_FLOAT:
          a = mrb_float(_a);
          break;
        default:
          mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid alpha argument");
          return self;
    }
  }

  cairo_pattern_add_color_stop_rgba(pattern->cr_pattern,
                                    off, (double)r / 255.0,
                                    (double)g / 255.0, (double)b / 255.0, a);

  return self;
}

static mrb_value
canvas_image(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_value mrb_image;
  mrb_float x = 0, y = 0;
  waah_image_t *image;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "o|ff", &mrb_image, &x, &y);
  Data_Get_Struct(mrb, mrb_image, &_waah_image_type_info, image);

  cairo_set_source_surface(cr, image->surface, x, y);

  return self;
}

static mrb_value
canvas_pattern(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_value mrb_pattern;
  waah_pattern_t *pattern;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "o", &mrb_pattern);
  Data_Get_Struct(mrb, mrb_pattern, &_waah_pattern_type_info, pattern);

  cairo_set_source(cr, pattern->cr_pattern);

  return self;
}

static void
_cairo_ellipse(cairo_t *cr, double cx, double cy, double rw, double rh) {
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_scale(cr, rw / 2., rh / 2.);
  cairo_arc(cr, 0., 0., 1., 0., 2 * M_PI);
  cairo_restore(cr);
}

static mrb_value
canvas_ellipse(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_float cx, cy, rw, rh;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "ffff", &cx, &cy, &rw, &rh);

  _cairo_ellipse(cr, cx, cy, rw, rh);

  return self;
}

static mrb_value
canvas_circle(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_float cx, cy, r;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "fff", &cx, &cy, &r);

  cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
  /*_cairo_ellipse(cr, cx, cy, r, r);*/

  return self;
}


static mrb_value
canvas_rect(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_float x, y, w, h;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "ffff", &x, &y, &w, &h);

  cairo_rectangle(cr, x, y, w, h);

  return self;
}

static mrb_value
canvas_rounded_rect(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_float x, y, w, h, r;
  double deg = M_PI / 180.0;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "fffff", &x, &y, &w, &h, &r);
  /* From: http://cairographics.org/samples/rounded_rectangle/ */
  cairo_new_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -90 * deg, 0 * deg);
  cairo_arc(cr, x + w - r, y + h - r, r, 0 * deg, 90 * deg);
  cairo_arc(cr, x + r, y + h - r, r, 90 * deg, 180 * deg);
  cairo_arc(cr, x + r, y + r, r, 180 * deg, 270 * deg);
  cairo_close_path (cr);

  return self;
}




static mrb_value
canvas_text(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_float x, y;
  char *text;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "ffz", &x, &y, &text);

  cairo_move_to(cr, (double)x, (double)y);
  cairo_text_path(cr, text);

  return self;
}

static mrb_value
canvas_text_extends(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  cairo_text_extents_t e;
  char *text;
  mrb_value vals[6];
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "z", &text);

  cairo_text_extents(cr, text, &e);

  vals[0] = mrb_float_value(mrb, e.width);
  vals[1] = mrb_float_value(mrb, e.height);
  vals[2] = mrb_float_value(mrb, e.x_bearing);
  vals[3] = mrb_float_value(mrb, e.y_bearing);
  vals[4] = mrb_float_value(mrb, e.x_advance);
  vals[5] = mrb_float_value(mrb, e.y_advance);

  return mrb_ary_new_from_values(mrb, 6, vals);
}

static mrb_value
canvas_path_extends(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double x1, x2, y1, y2;
  mrb_value vals[4];
  CANVAS_DEFAULT_DECL_INITS;

  cairo_path_extents(cr,
                     &x1,
                     &y1,
                     &x2,
                     &y2);
  vals[0] = mrb_float_value(mrb, x1);
  vals[1] = mrb_float_value(mrb, y1);
  vals[2] = mrb_float_value(mrb, x2);
  vals[3] = mrb_float_value(mrb, y2);
  return mrb_ary_new_from_values(mrb, 4, vals);
}

static mrb_value
canvas_font_size(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_float s;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "f", &s);

  cairo_set_font_size(cr, s);

  return self;
}

static mrb_value
canvas_font(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_value mrb_font;
  mrb_sym sym_slant, sym_weight;
  int n_args;
  cairo_font_slant_t slant = CAIRO_FONT_SLANT_NORMAL;
  cairo_font_weight_t weight = CAIRO_FONT_WEIGHT_NORMAL;
  CANVAS_DEFAULT_DECL_INITS;

  n_args = mrb_get_args(mrb, "o|nn", &mrb_font, &sym_weight, &sym_slant);

  switch (mrb_type(mrb_font)) {
    case MRB_TT_DATA: {
      waah_font_t *font;
      Data_Get_Struct(mrb, mrb_font, &_waah_font_type_info, font);
      if(font->cr_face == NULL) {
#ifdef CAIRO_HAS_FC_FONT
        if(font->fc_pattern != NULL) {
          font->cr_face = cairo_ft_font_face_create_for_pattern(font->fc_pattern);
        } else
#endif
          if(font->ft_face != NULL) {
            font->cr_face = cairo_ft_font_face_create_for_ft_face(font->ft_face, 0);
          }
      }
      cairo_set_font_face(canvas->cr, font->cr_face);
      break;
    }
    case MRB_TT_STRING: {
      char *font_name = mrb_str_to_cstr(mrb, mrb_font);
      if(sym_slant == id_italic) slant = CAIRO_FONT_SLANT_ITALIC;
      else if(sym_slant == id_oblique) slant = CAIRO_FONT_SLANT_OBLIQUE;

      if(sym_weight == id_bold) {
        weight = CAIRO_FONT_WEIGHT_BOLD;
      } else if(n_args == 2 && sym_weight == id_italic) {
        slant = CAIRO_FONT_SLANT_ITALIC;
      }
      cairo_select_font_face(canvas->cr, font_name, slant, weight);
      break;
    }
    default: {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "font must either be String or Font");
      return mrb_nil_value();
    }
  }

  return self;
}

static mrb_value
canvas_line_width(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_float w;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "f", &w);

  cairo_set_line_width(cr, (double) w);

  return self;
}

static mrb_value
canvas_path(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_value path;
  CANVAS_DEFAULT_DECL_INITS;

  if(mrb_get_args(mrb, "|o", &path) == 1) {
  }

  return self;
}

static mrb_value
canvas_m(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double cx, cy, x, y;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "ff", &x, &y);

  cairo_get_current_point(cr, &cx, &cy);
  cairo_move_to(cr, cx + x, cy + y);

  return self;
}

static mrb_value
canvas_M(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double x, y;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "ff", &x, &y);
  cairo_move_to(cr, x, y);

  return self;
}

static mrb_value
canvas_l(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double x, y;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "ff", &x, &y);
  cairo_rel_line_to(cr, x, y);

  return self;
}

static mrb_value
canvas_L(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double x, y;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "ff", &x, &y);
  cairo_line_to(cr, x, y);

  return self;
}

static mrb_value
canvas_h(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double x;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "f", &x);

  cairo_rel_line_to(cr, x, 0);
  return self;
}

static mrb_value
canvas_H(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double x, cx, cy;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "f", &x);

  cairo_get_current_point(cr, &cx, &cy);
  cairo_line_to(cr, x, cy);
  return self;
}

static mrb_value
canvas_v(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double y;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "f", &y);

  cairo_rel_line_to(cr, 0, y);
  return self;
}

static mrb_value
canvas_V(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double y, cx, cy;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "f", &y);

  cairo_get_current_point(cr, &cx, &cy);
  cairo_line_to(cr, cx, y);
  return self;
}

static mrb_value
canvas_c(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double a, b, c, d, e, f;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "ffffff", &a, &b, &c, &d, &e, &f);

  cairo_rel_curve_to(cr, a, b, c, d, e, f);
  return self;
}

static mrb_value
canvas_C(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double a, b, c, d, e, f;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "ffffff", &a, &b, &c, &d, &e, &f);

  cairo_curve_to(cr, a, b, c, d, e, f);
  return self;
}

static mrb_value
canvas_a(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double a, b, c, d, e, f;
  double cx, cy, x, y;
  int argc;
  CANVAS_DEFAULT_DECL_INITS;

  argc = mrb_get_args(mrb, "fffff|f", &a, &b, &c, &d, &e, &f);


  cairo_get_current_point(cr, &cx, &cy);
  x = cx + a;
  y = cy + b;

  if(argc > 5 && f > 0) {
    cairo_arc_negative(cr,
                       x,
                       y,
                       c,
                       d,
                       e);
  } else {
    cairo_arc(cr,
              x,
              y,
              c,
              d,
              e);
  }
  return self;
}

static mrb_value
canvas_A(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double a, b, c, d, e;
  mrb_bool neg = FALSE;
  double x, y;
  int argc;
  CANVAS_DEFAULT_DECL_INITS;

  argc = mrb_get_args(mrb, "fffff|b", &a, &b, &c, &d, &e, &neg);

  x = a;
  y = b;

  if(argc > 5 && neg) {
    cairo_arc_negative(cr,
                       x,
                       y,
                       c,
                       d,
                       e);
  } else {
    cairo_arc(cr,
              x,
              y,
              c,
              d,
              e);
  }
  return self;
}

static mrb_value
canvas_z(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  CANVAS_DEFAULT_DECL_INITS;

  cairo_close_path(cr);
  return self;
}

static mrb_value
canvas_fill(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_bool preserve = FALSE;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "|b", &preserve);
  if(!preserve) {
    cairo_fill(cr);
  } else {
    cairo_fill_preserve(cr);
  }

  return self;
}

static mrb_value
canvas_stroke(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_bool preserve = FALSE;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "|b", &preserve);
  if(!preserve) {
    cairo_stroke(cr);
  } else {
    cairo_stroke_preserve(cr);
  }

  return self;
}

static mrb_value
canvas_clear(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  CANVAS_DEFAULT_DECL_INITS;

  cairo_paint(cr);

  return self;
}

static mrb_value
canvas_push(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_value blk;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "&", &blk);

  cairo_save(cr);

  if(!mrb_nil_p(blk)) {
    mrb_funcall_with_block(mrb, self, id_instance_eval, 0, NULL, blk);
    cairo_restore(cr);
  }

  return self;
}

static mrb_value
canvas_translate(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double x, y;
  mrb_value block;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "ff&", &x, &y, &block);

  cairo_translate(cr, x, y);
  if(!mrb_nil_p(block)) {
    mrb_funcall_with_block(mrb, self, id_instance_eval, 0, NULL, block);
    cairo_translate(cr, -x, -y);
  }
  return self;
}

static mrb_value
canvas_scale(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double x, y;
  mrb_value block;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "ff&", &x, &y, &block);

  if(!mrb_nil_p(block)) {
    cairo_save(cr);
  }

  cairo_scale(cr, x, y);

  if(!mrb_nil_p(block)) {
    mrb_funcall_with_block(mrb, self, id_instance_eval, 0, NULL, block);
    cairo_restore(cr);
  }
  return self;
}

static mrb_value
canvas_clip(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_bool preserve = FALSE;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "|b", &preserve);
  if(!preserve) {
    cairo_clip(cr);
  } else {
    cairo_clip_preserve(cr);
  }
  return self;
}

static mrb_value
canvas_rotate(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  double r;
  mrb_value block;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_get_args(mrb, "f&", &r, &block);

  cairo_rotate(cr, r);
  if(!mrb_nil_p(block)) {
    mrb_funcall_with_block(mrb, self, id_instance_eval, 0, NULL, block);
    cairo_rotate(cr, -r);
  }
  return self;
}

static mrb_value
canvas_pop(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  CANVAS_DEFAULT_DECL_INITS;

  cairo_restore(cr);

  return self;
}

cairo_status_t cairo_write_to_mrb_str (void *user_data,
                                       const unsigned char *data,
                                       unsigned int length) {
  mrb_value buf = *((mrb_value *)(((void **)user_data)[0]));
  mrb_state *mrb = (mrb_state *) ((void **)user_data)[1];

  mrb_str_cat(mrb, buf, (const char *)data, length);

  return CAIRO_STATUS_SUCCESS;
}


static mrb_value
canvas_snapshot(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  mrb_value mrb_image;
  waah_image_t *image;
  cairo_t *image_cr;
  CANVAS_DEFAULT_DECL_INITS;

  mrb_image = image_new(mrb, &image);

  image->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, canvas->width, canvas->height);
  image_cr = cairo_create(image->surface);
  cairo_set_source_surface(image_cr, canvas->surface, 0, 0);
  cairo_paint(image_cr);
  cairo_destroy(image_cr);

  return mrb_image;
}

static mrb_value
canvas_width(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  CANVAS_DEFAULT_DECL_INITS;

  return mrb_fixnum_value(canvas->width);
}

static mrb_value
canvas_height(mrb_state *mrb, mrb_value self) {
  CANVAS_DEFAULT_DECLS;
  CANVAS_DEFAULT_DECL_INITS;

  return mrb_fixnum_value(canvas->height);
}


static mrb_value
image_to_png(mrb_state *mrb, mrb_value self) {
  waah_image_t *image;
  cairo_status_t status;
  mrb_value retval;
  char *filename;
  Data_Get_Struct(mrb, self, &_waah_image_type_info, image);

  if(image->surface == NULL) {
    return mrb_nil_value();
  }

  if(mrb_get_args(mrb, "|z", &filename) > 0) {
    status = cairo_surface_write_to_png(image->surface, filename);
    retval = mrb_true_value();
  } else {
    void *data[2];
    retval = mrb_str_buf_new(mrb, 64);
    data[0] = &retval;
    data[1] = mrb;
    status = cairo_surface_write_to_png_stream(image->surface,
                                               cairo_write_to_mrb_str,
                                               data);
  }

  if(raise_cairo_status(mrb, status)) {
    return mrb_nil_value();
  }

  return retval;
}

static mrb_value
image_width(mrb_state *mrb, mrb_value self) {
  waah_image_t *image;
  Data_Get_Struct(mrb, self, &_waah_image_type_info, image);

  return mrb_fixnum_value(cairo_image_surface_get_width(image->surface));
}

static mrb_value
image_height(mrb_state *mrb, mrb_value self) {
  waah_image_t *image;
  Data_Get_Struct(mrb, self, &_waah_image_type_info, image);

  return mrb_fixnum_value(cairo_image_surface_get_height(image->surface));
}

void
mrb_waah_canvas_gem_init(mrb_state *mrb) {

  FT_Init_FreeType(&ft_lib);

  mWaah = mrb_define_module(mrb, "Waah");

  cCanvas = mrb_define_class_under(mrb, mWaah, "Canvas", mrb->object_class);
  MRB_SET_INSTANCE_TT(cCanvas, MRB_TT_DATA);

  cFont = mrb_define_class_under(mrb, mWaah, "Font", mrb->object_class);
  MRB_SET_INSTANCE_TT(cFont, MRB_TT_DATA);

  cImage = mrb_define_class_under(mrb, mWaah, "Image", mrb->object_class);
  MRB_SET_INSTANCE_TT(cImage, MRB_TT_DATA);

  cPattern = mrb_define_class_under(mrb, mWaah, "Pattern", mrb->object_class);
  MRB_SET_INSTANCE_TT(cPattern, MRB_TT_DATA);

  cPath = mrb_define_class_under(mrb, mWaah, "Path", mrb->object_class);
  MRB_SET_INSTANCE_TT(cPath, MRB_TT_DATA);


  mrb_define_method(mrb, cCanvas, "initialize", canvas_initialize, ARGS_REQ(2));

  mrb_define_method(mrb, cCanvas, "color", canvas_color, ARGS_REQ(3) | ARGS_OPT(1));
  mrb_define_method(mrb, cCanvas, "image", canvas_image, ARGS_REQ(1) | ARGS_OPT(2));
  mrb_define_method(mrb, cCanvas, "pattern", canvas_pattern, ARGS_REQ(1));
  mrb_define_method(mrb, cCanvas, "ellipse", canvas_ellipse, ARGS_REQ(4));
  mrb_define_method(mrb, cCanvas, "circle", canvas_circle, ARGS_REQ(3));
  mrb_define_method(mrb, cCanvas, "rect", canvas_rect, ARGS_REQ(4));
  mrb_alias_method(mrb, cCanvas, mrb_intern_cstr(mrb, "rectangle"), mrb_intern_cstr(mrb, "rect"));
  mrb_define_method(mrb, cCanvas, "rounded_rect", canvas_rounded_rect, ARGS_REQ(5));
  mrb_define_method(mrb, cCanvas, "path", canvas_path, ARGS_OPT(1));
  mrb_define_method(mrb, cCanvas, "path_extends", canvas_path_extends, ARGS_NONE());
  mrb_define_method(mrb, cCanvas, "z", canvas_z, ARGS_NONE());
  mrb_define_method(mrb, cCanvas, "m", canvas_m, ARGS_REQ(2));
  mrb_define_method(mrb, cCanvas, "M", canvas_M, ARGS_REQ(2));
  mrb_define_method(mrb, cCanvas, "l", canvas_l, ARGS_REQ(2));
  mrb_define_method(mrb, cCanvas, "L", canvas_L, ARGS_REQ(2));
  mrb_define_method(mrb, cCanvas, "h", canvas_h, ARGS_REQ(1));
  mrb_define_method(mrb, cCanvas, "H", canvas_H, ARGS_REQ(1));
  mrb_define_method(mrb, cCanvas, "v", canvas_v, ARGS_REQ(1));
  mrb_define_method(mrb, cCanvas, "V", canvas_V, ARGS_REQ(1));
  mrb_define_method(mrb, cCanvas, "c", canvas_c, ARGS_REQ(6));
  mrb_define_method(mrb, cCanvas, "C", canvas_C, ARGS_REQ(6));
  mrb_define_method(mrb, cCanvas, "a", canvas_a, ARGS_REQ(5)| ARGS_OPT(1));
  mrb_define_method(mrb, cCanvas, "A", canvas_A, ARGS_REQ(5) | ARGS_OPT(1));

  mrb_define_method(mrb, cCanvas, "text", canvas_text, ARGS_REQ(3));
  mrb_define_method(mrb, cCanvas, "text_extends", canvas_text_extends, ARGS_REQ(1));
  mrb_define_method(mrb, cCanvas, "font_size", canvas_font_size, ARGS_REQ(1));
  mrb_define_method(mrb, cCanvas, "font", canvas_font, ARGS_REQ(1) | ARGS_OPT(2));


  mrb_define_method(mrb, cCanvas, "fill", canvas_fill, ARGS_OPT(1));
  mrb_define_method(mrb, cCanvas, "stroke", canvas_stroke, ARGS_OPT(1));
  mrb_define_method(mrb, cCanvas, "line_width", canvas_line_width, ARGS_REQ(1));
  mrb_define_method(mrb, cCanvas, "clear", canvas_clear, ARGS_NONE());
  mrb_define_method(mrb, cCanvas, "push", canvas_push, ARGS_BLOCK());
  mrb_define_method(mrb, cCanvas, "pop", canvas_pop, ARGS_NONE());
  mrb_define_method(mrb, cCanvas, "translate", canvas_translate, ARGS_REQ(2) | ARGS_BLOCK());
  mrb_define_method(mrb, cCanvas, "clip", canvas_clip, ARGS_OPT(1));
  mrb_define_method(mrb, cCanvas, "scale", canvas_scale, ARGS_REQ(2) | ARGS_BLOCK());
  mrb_define_method(mrb, cCanvas, "rotate", canvas_rotate, ARGS_REQ(1) | ARGS_BLOCK());
  mrb_define_method(mrb, cCanvas,  "snapshot", canvas_snapshot, ARGS_NONE());
  mrb_define_method(mrb, cCanvas, "width", canvas_width, ARGS_NONE());
  mrb_define_method(mrb, cCanvas, "height", canvas_height, ARGS_NONE());

  mrb_define_class_method(mrb, cImage, "load", image_load, ARGS_REQ(1));
  mrb_undef_class_method(mrb, cImage, "new");
  mrb_define_method(mrb, cImage, "to_png", image_to_png, ARGS_OPT(1));
  mrb_define_method(mrb, cImage, "width", image_width, ARGS_NONE());
  mrb_define_method(mrb, cImage, "height", image_height, ARGS_NONE());

  mrb_define_class_method(mrb, cFont, "load", font_load, ARGS_REQ(1));
  mrb_define_class_method(mrb, cFont, "find", font_find, ARGS_REQ(1));
  mrb_define_class_method(mrb, cFont, "list", font_list, ARGS_NONE());
  mrb_undef_class_method(mrb, cFont, "new");
  mrb_define_method(mrb, cFont, "name", font_name, ARGS_NONE());
  mrb_define_method(mrb, cFont, "family", font_family, ARGS_NONE());
  mrb_define_method(mrb, cFont, "style", font_style, ARGS_NONE());

  mrb_define_method(mrb, cPath, "initialize", path_initialize, ARGS_NONE());

  /*mrb_define_method(mrb, cPath, "move_to", path_initialize, ARGS_REQ(2));
  mrb_define_method(mrb, cPath, "line_to", path_initialize, ARGS_REQ(2));
  mrb_define_method(mrb, cPath, "curve_to", path_initialize, ARGS_REQ(2));
  mrb_define_method(mrb, cPath, "i", path_initialize, ARGS_REQ(2));
   */

  mrb_define_class_method(mrb, cPattern, "linear", pattern_linear, ARGS_REQ(4));
  mrb_define_class_method(mrb, cPattern, "radial", pattern_radial, ARGS_REQ(6));
  mrb_undef_class_method(mrb, cPattern, "new");
  mrb_define_method(mrb, cPattern, "color_stop", pattern_color_stop, ARGS_REQ(4) | ARGS_OPT(1));

  id_instance_eval = mrb_intern_lit(mrb, "instance_eval");
  id_normal = mrb_intern_lit(mrb, "normal");
  id_italic = mrb_intern_lit(mrb, "italic");
  id_oblique = mrb_intern_lit(mrb, "oblique");
  id_bold = mrb_intern_lit(mrb, "bold");
}

void
mrb_waah_canvas_gem_final(mrb_state* mrb) {
  //FT_Done_FreeType(ft_lib);
  /* finalizer */
}
