#pragma once

// Controller for https://www.waveshare.com/wiki/2.13inch_e-Paper_HAT_Manual
//
// Pixel wire format:
// * Looking at the display with the wires coming off the top and the ribbon
// cable to the right
// * There are 250*122 = 30500 pixels
// * 0,0 is at the bottom left of the display
// * Each pixel is "vertical" (ie 0XFF will draw 8 vertical pixels at 0,0)
// * Pixels are sent scanning from 0x0 to 0x122, 1x0 to 1x122...
// * Pixels are sent 8 at a time (so the first column is an array of 16 bytes)
// * Last drawable colum seems to be 237, even though display says 250. I'm sure
// it's a bug in my code.
//

// fwd-decls
struct EInkDisplay;
typedef struct _cairo cairo_t;

struct EInkDisplay *eink_init();
void eink_delete(struct EInkDisplay *display);

cairo_t *eink_get_cairo(struct EInkDisplay *display);
void eink_render(struct EInkDisplay *display);
void eink_render_partial(struct EInkDisplay *display);
void eink_clear(struct EInkDisplay *display);

