#ifndef __LEDS_AW210XX_REG_H__
#define __LEDS_AW210XX_REG_H__
#include "aw_lamp_interface.h"
#define LED_NUM 24

#define AW21024	0
#define AW21036	0
#define AW21012 1
/******************************************************
 *
 * Register List
 *
 *****************************************************/
#define REG_GCR					0x20
#define REG_BR0					0x21
#define REG_BR1					0x23
#define REG_BR2					0x25
#define REG_BR3					0x27
#define REG_BR4					0x29
#define REG_BR5					0x2B
#define REG_BR6					0x2D
#define REG_BR7					0x2F
#define REG_BR8					0x31
#define REG_BR9					0x33
#define REG_BR10				0x35
#define REG_BR11				0x37


#define REG_UPDATE				0x45
#define REG_COL0				0x46
#define REG_COL1				0x47
#define REG_COL2				0x48
#define REG_COL3				0x49
#define REG_COL4				0x4A
#define REG_COL5				0x4B
#define REG_COL6				0x4C
#define REG_COL7				0x4D
#define REG_COL8				0x4E
#define REG_COL9				0x4F
#define REG_COL10				0x50
#define REG_COL11				0x51



#define REG_GCCR				0x58
#define REG_PHCR				0x59
#define REG_OSDCR				0x5A

#define REG_OSST0				0x5B
#define REG_OSST1				0x5C

#define REG_OTCR				0x5E
#define REG_SSCR				0x5F
#define REG_UVCR				0x60

#define REG_GCR2				0x61
#define REG_GCR3				0x62

#define REG_RESET				0x70


#define REG_PATCFG				0x80
#define REG_PATGO				0x81
#define REG_PATCT0				0x82
#define REG_PATCT1				0x83
#define REG_PATCT2				0x84
#define REG_PATCT3				0x85
#define REG_FADEH				0x86
#define REG_FADEL				0x87
#define REG_GCOLR				0x88
#define REG_GCOLG				0x89
#define REG_GCOLB				0x8A
#define REG_GCFG0				0x8B


/******************************************************
 *
 * Register Write/Read Access
 *
 *****************************************************/
/*#define REG_NONE_ACCESS			0
#define REG_RD_ACCESS			1
#define REG_WR_ACCESS			0
#define AW210XX_REG_MAX			0x100*/

/*********************************************************
 *
 * chip info
 *
 ********************************************************/
#define AW210XX_CHIPID				0x22
#define AW21012_DEVICE_ADDR 		(0x20 << 1)
#define AW21024_DEVICE_ADDR			(0x30 << 1) /* AD0 and AD1 both GND, addr = 0x30 */
#define AW21036_DEVICE_ADDR			(0x34 << 1) /* AD0 GND, addr = 0x38 */

/*#if AW21024
#define AW210XX_DEVICE_ADDR			AW21024_DEVICE_ADDR
#elif AW21036
#define AW210XX_DEVICE_ADDR			AW21036_DEVICE_ADDR
#else
#define AW210XX_DEVICE_ADDR			AW21012_DEVICE_ADDR
#endif */

/*#define AW21012_LED_NUM				12
#if AW21024
#define AW210XX_LED_NUM				AW21024_LED_NUM
#elif AW21036
#define AW210XX_LED_NUM				AW21036_LED_NUM
#else
#define AW210XX_LED_NUM				AW21012_LED_NUM
#endif*/

static const int aw210xx_reg_map[] = {
	REG_COL0, REG_BR0,
	REG_COL1, REG_BR1,
	REG_COL2, REG_BR2,
	
	REG_COL0, REG_BR0,
	REG_COL1, REG_BR1,
	REG_COL2, REG_BR2,
	
	REG_COL3, REG_BR3,
	REG_COL4, REG_BR4,
	REG_COL5, REG_BR5,
	
	REG_COL3, REG_BR3,
	REG_COL4, REG_BR4,
	REG_COL5, REG_BR5,
	
	REG_COL6, REG_BR6,
	REG_COL7, REG_BR7,
	REG_COL8, REG_BR8,

	REG_COL6, REG_BR6,
	REG_COL7, REG_BR7,
	REG_COL8, REG_BR8,
	
	REG_COL9, REG_BR9,
	REG_COL10, REG_BR10,
	REG_COL11, REG_BR11,
	
	REG_COL9, REG_BR9,
	REG_COL10, REG_BR10,
	REG_COL11, REG_BR11,
};

/*********************************************************
 *
 * effect data
 *
 ********************************************************/
/* breath */
#if 1
static  AW_COLOR_STRUCT br_fadel[] = {
	{ 0, 0, 0 },
};

static AW_COLOR_STRUCT rgb_poweron_list[] = {
	{ 0,201, 128}, // default green
};

static AW_COLOR_STRUCT br_poweron_fadeh[] = {
	{ 0,255,200}, // default green
};

static AW_COLOR_STRUCT rgb_notify_list[] = {
	{ 0,201, 128}, // default green
};

static AW_COLOR_STRUCT br_notify_fadeh[] = {
	{ 0,255,200}, // default green
};

int notify_breath_cycle[] = {
    ((0x07 << 4) |  (0x00)),((0x07 << 4) |  (0x08)),
};

static AW_COLOR_STRUCT rgb_incall_list[] = {
	{ 0,201, 128}, // default green
};

static AW_COLOR_STRUCT br_incall_fadeh[] = {
	{ 0,255,200}, // default green
};

/*
static AW_COLOR_STRUCT rgb_single_color_list[] = {
	{ 0, 201, 128 }, // green
	{ 255, 149, 5 }, //orange
	{ 130, 100, 255 }, // purple
	{ 90, 255, 90 }, // grey
	{ 0, 247, 255 }, // blue
	{ 255, 130, 130}, // pink
	{ 150, 255, 0 }, // golden
	{ 255, 0, 25 }, // red
};
static AW_COLOR_STRUCT br_single_color_fadeh[] = {
	{ 0,255,200}, // green
	{ 175,175,175}, //orange
	{ 150,150,200}, // purple
	{ 180,180,180}, // grey
	{ 0,130,178}, // blue
	{ 150,150,150}, // pink
	{ 180,180,0}, // golden
	{ 255,0,200}, // red
};
*/

static AW_COLOR_STRUCT rgb_green_list[] = {
	{ 0, 255, 0 }, // green
};

static AW_COLOR_STRUCT br_green_fadeh[] = {
	{ 0, 180, 0 }, // green
};

static  AW_COLOR_STRUCT rgb_multi_game_racing[] = {
	{ 255, 130, 130 },
	{ 255, 130, 130 },
	{ 255, 130, 130 },
	{ 255, 130, 130 },
	{ 130, 100, 255 },
	{ 130, 100, 255 },
	{ 130, 100, 255 },
	{ 130, 100, 255 },
	{ 255, 255, 0 },
};

static  AW_COLOR_STRUCT br_game_fadeh[] = {
	{ 0, 255, 200 },
};

static AW_COLOR_STRUCT rgb_multi_game_lap[] = {
	{ 255, 130, 130 },
	{ 130, 100, 255 },
	{ 255, 130, 130 },
	{ 130, 100, 255 },
	{ 255, 130, 130 },
};

static  AW_MULTI_BREATH_DATA_STRUCT charge_stage1_effect_data1[] = {
	//effect1:moving
	{20, {120, 60, 180, 60,0, 180}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {180, 60, 60, 60,0, 240}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {180, 60, 60, 60,0, 240}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {120, 60, 180, 60,0, 180}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {60, 60, 300, 60,0, 120}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {0, 60, 420, 60,0, 60}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {0, 60, 420, 60,0, 60}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {60, 60, 300, 60,0, 120}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
};

static  AW_MULTI_BREATH_DATA_STRUCT charger_stage2_effect_data[] = {
	//effect2:moving
	{20, {120, 60, 240, 60,0, 120}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {180, 60, 240, 60,0,60}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {180, 60, 240, 60,0, 60}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {120, 60, 240, 60,0, 120}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {60, 60, 240, 60,0, 180}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {0, 60, 240, 60,0, 240}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {0, 60, 240, 60,0, 240}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
	{20, {60, 60, 240, 60,0, 180}, 1, 1, rgb_green_list, br_green_fadeh, br_fadel},
};

static  AW_MULTI_BREATH_DATA_STRUCT incall_effect_data1[] = {
	//effect2:moving
	{1, {20, 10, 40, 10,0, 20}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {30, 10, 40, 10,0,10}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {30, 10, 40, 10,0, 10}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {20, 10, 40, 10,0, 20}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {10, 10, 40, 10,0, 30}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {0, 10, 40, 10,0, 40}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {0, 10, 40, 10,0, 40}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {10, 10, 40, 10,0, 30}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},

	//effect2:moving
	{1, {10, 10, 40, 10,0, 30}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {0, 10, 40, 10,0, 40}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {0, 10, 40, 10,0, 40}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {10, 10, 40, 10,0, 30}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {20, 10, 40, 10,0, 20}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {30, 10, 40, 10,0, 10}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {30, 10, 40, 10,0,10}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
	{1, {20, 10, 40, 10,0, 20}, 1, 1, rgb_incall_list, br_incall_fadeh, br_fadel},
};

static  AW_MULTI_BREATH_DATA_STRUCT poweron_effect_data1[] = {
	{20, {720,80,160,80,360,120}, 2, 1, rgb_poweron_list, br_poweron_fadeh, br_fadel},
	{20, {840,80,160,80,360,0}, 2, 1, rgb_poweron_list, br_poweron_fadeh, br_fadel},
	{20, {0,80,160,80,360,840}, 2, 1, rgb_poweron_list, br_poweron_fadeh, br_fadel},
	{20, {120,80,160,80,360,720}, 2, 1, rgb_poweron_list, br_poweron_fadeh, br_fadel},
	{20, {240,80,160,80,360,600}, 2, 1, rgb_poweron_list, br_poweron_fadeh, br_fadel},
	{20, {360,80,160,80,360,480}, 2, 1, rgb_poweron_list, br_poweron_fadeh, br_fadel},
	{20, {480,80,160,80,360,360}, 2, 1, rgb_poweron_list, br_poweron_fadeh, br_fadel},
	{20, {600,80,160,80,360,240}, 2, 1, rgb_poweron_list, br_poweron_fadeh, br_fadel},
};

static  AW_MULTI_BREATH_DATA_STRUCT notify_effect_data1[] = {
	{1, {0, 23, 45, 23, 23, 0}, 1, 1, rgb_notify_list, br_notify_fadeh, br_fadel},
	{1, {0, 23, 45, 23, 23, 0}, 1, 1, rgb_notify_list, br_notify_fadeh, br_fadel},
	{1, {0, 23, 45, 23, 23, 0}, 1, 1, rgb_notify_list, br_notify_fadeh, br_fadel},
	{1, {0, 23, 45, 23, 23, 0}, 1, 1, rgb_notify_list, br_notify_fadeh, br_fadel},
	{1, {0, 23, 45, 23, 23, 0}, 1, 1, rgb_notify_list, br_notify_fadeh, br_fadel},
	{1, {0, 23, 45, 23, 23, 0}, 1, 1, rgb_notify_list, br_notify_fadeh, br_fadel},
	{1, {0, 23, 45, 23, 23, 0}, 1, 1, rgb_notify_list, br_notify_fadeh, br_fadel},
	{1, {0, 23, 45, 23, 23, 0}, 1, 1, rgb_notify_list, br_notify_fadeh, br_fadel},
};

/*
	//moving color effect
	{{0, 0, 400, 0,0, 100}, 12, sizeof(rgb_color_list9)/sizeof(AW_COLOR_STRUCT), rgb_color_list9},
	{{0, 0, 400, 0,0, 100}, 12, sizeof(rgb_color_list9)/sizeof(AW_COLOR_STRUCT), rgb_color_list9},
	{{0, 0, 400, 0,0, 100}, 12, sizeof(rgb_color_list9)/sizeof(AW_COLOR_STRUCT), rgb_color_list9},
	{{0, 0, 400, 0,0, 100}, 12, sizeof(rgb_color_list9)/sizeof(AW_COLOR_STRUCT), rgb_color_list9},
	{{0, 0, 400, 0,0, 100}, 12, sizeof(rgb_color_list9)/sizeof(AW_COLOR_STRUCT), rgb_color_list9},
	{{0, 0, 400, 0,0, 100}, 12, sizeof(rgb_color_list9)/sizeof(AW_COLOR_STRUCT), rgb_color_list9},
	{{0, 0, 400, 0,0, 100}, 12, sizeof(rgb_color_list9)/sizeof(AW_COLOR_STRUCT), rgb_color_list9},
	{{0, 0, 400, 0,0, 100}, 12, sizeof(rgb_color_list9)/sizeof(AW_COLOR_STRUCT), rgb_color_list9},

*/

static  AW_MULTI_BREATH_DATA_STRUCT gameenter_effect_data1[] = {
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},

	{1, {2,1,3,1,0,12}, 5, sizeof(rgb_multi_game_lap)/sizeof(AW_COLOR_STRUCT), rgb_multi_game_lap, br_game_fadeh, br_fadel},
	{1, {0,1,3,1,0,14}, 5, sizeof(rgb_multi_game_lap)/sizeof(AW_COLOR_STRUCT), rgb_multi_game_lap, br_game_fadeh, br_fadel},
	{1, {14,1,3,1,0,0}, 5, sizeof(rgb_multi_game_lap)/sizeof(AW_COLOR_STRUCT), rgb_multi_game_lap, br_game_fadeh, br_fadel},
	{1, {12,1,3,1,0,2}, 5, sizeof(rgb_multi_game_lap)/sizeof(AW_COLOR_STRUCT), rgb_multi_game_lap, br_game_fadeh, br_fadel},
	{1, {10,1,3,1,0,4}, 5, sizeof(rgb_multi_game_lap)/sizeof(AW_COLOR_STRUCT), rgb_multi_game_lap, br_game_fadeh, br_fadel},
	{1, {8,1,3,1,0,6}, 5, sizeof(rgb_multi_game_lap)/sizeof(AW_COLOR_STRUCT), rgb_multi_game_lap, br_game_fadeh, br_fadel},
	{1, {6,1,3,1,0,8}, 5, sizeof(rgb_multi_game_lap)/sizeof(AW_COLOR_STRUCT), rgb_multi_game_lap, br_game_fadeh, br_fadel},
	{1, {4,1,3,1,0,10}, 5, sizeof(rgb_multi_game_lap)/sizeof(AW_COLOR_STRUCT), rgb_multi_game_lap, br_game_fadeh, br_fadel},

	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},
	{1, {0, 0, 1, 0,0, 1}, 8, 9, rgb_multi_game_racing, br_game_fadeh, br_fadel},

	{1, {0, 3, 6, 3, 2, 0}, 3, 1, &rgb_multi_game_racing[0], br_game_fadeh, br_fadel},
	{1, {0, 3, 6, 3, 2, 0}, 3, 1, &rgb_multi_game_racing[1], br_game_fadeh, br_fadel},
	{1, {0, 3, 6, 3, 2, 0}, 3, 1, &rgb_multi_game_racing[2], br_game_fadeh, br_fadel},
	{1, {0, 3, 6, 3, 2, 0}, 3, 1, &rgb_multi_game_racing[3], br_game_fadeh, br_fadel},
	{1, {0, 3, 6, 3, 2, 0}, 3, 1, &rgb_multi_game_racing[4], br_game_fadeh, br_fadel},
	{1, {0, 3, 6, 3, 2, 0}, 3, 1, &rgb_multi_game_racing[5], br_game_fadeh, br_fadel},
	{1, {0, 3, 6, 3, 2, 0}, 3, 1, &rgb_multi_game_racing[6], br_game_fadeh, br_fadel},
	{1, {0, 3, 6, 3, 2, 0}, 3, 1, &rgb_multi_game_racing[7], br_game_fadeh, br_fadel},
};

static unsigned char charge_ongoing_frame = 0;

int charge_frame_map[] = {
    3,6,9,15,30,
};

//00c980
int new_always_on_color[][9] ={
	{ 0,0,0,0,0,0,0,0,180},
	{ 0xc9,0xc9,0xc9,0xc9,0xc9,0xc9,0xc9,0xc9,255},
	{ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,200},
};

int temp_color[][9] ={
	{ 0,0,0,0,0,0,0,0,180},
	{ 0xc9,0xc9,0xc9,0xc9,0xc9,0xc9,0xc9,0xc9,255},
	{ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,200},
};
int flow_color[][9] ={
	{ 0,0,0,0,0,0,0,0,180},
	{ 0xc9,0xc9,0xc9,0xc9,0xc9,0xc9,0xc9,0xc9,255},
	{ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,200},
};
int bainbow_color[][9] ={
	{ 0,0,0,0,0,0,0,0,180},
	{ 0xc9,0xc9,0xc9,0xc9,0xc9,0xc9,0xc9,0xc9,255},
	{ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,200},
};

int music_color[][9] ={
	{ 130,255,255,130,130,255,255,130,175 },
	{ 100,130,130,100,100,130,130,100,175 },
	{ 255,130,130,255,255,130,130,255,175 },
};

#endif
/* horse race lamp */
#if 0
static const AW_COLOR_STRUCT rgb_color_list[] = {
	{  125,  0, 0},
	{  0,  125, 0},
	{  0,  0, 125},
};
static const AW_MULTI_BREATH_DATA_STRUCT aw210xx_rgb_data[] = {
	{{  0, 100, 50, 100, 50}, 2, 120, 0, sizeof(rgb_color_list)/sizeof(AW_COLOR_STRUCT), rgb_color_list},
	{{ 50, 100, 50, 100, 50}, 2, 120, 0, sizeof(rgb_color_list)/sizeof(AW_COLOR_STRUCT), rgb_color_list},
	{{100, 100, 50, 100, 50}, 2, 120, 0, sizeof(rgb_color_list)/sizeof(AW_COLOR_STRUCT), rgb_color_list},
	{{150, 100, 50, 100, 50}, 2, 120, 0, sizeof(rgb_color_list)/sizeof(AW_COLOR_STRUCT), rgb_color_list},
	{{200, 100, 50, 100, 50}, 2, 120, 0, sizeof(rgb_color_list)/sizeof(AW_COLOR_STRUCT), rgb_color_list},
	{{250, 100, 50, 100, 50}, 2, 120, 0, sizeof(rgb_color_list)/sizeof(AW_COLOR_STRUCT), rgb_color_list},
	{{300, 100, 50, 100, 50}, 2, 120, 0, sizeof(rgb_color_list)/sizeof(AW_COLOR_STRUCT), rgb_color_list},
	{{350, 100, 50, 100, 50}, 2, 120, 0, sizeof(rgb_color_list)/sizeof(AW_COLOR_STRUCT), rgb_color_list},
	{{400, 100, 50, 100, 50}, 2, 120, 0, sizeof(rgb_color_list)/sizeof(AW_COLOR_STRUCT), rgb_color_list},
	{{450, 100, 50, 100, 50}, 2, 120, 0, sizeof(rgb_color_list)/sizeof(AW_COLOR_STRUCT), rgb_color_list},
	{{500, 100, 50, 100, 50}, 2, 120, 0, sizeof(rgb_color_list)/sizeof(AW_COLOR_STRUCT), rgb_color_list},
	{{550, 100, 50, 100, 50}, 2, 120, 0, sizeof(rgb_color_list)/sizeof(AW_COLOR_STRUCT), rgb_color_list},
};
#endif

#endif
