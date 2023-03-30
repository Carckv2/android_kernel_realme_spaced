/******************************************************************
** Copyright (C), 2004-2020 OPLUS Mobile Comm Corp., Ltd.
** File: - camera_protecthub.c
** Description: Source file for camera_protect sensor linux driver.
** Version: 1.0
** Date : 2020/03/31
** Author: Baixue.Jie@PSW.BSP.Sensor
**
** --------------------------- Revision History: ---------------------
* <version> <date>      <author>                    <desc>
* Revision 1.0      2020/03/31       Baixue.Jie@PSW.BSP.Sensor      Created
*******************************************************************/

#include "camera_protecthub.h"
#include "sensor_cmd.h"
#include "virtual_sensor.h"
#include <linux/notifier.h>
#include <linux/pm_wakeup.h>
#include <linux/version.h>

#define CAMERA_PROTECT_TAG                   "[camera_protecthub] "
#define CAMERA_PROTECT_FUN(f)                pr_err(CAMERA_PROTECT_TAG"%s\n", __func__)
#define CAMERA_PROTECT_PR_ERR(fmt, args...)  pr_err(CAMERA_PROTECT_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define CAMERA_PROTECT_LOG(fmt, args...)     pr_err(CAMERA_PROTECT_TAG fmt, ##args)

static struct virtual_sensor_init_info camera_protecthub_init_info;

// for motor down if free fall
#ifdef CONFIG_OPLUS_MOTOR
extern void oplus_motor_downward(void);
#endif


static int camera_protect_open_report_data(int open)
{
    return 0;
}

static int camera_protect_enable_nodata(int en)
{
    CAMERA_PROTECT_LOG("camera_protect enable nodata, en = %d\n", en);

    return oplus_enable_to_hub(ID_CAMERA_PROTECT, en);
}

static int camera_protect_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    unsigned int delayms = 0;

    delayms = delay / 1000 / 1000;
    return oplus_set_delay_to_hub(ID_CAMERA_PROTECT, delayms);
#elif defined CONFIG_NANOHUB
    return 0;
#else
    return 0;
#endif
}

static int camera_protect_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    camera_protect_set_delay(samplingPeriodNs);
#endif

    CAMERA_PROTECT_LOG("camera_protect: samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n",
        samplingPeriodNs,
        maxBatchReportLatencyNs);

    return oplus_batch_to_hub(ID_CAMERA_PROTECT, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int camera_protect_flush(void)
{
    return oplus_flush_to_hub(ID_CAMERA_PROTECT);
}

static int camera_protect_data_report(struct data_unit_t *input_event)
{
    struct oplus_sensor_event event;

    memset(&event, 0, sizeof(struct oplus_sensor_event));

    event.handle = ID_CAMERA_PROTECT;
    event.flush_action = DATA_ACTION;
    event.time_stamp = (int64_t)input_event->time_stamp;
    event.word[0] = input_event->oplus_data_t.camera_protect_data_t.value;
    event.word[1] = input_event->oplus_data_t.camera_protect_data_t.report_count;
    return virtual_sensor_data_report(event);
}

static int camera_protect_flush_report()
{
    return virtual_sensor_flush_report(ID_CAMERA_PROTECT);
}

static int camera_protect_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;

    CAMERA_PROTECT_LOG("camera_protect recv data, flush_action = %d, value = %d, report_count = %d, timestamp = %lld\n",
        event->flush_action, event->oplus_data_t.camera_protect_data_t.value,
        event->oplus_data_t.camera_protect_data_t.report_count,
        (int64_t)event->time_stamp);

#ifdef CONFIG_OPLUS_MOTOR

    // for motor down if freefall
    if (event->oplus_data_t.camera_protect_data_t.value) {
        oplus_motor_downward();
    }

#endif

    if (event->flush_action == DATA_ACTION) {
        camera_protect_data_report(event);
    } else if (event->flush_action == FLUSH_ACTION) {
        err = camera_protect_flush_report();
    }

    return err;
}

static int camera_protecthub_local_init(void)
{
    struct virtual_sensor_control_path ctl = {0};
    int err = 0;

    CAMERA_PROTECT_PR_ERR("camera_protecthub_local_init start.\n");

    ctl.open_report_data = camera_protect_open_report_data;
    ctl.enable_nodata = camera_protect_enable_nodata;
    ctl.set_delay = camera_protect_set_delay;
    ctl.batch = camera_protect_batch;
    ctl.flush = camera_protect_flush;
    ctl.report_data = camera_protect_recv_data;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#if OPLUS_FEATURE_SENSOR_ALGORITHM
    ctl.is_support_wake_lock = true;
#endif
#elif defined CONFIG_NANOHUB
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#if OPLUS_FEATURE_SENSOR_ALGORITHM
    ctl.is_support_wake_lock = true;
#endif
#else
#endif

    err = virtual_sensor_register_control_path(&ctl, ID_CAMERA_PROTECT);

    if (err) {
        CAMERA_PROTECT_PR_ERR("register camera_protect control path err\n");
        goto exit;
    }
#ifdef _OPLUS_SENSOR_HUB_VI
    err = scp_sensorHub_data_registration(ID_CAMERA_PROTECT, camera_protect_recv_data);

    if (err < 0) {
        CAMERA_PROTECT_PR_ERR("SCP_sensorHub_data_registration failed\n");
        goto exit;
    }
#endif


    CAMERA_PROTECT_PR_ERR("camera_protecthub_local_init over.\n");
    return 0;
exit:
    return -1;
}

static int camera_protecthub_local_uninit(void)
{
    return 0;
}

static struct virtual_sensor_init_info camera_protecthub_init_info = {
    .name = "camera_protect_hub",
    .init = camera_protecthub_local_init,
    .uninit = camera_protecthub_local_uninit,
};

static int __init camera_protecthub_init(void)
{
    virtual_sensor_driver_add(&camera_protecthub_init_info, ID_CAMERA_PROTECT);
    return 0;
}

static void __exit camera_protecthub_exit(void)
{
    CAMERA_PROTECT_FUN();
}

module_init(camera_protecthub_init);
module_exit(camera_protecthub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("zhq@oplus.com");

