/*
 * ASUS xtion color channel
 */

#ifndef XTION_COLOR_H
#define XTION_COLOR_H

#include "xtion.h"

int xtion_color_init(struct xtion_color *color, struct xtion *xtion);
void xtion_color_release(struct xtion_color *color);

#endif
