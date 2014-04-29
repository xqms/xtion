/*
 * USB I/O for ASUS xtion
 *
 * Author: Max Schwarz <max.schwarz@online.de>
 */

#ifndef XTION_IO_H
#define XTION_IO_H

#include <linux/types.h>

struct xtion;

int xtion_control(struct xtion* xtion, u8 *src, u16 size, u8 *dst, u16 *dst_size);
int xtion_read_version(struct xtion* xtion);
int xtion_read_fixed_params(struct xtion* xtion);
int xtion_read_serial_number(struct xtion *xtion);
int xtion_set_param(struct xtion *xtion, u16 parameter, u16 value);
int xtion_reset(struct xtion *xtion);

#endif
