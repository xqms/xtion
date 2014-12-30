#ifndef XTION_MATH_EMU_H
#define XTION_MATH_EMU_H

#include <linux/types.h>

#define xtion_max(a, b) (a > b ? a: b)
#define xtion_min(a, b) (a < b ? a: b)

typedef union Float {
	float float_;
	uint32_t uint32_;
	struct {
		uint32_t mantisa : 23;
		uint32_t exp : 8;
		unsigned sign : 1;
	};

} float32;

#define I2F_MAX_BITS 16
#define I2F_MAX_INPUT  ((1 << I2F_MAX_BITS) - 1)
#define I2F_SHIFT (24 - I2F_MAX_BITS)

u32 u2f(u32 input)
{
	u32 result, i, exponent, fraction;

	if ((input & I2F_MAX_INPUT) == 0)
		result = 0;
	else {
		exponent = 126 + I2F_MAX_BITS;
		fraction = (input & I2F_MAX_INPUT) << I2F_SHIFT;

		for (i = 0; i < I2F_MAX_BITS; i++) {
			if (fraction & 0x800000)
				break;
			else {
				fraction = fraction << 1;
				exponent = exponent - 1;
			}
		}
		result = exponent << 23 | (fraction & 0x7fffff);
	}
	return result;
}

/* counts the number of set bits */
u32 bit_count(u32 u)
{
	u32 u_count;

	u_count = u - ((u >> 1) & 033333333333) - ((u >> 2) & 011111111111);
	return ((u_count + (u_count >> 3)) & 030707070707) % 63;
}

/* counts the number of leading zeros */
int lead_zeros(int x)
{
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return(32 - bit_count(x));
}

int div_f32(float32* a, float32* b)
{
	u16 shift;
	u64 aa, bb;
	int mantisa;

	shift = xtion_max(b->exp, a->exp);
	
	aa = (a->mantisa | 0x800000);
	aa = aa << (shift - b->exp); 
	
	bb = (b->mantisa | 0x800000);
	bb = bb << (shift - a->exp);

	mantisa = aa / bb;
	
	// rounding
	//if (aa % bb > bb / 2)
	//	mantisa += 1;
	
	if (b->sign ^ a->sign)
		mantisa = -mantisa;

	return mantisa;
}

void mul_f32(float32* a, float32* b, float32* c)
{
	static const u64 mask = 0x7FFFFF;
	
	u64 aa = a->mantisa | 0x800000;
	u64 bb = b->mantisa | 0x800000;
	u64 cc = (aa * bb) >> 23;

	u32 cc_ = (cc & (~mask)) >> 24;
	u16 bc = bit_count(cc_);
	
	c->mantisa = (cc >> bc) & 0x7FFFFF;
	c->exp = bc + a->exp + b->exp - 127;
	c->sign = a->sign ^ b->sign;
}

void add_f32(float32* a, float32* b, float32* c)
{
	static const u32 mask = 0x800000;
	short lz;

	u32 aa = a->mantisa | mask;
	u32 bb = b->mantisa | mask;
	u32 cc;

	c->exp = xtion_max(b->exp, a->exp);
	aa = aa >> (c->exp - a->exp);
	bb = bb >> (c->exp - b->exp);

	if (a->sign ^ b->sign)
	{ 
		c->sign = ((b->sign && bb > aa) || (a->sign && aa > bb)) ? 1 : 0;
		cc = (bb > aa) ? bb - aa : aa - bb;
	}
	else
	{
		cc = bb + aa;
		c->sign = a->sign & b->sign;
	}

	lz = lead_zeros(cc) - 8;
	c->mantisa = (lz > 0) ? cc << lz : cc >> (-lz);
	c->exp -= lz;
}

#endif
