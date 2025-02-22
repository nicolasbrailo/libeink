#include "libeink/eink.h"
#include <cairo/cairo.h>
#include <time.h>
#include <stdio.h>

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
  const char *text = "Hola " __TIME__;
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

  // eink_clear(display);

  eink_delete(display);

  return 0;
}

