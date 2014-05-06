
.intel_syntax noprefix

.section .text

.align 32
xtion_depth_unpack_shuffle:

# Code word 0
.byte 1
.byte 0
.byte 0xFF
.byte 0xFF

# Code word 1
.byte 2
.byte 1
.byte 0xFF
.byte 0xFF

# Code word 2
.byte 4
.byte 3
.byte 2
.byte 0xFF

# Code word 3
.byte 5
.byte 4
.byte 0xFF
.byte 0xFF

# Code word 4
.byte 6
.byte 5
.byte 0xFF
.byte 0xFF

# Code word 5
.byte 8
.byte 7
.byte 6
.byte 0xFF

# Code word 6
.byte 9
.byte 8
.byte 0xFF
.byte 0xFF

# Code word 7
.byte 10
.byte 9
.byte 0xFF
.byte 0xFF

.align 32
xtion_depth_unpack_shift:

.int 5
.int 2
.int 7
.int 4
.int 1
.int 6
.int 3
.int 0

.align 32
xtion_depth_unpack_mask:

.int 0x000007FF
.int 0x000007FF
.int 0x000007FF
.int 0x000007FF
.int 0x000007FF
.int 0x000007FF
.int 0x000007FF
.int 0x000007FF

.align 32
xtion_depth_unpack_cond:

.int 0xFFFFFFFF
.int 0xFFFFFFFF
.int 0xFFFFFFFF
.int 0xFFFFFFFF
.int 0xFFFFFFFF
.int 0xFFFFFFFF
.int 0xFFFFFFFF
.int 0xFFFFFFFF

.align 32
xtion_depth_unpack_mask2:

.int 0x0000FFFF
.int 0x0000FFFF
.int 0x0000FFFF
.int 0x0000FFFF
.int 0x0000FFFF
.int 0x0000FFFF
.int 0x0000FFFF
.int 0x0000FFFF

.globl xtion_depth_unpack_AVX2
xtion_depth_unpack_AVX2:
	#  signature: extern void __parseDepth_AVX2(const uint8_t* __restrict input, const uint16_t* __restrict lut, uint16_t* output, uint32_t size);
	# ARGUMENTS
	#  rdi input
	#  rsi lut
	#  rdx output
	#  rcx size

	vzeroall

	# Setup constants
	vpxor ymm0, ymm0, ymm0
	vmovdqa ymm10, [rip+xtion_depth_unpack_shuffle]
	vmovdqa ymm11, [rip+xtion_depth_unpack_shift]
	vmovdqa ymm12, [rip+xtion_depth_unpack_mask]
	vmovdqa ymm14, [rip+xtion_depth_unpack_mask2]

	# Loop over all values
	1:
		vpcmpeqd ymm13, ymm13, ymm13

		# Replicate input in both 128 bit lanes
		vmovdqu xmm1, XMMWORD PTR [rdi]
		vinsertf128 ymm1, ymm1, XMMWORD PTR [rdi], 1

		# Byte shuffle => each code word lies in its own DWORD
		vpshufb ymm1, ymm1, ymm10

		# Shift right to align
		vpsrlvd ymm1, ymm1, ymm11

		# Mask to clean upper bits
		vpand ymm1, ymm1, ymm12

		# Lookup each element in the LUT
		vpgatherdd ymm2, [rsi + ymm1*2], ymm13

		# Mask to clean upper bits
		vpand ymm2, ymm2, ymm14

		# Extract upper half
		vextracti128 xmm3, ymm2, 1

		# Pack to 16 bit each
		vpackusdw ymm1, ymm2, ymm3

		vmovdqa XMMWORD PTR [rdx], xmm1

		# Increase pointers
		add rdi, 11
		add rdx, 16

		dec rcx
		test rcx, rcx
		jnz 1b

	vzeroall
	ret
