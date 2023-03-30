/***************************************************************
** Copyright (C),  2018,  OPLUS Mobile Comm Corp.,  Ltd
** File : oplus_display_private_api.h
** Description : oplus display private api implement
** Version : 1.0
** Date : 2018/03/20
** Author : Jie.Hu@PSW.MM.Display.Stability
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**   Hu.Jie          2018/03/20        1.0           Build this moudle
**   Guo.Ling        2018/10/11        1.1           Modify for SDM660
**   Guo.Ling        2018/11/27        1.2           Modify for mt6779
**   Li.Ping         2019/11/18        1.3           Modify for mt6885
******************************************************************/
#include "oplus_display_private_api.h"
#include "mtk_disp_aal.h"
#include <soc/oplus/system/oplus_project.h>
/*
 * we will create a sysfs which called /sys/kernel/oplus_display,
 * In that directory, oplus display private api can be called
 */

#define PANEL_SERIAL_NUM_REG 0xD8
#define PANEL_REG_READ_LEN   10
uint64_t serial_number = 0x0;
typedef struct panel_serial_info
{
    int reg_index;
    uint64_t year;
    uint64_t month;
    uint64_t day;
    uint64_t hour;
    uint64_t minute;
    uint64_t second;
    uint64_t reserved[2];
} PANEL_SERIAL_INFO;

extern struct drm_device* get_drm_device(void);
extern int mtk_drm_setbacklight(struct drm_crtc *crtc, unsigned int level);
extern int oplus_mtk_drm_sethbm(struct drm_crtc *crtc, unsigned int hbm_mode);
extern int oplus_mtk_drm_setcabc(struct drm_crtc *crtc, unsigned int hbm_mode);
extern int oplus_mtk_drm_setseed(struct drm_crtc *crtc, unsigned int seed_mode);
unsigned long oplus_display_brightness = 0;
unsigned long oplus_max_normal_brightness = 0;
int oplus_dc_alpha = 0;
int esd_status = 0;
bool oplus_fp_notify_down_delay = false;
bool oplus_fp_notify_up_delay = false;
extern void fingerprint_send_notify(unsigned int fingerprint_op_mode);
unsigned long seed_mode = 0;
unsigned long esd_mode = 0;
EXPORT_SYMBOL(esd_mode);

/* lihao@MULTIMEDIA.DISPLAY.LCD 2021/07/30 Turn on black screen gesture shutdown timing modification */
/* #ifdef OPLUS_BUG_COMPATIBILITY */
int shutdown_flag = 0;
/* #endif */
static ssize_t oplus_display_get_brightness(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%ld\n", oplus_display_brightness);
}

static ssize_t oplus_display_get_esd_status(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", esd_status);
}

static ssize_t oplus_display_set_esd_status(struct device *dev,
struct device_attribute *attr, const char *buf, size_t num)
{
	esd_status = 0;
	return num;
}

/* lihao@MULTIMEDIA.DISPLAY.LCD 2021/07/30 Turn on black screen gesture shutdown timing modification */
/* #ifdef OPLUS_BUG_COMPATIBILITY */
static ssize_t oplus_get_shutdownflag(struct device *dev,
struct device_attribute *attr, char *buf)
{
	printk("get shutdown_flag = %d\n", shutdown_flag);
	return sprintf(buf, "%d\n", shutdown_flag);
}

static ssize_t oplus_set_shutdownflag(struct device *dev,
struct device_attribute *attr, const char *buf, size_t num)
{
	int flag = 0;
	sscanf(buf, "%du", &flag);
	if (1 == flag) {
		shutdown_flag = 1;
	}
	pr_err("shutdown_flag = %d\n", shutdown_flag);
	return num;
}
/* #endif */
static ssize_t oplus_display_set_brightness(struct device *dev,
struct device_attribute *attr, const char *buf, size_t num)
{
	int ret;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	unsigned int oplus_set_brightness = 0;

	ret = kstrtouint(buf, 10, &oplus_set_brightness);

	printk("%s %d\n", __func__, oplus_set_brightness);

	if (oplus_set_brightness > OPLUS_MAX_BRIGHTNESS || oplus_set_brightness < OPLUS_MIN_BRIGHTNESS) {
		printk(KERN_ERR "%s, brightness:%d out of scope\n", __func__, oplus_set_brightness);
		return num;
	}

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		printk(KERN_ERR "find crtc fail\n");
		return 0;
	}
	mtk_drm_setbacklight(crtc, oplus_set_brightness);

	return num;
}

static ssize_t oplus_display_get_max_brightness(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", OPLUS_MAX_BRIGHTNESS);
}

static ssize_t oplus_display_get_maxbrightness(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", oplus_max_normal_brightness);
}

unsigned int hbm_mode = 0;
static ssize_t oplus_display_get_hbm(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%ld\n", hbm_mode);
}

static ssize_t oplus_display_set_hbm(struct device *dev,
struct device_attribute *attr, const char *buf, size_t num)
{
	int ret;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	unsigned int tmp = 0;

	ret = kstrtouint(buf, 10, &tmp);

	printk("%s, %d to be %d\n", __func__, hbm_mode, tmp);

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		printk(KERN_ERR "find crtc fail\n");
		return 0;
	}

	oplus_mtk_drm_sethbm(crtc, tmp);
	hbm_mode = tmp;

	if (tmp == 1)
		usleep_range(30000, 31000);
	return num;
}

static ssize_t fingerprint_notify_trigger(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t num)
{
	unsigned int fingerprint_op_mode = 0x0;

	if (kstrtouint(buf, 0, &fingerprint_op_mode)) {
		pr_err("%s kstrtouu8 buf error!\n", __func__);
		return num;
	}

	if (fingerprint_op_mode == 1) {
		oplus_fp_notify_down_delay = true;
	} else {
		oplus_fp_notify_up_delay = true;
	}

	printk(KERN_ERR "%s receive uiready %d\n", __func__, fingerprint_op_mode);
	return num;
}

/*
* add for lcd serial
*/
int panel_serial_number_read(char cmd, int num)
{
        char para[20] = {0};
        int count = 10;
        PANEL_SERIAL_INFO panel_serial_info;
        struct mtk_ddp_comp *comp;
        struct drm_crtc *crtc;
        struct mtk_drm_crtc *mtk_crtc;
    struct drm_device *ddev = get_drm_device();

        #ifdef NO_AOD_6873
        return 0;
        #endif

        para[0] = cmd;
        para[1] = num;

        /* this debug cmd only for crtc0 */
        crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
            typeof(*crtc), head);
        if (!crtc) {
            DDPPR_ERR("find crtc fail\n");
            return 0;
        }

        mtk_crtc = to_mtk_crtc(crtc);
        comp = mtk_ddp_comp_request_output(mtk_crtc);
        if (!comp->funcs || !comp->funcs->io_cmd) {
            DDPINFO("cannot find output component\n");
            return 0;
        }
        while (count > 0) {
                comp->funcs->io_cmd(comp, NULL, DSI_READ, para);
                count--;
                panel_serial_info.reg_index = 4;
                panel_serial_info.month     = para[panel_serial_info.reg_index] & 0x0F;
                panel_serial_info.year      = ((para[panel_serial_info.reg_index + 1] & 0xE0) >> 5) + 7;
                panel_serial_info.day       = para[panel_serial_info.reg_index + 1] & 0x1F;
                panel_serial_info.hour      = para[panel_serial_info.reg_index + 2] & 0x17;
                panel_serial_info.minute    = para[panel_serial_info.reg_index + 3];
                panel_serial_info.second    = para[panel_serial_info.reg_index + 4];
                panel_serial_info.reserved[0] = 0;
                panel_serial_info.reserved[1] = 0;
                serial_number = (panel_serial_info.year     << 56)\
                        + (panel_serial_info.month      << 48)\
                        + (panel_serial_info.day        << 40)\
                        + (panel_serial_info.hour       << 32)\
                        + (panel_serial_info.minute << 24)\
                        + (panel_serial_info.second << 16)\
                        + (panel_serial_info.reserved[0] << 8)\
                        + (panel_serial_info.reserved[1]);
         if (panel_serial_info.year <= 7) {
             continue;
         } else {
             printk("%s year:0x%llx, month:0x%llx, day:0x%llx, hour:0x%llx, minute:0x%llx, second:0x%llx!\n",
                 __func__,
                 panel_serial_info.year,
                 panel_serial_info.month,
                 panel_serial_info.day,
                 panel_serial_info.hour,
                 panel_serial_info.minute,
                 panel_serial_info.second);
             break;
         }
        }
        printk("%s Get panel serial number[0x%llx]\n",__func__, serial_number);
        return 1;
}

static ssize_t oplus_get_panel_serial_number(struct device *dev,
struct device_attribute *attr, char *buf)
{
        int ret = 0;
        ret = panel_serial_number_read(PANEL_SERIAL_NUM_REG,PANEL_REG_READ_LEN);
        if (ret <= 0)
                return scnprintf(buf, PAGE_SIZE, "Get serial number failed: %d\n",ret);
        else
                return scnprintf(buf, PAGE_SIZE, "%llx\n",serial_number);
}

static ssize_t panel_serial_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	printk("[soso] Lcm read 0xA1 reg = 0x%llx\n", serial_number);
        return count;
}

//unsigned long CABC_mode = 2;
/*
* add dre only use for camera
*/
//extern void disp_aal_set_dre_en(int enable);

/*static ssize_t LCM_CABC_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    printk("%s CABC_mode=%ld\n", __func__, CABC_mode);
    return sprintf(buf, "%ld\n", CABC_mode);
}

static ssize_t LCM_CABC_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t num)
{
    int ret = 0;

    ret = kstrtoul(buf, 10, &CABC_mode);
    if( CABC_mode > 3 ){
        CABC_mode = 3;
    }
    printk("%s CABC_mode=%ld\n", __func__, CABC_mode);

    if (CABC_mode == 0) {
        disp_aal_set_dre_en(1);
        printk("%s enable dre\n", __func__);
    } else {
        disp_aal_set_dre_en(0);
        printk("%s disable dre\n", __func__);
    }
*/
    /*
    * modify for oled not need set cabc,but TFT lcd need config this api
    */
    //if (oplus_display_cabc_support) {
    //    ret = primary_display_set_cabc_mode((unsigned int)CABC_mode);
    //}
/*
    return num;
}*/

unsigned long silence_mode = 0;
static ssize_t silence_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	printk("%s silence_mode=%ld\n", __func__, silence_mode);
	return sprintf(buf, "%ld\n", silence_mode);
}

static ssize_t silence_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t num)
{
	int ret;
	msleep(1000);
	ret = kstrtoul(buf, 10, &silence_mode);
	printk("%s silence_mode=%ld\n", __func__, silence_mode);
	return num;
}

static ssize_t oplus_display_get_ESD(struct device *dev,
struct device_attribute *attr, char *buf)
{
	printk("%s esd=%ld\n", __func__, esd_mode);
	return sprintf(buf, "%ld\n", esd_mode);
}

static ssize_t oplus_display_set_ESD(struct device *dev,
struct device_attribute *attr, const char *buf, size_t num)
{
	int ret = 0;

	ret = kstrtoul(buf, 10, &esd_mode);
	printk("%s,esd mode is %d\n", __func__, esd_mode);
	return num;
}

unsigned long cabc_mode = 1;
unsigned long cabc_true_mode = 1;
unsigned long cabc_sun_flag = 0;
unsigned long cabc_back_flag = 1;
extern void disp_aal_set_dre_en(int enable);

enum{
	CABC_LEVEL_0,
	CABC_LEVEL_1,
	CABC_LEVEL_2 = 3,
	CABC_EXIT_SPECIAL = 8,
	CABC_ENTER_SPECIAL = 9,
};

static ssize_t oplus_display_get_CABC(struct device *dev,
struct device_attribute *attr, char *buf)
{
	printk("%s CABC_mode=%ld\n", __func__, cabc_true_mode);
	return sprintf(buf, "%ld\n", cabc_true_mode);
}

static ssize_t oplus_display_set_CABC(struct device *dev,
struct device_attribute *attr, const char *buf, size_t num)
{
	int ret = 0;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();

	ret = kstrtoul(buf, 10, &cabc_mode);
	cabc_true_mode = cabc_mode;
/* #ifdef OPLUS_BUG_COMPATIBILITY  */
/* wangcheng@MULTIMEDIA.DISPLAY.LCD 2021/08/12 modified close unnecessary log print */
/*	printk("%s,cabc mode is %d, cabc_back_flag is %d\n", __func__, cabc_mode, cabc_back_flag); */
/* #endif */
	if(cabc_mode < 4)
		cabc_back_flag = cabc_mode;

	if (cabc_mode == CABC_ENTER_SPECIAL) {
		cabc_sun_flag = 1;
		cabc_true_mode = 0;
	} else if (cabc_mode == CABC_EXIT_SPECIAL) {
		cabc_sun_flag = 0;
		cabc_true_mode = cabc_back_flag;
	} else if (cabc_sun_flag == 1) {
		if (cabc_back_flag == CABC_LEVEL_0) {
			disp_aal_set_dre_en(1);
			printk("%s sun enable dre\n", __func__);
		} else {
/* wangcheng@MULTIMEDIA.DISPLAY.LCD 2021/07/19 add enable global dre */
/* #ifdef OPLUS_BUG_COMPATIBILITY */
			if (is_project(21690) || is_project(21691) || is_project(21692) || is_project(21684) || is_project(21685) || is_project(21686)) {
				disp_aal_set_dre_en(1);
			} else {
				disp_aal_set_dre_en(0);
				printk("%s sun disable dre\n", __func__);
			}
/* #endif */
		}
		return num;
	}
/* #ifdef OPLUS_BUG_COMPATIBILITY  */
/* wangcheng@MULTIMEDIA.DISPLAY.LCD 2021/08/12 modified close unnecessary log print */
/*	printk("%s,cabc mode is %d\n", __func__, cabc_true_mode); */
/* #endif */

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		printk(KERN_ERR "find crtc fail\n");
		return 0;
	}
	if (cabc_true_mode == CABC_LEVEL_0 && cabc_back_flag == CABC_LEVEL_0) {
		disp_aal_set_dre_en(1);
		printk("%s enable dre\n", __func__);
	} else {
/* wangcheng@MULTIMEDIA.DISPLAY.LCD 2021/07/19 add enable global dre */
/* #ifdef OPLUS_BUG_COMPATIBILITY */
			if (is_project(21690) || is_project(21691) || is_project(21692) || is_project(21684) || is_project(21685) || is_project(21686)) {
				disp_aal_set_dre_en(1);
			} else {
				disp_aal_set_dre_en(0);
				printk("%s sun disable dre\n", __func__);
			}
/* #endif */
	}
	oplus_mtk_drm_setcabc(crtc, cabc_true_mode);

	return num;
}

static struct kobject *oplus_display_kobj;
static DEVICE_ATTR(oplus_brightness, S_IRUGO|S_IWUSR, oplus_display_get_brightness, oplus_display_set_brightness);
static DEVICE_ATTR(oplus_max_brightness, S_IRUGO|S_IWUSR, oplus_display_get_max_brightness, NULL);
static DEVICE_ATTR(max_brightness, S_IRUGO|S_IWUSR, oplus_display_get_maxbrightness, NULL);
static DEVICE_ATTR(hbm, S_IRUGO|S_IWUSR, oplus_display_get_hbm, oplus_display_set_hbm);
static DEVICE_ATTR(oplus_notify_fppress, S_IRUGO|S_IWUSR, NULL, fingerprint_notify_trigger);
static DEVICE_ATTR(panel_serial_number, S_IRUGO|S_IWUSR, oplus_get_panel_serial_number, panel_serial_store);
static DEVICE_ATTR(LCM_CABC, S_IRUGO|S_IWUSR, oplus_display_get_CABC, oplus_display_set_CABC);
static DEVICE_ATTR(esd, S_IRUGO|S_IWUSR, oplus_display_get_ESD, oplus_display_set_ESD);
static DEVICE_ATTR(sau_closebl_node, S_IRUGO|S_IWUSR, silence_show, silence_store);
static DEVICE_ATTR(esd_status, S_IRUGO|S_IWUSR, oplus_display_get_esd_status, oplus_display_set_esd_status);

/* lihao@MULTIMEDIA.DISPLAY.LCD 2021/07/30 Turn on black screen gesture shutdown timing modification */
/* #ifdef OPLUS_BUG_COMPATIBILITY */
static DEVICE_ATTR(shutdownflag, S_IRUGO|S_IWUSR, oplus_get_shutdownflag, oplus_set_shutdownflag);
/* #endif */

/*static DEVICE_ATTR(dim_alpha, S_IRUGO|S_IWUSR, oplus_display_get_dim_alpha, oplus_display_set_dim_alpha);
static DEVICE_ATTR(seed, S_IRUGO|S_IWUSR, oplus_display_get_seed, oplus_display_set_seed);
static DEVICE_ATTR(dimlayer_bl_en, S_IRUGO|S_IWUSR, oplus_display_get_dc_enable, oplus_display_set_dc_enable);
static DEVICE_ATTR(dim_dc_alpha, S_IRUGO|S_IWUSR, oplus_display_get_dim_dc_alpha, oplus_display_set_dim_dc_alpha);
static DEVICE_ATTR(oplus_brightness, S_IRUGO|S_IWUSR, oplus_display_get_brightness, oplus_display_set_brightness);
static DEVICE_ATTR(oplus_max_brightness, S_IRUGO|S_IWUSR, oplus_display_get_max_brightness, NULL);
static DEVICE_ATTR(fingerprint_notify, S_IRUGO|S_IWUSR, NULL, fingerprint_notify_trigger);
static DEVICE_ATTR(LCM_CABC, S_IRUGO|S_IWUSR, LCM_CABC_show, LCM_CABC_store);
static DEVICE_ATTR(aod_area, S_IRUGO|S_IWUSR, NULL, oplus_display_set_aod_area);
static DEVICE_ATTR(sau_closebl_node, S_IRUGO|S_IWUSR, silence_show, silence_store);
static DEVICE_ATTR(panel_serial_number, S_IRUGO|S_IWUSR, oplus_get_panel_serial_number, panel_serial_store);
static DEVICE_ATTR(ffl_set, S_IRUGO|S_IWUSR, FFL_SET_show, FFL_SET_store);
static DEVICE_ATTR(LCM_CABC, S_IRUGO|S_IWUSR, LCM_CABC_show, LCM_CABC_store);
static DEVICE_ATTR(aod_light_mode_set, S_IRUGO|S_IWUSR, oplus_get_aod_light_mode, oplus_set_aod_light_mode);
static DEVICE_ATTR(cabc, S_IRUGO|S_IWUSR, oplus_display_get_CABC, oplus_display_set_CABC);
static DEVICE_ATTR(esd, S_IRUGO|S_IWUSR, oplus_display_get_ESD, oplus_display_set_ESD);*/

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *oplus_display_attrs[] = {
	&dev_attr_oplus_brightness.attr,
	&dev_attr_oplus_max_brightness.attr,
	&dev_attr_max_brightness.attr,
	&dev_attr_hbm.attr,
//	&dev_attr_seed.attr,
	&dev_attr_oplus_notify_fppress.attr,
	&dev_attr_panel_serial_number.attr,
	&dev_attr_LCM_CABC.attr,
	&dev_attr_esd.attr,
	&dev_attr_sau_closebl_node.attr,
	&dev_attr_esd_status.attr,
/* lihao@MULTIMEDIA.DISPLAY.LCD 2021/07/30 Turn on black screen gesture shutdown timing modification */
/* #ifdef OPLUS_BUG_COMPATIBILITY */
	&dev_attr_shutdownflag.attr,
/* #endif */
	/*&dev_attr_dim_alpha.attr,
	&dev_attr_dimlayer_bl_en.attr,
	&dev_attr_dim_dc_alpha.attr,
	&dev_attr_oplus_brightness.attr,
	&dev_attr_oplus_max_brightness.attr,
	&dev_attr_fingerprint_notify.attr,
	&dev_attr_aod_area.attr,
	&dev_attr_sau_closebl_node.attr,
	&dev_attr_esd_status.attr,
	&dev_attr_panel_serial_number.attr,
	&dev_attr_ffl_set.attr,
	&dev_attr_LCM_CABC.attr,
	&dev_attr_cabc.attr,
	&dev_attr_esd.attr,
	&dev_attr_aod_light_mode_set.attr,*/
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group oplus_display_attr_group = {
	.attrs = oplus_display_attrs,
};

static int __init oplus_display_private_api_init(void)
{
	int retval;

	oplus_display_kobj = kobject_create_and_add("oplus_display", kernel_kobj);
	if (!oplus_display_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(oplus_display_kobj, &oplus_display_attr_group);
	if (retval)
		kobject_put(oplus_display_kobj);

	return retval;
}

static void __exit oplus_display_private_api_exit(void)
{
	kobject_put(oplus_display_kobj);
}

module_init(oplus_display_private_api_init);
module_exit(oplus_display_private_api_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hujie <hujie@oplus.com>");
