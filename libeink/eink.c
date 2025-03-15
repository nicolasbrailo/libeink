#include "eink.h"

#include "liblgpio/lgpio.h"
#include <cairo/cairo.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct EInkDisplay {
  struct EInkConfig cfg;

  int gpio_handle;
  int spi_handle;

  size_t width;
  size_t height;
  bool invert_color;

  cairo_surface_t *surface;
  cairo_t *cr;
  int cairo_fg_color;
  int cairo_bg_color;

  // Used as a temporary buffer when rendering
  size_t render_buff_sz;
  uint8_t *render_buff;
};

#define EPD_2in13_V4_WIDTH 122
#define EPD_2in13_V4_HEIGHT 250
#define EPD_RST_PIN 17
#define EPD_DC_PIN 25
#define EPD_CS_PIN 8
#define EPD_PWR_PIN 18
#define EPD_BUSY_PIN 24
#define EPD_MOSI_PIN 10
#define EPD_SCLK_PIN 11

static void sleep_ms(size_t xms) {
  struct timespec ts, rem;

  if (xms > 0) {
    ts.tv_sec = xms / 1000;
    ts.tv_nsec = (xms - (1000 * ts.tv_sec)) * 1E6;

    while (clock_nanosleep(CLOCK_REALTIME, 0, &ts, &rem))
      ts = rem;
  }
}

static void dev_sleep_until_ready(struct EInkDisplay *display) {
  size_t timeout_cnt = 50;
  while (timeout_cnt-- > 0) {
    if (lgGpioRead(display->gpio_handle, EPD_BUSY_PIN) == 0)
      break;
    sleep_ms(10);
  }
  sleep_ms(10);

  if (timeout_cnt == 0) {
    fprintf(stderr,
            "Warning: timeout while waiting for device to become ready\n");
  }
}

enum Cmd_Or_Data {
  TX_CMD,
  TX_DATA,
};

static void dev_tx(struct EInkDisplay *display, enum Cmd_Or_Data is_cmd,
                   uint8_t d) {
  const int v = (is_cmd == TX_CMD) ? 0 : 1;
  lgGpioWrite(display->gpio_handle, EPD_DC_PIN, v);
  lgGpioWrite(display->gpio_handle, EPD_CS_PIN, 0);
  lgSpiWrite(display->spi_handle, (char *)&d, 1);
  lgGpioWrite(display->gpio_handle, EPD_CS_PIN, 1);
}

static void dev_wakeup(struct EInkDisplay *display, bool partial) {
  const int d = partial ? 0xff : 0xf7; // fast:0x0c, quality:0x0f, 0xcf
  dev_tx(display, TX_CMD, 0x22);       // Display Update Control
  dev_tx(display, TX_DATA, d);
  dev_tx(display, TX_CMD, 0x20); // Activate Display Update Sequence
  dev_sleep_until_ready(display);
}

static void dev_quick_reset(struct EInkDisplay *display) {
  dev_tx(display, TX_CMD, 0x01); // Driver output control
  dev_tx(display, TX_DATA, 0xF9);
  dev_tx(display, TX_DATA, 0x00);
  dev_tx(display, TX_DATA, 0x00);

  dev_tx(display, TX_CMD, 0x11); // data entry mode
  dev_tx(display, TX_DATA, 0x03);

  // SetWindows
  dev_tx(display, TX_CMD, 0x44); // SET_RAM_X_ADDRESS_START_END_POSITION
  dev_tx(display, TX_DATA, (0 >> 3) & 0xFF);
  dev_tx(display, TX_DATA, ((display->height - 1) >> 3) & 0xFF);

  dev_tx(display, TX_CMD, 0x45); // SET_RAM_Y_ADDRESS_START_END_POSITION
  dev_tx(display, TX_DATA, 0 & 0xFF);
  dev_tx(display, TX_DATA, (0 >> 8) & 0xFF);
  dev_tx(display, TX_DATA, (display->width - 1) & 0xFF);
  dev_tx(display, TX_DATA, ((display->width - 1) >> 8) & 0xFF);

  // Set cursor
  dev_tx(display, TX_CMD, 0x4E); // SET_RAM_X_ADDRESS_COUNTER
  dev_tx(display, TX_DATA, 0 & 0xFF);

  dev_tx(display, TX_CMD, 0x4F); // SET_RAM_Y_ADDRESS_COUNTER
  dev_tx(display, TX_DATA, 0 & 0xFF);
  dev_tx(display, TX_DATA, (0 >> 8) & 0xFF);
}

void dev_init(struct EInkDisplay *display) {
  // Reset device
  lgGpioWrite(display->gpio_handle, EPD_RST_PIN, 1);
  sleep_ms(20);
  lgGpioWrite(display->gpio_handle, EPD_RST_PIN, 0);
  sleep_ms(2);
  lgGpioWrite(display->gpio_handle, EPD_RST_PIN, 1);
  sleep_ms(20);

  // Init device
  dev_sleep_until_ready(display);
  dev_tx(display, TX_CMD, 0x12); // SWRESET
  dev_sleep_until_ready(display);

  dev_quick_reset(display);

  dev_tx(display, TX_CMD, 0x3C); // BorderWavefrom
  dev_tx(display, TX_DATA, 0x05);

  dev_tx(display, TX_CMD, 0x21); //  Display update control
  dev_tx(display, TX_DATA, 0x00);
  dev_tx(display, TX_DATA, 0x80);

  dev_tx(display, TX_CMD, 0x18); // Read built-in temperature sensor
  dev_tx(display, TX_DATA, 0x80);
  dev_sleep_until_ready(display);
}

void dev_shutdown(struct EInkDisplay *display) {
  dev_tx(display, TX_CMD, 0x10); // enter deep sleep
  dev_tx(display, TX_DATA, 0x01);
  sleep_ms(2000); // important, at least 2s
}

void dev_render(struct EInkDisplay *display, uint8_t *Image,
                bool is_partial_update) {
  if (is_partial_update) {
    // Reset
    lgGpioWrite(display->gpio_handle, EPD_RST_PIN, 0);
    sleep_ms(1);
    lgGpioWrite(display->gpio_handle, EPD_RST_PIN, 1);

    dev_tx(display, TX_CMD, 0x3C); // BorderWavefrom
    dev_tx(display, TX_DATA, 0x80);
    dev_quick_reset(display);
  } else {
    // This seems to control how each pixel is updated in the display
    dev_tx(display, TX_CMD, 0x3C); // BorderWavefrom
    dev_tx(display, TX_DATA, 0x05);
  }

  // Write Black and White image to RAM
  dev_tx(display, TX_CMD, 0x24);

  const size_t byte_height = (display->height % 8 == 0)
                                 ? (display->height / 8)
                                 : (display->height / 8 + 1);
  for (size_t j = 0; j < display->width; j++) {
    for (size_t i = 0; i < byte_height; i++) {
      dev_tx(display, TX_DATA, Image[i + j * byte_height]);
    }
  }

  if (is_partial_update) {
    dev_tx(display, TX_CMD, 0x26);
    const size_t byte_height = (display->height % 8 == 0)
                                   ? (display->height / 8)
                                   : (display->height / 8 + 1);
    for (size_t j = 0; j < display->width; j++) {
      for (size_t i = 0; i < byte_height; i++) {
        dev_tx(display, TX_DATA, Image[i + j * byte_height]);
      }
    }
  }

  dev_wakeup(display, is_partial_update);
}

struct EInkDisplay *eink_init(struct EInkConfig* cfg) {
  struct EInkDisplay *display = malloc(sizeof(struct EInkDisplay));
  if (!display) {
    fprintf(stderr, "bad_alloc: display\n");
    return NULL;
  }

  display->cfg.mock_display = cfg->mock_display;
  display->cfg.save_render_to_png_file = strdup(cfg->save_render_to_png_file);
  if (cfg->save_render_to_png_file && !display->cfg.save_render_to_png_file) {
    fprintf(stderr, "bad_alloc: save_render_to_png_file\n");
    goto err;
  }

  display->gpio_handle = -1;
  display->spi_handle = -1;
  display->width = EPD_2in13_V4_HEIGHT;
  display->height = EPD_2in13_V4_WIDTH;
  display->surface = NULL;
  display->cr = NULL;

  // Select black-on-white or reverse
  display->invert_color = false;
  display->cairo_fg_color = 1;
  display->cairo_bg_color = 0;

  // Create a monochrome (1-bit) surface
  display->surface = cairo_image_surface_create(CAIRO_FORMAT_A1, display->width,
                                                display->height);

  const size_t height_bytes = (display->height + 7) / 8; // Same as ceil(height / 8)
  display->render_buff_sz = height_bytes * display->width;
  display->render_buff = malloc(display->render_buff_sz);
  display->cr = cairo_create(display->surface);

  if (!display->surface || !display->render_buff || !display->cr) {
    fprintf(stderr, "bad_alloc\n");
    goto err;
  }

  cairo_set_operator(display->cr, CAIRO_OPERATOR_SOURCE);

  // Set background color to white (transparent in A1 format)
  cairo_set_source_rgba(display->cr, 1, 1, 1, display->cairo_bg_color);
  cairo_paint(display->cr);

  if (!display->cfg.mock_display) {
    display->gpio_handle = lgGpiochipOpen(0);
    if (display->gpio_handle < 0) {
      display->gpio_handle = lgGpiochipOpen(4);
      if (display->gpio_handle < 0) {
        fprintf(stderr, "Can't find /dev/gpiochip[1|4]\n");
        goto err;
      }
    }

    display->spi_handle = lgSpiOpen(0, 0, 10000000, 0);
    if (display->spi_handle == LG_SPI_IOCTL_FAILED) {
      fprintf(stderr, "Can't open SPI\n");
      goto err;
    }

    lgGpioClaimInput(display->gpio_handle, 0, EPD_BUSY_PIN);
    lgGpioClaimOutput(display->gpio_handle, 0, EPD_RST_PIN, LG_LOW);
    lgGpioClaimOutput(display->gpio_handle, 0, EPD_DC_PIN, LG_LOW);
    lgGpioClaimOutput(display->gpio_handle, 0, EPD_CS_PIN, LG_LOW);
    lgGpioClaimOutput(display->gpio_handle, 0, EPD_PWR_PIN, LG_LOW);

    // This is commented out in the manufacturer example, not sure why
    // lgGpioClaimInput(display->gpio_handle, 0, EPD_MOSI_PIN);
    // lgGpioClaimOutput(display->gpio_handle, 0, EPD_SCLK_PIN, LG_LOW);

    lgGpioWrite(display->gpio_handle, EPD_CS_PIN, 1);
    lgGpioWrite(display->gpio_handle, EPD_PWR_PIN, 1);

    dev_init(display);
  }

  return display;

err:
  eink_delete(display);
  return NULL;
}

void eink_delete(struct EInkDisplay *display) {
  if (!display) {
    return;
  }

  if (display->surface) {
    cairo_destroy(display->cr);
  }

  if (display->render_buff) {
    free(display->render_buff);
  }

  if (display->cr) {
    cairo_surface_destroy(display->surface);
  }

  if ((display->gpio_handle >= 0) && (display->spi_handle >= 0)) {
    dev_shutdown(display);
  }

  if (display->gpio_handle >= 0) {
    // This is commented out in the manufacturer example, not sure why
    // lgGpioWrite(display->gpio_handle, EPD_CS_PIN, 0);
    // lgGpioWrite(display->gpio_handle, EPD_PWR_PIN, 0);
    // lgGpioWrite(display->gpio_handle, EPD_DC_PIN, 0);
    // lgGpioWrite(display->gpio_handle, EPD_RST_PIN, 0);
    lgGpiochipClose(display->gpio_handle);
  }

  if (display->spi_handle >= 0) {
    lgSpiClose(display->spi_handle);
  }

  free((void*)display->cfg.save_render_to_png_file);
  free(display);
}

cairo_t *eink_get_cairo(struct EInkDisplay *display) { return display->cr; }

static void eink_render_impl(struct EInkDisplay *display, bool is_partial) {
  const size_t stride = cairo_image_surface_get_stride(display->surface);
  uint8_t *img_data = cairo_image_surface_get_data(display->surface);
  const size_t height_bytes = (display->height + 7) / 8; // Same as ceil(height / 8)

  memset(display->render_buff, 0, display->render_buff_sz);

  for (size_t y = 0; y < display->height; y++) {
    for (size_t x = 0; x < display->width; x++) {
      const size_t src_byte_index = x / 8 + y * stride;
      const size_t src_bit_index = x % 8;

      // Display memory is rotated; rotate coordinates
      const size_t dest_x = display->height - 1 - y;
      const size_t dest_y = x;
      const size_t dest_byte_index = dest_x / 8 + dest_y * height_bytes;
      const size_t dest_bit_index = dest_x % 8;

      bool pixel_set = img_data[src_byte_index] & (1 << src_bit_index);
      if (pixel_set == display->invert_color) {
        display->render_buff[dest_byte_index] |= (1 << (7 - dest_bit_index));
      } else {
        display->render_buff[dest_byte_index] &= ~(1 << (7 - dest_bit_index));
      }
    }
  }

  if (!display->cfg.mock_display) {
    printf("EInk: skip render, mocking display\n");
  } else {
    dev_render(display, display->render_buff, is_partial);
  }

  if (display->cfg.save_render_to_png_file) {
    cairo_surface_write_to_png(display->surface, display->cfg.save_render_to_png_file);
  }
}

void eink_render(struct EInkDisplay *display) {
  eink_render_impl(display, false);
}

void eink_render_partial(struct EInkDisplay *display) {
  eink_render_impl(display, true);
}

static int cairo_get_last_set_color(struct EInkDisplay *display) {
  cairo_pattern_t *pattern = cairo_get_source(display->cr);
  double r, g, b, a;
  if (cairo_pattern_get_rgba(pattern, &r, &g, &b, &a) != CAIRO_STATUS_SUCCESS) {
    return display->cairo_fg_color;
  }
  return a < 0.1 ? 0 : 1;
}

void eink_invalidate_rect(struct EInkDisplay *display, size_t x_start,
                          size_t y_start, size_t x_end, size_t y_end) {
  const int user_set_color = cairo_get_last_set_color(display);

  // Resetting to fg color and then to bg color may have better results?
  const int invalidate_color = display->cairo_bg_color;

  // "Cover" invalidated area with a box
  cairo_set_source_rgba(display->cr, 0, 0, 0, invalidate_color);
  cairo_rectangle(display->cr, x_start, y_start, x_end, y_end);
  cairo_fill(display->cr);
  cairo_stroke(display->cr);

  // Quick-draw to screen: this will force all pixels to cycle, and the real
  // call to render_partial by the user will set the right pixels without
  // glitches
  eink_render_partial(display);

  if (invalidate_color == display->cairo_fg_color) {
    // Reset invalidated area to blank
    cairo_set_source_rgba(display->cr, 0, 0, 0, display->cairo_bg_color);
    cairo_rectangle(display->cr, x_start, y_start, x_end, y_end);
    cairo_fill(display->cr);
  }

  // Restore "pen"
  cairo_set_source_rgba(display->cr, 0, 0, 0, user_set_color);
}

void eink_clear(struct EInkDisplay *display) {
  const int user_set_color = cairo_get_last_set_color(display);
  // Clear canvas
  cairo_set_source_rgba(display->cr, 0, 0, 0, display->cairo_bg_color);
  cairo_paint(display->cr);
  // Restore "pen"
  cairo_set_source_rgba(display->cr, 0, 0, 0, user_set_color);

  dev_tx(display, TX_CMD, 0x24);
  for (size_t c = 0; c < 2; ++c) {
    // Screen memory is rotated 90 degs
    const size_t height_bytes = (display->height % 8) == 0
                                    ? display->height / 8
                                    : display->height / 8 + 1;
    for (size_t i = 0; i < display->width; ++i) {
      for (size_t j = 0; j < height_bytes; ++j) {
        dev_tx(display, TX_DATA, display->invert_color ? 0x00 : 0xFF);
      }
    }

    dev_tx(display, TX_CMD, 0x26);
  }

  dev_wakeup(display, false);
}
