/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/gpio.h>
#include "msm_sensor.h"
#include <linux/regulator/machine.h> //F_YACD5C1SBDBC_POWER
//#include "sensor_i2c.h"
#include "sensor_ctrl.h"
#include "yacd5c1sbdbc_v4l2_cfg.h"
#include "msm_camera_i2c.h"

#include "msm.h"
#include "msm_ispif.h"


#ifdef CONFIG_PANTECH_CAMERA_TUNER
#include "ptune_parser.h"
msm_camera_i2c_reg_tune_t *yacd5c1sbdbc_recommend_tuner_settings;
int tuner_line;
int8_t tuner_init_check = 0;
#endif

#ifdef CONFIG_PANTECH_CAMERA_YACD5C1SBDBC// for VTS
int8_t preview_24fps_for_motion_detect_check = 0;
#endif

#define SENSOR_NAME "yacd5c1sbdbc"
#define PLATFORM_DRIVER_NAME "msm_camera_yacd5c1sbdbc"
#define yacd5c1sbdbc_obj yacd5c1sbdbc_##obj

/*=============================================================
	SENSOR REGISTER DEFINES
==============================================================*/

/* Sensor Model ID */
#define YACD5C1SBDBC_PIDH_REG		0x04
#define YACD5C1SBDBC_MODEL_ID		0xB4

//wsyang_temp
#define F_YACD5C1SBDBC_POWER

#ifdef F_YACD5C1SBDBC_POWER
#define CAMIO_RST_N	0
#define CAMIO_STB_N	1

#define CAMIO_MAX	2

#define CAMIO_PM_MAX 1
#if (CONFIG_BOARD_VER <= CONFIG_WS10)
#define CAM2_STANDBY	2
#else
#define CAM2_STANDBY	12
#endif 

#define CAM2_RST_N		29
#define CAM1_IOVDD		77

static int8_t g_preview_fps;
static int yacd5c1sbdbc_sensor_set_preview_fps(struct msm_sensor_ctrl_t *s_ctrl ,int8_t preview_fps);
int32_t yacd5c1sbdbc_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id);

static sgpio_ctrl_t sgpios[CAMIO_MAX] = {
	{CAMIO_RST_N, "CAM_RST_N_PM", CAM2_RST_N},
	{CAMIO_STB_N, "CAM_STB_N", CAM2_STANDBY},	
};

#define CAMV_IO_1P8V	0
#define CAMV_CORE_1P8V	1
#define CAMV_A_2P8V	2
#define CAMV_MAX	3

static svreg_ctrl_t svregs[CAMV_MAX] = {
	{CAMV_IO_1P8V,   "8921_lvs5", NULL, 0},
#if (CONFIG_BOARD_VER <= CONFIG_WS10)
	{CAMV_CORE_1P8V, "8921_lvs3",   NULL, 0},
#else
	{CAMV_CORE_1P8V, "8921_lvs2",   NULL, 0},
#endif
	{CAMV_A_2P8V,    "8921_l22",  NULL, 2800},
};
#endif

DEFINE_MUTEX(yacd5c1sbdbc_mut);
static struct msm_sensor_ctrl_t yacd5c1sbdbc_s_ctrl;


static struct v4l2_subdev_info yacd5c1sbdbc_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,//V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array yacd5c1sbdbc_init_conf[] = {
	{&yacd5c1sbdbc_recommend_settings[0],
	ARRAY_SIZE(yacd5c1sbdbc_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array yacd5c1sbdbc_confs[] = {
	{&yacd5c1sbdbc_snap_settings[0],
	ARRAY_SIZE(yacd5c1sbdbc_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&yacd5c1sbdbc_prev_settings[0],
	ARRAY_SIZE(yacd5c1sbdbc_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&yacd5c1sbdbc_prev_settings[0],
	ARRAY_SIZE(yacd5c1sbdbc_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&yacd5c1sbdbc_snap_settings[0],
	ARRAY_SIZE(yacd5c1sbdbc_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};


#define YACD5C1SBDBC_FULL_SIZE_DUMMY_PIXELS     0
#define YACD5C1SBDBC_FULL_SIZE_DUMMY_LINES    0
#define YACD5C1SBDBC_FULL_SIZE_WIDTH    1600 //3280 //1936;//3280; //1632;//3280;
#define YACD5C1SBDBC_FULL_SIZE_HEIGHT   1200 //2464//1096;//2456;//1224;//2464; //

#define YACD5C1SBDBC_QTR_SIZE_DUMMY_PIXELS  0
#define YACD5C1SBDBC_QTR_SIZE_DUMMY_LINES   0
#define YACD5C1SBDBC_QTR_SIZE_WIDTH     800 //640//1640//1936;//1632;//1936;//3280;//1920
#define YACD5C1SBDBC_QTR_SIZE_HEIGHT    600 //480//1232//1456;//1224;//1096;//2464;//1080//1096

#define YACD5C1SBDBC_HRZ_FULL_BLK_PIXELS   0//16 //190//304//696; //890;//
#define YACD5C1SBDBC_VER_FULL_BLK_LINES     0//12 //32//24//44; //44;
#define YACD5C1SBDBC_HRZ_QTR_BLK_PIXELS    0//16 //1830//1884//1648;//890;//888; //696;
#define YACD5C1SBDBC_VER_QTR_BLK_LINES      0//12 //16//40//924;//44;

static struct msm_sensor_output_info_t yacd5c1sbdbc_dimensions[] = {
	{
		.x_output = YACD5C1SBDBC_FULL_SIZE_WIDTH,//0x1070,
		.y_output = YACD5C1SBDBC_FULL_SIZE_HEIGHT,
		.line_length_pclk = YACD5C1SBDBC_FULL_SIZE_WIDTH + YACD5C1SBDBC_HRZ_FULL_BLK_PIXELS ,
		.frame_length_lines = YACD5C1SBDBC_FULL_SIZE_HEIGHT+ YACD5C1SBDBC_VER_FULL_BLK_LINES ,
		.vt_pixel_clk = 18163200,//48000000,//50000000,
		.op_pixel_clk = 48000000,//128000000,//150000000,
		.binning_factor = 1,
	},
	{
		.x_output = YACD5C1SBDBC_QTR_SIZE_WIDTH,
		.y_output = YACD5C1SBDBC_QTR_SIZE_HEIGHT,
		.line_length_pclk = YACD5C1SBDBC_QTR_SIZE_WIDTH + YACD5C1SBDBC_HRZ_QTR_BLK_PIXELS,
		.frame_length_lines = YACD5C1SBDBC_QTR_SIZE_HEIGHT+ YACD5C1SBDBC_VER_QTR_BLK_LINES,
		.vt_pixel_clk = 8832000,//48000000,//50000000,
		.op_pixel_clk = 48000000,//24000000,//128000000,//150000000,
		.binning_factor = 1,
	},
	{
		.x_output = YACD5C1SBDBC_QTR_SIZE_WIDTH,
		.y_output = YACD5C1SBDBC_QTR_SIZE_HEIGHT,
		.line_length_pclk = YACD5C1SBDBC_QTR_SIZE_WIDTH + YACD5C1SBDBC_HRZ_QTR_BLK_PIXELS,
		.frame_length_lines = YACD5C1SBDBC_QTR_SIZE_HEIGHT+ YACD5C1SBDBC_VER_QTR_BLK_LINES,
		.vt_pixel_clk = 8832000,//48000000,//50000000,
		.op_pixel_clk = 48000000,//24000000,//128000000,//150000000,
		.binning_factor = 1,
	},
	{
		.x_output = YACD5C1SBDBC_FULL_SIZE_WIDTH,//0x1070,
		.y_output = YACD5C1SBDBC_FULL_SIZE_HEIGHT,
		.line_length_pclk = YACD5C1SBDBC_FULL_SIZE_WIDTH + YACD5C1SBDBC_HRZ_FULL_BLK_PIXELS ,
		.frame_length_lines = YACD5C1SBDBC_FULL_SIZE_HEIGHT+ YACD5C1SBDBC_VER_FULL_BLK_LINES ,
		.vt_pixel_clk = 18163200,//48000000,//50000000,
		.op_pixel_clk = 48000000,//128000000,//150000000,
		.binning_factor = 1,
	},
};

#if 0
static struct msm_camera_csid_vc_cfg yacd5c1sbdbc_cid_cfg[] = {
	{0, 0x1E, CSI_DECODE_8BIT}, //{0, CSI_RAW10, CSI_DECODE_10BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
	{2, CSI_EMBED_DATA, CSI_DECODE_8BIT},
	{3, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params yacd5c1sbdbc_csi_params = {
	.csid_params = {
		.lane_assign = 0xe4,
		.lane_cnt = 1,//2,
		.lut_params = {
			.num_cid = 2,
			.vc_cfg = yacd5c1sbdbc_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 1,//2,
		.settle_cnt = 0x11,//0x1B,//0x23,//400Mhz 
		.lane_mask = 1,
	},
};

static struct msm_camera_csi2_params *yacd5c1sbdbc_csi_params_array[] = {
	&yacd5c1sbdbc_csi_params,
	&yacd5c1sbdbc_csi_params,
	&yacd5c1sbdbc_csi_params,
	&yacd5c1sbdbc_csi_params,
};
#endif

#if 0//
static struct msm_sensor_output_reg_addr_t yacd5c1sbdbc_reg_addr = {
	.x_output = 0x034C,
	.y_output = 0x034E,
	.line_length_pclk = REG_LINE_LENGTH_PCK,//0x342
	.frame_length_lines = REG_FRAME_LENGTH_LINES,//0x0340
};
#endif

static struct msm_sensor_id_info_t yacd5c1sbdbc_id_info = {
	.sensor_id_reg_addr = YACD5C1SBDBC_PIDH_REG,
	.sensor_id = 0, //YACD5C1SBDBC_MODEL_ID,
};

static const struct i2c_device_id yacd5c1sbdbc_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&yacd5c1sbdbc_s_ctrl},
	{ }
};

static struct i2c_driver yacd5c1sbdbc_i2c_driver = {
	.id_table = yacd5c1sbdbc_i2c_id,
	//.probe  = msm_sensor_i2c_probe,
	.probe  = yacd5c1sbdbc_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};


static struct msm_camera_i2c_client yacd5c1sbdbc_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,//MSM_CAMERA_I2C_WORD_ADDR,
};
#if 0
static struct msm_camera_i2c_client yacd5c1sbdbc_eeprom_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static struct msm_camera_eeprom_read_t yacd5c1sbdbc_eeprom_read_tbl[] = {
	{0x10, &yacd5c1sbdbc_calib_data.r_over_g, 2, 1},
	{0x12, &yacd5c1sbdbc_calib_data.b_over_g, 2, 1},
	{0x14, &yacd5c1sbdbc_calib_data.gr_over_gb, 2, 1},
};

static struct msm_camera_eeprom_data_t yacd5c1sbdbc_eeprom_data_tbl[] = {
	{&yacd5c1sbdbc_calib_data, sizeof(struct sensor_calib_data)},
};

static struct msm_camera_eeprom_client yacd5c1sbdbc_eeprom_client = {
	.i2c_client = &yacd5c1sbdbc_eeprom_i2c_client,
	.i2c_addr = 0xA4,

	.func_tbl = {
		.eeprom_set_dev_addr = NULL,
		.eeprom_init = msm_camera_eeprom_init,
		.eeprom_release = msm_camera_eeprom_release,
		.eeprom_get_data = msm_camera_eeprom_get_data,
	},

	.read_tbl = yacd5c1sbdbc_eeprom_read_tbl,
	.read_tbl_size = ARRAY_SIZE(yacd5c1sbdbc_eeprom_read_tbl),
	.data_tbl = yacd5c1sbdbc_eeprom_data_tbl,
	.data_tbl_size = ARRAY_SIZE(yacd5c1sbdbc_eeprom_data_tbl),
};
#endif



int32_t yacd5c1sbdbc_sensor_set_fps(struct msm_sensor_ctrl_t *s_ctrl,
						struct fps_cfg *fps)
{
	//uint16_t total_lines_per_frame;
	int32_t rc = 0;
	SKYCDBG("%s: %d\n", __func__, __LINE__);
#if 0	
	s_ctrl->fps_divider = fps->fps_div;


	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines,
			total_lines_per_frame, MSM_CAMERA_I2C_WORD_DATA);
#endif	
	return rc;
}

/* msm_sensor_write_init_settings */
int32_t yacd5c1sbdbc_sensor_write_init_settings(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
#ifdef CONFIG_PANTECH_CAMERA_TUNER
    int tuner_data_type;

//    if (s_ctrl->sensor_id_info->sensor_id == YACD5C1SBDBC_ID)
    {
        SKYCDBG("[CONFIG_PANTECH_CAMERA_TUNER] %s\n",__func__);
        tuner_data_type = s_ctrl->msm_sensor_reg->default_data_type;

        rc = msm_camera_i2c_write_tuner(
            s_ctrl->sensor_i2c_client,
            yacd5c1sbdbc_recommend_tuner_settings,
            tuner_line,//ARRAY_SIZE(yacd5c1sbdbc_recommend_tuner_settings),
            tuner_data_type);//MSM_CAMERA_I2C_BYTE_DATA);
    }
    else
    {
        rc = msm_sensor_write_all_conf_array(
            s_ctrl->sensor_i2c_client,
            s_ctrl->msm_sensor_reg->init_settings,
            s_ctrl->msm_sensor_reg->init_size);
    }
#else
	rc = msm_sensor_write_all_conf_array(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->init_settings,
		s_ctrl->msm_sensor_reg->init_size);
#endif	
	return rc;
}

/* msm_sensor_write_res_settings */
int32_t yacd5c1sbdbc_sensor_write_res_settings(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	int32_t rc = 0;

    if(res == 0) {
        SKYCDBG("%s:[F_PANTECH_CAMERA] Write reg [AE, AWB Off SET]\n", __func__);        
        rc = msm_camera_i2c_write_tbl(
            s_ctrl->sensor_i2c_client,
            s_ctrl->msm_sensor_reg->checkzsl_cfg_settings[0],//[AE, AWB Off SET]
            s_ctrl->msm_sensor_reg->checkzsl_cfg_settings_size,
            s_ctrl->msm_sensor_reg->default_data_type);
    }

	rc = msm_sensor_write_conf_array(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->mode_settings, res);
	if (rc < 0)
		return rc;
#ifndef CONFIG_PANTECH_CAMERA_YACD5C1SBDBC
	rc = msm_sensor_write_output_settings(s_ctrl, res);
#endif
	return rc;
}

/* msm_sensor_setting */
int32_t yacd5c1sbdbc_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;


SKYCDBG("%s:[F_PANTECH_CAMERA] %d, %d res=%d\n", __func__, __LINE__,update_type,res);
	
	if (update_type == MSM_SENSOR_REG_INIT) {
		msm_sensor_write_init_settings(s_ctrl);
   if (s_ctrl->func_tbl->sensor_stop_stream)
	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);

	msleep(30);
		g_preview_fps= 0;
#ifdef CONFIG_PANTECH_CAMERA_TUNER 
        SKYCDBG("[CONFIG_PANTECH_CAMERA_TUNER]%s PASS init_setting\n ",__func__);
        tuner_init_check = 1;
#endif

    } else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {

#ifdef CONFIG_PANTECH_CAMERA_TUNER
        if (tuner_init_check == 1) {
            SKYCDBG("[CONFIG_PANTECH_CAMERA_TUNER]%s init_setting\n ",__func__);
            yacd5c1sbdbc_sensor_write_init_settings(s_ctrl);
            tuner_init_check = 0;
        }

        SKYCDBG("[CONFIG_PANTECH_CAMERA_TUNER]%s res=%d res_setting\n ",__func__,res);
        yacd5c1sbdbc_sensor_write_res_settings(s_ctrl, res);
#else
        yacd5c1sbdbc_sensor_write_res_settings(s_ctrl, res);
#endif

#ifdef CONFIG_PANTECH_CAMERA_YACD5C1SBDBC// for VTS
        if(preview_24fps_for_motion_detect_check == 1)
        {

            //SKYCDBG("[CONFIG_PANTECH_CAMERA for VTS] msleep(133)==>\n");
            //msleep(133);
        
            SKYCDBG("[CONFIG_PANTECH_CAMERA for VTS]preview_24fps_for_motion_detect_cfg_settings\n ");

            rc = msm_camera_i2c_write_tbl(
                s_ctrl->sensor_i2c_client,
                s_ctrl->msm_sensor_reg->preview_24fps_for_motion_detect_cfg_settings[0],
                s_ctrl->msm_sensor_reg->preview_24fps_for_motion_detect_cfg_settings_size,
                s_ctrl->msm_sensor_reg->default_data_type);

            preview_24fps_for_motion_detect_check = 0;
        	if (rc < 0)
        	{
        		SKYCERR("ERR:%s 24fps FAIL!!! rc=%d \n", __func__, rc);
        		return rc;
        	}            
        }
#endif

		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
			output_settings[res].op_pixel_clk);
#if 0
        if (s_ctrl->func_tbl->sensor_start_stream)
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);

#endif		
		msleep(200);//msleep(150); //msleep(30);
	}
	SKYCDBG("%s: %d x\n", __func__, __LINE__);
	return rc;
}

#ifdef CONFIG_PANTECH_CAMERA_TUNER
static int yacd5c1sbdbc_sensor_set_tuner(struct tuner_cfg tuner)
{
	//si2c_cmd_t *cmds = NULL;
    msm_camera_i2c_reg_tune_t *cmds = NULL;

	char *fbuf = NULL;

	SKYCDBG("%s fbuf=%p, fsize=%d\n", __func__, tuner.fbuf, tuner.fsize);

	if (!tuner.fbuf || (tuner.fsize == 0)) {
		SKYCERR("%s err(-EINVAL)\n", __func__);
		return -EINVAL;
	}

	fbuf = (char *)kmalloc(tuner.fsize, GFP_KERNEL);
	if (!fbuf) {
		SKYCERR("%s err(-ENOMEM)\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(fbuf, tuner.fbuf, tuner.fsize)) {
		SKYCERR("%s err(-EFAULT)\n", __func__);
		kfree(fbuf);
		return -EFAULT;
	}

	cmds = ptune_parse("@init", fbuf);
	if (!cmds) {
		SKYCERR("%s no @init\n", __func__);
		kfree(fbuf);
		return -EFAULT;
	}

    yacd5c1sbdbc_recommend_tuner_settings = cmds;//(struct msm_camera_i2c_reg_conf *)cmds;

	kfree(fbuf);

	SKYCDBG("%s X\n", __func__);
	return 0;
}    
#endif

#ifdef CONFIG_PANTECH_CAMERA
static int yacd5c1sbdbc_sensor_set_brightness(struct msm_sensor_ctrl_t *s_ctrl ,int8_t brightness)
{
	int rc = 0;

	SKYCDBG("%s brightness=%d start \n",__func__,brightness); //SKYCDBG

	if(brightness < 0 || brightness > 8){
		SKYCERR("%s error. brightness=%d\n", __func__, brightness); //SKYCERR
		return -EINVAL;
	}

	rc = msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->bright_cfg_settings[brightness],
		s_ctrl->msm_sensor_reg->bright_cfg_settings_size,
		s_ctrl->msm_sensor_reg->default_data_type);

	if (rc < 0)
	{
		SKYCERR("ERR:%s FAIL!!! rc=%d \n", __func__, rc); //SKYCERR
		return rc;
	}	
	SKYCDBG("%s rc=%d end \n",__func__,rc); //SKYCDBG
	
	return rc;
}

static int yacd5c1sbdbc_sensor_set_effect(struct msm_sensor_ctrl_t *s_ctrl ,int8_t effect)
{
	int rc = 0;

	SKYCDBG("%s effect=%d start \n",__func__,effect); //SKYCDBG

	if(effect < CAMERA_EFFECT_OFF || effect >= CAMERA_EFFECT_MAX){
		SKYCERR("%s error. effect=%d\n", __func__, effect); //SKYCERR
		return -EINVAL;
	}

	rc = msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->effect_cfg_settings[effect],
		s_ctrl->msm_sensor_reg->effect_cfg_settings_size,
		s_ctrl->msm_sensor_reg->default_data_type);

	if (rc < 0)
	{
		SKYCERR("ERR:%s FAIL!!! rc=%d \n", __func__, rc); //SKYCERR
		return rc;
	}	
	SKYCDBG("%s rc=%d end \n",__func__,rc); //SKYCDBG
	
	return rc;
}

static int yacd5c1sbdbc_sensor_set_exposure_mode(struct msm_sensor_ctrl_t *s_ctrl ,int8_t exposure)
{
	int rc = 0;

	SKYCDBG("%s exposure=%d start \n",__func__,exposure); //SKYCDBG

	if(exposure < 0 || exposure > 3){
		SKYCERR("%s error. exposure=%d\n", __func__, exposure); //SKYCERR
		return -EINVAL;
	}

	rc = msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->exposure_mode_cfg_settings[exposure],
		s_ctrl->msm_sensor_reg->exposure_mode_cfg_settings_size,
		s_ctrl->msm_sensor_reg->default_data_type);

	if (rc < 0)
	{
		SKYCERR("ERR:%s FAIL!!! rc=%d \n", __func__, rc); //SKYCERR
		return rc;
	}	
	SKYCDBG("%s rc=%d end \n",__func__,rc); //SKYCDBG
	
	return rc;
}

static int yacd5c1sbdbc_sensor_set_reflect(struct msm_sensor_ctrl_t *s_ctrl ,int8_t reflect)
{
	int rc = 0;

	SKYCDBG("%s reflect=%d start \n",__func__,reflect); //SKYCDBG

	if(reflect < 0 || reflect > 3){
		SKYCERR("%s error. reflect=%d\n", __func__, reflect); //SKYCERR
		return -EINVAL;
	}

	rc = msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->reflect_cfg_settings[reflect],
		s_ctrl->msm_sensor_reg->reflect_cfg_settings_size,
		s_ctrl->msm_sensor_reg->default_data_type);

	if (rc < 0)
	{
		SKYCERR("ERR:%s FAIL!!! rc=%d \n", __func__, rc); //SKYCERR
		return rc;
	}	
	SKYCDBG("%s rc=%d end \n",__func__,rc); //SKYCDBG
	
	return rc;
}
static int yacd5c1sbdbc_sensor_set_wb(struct msm_sensor_ctrl_t *s_ctrl ,int8_t wb)
{
	int rc = 0;

	SKYCDBG("%s wb=%d start \n",__func__,wb); //SKYCDBG

	if(wb < 0 || wb > 6){
		SKYCERR("%s error. wb=%d\n", __func__, wb); //SKYCERR
		return -EINVAL;
	}

	rc = msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->wb_cfg_settings[wb],
		s_ctrl->msm_sensor_reg->wb_cfg_settings_size,
		s_ctrl->msm_sensor_reg->default_data_type);

	if (rc < 0)
	{
		SKYCERR("ERR:%s FAIL!!! rc=%d \n", __func__, rc); //SKYCERR
		return rc;
	}	
	SKYCDBG("%s rc=%d end \n",__func__,rc); //SKYCDBG
	
	return rc;
}

static int yacd5c1sbdbc_sensor_set_preview_fps(struct msm_sensor_ctrl_t *s_ctrl ,int8_t preview_fps)
{
	int rc = 0;

	SKYCDBG("%s preview_fps=%d start \n",__func__,preview_fps); //SKYCDBG

	if(preview_fps < 5 || preview_fps > 31){
		SKYCERR("%s error. preview_fps=%d\n", __func__, preview_fps); //SKYCERR
		return -EINVAL;
	}

	g_preview_fps = preview_fps;
	
	//if (preview_fps > 30) preview_fps = 30;
	
#if 1//CONFIG_PANTECH_CAMERA_YACD5C1SBDBC for VTS
    if(preview_fps == 24) {
        preview_24fps_for_motion_detect_check = 1;
    }else{
    	rc = msm_camera_i2c_write_tbl(
    		s_ctrl->sensor_i2c_client,
    		s_ctrl->msm_sensor_reg->preview_fps_cfg_settings[preview_fps-5], //original
		//s_ctrl->msm_sensor_reg->preview_fps_cfg_settings[10], //temporary
    		s_ctrl->msm_sensor_reg->preview_fps_cfg_settings_size,
    		s_ctrl->msm_sensor_reg->default_data_type);
    }
#else
	rc = msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->preview_fps_cfg_settings[preview_fps],
		s_ctrl->msm_sensor_reg->preview_fps_cfg_settings_size,
		s_ctrl->msm_sensor_reg->default_data_type);
}
#endif

	if (rc < 0)
	{
		SKYCERR("ERR:%s FAIL!!! rc=%d \n", __func__, rc); //SKYCERR
		return rc;
	}	
	SKYCDBG("%s rc=%d end \n",__func__,rc); //SKYCDBG
	
	return rc;
}
static int yacd5c1sbdbc_vreg_init(void)
{
	int rc = 0;
	SKYCDBG("%s:%d\n", __func__, __LINE__);
	
	rc = sgpio_init(sgpios, CAMIO_MAX);
	if (rc < 0) {
		SKYCERR("%s: sgpio_init failed \n", __func__);
		goto sensor_init_fail;
	}

	rc = svreg_init(svregs, CAMV_MAX);
	if (rc < 0) {
		SKYCERR("%s: svreg_init failed \n", __func__);
		goto sensor_init_fail;
	}

	return rc;

sensor_init_fail:
    return -ENODEV;
}
int32_t yacd5c1sbdbc_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	SKYCDBG("%s: %d\n", __func__, __LINE__);

#if 0
	msm_sensor_probe_on(&s_ctrl->sensor_i2c_client->client->dev);
	SKYCDBG("%s msm_sensor_probe_on ok\n", __func__); 
	msm_camio_clk_rate_set(MSM_SENSOR_MCLK_24HZ);
	SKYCDBG("%s msm_camio_clk_rate_set ok\n", __func__);
#else
    rc = msm_sensor_power_up(s_ctrl);
    SKYCDBG(" %s : msm_sensor_power_up : rc = %d E\n",__func__, rc);  
#endif

    	yacd5c1sbdbc_vreg_init();

	if (svreg_ctrl(svregs, CAMV_CORE_1P8V, 1) < 0)	rc = -EIO;

	if (sgpio_ctrl(sgpios, CAMIO_RST_N, 0) < 0)	rc = -EIO;

	if (svreg_ctrl(svregs, CAMV_IO_1P8V, 1) < 0)	rc = -EIO;
	mdelay(1); /* > 20us */
	if (svreg_ctrl(svregs, CAMV_A_2P8V, 1) < 0)	rc = -EIO;
	mdelay(1); /* > 15us */
	if (svreg_ctrl(svregs, CAMV_CORE_1P8V, 1) < 0)	rc = -EIO;
	mdelay(1);
	if (sgpio_ctrl(sgpios, CAMIO_STB_N, 1) < 0)	rc = -EIO;
	//msm_camio_clk_rate_set(24000000);
    mdelay(35); //yacd5c1sbdbc spec: >30ms
	if (sgpio_ctrl(sgpios, CAMIO_RST_N, 1) < 0)	rc = -EIO;
	mdelay(1); /* > 50us */

	SKYCDBG("%s X (%d)\n", __func__, rc);
	return rc;

}

int32_t yacd5c1sbdbc_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
    int32_t rc = 0;
	SKYCDBG("%s\n", __func__);

#if 0
	msm_sensor_probe_off(&s_ctrl->sensor_i2c_client->client->dev);
#else
    msm_sensor_power_down(s_ctrl);
    SKYCDBG(" %s : msm_sensor_power_down : rc = %d E\n",__func__, rc);  
#endif

	if (sgpio_ctrl(sgpios, CAMIO_RST_N, 0) < 0)	rc = -EIO;
	mdelay(1); /* > 20 cycles (approx. 0.64us) */
	if (sgpio_ctrl(sgpios, CAMIO_STB_N, 0) < 0)	rc = -EIO;

	if (svreg_ctrl(svregs, CAMV_CORE_1P8V, 0) < 0)	rc = -EIO;
	if (svreg_ctrl(svregs, CAMV_A_2P8V, 0) < 0)	rc = -EIO;
	if (svreg_ctrl(svregs, CAMV_IO_1P8V, 0) < 0)	rc = -EIO;

	if (svreg_ctrl(svregs, CAMV_CORE_1P8V, 0) < 0)	rc = -EIO;

	svreg_release(svregs, CAMV_MAX);
	sgpio_release(sgpios, CAMIO_MAX);

	SKYCDBG("%s X (%d)\n", __func__, rc);
	return rc;

}

#endif

static int32_t yacd5c1sbdbc_trans_gpio_pm_to_sys(sgpio_ctrl_t *gpios, struct msm_sensor_ctrl_t *s_ctrl, uint32_t gpio_max_num)
{
	int i;
	int rc = 0;
	struct msm_camera_gpio_conf *gpio_conf =
		s_ctrl->sensordata->sensor_platform_info->gpio_conf;
	
	for (i = 0; i < gpio_max_num; i++) {		
		//if(strstr(gpios[i].label, gpio_conf->cam_gpio_common_tbl[i].label))
		if(strstr(gpios[i].label, "PM"))
		{	
			SKYCDBG("%s MATCH gpio string_111~~~[board=%s, %d], [driver=%s, %d]\n", __func__,
				gpio_conf->cam_gpio_req_tbl[i].label, gpio_conf->cam_gpio_req_tbl[i].gpio, gpios[i].label, gpios[i].nr);
			
			gpios[i].nr = gpio_conf->cam_gpio_req_tbl[i].gpio;
			
			SKYCDBG("%s MATCH gpio string_222~~~[board=%s, %d], [driver=%s, %d]\n", __func__,
				gpio_conf->cam_gpio_req_tbl[i].label, gpio_conf->cam_gpio_req_tbl[i].gpio, gpios[i].label, gpios[i].nr);
		}
		else
			SKYCDBG("%s DON'T MATCH gpio string~~~(board=%s, driver=%s)\n", __func__, gpio_conf->cam_gpio_req_tbl[i].label, gpios[i].label);
	}

	return rc;
}
int32_t yacd5c1sbdbc_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;
	SKYCDBG("%s_i2c_probe called\n", client->name);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SKYCERR("i2c_check_functionality failed\n");
		//rc = -EFAULT;
		return rc;
		//goto probe_fail;
	}

	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		if (s_ctrl->sensor_i2c_addr != 0)
			s_ctrl->sensor_i2c_client->client->addr =
				s_ctrl->sensor_i2c_addr;
	} else {
		rc = -EFAULT;
		//return rc;
		goto probe_fail;
	}

	s_ctrl->sensordata = client->dev.platform_data;
	if (s_ctrl->sensordata == NULL) {
		SKYCERR("%s %s NULL sensor data\n", __func__, client->name);
		rc = -EFAULT;
		goto probe_fail;
	}

	//We need to change gpio number for using pmic gpio
	yacd5c1sbdbc_trans_gpio_pm_to_sys(&sgpios[0], s_ctrl, CAMIO_PM_MAX);
	rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	if (rc < 0) {
		SKYCERR("%s %s power up failed\n", __func__, client->name);
		//return rc;
		rc = -EFAULT;
		goto probe_fail;
	}
	if (s_ctrl->func_tbl->sensor_match_id)
		rc = s_ctrl->func_tbl->sensor_match_id(s_ctrl);
	else
		rc = msm_sensor_match_id(s_ctrl);
	if (rc < 0)
		goto probe_fail;	

	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);
	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		s_ctrl->sensor_v4l2_subdev_ops);

	msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);
	goto i2c_probe_end;
probe_fail:
	SKYCERR("%s_i2c_probe failed\n", client->name);
i2c_probe_end:
	if (rc > 0)
		rc = 0;
	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	SKYCDBG("%s_probe Success!\n", client->name);
	return rc;
}


static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&yacd5c1sbdbc_i2c_driver);
}

static struct v4l2_subdev_core_ops yacd5c1sbdbc_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops yacd5c1sbdbc_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops yacd5c1sbdbc_subdev_ops = {
	.core = &yacd5c1sbdbc_subdev_core_ops,
	.video  = &yacd5c1sbdbc_subdev_video_ops,
};

static struct msm_sensor_fn_t yacd5c1sbdbc_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_setting = yacd5c1sbdbc_sensor_setting,//msm_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
#ifdef F_YACD5C1SBDBC_POWER
	.sensor_power_up = yacd5c1sbdbc_sensor_power_up,//msm_sensor_power_up,
	.sensor_power_down = yacd5c1sbdbc_sensor_power_down,//msm_sensor_power_down,
#else
    .sensor_power_up = msm_sensor_power_up,
    .sensor_power_down = msm_sensor_power_down,
#endif
#ifdef CONFIG_PANTECH_CAMERA_TUNER
    .sensor_set_tuner = yacd5c1sbdbc_sensor_set_tuner,
#endif
#ifdef CONFIG_PANTECH_CAMERA
    .sensor_set_effect = yacd5c1sbdbc_sensor_set_effect,
    .sensor_set_brightness = yacd5c1sbdbc_sensor_set_brightness,
    .sensor_set_exposure_mode = yacd5c1sbdbc_sensor_set_exposure_mode,
    .sensor_set_reflect = yacd5c1sbdbc_sensor_set_reflect,    
    .sensor_set_wb = yacd5c1sbdbc_sensor_set_wb,
    .sensor_set_preview_fps = yacd5c1sbdbc_sensor_set_preview_fps,
#endif
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t yacd5c1sbdbc_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
#ifdef F_STREAM_ON_OFF	
	.start_stream_conf = yacd5c1sbdbc_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(yacd5c1sbdbc_start_settings),
	.stop_stream_conf = yacd5c1sbdbc_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(yacd5c1sbdbc_stop_settings),
#endif
	.init_settings = &yacd5c1sbdbc_init_conf[0],
	.init_size = ARRAY_SIZE(yacd5c1sbdbc_init_conf),
	.mode_settings = &yacd5c1sbdbc_confs[0],
	.output_settings = &yacd5c1sbdbc_dimensions[0],
	.num_conf = ARRAY_SIZE(yacd5c1sbdbc_confs),
	//.num_conf = 2,
#ifdef CONFIG_PANTECH_CAMERA
    .effect_cfg_settings = &yacd5c1sbdbc_cfg_effect[0],
    .effect_cfg_settings_size = ARRAY_SIZE(yacd5c1sbdbc_cfg_effect[0]),
    .bright_cfg_settings = &yacd5c1sbdbc_cfg_brightness[0],
    .bright_cfg_settings_size = ARRAY_SIZE(yacd5c1sbdbc_cfg_brightness[0]),
    .exposure_mode_cfg_settings = &yacd5c1sbdbc_cfg_exposure_mode[0],
    .exposure_mode_cfg_settings_size = ARRAY_SIZE(yacd5c1sbdbc_cfg_exposure_mode[0]),
    .reflect_cfg_settings = &yacd5c1sbdbc_cfg_reflect[0],
    .reflect_cfg_settings_size = ARRAY_SIZE(yacd5c1sbdbc_cfg_reflect[0]),    
    .wb_cfg_settings = &yacd5c1sbdbc_cfg_wb[0],
    .wb_cfg_settings_size = ARRAY_SIZE(yacd5c1sbdbc_cfg_wb[0]),
    .preview_fps_cfg_settings = &yacd5c1sbdbc_cfg_preview_fps[0],
    .preview_fps_cfg_settings_size = ARRAY_SIZE(yacd5c1sbdbc_cfg_preview_fps[0]),
    .preview_24fps_for_motion_detect_cfg_settings = &yacd5c1sbdbc_cfg_preview_24fps_for_motion_detect[0],
    .preview_24fps_for_motion_detect_cfg_settings_size = ARRAY_SIZE(yacd5c1sbdbc_cfg_preview_24fps_for_motion_detect[0]),    
    .checkzsl_cfg_settings = &yacd5c1sbdbc_cfg_checkzsl[0],
    .checkzsl_cfg_settings_size = ARRAY_SIZE(yacd5c1sbdbc_cfg_checkzsl[0]),       
#endif
};

static struct msm_sensor_ctrl_t yacd5c1sbdbc_s_ctrl = {
	.msm_sensor_reg = &yacd5c1sbdbc_regs,
	.sensor_i2c_client = &yacd5c1sbdbc_sensor_i2c_client,
	.sensor_i2c_addr = 0x40,//0x6E,//0x20,//0x6C,//34,
	.sensor_id_info = &yacd5c1sbdbc_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
#if 0
	.csi_params = &yacd5c1sbdbc_csi_params_array[0],
#endif
	.msm_sensor_mutex = &yacd5c1sbdbc_mut,
	.sensor_i2c_driver = &yacd5c1sbdbc_i2c_driver,
	.sensor_v4l2_subdev_info = yacd5c1sbdbc_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(yacd5c1sbdbc_subdev_info),
	.sensor_v4l2_subdev_ops = &yacd5c1sbdbc_subdev_ops,
	.func_tbl = &yacd5c1sbdbc_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

//module_init(msm_sensor_init_module);
late_initcall(msm_sensor_init_module);
MODULE_DESCRIPTION("Hynix 2MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
