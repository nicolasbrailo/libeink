#include "libeink/EPD_2in13_V4.h"

#include <cairo/cairo.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#define WHITE 0xFF
#define BLACK 0x00

void render(cairo_surface_t *surface) {
  if (cairo_image_surface_get_format(surface) != CAIRO_FORMAT_A1) {
    printf("Only monocrome surfaces are supported");
    exit(0);
  }



  const int cairo_width = cairo_image_surface_get_width(surface);
  const int cairo_height = cairo_image_surface_get_height(surface);
  const int stride = cairo_image_surface_get_stride(surface);
  uint8_t *data = cairo_image_surface_get_data(surface);

  const int row_size = ((cairo_width + 31) / 32) * 4; // Each row must be a multiple of 4 bytes
  const int bmp_data_sz = row_size * cairo_height;

  const UWORD Bcolor = BLACK;
  const UWORD Wcolor = WHITE;


  // Display canvas
  UBYTE *displayCanvas;
  UWORD Imagesize =
      ((EPD_2in13_V4_WIDTH % 8 == 0) ? (EPD_2in13_V4_WIDTH / 8)
                                     : (EPD_2in13_V4_WIDTH / 8 + 1)) *
      EPD_2in13_V4_HEIGHT;
  if ((displayCanvas = (UBYTE *)malloc(Imagesize)) == NULL) {
    Debug("Failed to apply for black memory...\r\n");
    return;
  }

  uint8_t bmp_data[bmp_data_sz];
  memset(bmp_data, 0, bmp_data_sz);
  for (int y = cairo_height - 1; y >= 0; y--) {
    for (int x = 0; x < cairo_width; x++) {
      int byte_index = (x / 8) + y * stride;
      int bit_index = x % 8;
      if (data[byte_index] & (1 << bit_index)) {
        const size_t offset = (y * stride) + (x / 8);
        bmp_data[offset] |= (1 << (7 - (x % 8))); // Convert LSB to MSB format
      }
    }
  }

  UWORD Image_Width_Byte =
      (cairo_width % 8 == 0) ? (cairo_width / 8) : (cairo_width / 8 + 1);
  const int WidthByte = (EPD_2in13_V4_WIDTH % 8 == 0) ? (EPD_2in13_V4_WIDTH / 8) : (EPD_2in13_V4_WIDTH / 8 + 1);

  for (size_t y = 0; y < cairo_height; y++) {
    for (size_t x = 0; x < cairo_width; x++) {
      uint8_t temp = bmp_data[(x / 8) + (y * Image_Width_Byte)];
      uint8_t color = (((temp << (x % 8)) & 0x80) == 0x80) ? Bcolor : Wcolor;
      const UWORD X = cairo_width - y - 1;
      const UWORD Y = x;
      UDOUBLE Addr = X / 8 + Y * WidthByte;
      if (Addr > Imagesize) {
          continue;
      }
      UBYTE Rdata = displayCanvas[Addr];
      if (color == BLACK)
        displayCanvas[Addr] = Rdata & ~(0x80 >> (X % 8));
      else
        displayCanvas[Addr] = Rdata | (0x80 >> (X % 8));
    }
  }

  EPD_2in13_V4_Display(displayCanvas);
  free(displayCanvas);
}

int main(int argc, char **argv) {
  if (DEV_Module_Init() != 0) {
    printf("No init\n");
    exit(1);
  }
  EPD_2in13_V4_Init();

  size_t display_width = 250;
  size_t display_height = 122;
  cairo_surface_t *surface;
  cairo_t *cr;

  // Create a monochrome (1-bit) surface
  surface = cairo_image_surface_create(CAIRO_FORMAT_A1, display_width,
                                       display_height);
  cr = cairo_create(surface);

  // Ensure correct blending behavior
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

  // Set background color to white (transparent in A1 format)
  cairo_set_source_rgba(cr, 1, 1, 1, 0);
  cairo_paint(cr);

  // Set text properties (black, fully opaque)
  cairo_set_source_rgba(cr, 0, 0, 0, 1);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 30);

  // Calculate text position
  cairo_text_extents_t extents;
  const char *text = "Hello " __TIME__;
  cairo_text_extents(cr, text, &extents);
  double x = (display_width - extents.width) / 2 - extents.x_bearing;
  double y = (display_height - extents.height) / 2 - extents.y_bearing;

  // Draw text
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, text);

  // Draw rectangle around text
  cairo_set_line_width(cr, 2);
  cairo_rectangle(cr, x + extents.x_bearing - 10, y + extents.y_bearing - 10,
                  extents.width + 20, extents.height + 20);
  cairo_stroke(cr);

  render(surface);

  cairo_destroy(cr);
  cairo_surface_destroy(surface);

  EPD_2in13_V4_Sleep();
  DEV_Delay_ms(2000); // important, at least 2s
  DEV_Module_Exit();
  return 0;
}
