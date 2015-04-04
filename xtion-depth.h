/*
 * ASUS xtion depth channel
 */

#ifndef XTION_DEPTH_H
#define XTION_DEPTH_H

#include "xtion.h"

// Defines from OpenNI2
#define MAX_SHIFT_VALUE       2047
#define PIXEL_SIZE_FACTOR			1.f
#define PARAM_COEFF           4.f
#define SHIFT_SCALE           10.f
#define MAX_DEPTH_VALUE       10000.f
#define DEPTH_MAX_CUTOFF      64434.f   // why perhaps 65534?
#define DEPTH_MIN_CUTOFF      0.f


int xtion_depth_init(struct xtion_depth *depth, struct xtion *xtion);

void xtion_depth_release(struct xtion_depth *depth);

#endif
