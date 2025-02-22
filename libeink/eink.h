#pragma once

// fwd-decls
struct EInkDisplay;
typedef struct _cairo cairo_t;

struct EInkDisplay* eink_init();
void eink_delete(struct EInkDisplay* display);
cairo_t* eink_get_cairo(struct EInkDisplay* display);
void eink_render(struct EInkDisplay* display);

