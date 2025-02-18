#include "ImageData.h"
#include "libeink/DEV_Config.h"
#include "libeink/Debug.h"
#include "libeink/EPD_2in13_V4.h"
#include "libeink/GUI_BMPfile.h"
#include "libeink/GUI_Paint.h"

#include <signal.h>
#include <stdlib.h>
#include <time.h>


#define DEFAULT_DISPLAY_WIDTH 250
#define DEFAULT_DISPLAY_HEIGHT 122

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
  size_t xx =0;
  Debug("RENDER IMG %db x %d = %d x %d\n", display->width_bytes, display->height_px, 8*display->width_bytes, display->height_px);
  for (size_t i = 0; i < display->height_px; ++i) {
    for (size_t j = 0; j < display->width_bytes; ++j) {
      EPD_2in13_V4_SendData(c);
      xx++;
    }
  }

  Debug("XXX SENT %zd\n", xx);

  EPD_2in13_V4_TurnOnDisplay();
}

void eink_clear_white() {
  eink_pattern(&gDisplay, 0xFF);
}

void eink_clear_black() {
  eink_pattern(&gDisplay, 0x00);
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
  const int width = cairo_image_surface_get_width(surface);
  const int height = cairo_image_surface_get_height(surface);
  const int stride = cairo_image_surface_get_stride(surface);
  const uint8_t *data = cairo_image_surface_get_data(surface);
  EPD_2in13_V4_SendCommand(0x24);
  size_t xx = 0;
  for (int i=0; i < width + height * stride; ++i) {
    EPD_2in13_V4_SendData(data[i]);
    xx++;
  }
  Debug("CCC SENT %zd\n", xx);
  EPD_2in13_V4_TurnOnDisplay();
}

void eink_cairo() {
  cairo_surface_t *surface;
  cairo_t *cr;

  // Create a monochrome (1-bit) surface
  surface = cairo_image_surface_create(CAIRO_FORMAT_A1, DEFAULT_DISPLAY_WIDTH, DEFAULT_DISPLAY_HEIGHT);
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
  double x = (DEFAULT_DISPLAY_WIDTH - extents.width) / 2 - extents.x_bearing;
  double y = (DEFAULT_DISPLAY_HEIGHT - extents.height) / 2 - extents.y_bearing;

  // Draw text
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, text);

  // Draw rectangle around text
  cairo_set_line_width(cr, 2);
  cairo_rectangle(cr, x + extents.x_bearing - 10, y + extents.y_bearing - 10, extents.width + 20, extents.height + 20);
  cairo_stroke(cr);

  // Write output to BMP file
  save_as_bmp("output.bmp", surface);
  render(surface);

  // Clean up
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}


void eink_draw_text(const char* txt) {
  UBYTE *BlackImage;
  UWORD Imagesize =
      ((EPD_2in13_V4_WIDTH % 8 == 0) ? (EPD_2in13_V4_WIDTH / 8)
                                     : (EPD_2in13_V4_WIDTH / 8 + 1)) *
      EPD_2in13_V4_HEIGHT;
  if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
    Debug("Failed to apply for black memory...\r\n");
    return ;
  }

  Paint_NewImage(BlackImage, EPD_2in13_V4_WIDTH, EPD_2in13_V4_HEIGHT, 90, BLACK);
  Debug("DRAW %s\n", txt);
  Paint_DrawString_EN(5, 5, txt, &Font24, BLACK, WHITE);
  EPD_2in13_V4_Display_Base(BlackImage);
  free(BlackImage);
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


int EPD_2in13_V4_test(void) {
  Debug("EPD_2in13_V4_test Demo\r\n");

  // Create a new image cache
  UBYTE *BlackImage;
  UWORD Imagesize =
      ((EPD_2in13_V4_WIDTH % 8 == 0) ? (EPD_2in13_V4_WIDTH / 8)
                                     : (EPD_2in13_V4_WIDTH / 8 + 1)) *
      EPD_2in13_V4_HEIGHT;
  if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
    Debug("Failed to apply for black memory...\r\n");
    return -1;
  }

  Paint_NewImage(BlackImage, EPD_2in13_V4_WIDTH, EPD_2in13_V4_HEIGHT, 90, WHITE);
  Paint_Clear(WHITE);

  // Why flash? Paint_NewImage(BlackImage, EPD_2in13_V4_WIDTH, EPD_2in13_V4_HEIGHT, 90, WHITE);
  // Why flash? Paint_SelectImage(BlackImage);
  GUI_ReadBmp("/home/batman/eink/pic/100x100.bmp", 10, 10);
  EPD_2in13_V4_Display(BlackImage);
  Debug("Paint done?");
  DEV_Delay_ms(5000);

  // Paint_SelectImage(BlackImage);
  GUI_ReadBmp("/home/batman/eink/pic/2in13_1.bmp", 0, 0);
  //EPD_2in13_V4_Display(BlackImage);
  DEV_Delay_ms(3000);
  return 0;

#if 1 // show image for array
  Debug("show image for array\r\n");
  EPD_2in13_V4_Init_Fast();
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);
  Paint_DrawBitMap(gImage_2in13_2);

  EPD_2in13_V4_Display_Fast(BlackImage);
  DEV_Delay_ms(2000);
#endif

#if 1 // Drawing on the image
  Paint_NewImage(BlackImage, EPD_2in13_V4_WIDTH, EPD_2in13_V4_HEIGHT, 90,
                 WHITE);
  Debug("Drawing\r\n");
  // 1.Select Image
  EPD_2in13_V4_Init();
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);

  // 2.Drawing on the image
  Paint_DrawPoint(5, 10, BLACK, DOT_PIXEL_1X1, DOT_STYLE_DFT);
  Paint_DrawPoint(5, 25, BLACK, DOT_PIXEL_2X2, DOT_STYLE_DFT);
  Paint_DrawPoint(5, 40, BLACK, DOT_PIXEL_3X3, DOT_STYLE_DFT);
  Paint_DrawPoint(5, 55, BLACK, DOT_PIXEL_4X4, DOT_STYLE_DFT);

  Paint_DrawLine(20, 10, 70, 60, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
  Paint_DrawLine(70, 10, 20, 60, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
  Paint_DrawRectangle(20, 10, 70, 60, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  Paint_DrawRectangle(85, 10, 135, 60, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);

  Paint_DrawLine(45, 15, 45, 55, BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
  Paint_DrawLine(25, 35, 70, 35, BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
  Paint_DrawCircle(45, 35, 20, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  Paint_DrawCircle(110, 35, 20, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);

  Paint_DrawString_EN(140, 15, "waveshare", &Font16, BLACK, WHITE);
  Paint_DrawNum(140, 40, 123456789, &Font16, BLACK, WHITE);

  EPD_2in13_V4_Display_Base(BlackImage);
  DEV_Delay_ms(3000);
#endif

#if 1 // Partial refresh, example shows time
  Paint_NewImage(BlackImage, EPD_2in13_V4_WIDTH, EPD_2in13_V4_HEIGHT, 90,
                 WHITE);
  Debug("Partial refresh\r\n");
  Paint_SelectImage(BlackImage);

  PAINT_TIME sPaint_time;
  sPaint_time.Hour = 12;
  sPaint_time.Min = 34;
  sPaint_time.Sec = 56;
  UBYTE num = 10;
  for (;;) {
    sPaint_time.Sec = sPaint_time.Sec + 1;
    if (sPaint_time.Sec == 60) {
      sPaint_time.Min = sPaint_time.Min + 1;
      sPaint_time.Sec = 0;
      if (sPaint_time.Min == 60) {
        sPaint_time.Hour = sPaint_time.Hour + 1;
        sPaint_time.Min = 0;
        if (sPaint_time.Hour == 24) {
          sPaint_time.Hour = 0;
          sPaint_time.Min = 0;
          sPaint_time.Sec = 0;
        }
      }
    }
    Paint_ClearWindows(150, 80, 150 + Font20.Width * 7, 80 + Font20.Height,
                       WHITE);
    Paint_DrawTime(150, 80, &sPaint_time, &Font20, WHITE, BLACK);

    num = num - 1;
    if (num == 0) {
      break;
    }
    EPD_2in13_V4_Display_Partial(BlackImage);
    DEV_Delay_ms(500); // Analog clock 1s
  }
#endif

  Debug("Clear...\r\n");
  EPD_2in13_V4_Init();
  EPD_2in13_V4_Clear();

  Debug("Goto Sleep...\r\n");
  EPD_2in13_V4_Sleep();
  free(BlackImage);
  BlackImage = NULL;
  DEV_Delay_ms(2000); // important, at least 2s
  // close 5V
  Debug("close 5V, Module enters 0 power consumption ...\r\n");
  DEV_Module_Exit();
  return 0;
}

void Handler(int signo) {
  // System Exit
  printf("\r\nHandler:exit\r\n");
  DEV_Module_Exit();

  exit(0);
}

int main(int argc, char** argv) {
  signal(SIGINT, Handler);
  eink_init();
  if (argc > 1) {
    if (strcmp(argv[1], "white") == 0) {
      eink_clear_white();
    } else if (strcmp(argv[1], "black") == 0) {
      eink_clear_black();
    } else if (strcmp(argv[1], "test") == 0) {
      EPD_2in13_V4_test();
    } else if (strcmp(argv[1], "cairo") == 0) {
      eink_cairo();
    } else {
      //eink_draw_text(argv[1]);
      eink_draw_img(argv[1]);
    }
  }
  eink_delete();
  return 0;
}
