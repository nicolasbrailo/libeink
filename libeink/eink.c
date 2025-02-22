#include "eink.h"

#include <stdio.h>
#include <string.h>

#include "liblgpio/lgpio.h"

#define EPD_2in13_V4_WIDTH 122
#define EPD_2in13_V4_HEIGHT 250
#define EPD_RST_PIN 17
#define EPD_DC_PIN 25
#define EPD_CS_PIN 8
#define EPD_PWR_PIN 18
#define EPD_BUSY_PIN 24
#define EPD_MOSI_PIN 10
#define EPD_SCLK_PIN 11

int GPIO_Handle;
int SPI_Handle;

void DEV_GPIO_Mode(size_t Pin, size_t Mode) {
  if (Mode == 0 || Mode == LG_SET_INPUT) {
    lgGpioClaimInput(GPIO_Handle, 0, Pin);
  } else {
    lgGpioClaimOutput(GPIO_Handle, 0, Pin, LG_LOW);
  }
}

void DEV_Digital_Write(size_t Pin, uint8_t Value) {
  lgGpioWrite(GPIO_Handle, Pin, Value);
}
uint8_t DEV_Module_Init(void) {
  printf("DEV_Module_Init\n");
  GPIO_Handle = lgGpiochipOpen(0);
  if (GPIO_Handle < 0) {
    GPIO_Handle = lgGpiochipOpen(4);
    if (GPIO_Handle < 0) {
      printf("Can't find /dev/gpiochip[1|4]\n");
      return -1;
    }
  }

  SPI_Handle = lgSpiOpen(0, 0, 10000000, 0);
  if (SPI_Handle == LG_SPI_IOCTL_FAILED) {
    printf("Can't open SPI\n");
    return -1;
  }

  DEV_GPIO_Mode(EPD_BUSY_PIN, 0);
  DEV_GPIO_Mode(EPD_RST_PIN, 1);
  DEV_GPIO_Mode(EPD_DC_PIN, 1);
  DEV_GPIO_Mode(EPD_CS_PIN, 1);
  DEV_GPIO_Mode(EPD_PWR_PIN, 1);
  // DEV_GPIO_Mode(EPD_MOSI_PIN, 0);
  // DEV_GPIO_Mode(EPD_SCLK_PIN, 1);

  DEV_Digital_Write(EPD_CS_PIN, 1);
  DEV_Digital_Write(EPD_PWR_PIN, 1);

  printf("/DEV_Module_Init\n");
  return 0;
}

void DEV_Module_Exit(void) {
  // DEV_Digital_Write(EPD_CS_PIN, 0);
  // DEV_Digital_Write(EPD_PWR_PIN, 0);
  // DEV_Digital_Write(EPD_DC_PIN, 0);
  // DEV_Digital_Write(EPD_RST_PIN, 0);
  lgSpiClose(SPI_Handle);
  lgGpiochipClose(GPIO_Handle);
}

uint8_t DEV_Digital_Read(size_t Pin) {
  uint8_t Read_value = 0;
  Read_value = lgGpioRead(GPIO_Handle, Pin);
  return Read_value;
}

void DEV_Delay_ms(size_t xms) {
  struct timespec ts, rem;

  if (xms > 0) {
    ts.tv_sec = xms / 1000;
    ts.tv_nsec = (xms - (1000 * ts.tv_sec)) * 1E6;

    while (clock_nanosleep(CLOCK_REALTIME, 0, &ts, &rem))
      ts = rem;
  }
}

void DEV_SPI_WriteByte(uint8_t Value) {
  lgSpiWrite(SPI_Handle, (char *)&Value, 1);
}

static void EPD_2in13_V4_Reset(void) {
  DEV_Digital_Write(EPD_RST_PIN, 1);
  DEV_Delay_ms(20);
  DEV_Digital_Write(EPD_RST_PIN, 0);
  DEV_Delay_ms(2);
  DEV_Digital_Write(EPD_RST_PIN, 1);
  DEV_Delay_ms(20);
}

void EPD_2in13_V4_ReadBusy(void) {
  while (1) { //=1 BUSY
    if (DEV_Digital_Read(EPD_BUSY_PIN) == 0)
      break;
    DEV_Delay_ms(10);
  }
  DEV_Delay_ms(10);
}

void EPD_2in13_V4_SendCommand(uint8_t Reg) {
  DEV_Digital_Write(EPD_DC_PIN, 0);
  DEV_Digital_Write(EPD_CS_PIN, 0);
  DEV_SPI_WriteByte(Reg);
  DEV_Digital_Write(EPD_CS_PIN, 1);
}

void EPD_2in13_V4_SendData(uint8_t Data) {
  DEV_Digital_Write(EPD_DC_PIN, 1);
  DEV_Digital_Write(EPD_CS_PIN, 0);
  DEV_SPI_WriteByte(Data);
  DEV_Digital_Write(EPD_CS_PIN, 1);
}

void EPD_2in13_V4_TurnOnDisplay_Partial(void) {
  EPD_2in13_V4_SendCommand(0x22); // Display Update Control
  EPD_2in13_V4_SendData(0xff);    // fast:0x0c, quality:0x0f, 0xcf
  EPD_2in13_V4_SendCommand(0x20); // Activate Display Update Sequence
  EPD_2in13_V4_ReadBusy();
}

static void EPD_2in13_V4_SetWindows(size_t Xstart, size_t Ystart, size_t Xend,
                                    size_t Yend) {
  EPD_2in13_V4_SendCommand(0x44); // SET_RAM_X_ADDRESS_START_END_POSITION
  EPD_2in13_V4_SendData((Xstart >> 3) & 0xFF);
  EPD_2in13_V4_SendData((Xend >> 3) & 0xFF);

  EPD_2in13_V4_SendCommand(0x45); // SET_RAM_Y_ADDRESS_START_END_POSITION
  EPD_2in13_V4_SendData(Ystart & 0xFF);
  EPD_2in13_V4_SendData((Ystart >> 8) & 0xFF);
  EPD_2in13_V4_SendData(Yend & 0xFF);
  EPD_2in13_V4_SendData((Yend >> 8) & 0xFF);
}

static void EPD_2in13_V4_SetCursor(size_t Xstart, size_t Ystart) {
  EPD_2in13_V4_SendCommand(0x4E); // SET_RAM_X_ADDRESS_COUNTER
  EPD_2in13_V4_SendData(Xstart & 0xFF);

  EPD_2in13_V4_SendCommand(0x4F); // SET_RAM_Y_ADDRESS_COUNTER
  EPD_2in13_V4_SendData(Ystart & 0xFF);
  EPD_2in13_V4_SendData((Ystart >> 8) & 0xFF);
}

void foo_EPD_2in13_V4_Init(void) {
  EPD_2in13_V4_Reset();

  EPD_2in13_V4_ReadBusy();
  EPD_2in13_V4_SendCommand(0x12); // SWRESET
  EPD_2in13_V4_ReadBusy();

  EPD_2in13_V4_SendCommand(0x01); // Driver output control
  EPD_2in13_V4_SendData(0xF9);
  EPD_2in13_V4_SendData(0x00);
  EPD_2in13_V4_SendData(0x00);

  EPD_2in13_V4_SendCommand(0x11); // data entry mode
  EPD_2in13_V4_SendData(0x03);

  EPD_2in13_V4_SetWindows(0, 0, EPD_2in13_V4_WIDTH - 1,
                          EPD_2in13_V4_HEIGHT - 1);
  EPD_2in13_V4_SetCursor(0, 0);

  EPD_2in13_V4_SendCommand(0x3C); // BorderWavefrom
  EPD_2in13_V4_SendData(0x05);

  EPD_2in13_V4_SendCommand(0x21); //  Display update control
  EPD_2in13_V4_SendData(0x00);
  EPD_2in13_V4_SendData(0x80);

  EPD_2in13_V4_SendCommand(0x18); // Read built-in temperature sensor
  EPD_2in13_V4_SendData(0x80);
  EPD_2in13_V4_ReadBusy();
}

void foo_EPD_2in13_V4_Sleep(void) {
  EPD_2in13_V4_SendCommand(0x10); // enter deep sleep
  EPD_2in13_V4_SendData(0x01);
  DEV_Delay_ms(100);
}

void EPD_2in13_V4_TurnOnDisplay(void) {
  EPD_2in13_V4_SendCommand(0x22); // Display Update Control
  EPD_2in13_V4_SendData(0xf7);
  EPD_2in13_V4_SendCommand(0x20); // Activate Display Update Sequence
  EPD_2in13_V4_ReadBusy();
}


void foo_EPD_2in13_V4_Display(uint8_t *Image) {
  size_t Width, Height;
  Width = (EPD_2in13_V4_WIDTH % 8 == 0) ? (EPD_2in13_V4_WIDTH / 8)
                                        : (EPD_2in13_V4_WIDTH / 8 + 1);
  Height = EPD_2in13_V4_HEIGHT;

  EPD_2in13_V4_SendCommand(0x24);
  for (size_t j = 0; j < Height; j++) {
    for (size_t i = 0; i < Width; i++) {
      EPD_2in13_V4_SendData(Image[i + j * Width]);
    }
  }

  EPD_2in13_V4_TurnOnDisplay();
}

void foo_EPD_2in13_V4_Display_Partial(uint8_t *Image) {
  size_t Width, Height;
  Width = (EPD_2in13_V4_WIDTH % 8 == 0) ? (EPD_2in13_V4_WIDTH / 8)
                                        : (EPD_2in13_V4_WIDTH / 8 + 1);
  Height = EPD_2in13_V4_HEIGHT;

  // Reset
  DEV_Digital_Write(EPD_RST_PIN, 0);
  DEV_Delay_ms(1);
  DEV_Digital_Write(EPD_RST_PIN, 1);

  EPD_2in13_V4_SendCommand(0x3C); // BorderWavefrom
  EPD_2in13_V4_SendData(0x80);

  EPD_2in13_V4_SendCommand(0x01); // Driver output control
  EPD_2in13_V4_SendData(0xF9);
  EPD_2in13_V4_SendData(0x00);
  EPD_2in13_V4_SendData(0x00);

  EPD_2in13_V4_SendCommand(0x11); // data entry mode
  EPD_2in13_V4_SendData(0x03);

  EPD_2in13_V4_SetWindows(0, 0, EPD_2in13_V4_WIDTH - 1,
                          EPD_2in13_V4_HEIGHT - 1);
  EPD_2in13_V4_SetCursor(0, 0);

  EPD_2in13_V4_SendCommand(0x24); // Write Black and White image to RAM
  for (size_t j = 0; j < Height; j++) {
    for (size_t i = 0; i < Width; i++) {
      EPD_2in13_V4_SendData(Image[i + j * Width]);
    }
  }
  EPD_2in13_V4_TurnOnDisplay_Partial();
}


#include <cairo/cairo.h>
#include <stdbool.h>
#include <stdlib.h>

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

  struct EInkDisplay* display = malloc(sizeof(struct EInkDisplay));
  if (!display) {
      goto err;
  }

  foo_EPD_2in13_V4_Init();

  display->width = EPD_2in13_V4_HEIGHT;
  display->height = EPD_2in13_V4_WIDTH;
  display->surface = NULL;
  display->cr = NULL;

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

err:
  eink_delete(display);
  return NULL;
}

void eink_delete(struct EInkDisplay* display) {
  if (!display) {
    return;
  }

  if (display->surface) {
      cairo_destroy(display->cr);
  }

  if (display->cr) {
      cairo_surface_destroy(display->surface);
  }

  foo_EPD_2in13_V4_Sleep();
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

  foo_EPD_2in13_V4_Display(display_canvas);
  // TODO: Partial can be used but requires blanking out the pixels to be re-renered first
  //foo_EPD_2in13_V4_Display_Partial(display_canvas);
}

