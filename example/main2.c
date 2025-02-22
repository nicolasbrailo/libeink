#include "libeink/EPD_2in13_V4.h"

#include <cairo/cairo.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

struct EInkDisplay {
  size_t width;
  size_t height;
  bool invert_color;
  cairo_surface_t *surface;
  cairo_t *cr;
};

struct EInkDisplay* eink_init() {
  if (DEV_Module_Init() != 0) {
      return NULL;
  }
  EPD_2in13_V4_Init();

  struct EInkDisplay* display = malloc(sizeof(struct EInkDisplay));
  display->width = EPD_2in13_V4_HEIGHT;
  display->height = EPD_2in13_V4_WIDTH;

  // Select black-on-white or reverse
  display->invert_color = false;

  // Create a monochrome (1-bit) surface
  display->surface = cairo_image_surface_create(CAIRO_FORMAT_A1, display->width, display->height);
  display->cr = cairo_create(display->surface);
  cairo_set_operator(display->cr, CAIRO_OPERATOR_SOURCE);

  // Set background color to white (transparent in A1 format)
  cairo_set_source_rgba(display->cr, 1, 1, 1, 0);
  cairo_paint(display->cr);

  return display;
}

void eink_delete(struct EInkDisplay* display) {
  cairo_destroy(display->cr);
  cairo_surface_destroy(display->surface);

  EPD_2in13_V4_Sleep();
  DEV_Delay_ms(2000); // important, at least 2s
  DEV_Module_Exit();
}

cairo_t* eink_get_cairo(struct EInkDisplay* display) {
  return display->cr;
}

void eink_render(struct EInkDisplay* display) {
  cairo_surface_t *surface = display->surface;
  const size_t width = cairo_image_surface_get_width(surface);
  const size_t height = cairo_image_surface_get_height(surface);
  const size_t stride = cairo_image_surface_get_stride(surface);
  uint8_t *img_data = cairo_image_surface_get_data(surface);
  const size_t height_bytes = (height + 7) / 8; // Same as ceil(height / 8)

  const size_t display_canvas_sz = height_bytes * width;
  uint8_t* display_canvas = malloc(display_canvas_sz);
  memset(display_canvas, 0, display_canvas_sz);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const size_t src_byte_index = x / 8 + y * stride;
      const size_t src_bit_index = x % 8;

      // Display memory is rotated; rotate coordinates
      const size_t dest_x = height - 1 - y;
      const size_t dest_y = x;
      const size_t dest_byte_index = dest_x / 8 + dest_y * height_bytes;
      const size_t dest_bit_index = dest_x % 8;

      bool pixel_set = img_data[src_byte_index] & (1 << src_bit_index);
      if (pixel_set == display->invert_color) {
        display_canvas[dest_byte_index] |= (1 << (7 - dest_bit_index));
      } else {
        display_canvas[dest_byte_index] &= ~(1 << (7 - dest_bit_index));
      }
    }
  }

  EPD_2in13_V4_Display(display_canvas);
}

int main(int argc, char **argv) {
  struct EInkDisplay* display = eink_init();
  cairo_t *cr = eink_get_cairo(display);
  cairo_surface_t *surface = cairo_get_target(cr);
  const size_t width = cairo_image_surface_get_width(surface);
  const size_t height = cairo_image_surface_get_height(surface);

  // Set text properties (black, fully opaque)
  cairo_set_source_rgba(cr, 0, 0, 0, 1);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 30);

  // Calculate text position
  cairo_text_extents_t extents;
  const char *text = "Hello " __TIME__;
  cairo_text_extents(cr, text, &extents);
  double x = (width - extents.width) / 2 - extents.x_bearing;
  double y = (height - extents.height) / 2 - extents.y_bearing;

  // Draw text
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, text);

  // Draw rectangle around text
  cairo_set_line_width(cr, 2);
  cairo_rectangle(cr, x + extents.x_bearing - 10, y + extents.y_bearing - 10,
                  extents.width + 20, extents.height + 20);
  cairo_stroke(cr);

  eink_render(display);

  eink_delete(display);

  return 0;
}
