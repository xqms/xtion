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
#define XTION_OPCODE_GET_CMOS_PRESETS 36
#define XTION_OPCODE_GET_SERIAL_NUMBER 37
#define XTION_OPCODE_GET_ALGORITHM_PARAMS 22 // might be 21 in the older FWs


#define XTION_P_FRAME_SYNC 1
#define XTION_P_REGISTRATION 2
#define XTION_P_GENERAL_STREAM0_MODE 5
#define XTION_P_GENERAL_STREAM1_MODE 6
#define XTION_P_CLOSE_RANGE 84

#define XTION_CHANNEL_P_FORMAT     0
#define XTION_CHANNEL_P_RESOLUTION 1
#define XTION_CHANNEL_P_FPS        2

#define XTION_P_IMAGE_BASE         12
#define XTION_P_DEPTH_BASE         18

#define XTION_VIDEO_STREAM_OFF       0
#define XTION_VIDEO_STREAM_COLOR     1
#define XTION_VIDEO_STREAM_DEPTH     2

#define XTION_P_IMAGE_FLICKER      17

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
	u16 magic;
	u16 size;
	u16 opcode;
	u16 id;
} __attribute__((packed));

struct XtionReplyHeader
{
	struct XtionHeader header;
	u16 error;
} __attribute__((packed));

struct XtionSensorReplyHeader
{
	u16 magic;
	u16 type;
	u16 packetID;
	u8 bufSize_high;
	u8 bufSize_low;
	u32 timestamp;
} __attribute__((packed));

struct XtionVersion
{
	u8 minor;
	u8 major;
	u16 build;
	u32 chip;
	u16 fpga;
	u16 system_version;
} __attribute__((packed));

struct XtionFixedParams
{
	// Misc
	u32 serial_number;
	u32 watch_dog_timeout;

	// Flash
	u32 flash_type;
	u32 flash_size;
	u32 flash_burst_enable;
	u32 fmif_read_burst_cycles;
	u32 fmif_read_access_cycles;
	u32 fmif_read_recover_cycles;
	u32 fmif_write_access_cycles;
	u32 fmif_write_recover_cycles;
	u32 fmif_write_assertion_cycles;

	// Audio
	u32 i2s_logic_clock_polartiy;

	// Depth
	u32 depth_ciu_horizontal_sync_polarity;
	u32 depth_ciu_vertical_sync_polarity;
	u32 depth_cmos_type;
	u32 depth_cmos_i2c_address;
	u32 depth_cmos_i2c_bus;

	// Image
	u32 image_ciu_horizontal_sync_polarity;
	u32 image_ciu_vertical_sync_polarity;
	u32 image_cmos_type;
	u32 image_cmos_i2c_address;
	u32 image_cmos_i2c_bus;

	// Geometry
	u32 ir_cmos_close_to_projector;
	float dcmos_emitter_distance;
	float dcmos_rcmos_distance;
	float reference_distance;
	float reference_pixel_size;

	// Clocks
	u32 pll_value;
	u32 system_clock_divider;
	u32 rcmos_clock_divider;
	u32 dcmos_clock_divider;
	u32 adc_clock_divider;
	u32 i2c_standard_speed_hcount;
	u32 i2c_standard_speed_lcount;
	u32 i2c_hold_fix_delay;

	u32 sensor_type;
	u32 debug_mode;
	u32 use_ext_phy;
	u32 projector_protection_enabled;
	u32 projector_dac_output_voltage;
	u32 projector_dac_output_voltage2;
	u32 tec_emitter_delay;
} __attribute__((packed));

struct XtionAlgorithmParams {
	u16 const_shift;
} __attribute__((packed));

struct XtionAlgorithmParamsRequest {
	struct XtionHeader header;
	u16 param_id;
	u16 format;
	u16 resolution;
	u16 fps;
	u16 offset;
} __attribute__((packed));

struct XtionFixedParamRequest
{
	struct XtionHeader header;
	u16 addr;
} __attribute__((packed));

struct XtionSetParamRequest
{
	struct XtionHeader header;
	u16 param;
	u16 value;
} __attribute__((packed));

struct XtionSetModeRequest
{
	struct XtionHeader header;
	u16 mode;
} __attribute__((packed));

struct XtionGetCmosModesRequest
{
	struct XtionHeader header;
	u16 cmos;
} __attribute__((packed));

struct XtionCmosMode
{
	u16 format;
	u16 resolution;
	u16 fps;
} __attribute__((packed));

#endif
