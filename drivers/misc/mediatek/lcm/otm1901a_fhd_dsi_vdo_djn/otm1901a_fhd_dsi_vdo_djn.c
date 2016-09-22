#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"


#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpio.h>
#endif

#include <cust_gpio_usage.h>

#ifndef MACH_FPGA
#include <cust_i2c.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID_NT35695 (0xf5)

static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))


/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)


/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
//#define LCM_DSI_CMD_MODE									1
#define LCM_DSI_CMD_MODE									0

#define FRAME_WIDTH										(1080)
#define FRAME_HEIGHT									(1920)

#ifndef MACH_FPGA
#define GPIO_65132_EN1 GPIO_LCD_BIAS_ENP_PIN
#define GPIO_65132_EN2 GPIO_LCD_BIAS_ENN_PIN
#endif

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

static LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

//the same i2c-addr can't to probe two times.
#ifdef BUILD_LK 
extern int TPS65132_write_byte(kal_uint8 addr, kal_uint8 value);
#else
extern int tps65132_write_bytes(unsigned char addr, unsigned char value);
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_sleep_in_setting[] =
{
	
	{0x00, 1,{0x00}},
	{0x99, 2,{0x95,0x27}},	
	
	{0x28, 1, {0x00}}, 
	{REGFLAG_DELAY, 50, {}}, 
	
	{0x10, 1, {0x00}}, 
	{REGFLAG_DELAY, 120, {}}, 
	
	{0x00, 1, {0x00}},
	{0xF7, 4, {0x5A,0xA5,0x19,0x01}},//deep sleep in
	
	{0x00, 1,{0x00}},
	{0x99, 2,{0x11,0x11}},	
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

//AUO_ini_1901_5P2(H520DAN01.1)_typeA_nmos_20150714.txt
static struct LCM_setting_table init_setting[] =
{
  {0x00, 1,{0x00}},
	{0xFF, 3,{0x19,0x01,0x01}},

	{0x00, 1,{0x80}}, 
	{0xFF, 2,{0x19,0x01}},

	{0x00, 1,{0x00}},   
	{0x1C, 1,{0x33}},

	{0x00, 1,{0xA0}},   
	{0xC1, 1,{0xE8}},

	{0x00, 1,{0xA7}},     
	{0xC1, 1,{0x00}},
       
	{0x00, 1,{0x90}},  
	{0xC0, 6,{0x00,0x2F,0x00,0x00,0x00,0x01}},   

	{0x00, 1,{0xC0}},  
	{0xC0, 6,{0x00,0x2F,0x00,0x00,0x00,0x01}}, 

	{0x00, 1,{0x9A}},            
	{0xC0, 1,{0x1E}},

	{0x00, 1,{0xAC}},          
	{0xC0, 1,{0x06}},

	{0x00, 1,{0xDC}},         
	{0xC0, 1,{0x06}},

	{0x00, 1,{0x81}},            
	{0xA5, 1,{0x04}},

	{0x00, 1,{0x84}},            
	{0xC4, 1,{0x20}},

	{0x00, 1,{0xA5}},     
	{0xB3, 1,{0x1d}},

	{0x00, 1,{0x92}},
	{0xE9, 1,{0x00}},

	{0x00, 1,{0x90}},
	{0xF3, 1,{0x01}},

	{0x00, 1,{0xB4}},
	{0xC0, 1,{0x80}},

	{0x00, 1,{0x93}},
	{0xC5, 1,{0x19}},

	{0x00, 1,{0x95}},
	{0xC5, 1,{0x2D}},

	{0x00, 1,{0x97}},
	{0xC5, 1,{0x14}},

	{0x00, 1,{0x99}},
	{0xC5, 1,{0x29}},

	{0x00, 1,{0x00}},
	{0xD8, 2,{0x1D,0x1D}},

	{0x00, 1,{0xB3}},
	{0xC0, 1,{0xCC}},

// panel scan mode bottom to top
	{0x00, 1,{0xB4}},
	{0xC0, 1,{0xD0}},


	{0x00, 1,{0xBC}},
	{0xC0, 1,{0x00}},

	{0x00, 1,{0xF7}},         
	{0xC3, 4,{0x04,0x18,0x04,0x04}},

	{0x00, 1,{0x81}},
	{0xA5, 1,{0x07}},

	{0x00, 1,{0x9D}},
	{0xC5, 1,{0x77}},

	{0x00, 1,{0x9B}},
	{0xC5, 2,{0x55,0x55}},

	{0x00, 1,{0x80}},
	{0xC4, 1,{0x15}},

//vcom set

//	{0x00, 1,{0x00}},
//	{0xD9, 1,{0x00}},

//	{0x00, 1,{0x01}},
//	{0xD9, 1,{0xB4}},

	{0x00, 1,{0x80}},
	{0xC0, 14,{0x00,0x87,0x00,0x0A,0x0A,0x00,0x87,0x0A,0x0A,0x00,0x87,0x00,0x0A,0x0A}},

	{0x00, 1,{0xF0}},
	{0xC3, 6,{0x22,0x02,0x00,0x00,0x00,0x0C}},

	{0x00, 1,{0xA0}},
	{0xC0, 7,{0x00,0x00,0x00,0x00,0x03,0x22,0x03}},

	{0x00, 1,{0xD0}},
	{0xC0, 7,{0x00,0x00,0x00,0x00,0x03,0x22,0x03}},

	{0x00, 1,{0x90}},
	{0xC2, 8,{0x83,0x01,0x00,0x00,0x82,0x01,0x00,0x00}},

	{0x00, 1,{0x80}},
	{0xC3, 12,{0x82,0x02,0x03,0x00,0x03,0x84,0x81,0x03,0x03,0x00,0x03,0x84}},

	{0x00, 1,{0x90}},
	{0xC3, 12,{0x00,0x01,0x03,0x00,0x03,0x84,0x01,0x02,0x03,0x00,0x03,0x84}},

	{0x00, 1,{0x80}},
	{0xCC, 15,{0x09,0x0A,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x28,0x28,0x28,0x28,0x28}},

	{0x00, 1,{0x90}},
	{0xCC, 15,{0x0A,0x09,0x14,0x13,0x12,0x11,0x15,0x16,0x17,0x18,0x28,0x28,0x28,0x28,0x28}},

	{0x00, 1,{0xA0}},
	{0xCC, 15,{0x1D,0x1E,0x1F,0x19,0x1A,0x1B,0x1C,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27}},

	{0x00, 1,{0xB0}},
	{0xCC, 8,{0x01,0x02,0x03,0x05,0x06,0x07,0x04,0x08}},


	{0x00, 1,{0xC0}},
	{0xCC, 12,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x77}},

	{0x00, 1,{0xD0}},
	{0xCC, 12,{0x00,0x00,0x00,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x77}},

	{0x00, 1,{0x80}},
	{0xCB, 15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00, 1,{0x90}},
	{0xCB, 15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00, 1,{0xA0}},
	{0xCB, 15,{0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00, 1,{0xB0}},
	{0xCB, 15,{0x00,0x01,0xFD,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00, 1,{0xC0}},
	{0xCB, 8,{0x00,0x00,0x00,0x00,0x00,0x00,0x77,0x77}},

	{0x00, 1,{0xD0}},
	{0xCB, 8,{0x00,0x00,0x00,0x00,0x00,0x00,0x77,0x77}},

	{0x00, 1,{0xE0}},
	{0xCB, 8,{0x00,0x00,0x00,0x00,0x00,0x00,0x77,0x77}},

	{0x00, 1,{0xF0}},
	{0xCB, 8,{0x01,0x01,0x01,0x00,0x00,0x00,0x77,0x77}},

	{0x00, 1,{0x80}},
	{0xCD, 15,{0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x02,0x12,0x11,0x3F,0x04,0x3F}},

	{0x00, 1,{0x90}},
	{0xCD, 11,{0x06,0x3F,0x3F,0x26,0x26,0x26,0x21,0x20,0x1F,0x26,0x26}},

	{0x00, 1,{0xA0}},
	{0xCD, 15,{0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x01,0x12,0x11,0x3F,0x03,0x3F}},

	{0x00, 1,{0xB0}},
	{0xCD, 11,{0x05,0x3F,0x3F,0x26,0x26,0x26,0x21,0x20,0x1F,0x26,0x26}},

	{0x00, 1,{0x00}},
	{0xE1, 24,{0x11,0x17,0x17,0x1e,0x22,0x27,0x2f,0x3d,0x45,0x57,0x61,0x6a,0x8e,0x85,0x7c,0x68,0x53,0x42,0x34,0x2d,0x25,0x1c,0x17,0x03}},

	{0x00, 1,{0x00}},
	{0xE2, 24,{0x11,0x17,0x17,0x1e,0x22,0x27,0x2f,0x3d,0x45,0x57,0x61,0x6a,0x8e,0x85,0x7c,0x68,0x53,0x42,0x34,0x2d,0x25,0x1c,0x17,0x03}},

	{0x00, 1,{0x00}},
	{0xE3, 24,{0x1c,0x1e,0x21,0x25,0x2a,0x2e,0x36,0x43,0x49,0x5a,0x63,0x6b,0x8d,0x84,0x7b,0x68,0x53,0x42,0x34,0x2d,0x25,0x1c,0x17,0x03}},

	{0x00, 1,{0x00}},
	{0xE4, 24,{0x1c,0x1e,0x21,0x25,0x2a,0x2e,0x36,0x43,0x49,0x5a,0x63,0x6b,0x8d,0x84,0x7b,0x68,0x53,0x42,0x34,0x2d,0x25,0x1c,0x17,0x03}},

	{0x00, 1,{0x00}},
	{0xE5, 24,{0x00,0x04,0x07,0x0e,0x15,0x1b,0x25,0x36,0x40,0x53,0x5f,0x68,0x8f,0x86,0x7c,0x6a,0x54,0x43,0x35,0x2e,0x26,0x1c,0x17,0x03}},

	{0x00, 1,{0x00}},
	{0xE6, 24,{0x00,0x04,0x07,0x0e,0x15,0x1b,0x25,0x36,0x40,0x53,0x5f,0x68,0x8f,0x86,0x7c,0x6a,0x54,0x43,0x35,0x2e,0x26,0x1c,0x17,0x03}},

//����EMI Code

	{0x00, 1,{0x81}},
	{0xA5, 1,{0x07}},
 
	{0x00, 1,{0x80}},
	{0xC4, 1,{0x06}},
              
	{0x00, 1,{0x9B}},
	{0xC5, 2,{0x55,0x50}},
 
	{0x00, 1,{0xF7}},
	{0xC3, 4,{0x04,0x16,0x04,0x04}},
 
	{0x00, 1,{0xF2}},
	{0xC1, 3,{0x80,0x00,0x00}},
 
	{0x00, 1,{0x9D}},
	{0xC5, 1,{0x66}},


	{0x00, 1,{0x00}},
	{0xFF, 3,{0xFF,0xFF,0xFF}},

	{0x00, 1,{0x00}},//te on
	{0x35, 1,{0x00}},//te on
	
	{0x11, 1,{0x00}},  
  	{REGFLAG_DELAY,120,{}},

    {0x29, 1,{0x00}},
    {REGFLAG_DELAY,20,{}},
	
	{0x00, 1,{0x00}},
	{0x99, 2,{0x11,0x11}},
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
#endif
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 4;//4;
	params->dsi.vertical_backporch = 3;//3;
	params->dsi.vertical_frontporch = 10;//10;

	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 8;//30; //10;
	params->dsi.horizontal_backporch = 60;//30; //20;
	params->dsi.horizontal_frontporch = 140;//30; //40;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable                                                   = 1; */
#ifndef MACH_FPGA
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 500;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 450;	/* this value must be in MTK suggested table */
#endif
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	//params->dsi.esd_check_enable = 1;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x1C;

}

static void lcm_init_power(void)
{
    LCM_LOGI("liuyang:otm1901a_fhd_dsi_vdo_djn is called\n");
}

static void lcm_suspend_power(void)
{
    LCM_LOGI("liuyang:otm1901a_fhd_dsi_vdo_djn is called\n");
}

static void lcm_resume_power(void)
{
    LCM_LOGI("liuyang:otm1901a_fhd_dsi_vdo_djn is called\n");
}


static void lcm_init(void)
{
	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret = 0;
	cmd = 0x00;
	data = 0x0F;
	
#ifndef MACH_FPGA
    mt_set_gpio_mode(GPIO_65132_EN1, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN1, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN1, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_65132_EN2, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN2, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN2, GPIO_OUT_ONE);
	
	MDELAY(10);

#ifdef BUILD_LK
	ret = TPS65132_write_byte(cmd, data);
#else
	ret = tps65132_write_bytes(cmd, data);
#endif

	if (ret < 0)
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write error----\n", cmd);
	else
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write success----\n", cmd);

	cmd = 0x01;
	data = 0x0F;

#ifdef BUILD_LK
	ret = TPS65132_write_byte(cmd, data);
#else
	ret = tps65132_write_bytes(cmd, data);
#endif

	if (ret < 0)
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write error----\n", cmd);
	else
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write success----\n", cmd);

#endif

    //begin:modified by fae for save time 20160105
	SET_RESET_PIN(1);
	MDELAY(10); //10
	SET_RESET_PIN(0);
	MDELAY(10); //10

	SET_RESET_PIN(1);
	MDELAY(120); //20
	push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);

	LCM_LOGI("liuyang:otm1901a_fhd_dsi_vdo_djn is init\n");
}

static void lcm_suspend(void)
{
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
	/* SET_RESET_PIN(0); */
	mt_set_gpio_mode(GPIO_65132_EN2, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN2, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN2, GPIO_OUT_ZERO);

	mt_set_gpio_mode(GPIO_65132_EN1, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN1, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN1, GPIO_OUT_ZERO);
	MDELAY(10);
}

static void lcm_resume(void)
{
	lcm_init();	
}


static unsigned int lcm_compare_id(void)
{
  
    return 0;
}

LCM_DRIVER otm1901a_fhd_dsi_vdo_djn_lcm_drv = {
	.name = "otm1901a_fhd_dsi_vdo_djn",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
};