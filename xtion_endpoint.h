/*
 * One data endpoint for ASUS Xtion Pro Live
 */

#ifndef XTION_ENDPOINT_H
#define XTION_ENDPOINT_H

#include "xtion.h"

int xtion_endpoint_init(struct xtion_endpoint *endp, struct xtion *xtion, const struct xtion_endpoint_config *config);
void xtion_endpoint_release(struct xtion_endpoint* endp);

#endif
