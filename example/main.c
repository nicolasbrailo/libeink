﻿#include "ImageData.h"
#include "libeink/DEV_Config.h"
#include "libeink/Debug.h"
#include "libeink/EPD_2in13_V4.h"
#include "libeink/GUI_BMPfile.h"
#include "libeink/GUI_Paint.h"

#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>


#define DEFAULT_DISPLAY_WIDTH 250
#define DEFAULT_DISPLAY_HEIGHT 122

void Handler(int signo) {
  // System Exit
  printf("\r\nHandler:exit\r\n");
  DEV_Module_Exit();

  exit(0);
}


struct Display {
  size_t width_px;
  size_t height_px;
  // Display is monochrome, so each byte holds 8 pixels
  size_t width_bytes;
} gDisplay;

struct Display* eink_init() {
  if (DEV_Module_Init() != 0) {
    return NULL;
  }
  EPD_2in13_V4_Init();

  gDisplay.width_px = DEFAULT_DISPLAY_WIDTH;
  gDisplay.height_px = DEFAULT_DISPLAY_HEIGHT;
  gDisplay.width_bytes = (gDisplay.width_px % 8 == 0) ? (gDisplay.width_px / 8) : (gDisplay.width_px / 8 + 1);
  return &gDisplay;
}

void eink_delete() {
  EPD_2in13_V4_Sleep();
  DEV_Delay_ms(2000); // important, at least 2s
  DEV_Module_Exit();
}

void eink_pattern(struct Display* display, uint8_t c) {
  EPD_2in13_V4_SendCommand(0x24);
  for (size_t i = 0; i < display->height_px; ++i) {
    for (size_t j = 0; j < display->width_bytes; ++j) {
      EPD_2in13_V4_SendData(c);
    }
  }
  EPD_2in13_V4_TurnOnDisplay();
}

void eink_clear_white() {
  eink_pattern(&gDisplay, 0xFF);
}

void eink_clear_black() {
  eink_pattern(&gDisplay, 0x00);
}

void pat_impl(uint8_t cmd) {
  EPD_2in13_V4_SendCommand(cmd);

  // Pixel wire format:
  // * Looking at the display with the wires coming off the top and the ribbon cable to the right
  // * There are 250*122 = 30500 pixels
  // * 0,0 is at the bottom left of the display
  // * Each pixel is "vertical" (ie 0XFF will draw 8 vertical pixels at 0,0)
  // * Pixels are sent scanning from 0x0 to 0x122, 1x0 to 1x122...
  // * Pixels are sent 8 at a time (so the first column is an array of 16 bytes)
  // * Last drawable colum seems to be 237, even though display says 250. I'm sure this is a bug in this code

  for (size_t col = 0; col < 237; ++col) {
    for (size_t row = 0; row < 16; ++row) {
      if (col < 237 / 3) {
        if (row < 8) {
          EPD_2in13_V4_SendData(0xFF);
        } else {
          EPD_2in13_V4_SendData(0xAA);
        }
      } else if (col > 2 * 237 / 3) {
        EPD_2in13_V4_SendData(0x55);
      } else {
        EPD_2in13_V4_SendData(0x00);
      }
    }
  }

  EPD_2in13_V4_TurnOnDisplay();
}
void pat() {
  //eink_clear_white();
  pat_impl(0x24);
  //pat_impl(0x26);
}

void eink_draw_img(const char* path) {
  UBYTE *BlackImage;
  UWORD Imagesize =
      ((EPD_2in13_V4_WIDTH % 8 == 0) ? (EPD_2in13_V4_WIDTH / 8)
                                     : (EPD_2in13_V4_WIDTH / 8 + 1)) *
      EPD_2in13_V4_HEIGHT;
  if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
    Debug("Failed to apply for black memory...\r\n");
    return ;
  }

  Paint_NewImage(BlackImage, EPD_2in13_V4_WIDTH, EPD_2in13_V4_HEIGHT, 90, WHITE);
  Paint_Clear(WHITE);

  // Why flash? Paint_NewImage(BlackImage, EPD_2in13_V4_WIDTH, EPD_2in13_V4_HEIGHT, 90, WHITE);
  // Why flash? Paint_SelectImage(BlackImage);
  GUI_ReadBmp(path, 0, 0);
  EPD_2in13_V4_Display(BlackImage);
}


#include <cairo/cairo.h>
void save_as_bmp(const char *filename, cairo_surface_t *surface) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);
    int stride = cairo_image_surface_get_stride(surface);
    uint8_t *data = cairo_image_surface_get_data(surface);

    int row_size = ((width + 31) / 32) * 4; // Each row must be a multiple of 4 bytes
    int file_size = 62 + row_size * height; // BMP header + image data

    uint8_t header[62] = {
        'B', 'M', file_size, file_size >> 8, file_size >> 16, file_size >> 24,
        0, 0, 0, 0, 62, 0, 0, 0, 40, 0, 0, 0,
        width, width >> 8, width >> 16, width >> 24,
        height, height >> 8, height >> 16, height >> 24,
        1, 0, 1, 0, 0, 0, 0, 0,
        file_size - 62, (file_size - 62) >> 8, (file_size - 62) >> 16, (file_size - 62) >> 24,
        0x13, 0x0B, 0, 0, 0x13, 0x0B, 0, 0,
        2, 0, 0, 0, 0, 0, 0, 0,
        0x00, 0x00, 0x00, 0x00, // Black color
        0xFF, 0xFF, 0xFF, 0x00  // White color
    };

    fwrite(header, sizeof(uint8_t), 62, file);

    uint8_t *row = calloc(row_size, 1);
    for (int y = height - 1; y >= 0; y--) {
        memset(row, 0, row_size);
        for (int x = 0; x < width; x++) {
            int byte_index = (x / 8) + y * stride;
            int bit_index = x % 8;
            if (data[byte_index] & (1 << bit_index)) {
                row[x / 8] |= (1 << (7 - (x % 8))); // Convert LSB to MSB format
            }
        }
        fwrite(row, sizeof(uint8_t), row_size, file);
    }
    free(row);
    fclose(file);
    printf("Monochrome BMP file '%s' created successfully.\n", filename);
}
void render(cairo_surface_t *surface) {
  EPD_2in13_V4_SendCommand(0x24);
  const int width = cairo_image_surface_get_width(surface);
  const int height = cairo_image_surface_get_height(surface);
  const int stride = cairo_image_surface_get_stride(surface);
  const uint8_t *data = cairo_image_surface_get_data(surface);
  for (int x = 0; x < width; x++) {
      int pixel_byte_group = 0;
      for (int y = 0; y < height; y++) {
          int byte_index = (x / 8) + y * stride;
          int bit_index = x % 8;
          int pixel = (data[byte_index] & (1 << bit_index)) ? 1 : 0;
          pixel_byte_group = (pixel_byte_group << 1) | pixel;
          const bool is_byte_boundary = (y > 0) && ((y % 7) == 0);
          const bool is_last_pixel = (y == height - 1);
          if (is_byte_boundary || is_last_pixel) {
              //printf("0x%02x ", pixel_byte_group);
              EPD_2in13_V4_SendData(pixel_byte_group);
              pixel_byte_group = 0;
          }
      }
      //printf("\n");
  }
  EPD_2in13_V4_TurnOnDisplay();
}

void cairo_test() {
  size_t display_width = 122;
  size_t display_height = 250;
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create(CAIRO_FORMAT_A1, display_width, display_height);
  cr = cairo_create(surface);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

  cairo_set_source_rgba(cr, 1, 1, 1, 0);
  cairo_paint(cr);

  for (size_t i=0; i < display_height; i+=10) {
#if 1
      cairo_move_to(cr, 0, i);
      cairo_line_to(cr, display_width, i);
#else
      cairo_move_to(cr, i, 0);
      cairo_line_to(cr, i, display_height);
#endif
      cairo_stroke(cr);
  }

  save_as_bmp("output.bmp", surface);
  render(surface);

  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}

void eink_cairo() {
  size_t display_width = 3;
  size_t display_height = 7;
  cairo_surface_t *surface;
  cairo_t *cr;

  // Create a monochrome (1-bit) surface
  surface = cairo_image_surface_create(CAIRO_FORMAT_A1, display_width, display_height);
  cr = cairo_create(surface);

  // Ensure correct blending behavior
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

  // Set background color to white (transparent in A1 format)
  cairo_set_source_rgba(cr, 1, 1, 1, 0);
  cairo_paint(cr);

  // Set text properties (black, fully opaque)
  cairo_set_source_rgba(cr, 0, 0, 0, 1);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 40);

  // Calculate text position
  cairo_text_extents_t extents;
  const char *text = "Hello world";
  cairo_text_extents(cr, text, &extents);
  double x = (display_width - extents.width) / 2 - extents.x_bearing;
  double y = (display_height - extents.height) / 2 - extents.y_bearing;

  // Draw text
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, text);

  // Draw rectangle around text
  cairo_set_line_width(cr, 2);
  cairo_rectangle(cr, x + extents.x_bearing - 10, y + extents.y_bearing - 10, extents.width + 20, extents.height + 20);
  cairo_stroke(cr);

  // Write output to BMP file
  save_as_bmp("output.bmp", surface);
  //render(surface);

  // Clean up
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}

int main(int argc, char** argv) {
  eink_init();
  if (argc > 1) {
    if (strcmp(argv[1], "white") == 0) {
      eink_clear_white();
    } else if (strcmp(argv[1], "black") == 0) {
      eink_clear_black();
    } else if (strcmp(argv[1], "pat") == 0) {
      pat();
    } else if (strcmp(argv[1], "cairo") == 0) {
      cairo_test();
    } else {
      eink_draw_img(argv[1]);
    }
  }
  eink_delete();
  return 0;
}
