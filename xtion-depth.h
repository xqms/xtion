/*
 * ASUS xtion depth channel
 */

#ifndef XTION_DEPTH_H
#define XTION_DEPTH_H

#include "xtion.h"

int xtion_depth_init(struct xtion_depth *depth, struct xtion *xtion);

void xtion_depth_release(struct xtion_depth *depth);

#endif
