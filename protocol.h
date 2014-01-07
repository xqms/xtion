// Xtion protocol
// Author: Max Schwarz <max.schwarz@online.de>
//  definitions taken from OpenNI2.

#ifndef XTION_PROTOCOL_H
#define XTION_PROTOCOL_H

#include <linux/types.h>

#define XTION_MAGIC_HOST 0x4d47
#define XTION_MAGIC_DEV 0x4252

#define XTION_OPCODE_GET_VERSION 0
#define XTION_OPCODE_SET_PARAM 3
#define XTION_OPCODE_SET_MODE 6
#define XTION_OPCODE_GET_FIXED_PARAMS 4
#define XTION_OPCODE_GET_SERIAL_NUMBER 37


#define XTION_P_FRAME_SYNC 1
#define XTION_P_REGISTRATION 2
#define XTION_P_GENERAL_STREAM0_MODE 5
#define XTION_P_GENERAL_STREAM1_MODE 6

#define XTION_CHANNEL_P_FORMAT     0
#define XTION_CHANNEL_P_RESOLUTION 1
#define XTION_CHANNEL_P_FPS        2

#define XTION_P_IMAGE_BASE         12
#define XTION_P_DEPTH_BASE         18

#define XTION_VIDEO_STREAM_OFF       0
#define XTION_VIDEO_STREAM_COLOR     1
#define XTION_VIDEO_STREAM_DEPTH     2


#define XTION_IMG_FORMAT_BAYER     0
#define XTION_IMG_FORMAT_YUV422    1
#define XTION_IMG_FORMAT_JPEG      2
#define XTION_IMG_FORMAT_UNC_YUV422 5

#define XTION_DEPTH_FORMAT_UNC_16_BIT 0
#define XTION_DEPTH_FORMAT_COMP_PS    1
#define XTION_DEPTH_FORMAT_UNC_10_BIT 2
#define XTION_DEPTH_FORMAT_UNC_11_BIT 3
#define XTION_DEPTH_FORMAT_UNC_12_BIT 4

struct XtionHeader
{
	__u16 magic;
	__u16 size;
	__u16 opcode;
	__u16 id;
} __attribute__((packed));

struct XtionReplyHeader
{
	struct XtionHeader header;
	__u16 error;
} __attribute__((packed));

struct XtionSensorReplyHeader
{
	__u16 magic;
	__u16 type;
	__u16 packetID;
	__u8 bufSize_high;
	__u8 bufSize_low;
	__u32 timestamp;
} __attribute__((packed));

struct XtionVersion
{
	__u8 major;
	__u8 minor;
	__u16 build;
	__u32 chip;
	__u16 fpga;
	__u16 system_version;
} __attribute__((packed));

struct XtionFixedParams
{
	// Misc
	__u32 serial_number;
	__u32 watch_dog_timeout;

	// Flash
	__u32 flash_type;
	__u32 flash_size;
	__u32 flash_burst_enable;
	__u32 fmif_read_burst_cycles;
	__u32 fmif_read_access_cycles;
	__u32 fmif_read_recover_cycles;
	__u32 fmif_write_access_cycles;
	__u32 fmif_write_recover_cycles;
	__u32 fmif_write_assertion_cycles;

	// Audio
	__u32 i2s_logic_clock_polartiy;

	// Depth
	__u32 depth_ciu_horizontal_sync_polarity;
	__u32 depth_ciu_vertical_sync_polarity;
	__u32 depth_cmos_type;
	__u32 depth_cmos_i2c_address;
	__u32 depth_cmos_i2c_bus;

	// Image
	__u32 image_ciu_horizontal_sync_polarity;
	__u32 image_ciu_vertical_sync_polarity;
	__u32 image_cmos_type;
	__u32 image_cmos_i2c_address;
	__u32 image_cmos_i2c_bus;

	// Geometry
	__u32 ir_cmos_close_to_projector;
	float dcmos_emitter_distance;
	float dcmos_rcmos_distance;
	float reference_distance;
	float reference_pixel_size;

	// Clocks
	__u32 pll_value;
	__u32 system_clock_divider;
	__u32 rcmos_clock_divider;
	__u32 dcmos_clock_divider;
	__u32 adc_clock_divider;
	__u32 i2c_standard_speed_hcount;
	__u32 i2c_standard_speed_lcount;
	__u32 i2c_hold_fix_delay;

	__u32 sensor_type;
	__u32 debug_mode;
	__u32 use_ext_phy;
	__u32 projector_protection_enabled;
	__u32 projector_dac_output_voltage;
	__u32 projector_dac_output_voltage2;
	__u32 tec_emitter_delay;
} __attribute__((packed));

struct XtionFixedParamRequest
{
	struct XtionHeader header;
	__u16 addr;
} __attribute__((packed));

struct XtionSetParamRequest
{
	struct XtionHeader header;
	__u16 param;
	__u16 value;
} __attribute__((packed));

struct XtionSetModeRequest
{
	struct XtionHeader header;
	__u16 mode;
} __attribute__((packed));

#endif
