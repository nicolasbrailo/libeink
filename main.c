#include "libeink/eink.h"

#include <cairo/cairo.h>

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

atomic_bool gUserStop = false;

void handle_sigint(int sig) {
    gUserStop = true;
    printf("Stopping...\n");
}

void show_clock(struct EInkDisplay* display) {
  cairo_t *cr = eink_get_cairo(display);
  cairo_surface_t *surface = cairo_get_target(cr);

  const size_t width = cairo_image_surface_get_width(surface);
  const size_t height = cairo_image_surface_get_height(surface);

  cairo_text_extents_t extents;
  const char *text = "Now: 00:00:00";
  cairo_text_extents(cr, text, &extents);
  double clock_x = (width - extents.width) / 2 - extents.x_bearing;
  double clock_y = (height - extents.height) / 2 - extents.y_bearing;

  cairo_set_source_rgba(cr, 0, 0, 0, 0);
  cairo_paint(cr);
  cairo_set_font_size(cr, 20);

  while (!gUserStop) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
#   define TXT_FMT "%02d:%02d:%02d"
    static char time_str[sizeof(TXT_FMT)];
    snprintf(time_str, sizeof(time_str), TXT_FMT, t->tm_hour, t->tm_min, t->tm_sec);

#if 1
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_move_to(cr, clock_x, clock_y);
    cairo_show_text(cr, time_str);
    eink_render(display);
#else
    eink_invalidate_rect(display, clock_x + extents.x_bearing, clock_y + extents.y_bearing, extents.width, extents.height);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_move_to(cr, clock_x, clock_y);
    cairo_show_text(cr, time_str);
    eink_render_partial(display);
#endif

    printf("Clock updated\n");
    sleep(3);
  }
}


int main(int argc, char **argv) {
  struct EInkDisplay* display = eink_init();
  signal(SIGINT, handle_sigint);

  cairo_t *cr = eink_get_cairo(display);
  cairo_surface_t *surface = cairo_get_target(cr);
  const size_t width = cairo_image_surface_get_width(surface);
  const size_t height = cairo_image_surface_get_height(surface);

  // Set text properties (black, fully opaque)
  cairo_set_source_rgba(cr, 0, 0, 0, 1);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 20);

  // Calculate text position
  cairo_text_extents_t extents;
  const char *text = "Built @ " __TIME__;
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
  printf("5...\n");
  sleep(5);
  printf("go.\n");

  show_clock(display);

  printf("eink clear\n");
  // TODO Not working?
  eink_clear(display);
  printf("shutdown\n");
  eink_delete(display);

  return 0;
}

