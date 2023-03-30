// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
 /**************************************************************************
 * Copyright (c) 2020-2030 OPLUS Mobile Comm Corp.,Ltd. All Rights Reserved.
 * File                   : mtk_ts_flashlight.cpp
 * Description     : file to set flashlight ntc as a mtk thermal zone
 * -------------Rivision History-----------------------------------------
 * <version>      <date>            <author>                                      <Modify.desc>
 * 1.0                  2020-6-5      lifeiping@Camera.Drvier              add new file
 * OPLUS Mobile Comm Proprietary and Confidential.
 *************************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/uaccess.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include <linux/uidgid.h>
#include <linux/slab.h>
#include "tzbatt_initcfg.h"
#include <linux/power_supply.h>

extern int mt6360_get_flashlight_temperature(int *temp);

/* ************************************ */
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);

static unsigned int interval;	/* seconds, 0 : no auto polling */
static int trip_temp[10] = { 120000, 110000, 100000, 90000, 80000,
				70000, 65000, 60000, 55000, 50000 };

/* static unsigned int cl_dev_dis_charge_state = 0; */
static struct thermal_zone_device *thz_dev;
/* static struct thermal_cooling_device *cl_dev_dis_charge; */
static int mtk_flashlight_debug_log;
static int kernelmode;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip = 0;
static char g_bind0[20] = { 0 };
static char g_bind1[20] = { 0 };
static char g_bind2[20] = { 0 };
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };

/* static int flashlight_write_flag=0; */

#define MTK_FLASHLIGHT_TEMP_CRIT 120000	/* 120.000 degree Celsius */

#define mtk_flashlight_dprintk(fmt, args...)   \
do {                                    \
	if (mtk_flashlight_debug_log) {                \
		pr_debug("[Thermal/TZ/flashlight]" fmt, ##args); \
	}                                   \
} while (0)

#define mtk_flashlight_printk(fmt, args...)   \
pr_info("[Thermal/TZ/flashlight]" fmt, ##args)

struct NTC_TEMPERATURE {
	int ntc_temp;
	int temperature_r;
};

#define TEMPERATURE_TBL_SIZE 166

static struct NTC_TEMPERATURE FLASHLIGHT_Temperature_Table[] = {
	{-40, 1795776},
	{-39, 1795468},
	{-38, 1795142},
	{-37, 1794793},
	{-36, 1794424},
	{-35, 1794030},
	{-34, 1793610},
	{-33, 1793166},
	{-32, 1792695},
	{-31, 1792194},
	{-30, 1791663},
	{-29, 1791098},
	{-28, 1790501},
	{-27, 1789871},
	{-26, 1789196},
	{-25, 1788491},
	{-24, 1787738},
	{-23, 1786947},
	{-22, 1786111},
	{-21, 1785222},
	{-20, 1784279},
	{-19, 1783302},
	{-18, 1782249},
	{-17, 1781152},
	{-16, 1779991},
	{-15, 1778769},
	{-14, 1777480},
	{-13, 1776124},
	{-12, 1774694},
	{-11, 1773194},
	{-10, 1771613},
	{-9,  1769950},
	{-8,  1768209},
	{-7,  1766374},
	{-6,  1764448},
	{-5,  1762430},
	{-4,  1760309},
	{-3,  1758091},
	{-2,  1755763},
	{-1,  1753320},
	{0,   1750766},
	{1,   1748097},
	{2,   1745305},
	{3,   1742382},
	{4,   1739333},
	{5,   1736125},
	{6,   1732786},
	{7,   1729329},
	{8,   1725681},
	{9,   1721909},
	{10,  1717958},
	{11,  1713834},
	{12,  1709548},
	{13,  1705113},
	{14,  1700498},
	{15,  1695652},
	{16,  1690644},
	{17,  1685496},
	{18,  1680080},
	{19,  1674477},
	{20,  1668613},
	{21,  1662595},
	{22,  1656459},
	{23,  1650000},
	{24,  1643206},
	{25,  1636364},
	{26,  1629222},
	{27,  1621853},
	{28,  1614241},
	{29,  1606389},
	{30,  1598274},
	{31,  1589940},
	{32,  1581341},
	{33,  1572497},
	{34,  1563376},
	{35,  1554031},
	{36,  1544391},
	{37,  1534552},
	{38,  1524391},
	{39,  1514013},
	{40,  1503362},
	{41,  1492465},
	{42,  1481360},
	{43,  1469906},
	{44,  1458249},
	{45,  1446365},
	{46,  1434221},
	{47,  1421849},
	{48,  1409205},
	{49,  1396413},
	{50,  1383237},
	{51,  1369995},
	{52,  1356432},
	{53,  1342683},
	{54,  1328796},
	{55,  1314694},
	{56,  1300416},
	{57,  1285861},
	{58,  1271210},
	{59,  1256358},
	{60,  1241341},
	{61,  1226203},
	{62,  1210995},
	{63,  1195567},
	{64,  1180165},
	{65,  1164407},
	{66,  1148770},
	{67,  1133086},
	{68,  1117147},
	{69,  1101242},
	{70,  1085147},
	{71,  1069184},
	{72,  1053112},
	{73,  1037288},
	{74,  1021116},
	{75,  1004947},
	{76,  988824},
	{77,  972794},
	{78,  956909},
	{79,  941221},
	{80,  925364},
	{81,  909352},
	{82,  893610},
	{83,  877963},
	{84,  862402},
	{85,  846964},
	{86,  831581},
	{87,  816340},
	{88,  801221},
	{89,  786258},
	{90,  771429},
	{91,  756764},
	{92,  742172},
	{93,  727806},
	{94,  713568},
	{95,  699554},
	{96,  685656},
	{97,  671968},
	{98,  658447},
	{99,  645117},
	{100, 631927},
	{101, 618975},
	{102, 606208},
	{103, 593647},
	{104, 581231},
	{105, 569062},
	{106, 557078},
	{107, 545295},
	{108, 533732},
	{109, 522317},
	{110, 511156},
	{111, 500173},
	{112, 489384},
	{113, 478802},
	{114, 468442},
	{115, 458218},
	{116, 448243},
	{117, 438427},
	{118, 428885},
	{119, 419420},
	{120, 410253},
	{121, 401181},
	{122, 392320},
	{123, 383681},
	{124, 375273},
	{125, 366993},
};

static int pre_temp_idx = 70;  /* start from 30 'c */

static int mtk_flashlight_get_temp(struct thermal_zone_device *thermal, int *t)
{
	int real_temp, temp_uv, uv_l, uv_r;

	int ret = mt6360_get_flashlight_temperature(&temp_uv);

	mtk_flashlight_printk("[%s] T_flashlight get_temp_status %d, temp_uv, %d\n", __func__, ret, temp_uv);

	if (pre_temp_idx > TEMPERATURE_TBL_SIZE - 1) {
		pre_temp_idx = TEMPERATURE_TBL_SIZE - 1;
	} else if (pre_temp_idx < 0) {
		pre_temp_idx = 0;
	}

	if (temp_uv < FLASHLIGHT_Temperature_Table[pre_temp_idx].temperature_r) {
		for (; pre_temp_idx < TEMPERATURE_TBL_SIZE - 1; pre_temp_idx++) {
			if (temp_uv <= FLASHLIGHT_Temperature_Table[pre_temp_idx].temperature_r
					&& temp_uv > FLASHLIGHT_Temperature_Table[pre_temp_idx+1].temperature_r)
				break;
		}
	} else if (temp_uv > FLASHLIGHT_Temperature_Table[pre_temp_idx].temperature_r) {
		for (; pre_temp_idx > 0; pre_temp_idx--) {
			if (temp_uv <= FLASHLIGHT_Temperature_Table[pre_temp_idx].temperature_r
					&& temp_uv > FLASHLIGHT_Temperature_Table[pre_temp_idx+1].temperature_r)
				break;
		}
	}
	if ((temp_uv >= FLASHLIGHT_Temperature_Table[0].temperature_r) || pre_temp_idx <= 0) {
		real_temp = FLASHLIGHT_Temperature_Table[0].ntc_temp * 1000;
	} else if (temp_uv <= FLASHLIGHT_Temperature_Table[TEMPERATURE_TBL_SIZE-1].temperature_r
				|| pre_temp_idx >= TEMPERATURE_TBL_SIZE-1) {
		real_temp = FLASHLIGHT_Temperature_Table[TEMPERATURE_TBL_SIZE-1].ntc_temp * 1000;
	} else {
		uv_l = FLASHLIGHT_Temperature_Table[pre_temp_idx].temperature_r;
		uv_r = FLASHLIGHT_Temperature_Table[pre_temp_idx+1].temperature_r;
		real_temp = FLASHLIGHT_Temperature_Table[pre_temp_idx].ntc_temp * 1000 +
			 ((temp_uv - uv_l) * 1000) / (uv_r - uv_l);
	}

	*t = real_temp;

	mtk_flashlight_printk("[%s] T_flashlight real_temp, %d\n", __func__, real_temp);

	thermal->polling_delay = 0;  /* no-auto polling */

	return 0;
}

static int mtk_flashlight_bind(struct thermal_zone_device *thermal,
			     struct thermal_cooling_device *cdev)
{
	/*int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtk_flashlight_dprintk(
			"[%s] error binding cooling dev\n", __func__);
		return -EINVAL;
	}

	mtk_flashlight_dprintk("[%s] binding OK, %d\n", __func__, table_val);*/
	return 0;
}

static int mtk_flashlight_unbind(struct thermal_zone_device *thermal,
			       struct thermal_cooling_device *cdev)
{
	/*int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtk_flashlight_dprintk("[%s] %s\n", __func__, cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtk_flashlight_dprintk(
				"[%s] error unbinding cooling dev\n", __func__);

		return -EINVAL;
	}

	mtk_flashlight_dprintk("[%s] unbinding OK\n", __func__);*/
	return 0;
}

static int mtk_flashlight_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtk_flashlight_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtk_flashlight_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtk_flashlight_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtk_flashlight_get_crit_temp(struct thermal_zone_device *thermal,
				      int *temperature)
{
	*temperature = MTK_FLASHLIGHT_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtk_flashlight_dev_ops = {
	.bind = mtk_flashlight_bind,
	.unbind = mtk_flashlight_unbind,
	.get_temp = mtk_flashlight_get_temp,
	.get_mode = mtk_flashlight_get_mode,
	.set_mode = mtk_flashlight_set_mode,
	.get_trip_type = mtk_flashlight_get_trip_type,
	.get_trip_temp = mtk_flashlight_get_trip_temp,
	.get_crit_temp = mtk_flashlight_get_crit_temp,
};

/*static struct thermal_cooling_device_ops mtk_flashlight_cooling_sysrst_ops = {
	.get_max_state = tsflashlight_sysrst_get_max_state,
	.get_cur_state = tsflashlight_sysrst_get_cur_state,
	.set_cur_state = tsflashlight_sysrst_set_cur_state,
};*/


static int mtk_flashlight_read(struct seq_file *m, void *v)
/* static int mtk_flashlight_read(
 * char *buf, char **start, off_t off, int count, int *eof, void *data)
 */
{

	seq_printf(m,
		"[mtk_flashlight_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,\n",
		trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

	seq_printf(m,
		"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n",
		trip_temp[4], trip_temp[5], trip_temp[6],
		trip_temp[7], trip_temp[8], trip_temp[9]);

	seq_printf(m,
		"g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,",
		g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
		g_THERMAL_TRIP[2], g_THERMAL_TRIP[3]);

	seq_printf(m,
		"g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,",
		g_THERMAL_TRIP[4], g_THERMAL_TRIP[5],
		g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);

	seq_printf(m,
		"g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
		g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);

	seq_printf(m,
		"cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n",
		g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

	seq_printf(m,
		"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
		g_bind5, g_bind6, g_bind7, g_bind8, g_bind9, interval * 1000);

	return 0;
}

static int mtk_flashlight_register_thermal(void);
static void mtk_flashlight_unregister_thermal(void);

static ssize_t mtk_flashlight_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
/* static ssize_t mtk_flashlight_write(
 * struct file *file, const char *buffer, int count, void *data)
 */
{
	pr_debug("Power/flashlight_Thermal: write, write, write!!!");
	pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	pr_debug("*****************************************");
	pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	return -EINVAL;
}

static int mtk_flashlight_register_thermal(void)
{
	mtk_flashlight_dprintk("[%s]\n", __func__);

	/* trips : trip 0~1 */
	thz_dev = mtk_thermal_zone_device_register("mtkflashlight", num_trip,
						NULL, &mtk_flashlight_dev_ops,
						0, 0, 0, interval * 1000);

	return 0;
}

static void mtk_flashlight_unregister_thermal(void)
{
	mtk_flashlight_dprintk("[%s]\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtk_flashlight_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_flashlight_read, NULL);
}

static const struct file_operations mtkts_flashlight_fops = {
	.owner = THIS_MODULE,
	.open = mtk_flashlight_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtk_flashlight_write,
	.release = single_release,
};

static int __init mtk_flashlight_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtk_flashlight_dir = NULL;

	mtk_flashlight_dprintk("[%s]\n", __func__);

	/*err = mtk_flashlight_register_cooler();
	if (err)
		return err;*/

	mtk_flashlight_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtk_flashlight_dir) {
		mtk_flashlight_dprintk("%s mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzflashlight", 0664, mtk_flashlight_dir,
							&mtkts_flashlight_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}
	mtk_flashlight_register_thermal();
	/* mtkTTimer_register("mtk_flashlight", mtkts_flashlight_start_thermal_timer,
					mtkts_flashlight_cancel_thermal_timer); */
	return 0;
}

static void __exit mtk_flashlight_exit(void)
{
	mtk_flashlight_dprintk("[%s]\n", __func__);
	mtk_flashlight_unregister_thermal();
	/* mtkTTimer_unregister("mtk_flashlight"); */
}
module_init(mtk_flashlight_init);
module_exit(mtk_flashlight_exit);
