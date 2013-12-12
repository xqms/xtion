/*
 * USB I/O for ASUS xtion
 *
 * Author: Max Schwarz <max.schwarz@online.de>
 */

#ifndef XTION_IO_H
#define XTION_IO_H

#include <linux/types.h>

struct xtion;

int xtion_control(struct xtion* xtion, __u8 *src, __u16 size, __u8 *dst, __u16 *dst_size);
int xtion_read_version(struct xtion* xtion);
int xtion_read_fixed_params(struct xtion* xtion);
int xtion_read_serial_number(struct xtion *xtion);
int xtion_set_param(struct xtion *xtion, __u16 parameter, __u16 value);

#endif
