#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#include "xtion-math-emu.h"

#define max(a, b) (a > b ? a: b)

const float eps = 1e-2;

void run_test(float a, float b, float eps)
{
	float32 a32;
	float32 b32;
	float32 c32;
	
	a32.float_ = a;
	b32.float_ = b;

	mul_f32(&a32, &b32, &c32);
	if (fabs(a*b - c32.float_) > eps) {
		printf("FAILURE: %f != %f (%f * %f)\n", a*b, c32.float_, a, b);
	} else {
		printf("SUCCESS: (%f * %f) = %f\n", a, b, c32.float_);
	}

	add_f32(&a32, &b32, &c32);
	if (fabs(a + b - c32.float_) > eps) {
		float32 tt = { .float_ = a + b };

		printf("FAILURE: %f != %f (%f + %f)\n", a+b, c32.float_, a, b);
	} else {
		printf("SUCCESS: (%f + %f) = %f\n", a, b, c32.float_);
	}

	int d = div_f32(&a32, &b32);
	if (d != (int) (a / b)) {
		printf("FAILURE: (%f / %f) != %d (should be %d)\n", a, b, d, (int)(a/b));
	} else {
		printf("SUCCESS: (%f / %f) = %d\n", a, b, d);
	}
}

void run_basic_tests()
{

	run_test(3.2f, 1.2f, eps);
	run_test(12.2f, 0.23f, eps);
	run_test(132.2f, 54.2f, eps);
	run_test(0.02f, 7.7f, eps);
	run_test(3.02f, 2.7f, eps);
	run_test(3.02f, -2.7f, eps);
	run_test(0.02f, -24.3f, eps);
	run_test(-187.02f, -14.3f, eps);
	run_test(-9980.f, 60.0f, eps);
	run_test(60.0f, -9980.f, eps);
	run_test(144.f, 281.554749f, eps);
	run_test(14400.f, -130.209839f, eps);
	run_test(-128.14151f, 240.f, eps);
	run_test(128.14151f, -128.14151f, eps);
}

int main(int argv, char* argc[]) {
	run_basic_tests();
  return 0;
}
