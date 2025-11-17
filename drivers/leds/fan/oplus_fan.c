// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include <linux/hwmon.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/timer.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/pm.h>

#define MAX_DUTY 100
#define DEFAULT_FAN_PWM_PERIOD_NS 40000
#define DEFAULT_PULSES_PER_REVOLUTION 2
#define MAX_LEVEL_DEFAULT 10
#define DEVICE_ID_HONGYING 0
#define DEVICE_ID_TAIDA 1
#define FAN_STATUS_PERIOD_DEFAULT 60000

static int dbg_rpm = -1;
module_param(dbg_rpm, int, 0644);
MODULE_PARM_DESC(dbg_rpm, "oplus debug fan rpm");

struct oplus_fan_tach {
	int fg_irq_gpio;
	atomic_t pulses;
	unsigned int rpm;
	bool fg_irq_en;
	struct mutex irq_lock;
};

struct fan_rpm_offset_config {
	int temp;
	u32 rpm_offset;
};

struct fan_hw_config {
	int max_level;
	int duty_config[MAX_LEVEL_DEFAULT];
	struct fan_rpm_offset_config *rpm_offset_config;
	int pulses_per_revolution;
	int rpm_offset_count;
};

struct pwm_setting {
	u64	pre_period_ns;
	u64	period_ns;
	u32	duty;
	bool	enabled;
};

struct fan_rpm_table {
	u32	duty;
	u32	rpm;
};

enum fan_status {
	FAN_STATUS_NORMAL = 0,
	FAN_STATUS_BLOCKED = 1,
	FAN_STATUS_DAMAGED = 2,
	FAN_STATUS_RPM_LOW = 3,
};

static const char *const fan_state_names[] = {
	[FAN_STATUS_NORMAL] = "NORMAL",
	[FAN_STATUS_BLOCKED] = "BLOCKED",
	[FAN_STATUS_DAMAGED] = "DAMAGED",
	[FAN_STATUS_RPM_LOW] = "RPM_LOW",
};

static const char *fan_status_string(enum fan_status status)
{
	if (status < 0 || status >= ARRAY_SIZE(fan_state_names))
		return "UNKNOWN";

	return fan_state_names[status];
}

struct oplus_fan_chip {
	struct device *dev;
	struct mutex lock;
	struct led_classdev cdev;
	struct pwm_device *pwm_dev;
	struct pwm_setting pwm_setting;
	struct fan_hw_config *hw_config;
	int device_count;
	int device_id;
	int level;
	struct regulator *reg_en;
	int reg_en_gpio;
	bool regulator_enabled;
	bool rpm_timer_enabled;
	bool force_rpm_timer_enabled;
	struct oplus_fan_tach tach;
	ktime_t sample_start;
	struct timer_list rpm_timer;
	struct delayed_work fan_status_work;
	struct delayed_work fan_retry_work;
	int status_check_period;
	bool force_disable_status_work;
	enum fan_status status;
	struct fan_rpm_table rpm_table[MAX_LEVEL_DEFAULT];
	bool rpm_table_initialized;
	bool fan_status_checking;
	bool fan_state_retrying;
	bool state_changed;
	struct thermal_zone_device *shell_themal;
};

static struct fan_hw_config default_hw_config = {
	.max_level = MAX_LEVEL_DEFAULT,
	.duty_config = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100},
	.pulses_per_revolution = DEFAULT_PULSES_PER_REVOLUTION,
	.rpm_offset_config = NULL,
};

/* This handler assumes self resetting edge triggered interrupt. */
static irqreturn_t pulse_handler(int irq, void *dev_id)
{
	struct oplus_fan_tach *tach = dev_id;

	atomic_inc(&tach->pulses);

	return IRQ_HANDLED;
}

static void sample_timer(struct timer_list *t)
{
	struct oplus_fan_chip *chip = from_timer(chip, t, rpm_timer);
	struct oplus_fan_tach *tach = &chip->tach;
	unsigned int delta = ktime_ms_delta(ktime_get(), chip->sample_start);
	int pulses;
	int pulses_per_rev;

	pulses_per_rev = chip->hw_config[chip->device_id].pulses_per_revolution;
	if (pulses_per_rev <= 0) {
		dev_err(chip->dev, "error pulses_per_revolution = %d\n", pulses_per_rev);
		pulses_per_rev = DEFAULT_PULSES_PER_REVOLUTION;
	}

	if (delta) {
		pulses = atomic_read(&tach->pulses);
		atomic_sub(pulses, &tach->pulses);
		tach->rpm = (unsigned int)(pulses * 1000 * 60) / (pulses_per_rev * delta);
		chip->sample_start = ktime_get();
	}

	if (!chip->rpm_timer_enabled) {
		tach->rpm = 0;
		dev_err(chip->dev, "sample_timer stop\n");
		return;
	}

	dev_err(chip->dev, "sample_timer:delta=%u ms, duty=%u, pulses=%d, rpm=%u\n",
			delta, chip->pwm_setting.duty, pulses, tach->rpm);

	if (dbg_rpm >= 0) {
		tach->rpm = dbg_rpm;
		dev_err(chip->dev, "dbg_rpm != 0, force rpm=%u\n", tach->rpm);
	}

	mod_timer(&chip->rpm_timer, jiffies + HZ);
}

static void oplus_fan_fg_irq_config(struct oplus_fan_chip *chip,
						bool enabled)
{
	if (!chip)
		return;

	if (!gpio_is_valid(chip->tach.fg_irq_gpio))
		return;

	mutex_lock(&chip->tach.irq_lock);
	if (!chip->tach.fg_irq_en && enabled) {
		enable_irq(gpio_to_irq(chip->tach.fg_irq_gpio));
		chip->tach.fg_irq_en = true;
	} else if (chip->tach.fg_irq_en && !enabled) {
		disable_irq_nosync(gpio_to_irq(chip->tach.fg_irq_gpio));
		chip->tach.fg_irq_en = false;
	}
	mutex_unlock(&chip->tach.irq_lock);
}

static void oplus_fan_sample_timer_enable(struct oplus_fan_chip *chip,
						bool enabled)
{
	int pulses;

	if (!chip)
		return;

	if (chip->fan_status_checking) {
		dev_err(chip->dev, "fan_status_work checking, don't allow change sample_timer state\n");
		return;
	}

	if (chip->force_rpm_timer_enabled && !enabled) {
		dev_err(chip->dev, "force_rpm_timer_enabled, don't disable sample_timer\n");
		return;
	}

	chip->rpm_timer_enabled = enabled;
	dev_err(chip->dev, "sample_timer enable = %d\n", enabled);

	if (enabled) {
		oplus_fan_fg_irq_config(chip, true);
		pulses = atomic_read(&chip->tach.pulses);
		atomic_sub(pulses, &chip->tach.pulses);
		chip->sample_start = ktime_get();
		mod_timer(&chip->rpm_timer, jiffies + HZ);
	} else {
		oplus_fan_fg_irq_config(chip, false);
	}
}

#define DEFAULT_SHELL_TEMP 25
static int oplus_fan_get_shell_temp(struct oplus_fan_chip *chip)
{
	int shell_temp;
	struct thermal_zone_device *tmp_shell_themal = NULL;
	int rc;

	if (chip->shell_themal == NULL) {
		tmp_shell_themal = thermal_zone_get_zone_by_name("shell_back");
		if (IS_ERR(tmp_shell_themal)) {
			dev_err(chip->dev, "Can't get shell_back\n");
			tmp_shell_themal = NULL;
		}
		chip->shell_themal = tmp_shell_themal;
	}

	if (IS_ERR_OR_NULL(chip->shell_themal)) {
		shell_temp = DEFAULT_SHELL_TEMP;
	} else {
		rc = thermal_zone_get_temp(chip->shell_themal, &shell_temp);
		if (rc) {
			dev_err(chip->dev, "thermal_zone_get_temp get error");
			shell_temp = DEFAULT_SHELL_TEMP;
		} else {
			shell_temp = shell_temp / 100;
		}
	}

	dev_err(chip->dev, "shell_back temp = %d", shell_temp);

	return shell_temp;
}

static u32 oplus_fan_get_target_rpm(struct oplus_fan_chip *chip, u32 duty)
{
	int i;

	for (i = 0; i < MAX_LEVEL_DEFAULT; i++) {
		if (duty == chip->rpm_table[i].duty)
			return chip->rpm_table[i].rpm;
	}

	return 0;
}

#define FAN_RPM_OFFSET_DEFAULT 1000
static u32 oplus_fan_get_rpm_offset(struct oplus_fan_chip *chip, int temp)
{
	struct fan_hw_config *config = &chip->hw_config[chip->device_id];
	int i;

	if (config->rpm_offset_count <= 0)
		return FAN_RPM_OFFSET_DEFAULT;

	for (i = 0; i < config->rpm_offset_count; i++) {
		if (temp < config->rpm_offset_config[i].temp)
			return config->rpm_offset_config[i].rpm_offset;
	}

	return FAN_RPM_OFFSET_DEFAULT;
}

static bool oplus_fan_check_duty_support(struct oplus_fan_chip *chip, u32 duty)
{
	int i;

	if (duty == 0)
		return false;

	for (i = 0; i < MAX_LEVEL_DEFAULT; i++) {
		if (duty == chip->rpm_table[i].duty)
			return true;
	}

	return false;
}

static bool oplus_fan_check_status_work_needed(struct oplus_fan_chip *chip)
{
	if (!chip->rpm_table_initialized) {
		dev_err(chip->dev, "rpm_table is not initialized\n");
		return false;
	}

	if (chip->force_disable_status_work) {
		dev_err(chip->dev, "force disabled\n");
		return false;
	}

	if (!chip->regulator_enabled || !chip->pwm_setting.enabled) {
		dev_err(chip->dev, "fan state is disabled\n");
		return false;
	}

	return true;
}

#define DAMAGED_RPM_THRESHOLD 0
#define RPM_LOW_THRESHOLD 3000
#define MAX_EVENT_PARAM 6
static void oplus_fan_status_work(struct work_struct *work)
{
	struct oplus_fan_chip *chip = container_of(work, struct oplus_fan_chip,
			fan_status_work.work);
	char *fan_env[MAX_EVENT_PARAM] = {0};
	u32 current_duty;
	u32 current_rpm;
	u32 target_rpm;
	u32 rpm_offset;
	int shell_temp;
	bool duty_support;
	int index = 0;
	int i;

	if (!oplus_fan_check_status_work_needed(chip)) {
		dev_err(chip->dev, "fan_status_work:don't need check, return\n");
		return;
	}

	current_duty = chip->pwm_setting.duty;
	oplus_fan_sample_timer_enable(chip, true);
	chip->fan_status_checking = true;
	msleep(1200);
	chip->fan_status_checking = false;
	oplus_fan_sample_timer_enable(chip, false);

	if (!chip->regulator_enabled || !chip->pwm_setting.enabled) {
		dev_err(chip->dev, "fan_status_work: fan state changed, return\n");
		return;
	}

	if (current_duty != chip->pwm_setting.duty) {
		dev_err(chip->dev, "fan_status_work:duty changed, skip this check\n");
		goto next_check;
	}

	current_rpm = chip->tach.rpm;

	duty_support = oplus_fan_check_duty_support(chip, current_duty);
	if (duty_support) {
		target_rpm = oplus_fan_get_target_rpm(chip, current_duty);
		if (target_rpm == 0) {
			dev_err(chip->dev, "fan_status_work:target_rpm = 0, return\n");
			return;
		}
	} else {
		target_rpm = 0;
		dev_err(chip->dev, "duty=%d not support in rpm_table\n", current_duty);
	}

	shell_temp = oplus_fan_get_shell_temp(chip);
	rpm_offset = oplus_fan_get_rpm_offset(chip, shell_temp);

	dev_err(chip->dev, "fan_status_work: rpm=%u, level=%d, duty=%u target_rpm=%u, rpm_offset=%u\n",
			current_rpm, chip->level, current_duty, target_rpm, rpm_offset);

	if (duty_support && current_rpm > target_rpm + rpm_offset) {
		chip->status = FAN_STATUS_BLOCKED;
	} else if (current_rpm == DAMAGED_RPM_THRESHOLD) {
		chip->status = FAN_STATUS_DAMAGED;
	} else if (duty_support && current_rpm < target_rpm + rpm_offset - RPM_LOW_THRESHOLD) {
		chip->status = FAN_STATUS_RPM_LOW;
	} else {
		chip->status = FAN_STATUS_NORMAL;
	}

	fan_env[index++] = kasprintf(GFP_KERNEL, "FAN_STATE=%s", fan_status_string(chip->status));
	fan_env[index++] = kasprintf(GFP_KERNEL, "FAN_DUTY=%u", current_duty);
	fan_env[index++] = kasprintf(GFP_KERNEL, "FAN_RPM=%u", current_rpm);
	fan_env[index++] = kasprintf(GFP_KERNEL, "FAN_TARGET_RPM=%u", target_rpm);
	fan_env[index++] = kasprintf(GFP_KERNEL, "FAN_RPM_OFFSET=%u", rpm_offset);
	fan_env[index++] = NULL;

	if (kobject_uevent_env(&chip->cdev.dev->kobj, KOBJ_CHANGE, fan_env))
		dev_err(chip->dev, "Failed to send fan status uevent\n");
	else
		dev_err(chip->dev, "sent uevent %s\n", fan_status_string(chip->status));

	for (i = 0; i < index - 1; i++)
		kfree(fan_env[i]);

next_check:
	schedule_delayed_work(&chip->fan_status_work, msecs_to_jiffies(chip->status_check_period));
}

static int oplus_fan_parse_hw_config(struct oplus_fan_chip *chip)
{
	struct device_node *np = chip->dev->of_node;
	struct device_node *temp;
	struct fan_hw_config *config;
	int buf[64] = {0};
	int ret;
	int count;
	int i = 0;
	int j;

	count = of_get_child_count(np);
	if (count < 1) {
		dev_err(chip->dev, "don't have hw config\n");
		goto parse_err;
	}

	chip->device_count = count;
	chip->hw_config = devm_kcalloc(chip->dev,
			count, sizeof(struct fan_hw_config), GFP_KERNEL);
	if (!chip->hw_config) {
		dev_err(chip->dev, "failed to kcalloc memory\n");
		goto parse_err;
	} else {
		memset(chip->hw_config, 0, count * sizeof(struct fan_hw_config));
	}

	for_each_child_of_node(np, temp) {
		config = &chip->hw_config[i];
		ret = of_property_read_u32(temp, "pulses-per-revolution",
				&config->pulses_per_revolution);
		if (ret < 0) {
			dev_err(chip->dev, "pulses-per-revolution is not set\n");
			goto parse_err;
		}

		count = of_property_count_elems_of_size(temp, "duty-config", sizeof(int));
		if (count > 0 && count <= MAX_LEVEL_DEFAULT) {
			ret = of_property_read_u32_array(temp, "duty-config", (u32 *)config->duty_config, count);
			if (ret) {
				dev_err(chip->dev, "failed to get duty-config ret = %d\n", ret);
				goto parse_err;
			}
			config->max_level = count;
		} else {
			dev_err(chip->dev, "failed to get duty-config count = %d\n", count);
			goto parse_err;
		}

		count = of_property_count_elems_of_size(temp, "rpm-offset-config", sizeof(int));
		if (count > 0 && count % 2 == 0) {
			ret = of_property_read_u32_array(temp, "rpm-offset-config", (u32 *)buf, count);
			if (ret) {
				dev_err(chip->dev, "failed to get rpm-offset-config ret = %d\n", ret);
				goto parse_err;
			}
			config->rpm_offset_count = count / 2;
			config->rpm_offset_config = devm_kcalloc(chip->dev,
					config->rpm_offset_count, sizeof(struct fan_rpm_offset_config), GFP_KERNEL);
			if (!config->rpm_offset_config) {
				dev_err(chip->dev, "fail to alloc rpm_offset_config memory\n");
				goto parse_err;
			}

			for (j = 0; j < config->rpm_offset_count; j++) {
				config->rpm_offset_config[j].temp = buf[j * 2 + 0];
				config->rpm_offset_config[j].rpm_offset = buf[j * 2 + 1];
				dev_err(chip->dev, "rpm_offset_config[%d]:temp=%d, rpm_offset=%u\n", j,
						config->rpm_offset_config[j].temp,
						config->rpm_offset_config[j].rpm_offset);
			}
		} else {
			dev_err(chip->dev, "failed to get rpm-offset-config count = %d\n", count);
			goto parse_err;
		}

		dev_err(chip->dev, "parse config[%d] pulses_per_revolution = %d\n", i, config->pulses_per_revolution);
		dev_err(chip->dev, "duty_config = %d,%d,%d,%d,%d, %d,%d,%d,%d,%d\n",
				config->duty_config[0], config->duty_config[1], config->duty_config[2],
				config->duty_config[3], config->duty_config[4], config->duty_config[5],
				config->duty_config[6], config->duty_config[7], config->duty_config[8],
				config->duty_config[9]);
		i++;
	}

	return 0;

parse_err:
	if (chip->hw_config)
		devm_kfree(chip->dev, chip->hw_config);
	return -1;
}


static int oplus_fan_parse_dt(struct oplus_fan_chip *chip)
{
	struct device_node *np = chip->dev->of_node;
	struct pwm_args pargs;
	struct oplus_fan_tach *tach = &chip->tach;
	int irq_flags;
	int ret;

	chip->reg_en = NULL;
	chip->reg_en_gpio = of_get_named_gpio(np, "reg-en-gpio", 0);
	if (gpio_is_valid(chip->reg_en_gpio)) {
		ret = devm_gpio_request_one(chip->dev, chip->reg_en_gpio,
					    GPIOF_OUT_INIT_LOW, "fan_reg_en");
		if (ret)
			dev_err(chip->dev, "failed to request reg-en-gpio, ret=%d\n", ret);
		else
			dev_err(chip->dev, "request reg-en-gpio = %d\n", chip->reg_en_gpio);
	} else {
		dev_err(chip->dev, "failed to get reg-en-gpio, try fan regulator\n");
		chip->reg_en = devm_regulator_get_optional(chip->dev, "fan");
		if (IS_ERR(chip->reg_en)) {
			dev_err(chip->dev, "failed to get fan regulator, ret=%ld\n",
					PTR_ERR(chip->reg_en));
			chip->reg_en = NULL;
		}
	}

	tach->fg_irq_gpio = of_get_named_gpio(np, "fg-irq-gpio", 0);
	if (gpio_is_valid(tach->fg_irq_gpio)) {
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(chip->dev,
						gpio_to_irq(tach->fg_irq_gpio),
						NULL, pulse_handler, irq_flags,
						"fan_fg", tach);
		if (ret != 0) {
			dev_err(chip->dev, "Failed to request irq: %d, ret=%d\n",
					gpio_to_irq(tach->fg_irq_gpio), ret);
			return ret;
		}
		disable_irq_nosync(gpio_to_irq(tach->fg_irq_gpio));
		tach->fg_irq_en = false;
	}
	dev_err(chip->dev, "tach: fg_irq_gpio=%d\n", tach->fg_irq_gpio);

	chip->pwm_dev = devm_pwm_get(chip->dev, NULL);
	if (IS_ERR(chip->pwm_dev)) {
		dev_err(chip->dev, "failed to get pwm device, ret=%ld\n",
				PTR_ERR(chip->pwm_dev));
		chip->pwm_dev = NULL;
	}

	if (chip->pwm_dev) {
		pwm_get_args(chip->pwm_dev, &pargs);
		if (pargs.period == 0)
			chip->pwm_setting.pre_period_ns = DEFAULT_FAN_PWM_PERIOD_NS;
		else
			chip->pwm_setting.pre_period_ns = pargs.period;
		dev_err(chip->dev, "pwm setting pre_period_ns = %llu, ret=%d\n",
				chip->pwm_setting.pre_period_ns, ret);
	}

	return 0;
}

static int oplus_fan_set_pwm(struct oplus_fan_chip *chip)
{
	struct pwm_setting *pwm = &chip->pwm_setting;
	struct pwm_state pstate;
	int ret;

	if (!chip->pwm_dev)
		return 0;

	mutex_lock(&chip->lock);
	pwm_get_state(chip->pwm_dev, &pstate);
	pstate.enabled = pwm->enabled;
	pstate.period = pwm->period_ns;
	pstate.duty_cycle = DIV_ROUND_UP(pwm->duty * (pwm->period_ns - 1), MAX_DUTY);
	dev_err(chip->dev, "configure PWM:enabled=%d, period=%llu, duty=%u, duty_cycle=%llu\n",
			pstate.enabled, pstate.period, pwm->duty, pstate.duty_cycle);

	ret = pwm_apply_state(chip->pwm_dev, &pstate);
	if (ret)
		dev_err(chip->dev, "Failed to configure PWM: %d\n", ret);
	mutex_unlock(&chip->lock);

	return ret;
}

static int oplus_fan_regulator_set(struct oplus_fan_chip *chip, bool enable)
{
	int ret;

	if (enable) {
		if (gpio_is_valid(chip->reg_en_gpio)) {
			gpio_set_value_cansleep(chip->reg_en_gpio, 1);
			usleep_range(2000, 4000);
		} else if (chip->reg_en) {
			ret = regulator_enable(chip->reg_en);
			if (ret)
				dev_err(chip->dev, "failed to enable regulator ret = %d.\n", ret);
		}
	} else {
		if (gpio_is_valid(chip->reg_en_gpio)) {
			gpio_set_value_cansleep(chip->reg_en_gpio, 0);
			usleep_range(2000, 4000);
		} else if (chip->reg_en) {
			ret = regulator_disable(chip->reg_en);
			if (ret)
				dev_err(chip->dev, "failed to disable regulator ret = %d.\n", ret);
		}
	}
	chip->regulator_enabled = enable;
	dev_err(chip->dev, "fan regulator enabled = %d\n", enable);

	return 0;
}

static void oplus_fan_enable(struct oplus_fan_chip *chip, bool enabled)
{
	if (!chip)
		return;

	chip->state_changed = false;

	if (enabled) {
		oplus_fan_regulator_set(chip, true);
		msleep(100);
		chip->pwm_setting.enabled = true;
		oplus_fan_set_pwm(chip);
		oplus_fan_sample_timer_enable(chip, true);
		cancel_delayed_work_sync(&chip->fan_status_work);
		if (chip->fan_state_retrying)
			schedule_delayed_work(&chip->fan_retry_work, msecs_to_jiffies(200));
		else
			schedule_delayed_work(&chip->fan_status_work, msecs_to_jiffies(2000));
	} else {
		chip->pwm_setting.enabled = false;
		oplus_fan_set_pwm(chip);
		msleep(100);
		oplus_fan_regulator_set(chip, false);
		oplus_fan_sample_timer_enable(chip, false);
		cancel_delayed_work_sync(&chip->fan_status_work);
	}

	return;
}

#define DEFAULT_RETRY_COUNT 3
#define FAN_RETRY_RPM_THRESHOLD 100
static void oplus_fan_retry_work(struct work_struct *work)
{
	struct oplus_fan_chip *chip = container_of(work, struct oplus_fan_chip,
			fan_retry_work.work);
	int i;
	int count = 0;

	for (i = 0; i < DEFAULT_RETRY_COUNT; i++) {
		if (chip->state_changed) {
			dev_err(chip->dev, "fan state_changed\n");
			break;
		}

		if (!chip->regulator_enabled || !chip->pwm_setting.enabled) {
			dev_err(chip->dev, "fan state is disabled\n");
			break;
		}

		oplus_fan_sample_timer_enable(chip, true);
		chip->fan_status_checking = true;
		msleep(1200);
		chip->fan_status_checking = false;
		oplus_fan_sample_timer_enable(chip, false);

		if (!chip->regulator_enabled || !chip->pwm_setting.enabled) {
			dev_err(chip->dev, "fan state is disabled\n");
			break;
		}

		if (chip->tach.rpm < FAN_RETRY_RPM_THRESHOLD) {
			count++;
		} else {
			break;
		}
	}

	chip->fan_state_retrying = false;

	if (count == DEFAULT_RETRY_COUNT) {
		dev_err(chip->dev, "fan status abnormal, retry fan enable\n");
		oplus_fan_enable(chip, false);
		msleep(20);
		oplus_fan_enable(chip, true);
	} else {
		dev_err(chip->dev, "fan status normal\n");
		schedule_delayed_work(&chip->fan_status_work, msecs_to_jiffies(2000));
	}
}

static int oplus_fan_init(struct oplus_fan_chip *chip)
{
	chip->pwm_setting.period_ns = chip->pwm_setting.pre_period_ns;
	chip->pwm_setting.duty = MAX_DUTY;
	chip->pwm_setting.enabled = false;
	chip->level = 0;
	chip->status_check_period = FAN_STATUS_PERIOD_DEFAULT;
	oplus_fan_set_pwm(chip);

	return 0;
}

static ssize_t speed_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
		container_of(fan_cdev, struct oplus_fan_chip, cdev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", chip->pwm_setting.duty);
}

static ssize_t speed_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
		container_of(fan_cdev, struct oplus_fan_chip, cdev);
	int rc;
	u32 speed;

	rc = kstrtouint(buf, 0, &speed);
	if (rc < 0)
		return rc;

	if (speed > MAX_DUTY)
		speed = MAX_DUTY;

	chip->level = 0;
	chip->pwm_setting.duty = speed;
	oplus_fan_set_pwm(chip);

	return count;
}
static DEVICE_ATTR_RW(speed);

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
		container_of(fan_cdev, struct oplus_fan_chip, cdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chip->pwm_setting.enabled);
}

static ssize_t state_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
		container_of(fan_cdev, struct oplus_fan_chip, cdev);
	int rc;
	bool enabled;

	rc = kstrtobool(buf, &enabled);
	if (rc < 0)
		return rc;
	dev_err(chip->dev, "state_store = %d\n", enabled);

	chip->state_changed = true;
	cancel_delayed_work_sync(&chip->fan_retry_work);
	chip->fan_state_retrying = enabled;
	oplus_fan_enable(chip, enabled);

	return count;
}
static DEVICE_ATTR_RW(state);

static ssize_t rpm_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
		container_of(fan_cdev, struct oplus_fan_chip, cdev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", chip->tach.rpm);
}

static ssize_t rpm_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
		container_of(fan_cdev, struct oplus_fan_chip, cdev);
	bool enabled;
	int rc;

	rc = kstrtobool(buf, &enabled);
	if (rc < 0)
		return rc;

	chip->force_rpm_timer_enabled = enabled;
	dev_err(chip->dev, "rpm_store force_rpm_timer_enabled = %d\n", enabled);
	oplus_fan_sample_timer_enable(chip, enabled);

	return count;
}
static DEVICE_ATTR_RW(rpm);

static ssize_t level_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
			container_of(fan_cdev, struct oplus_fan_chip, cdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chip->level);
}

static ssize_t level_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
			container_of(fan_cdev, struct oplus_fan_chip, cdev);
	int rc;
	u32 level;

	rc = kstrtouint(buf, 0, &level);
	if (rc < 0)
		return rc;

	if (level <= 0) {
		dev_err(chip->dev, "level range is between 1 and %d\n",
				chip->hw_config[chip->device_id].max_level);
		return count;
	}

	if (level > chip->hw_config[chip->device_id].max_level) {
		dev_err(chip->dev, "set level = %d, out of range, force level = %d\n",
				level, chip->hw_config[chip->device_id].max_level);
		level = chip->hw_config[chip->device_id].max_level;
	}

	chip->level = level;
	chip->pwm_setting.duty = chip->hw_config[chip->device_id].duty_config[level - 1];
	dev_err(chip->dev, "set level=%d, duty=%d\n", level, chip->pwm_setting.duty);

	oplus_fan_set_pwm(chip);

	return count;
}
static DEVICE_ATTR_RW(level);

static ssize_t device_id_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
			container_of(fan_cdev, struct oplus_fan_chip, cdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chip->device_id);
}

static ssize_t device_id_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
			container_of(fan_cdev, struct oplus_fan_chip, cdev);
	int rc;
	u32 val;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	dev_err(chip->dev, "set device_id = %u\n", val);

	if (val >= chip->device_count) {
		val = chip->device_count - 1;
		dev_err(chip->dev, "only %d device, force device_id = %u\n",
				chip->device_count, val);
	}

	chip->device_id = val;
	if (chip->level > 0)
		chip->pwm_setting.duty = chip->hw_config[chip->device_id].duty_config[chip->level - 1];

	oplus_fan_set_pwm(chip);

	return count;
}
static DEVICE_ATTR_RW(device_id);

static ssize_t status_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
			container_of(fan_cdev, struct oplus_fan_chip, cdev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", fan_status_string(chip->status));
}
static DEVICE_ATTR_RO(status);

static ssize_t check_period_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
			container_of(fan_cdev, struct oplus_fan_chip, cdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chip->status_check_period);
}

static ssize_t check_period_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
			container_of(fan_cdev, struct oplus_fan_chip, cdev);
	int rc;
	u32 val;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	dev_err(chip->dev, "set check_period = %u\n", val);

	if (val == 0) {
		chip->force_disable_status_work = true;
		dev_err(chip->dev, "force disable check status work\n");
	} else {
		chip->status_check_period = val;
		chip->force_disable_status_work = false;
	}

	return count;
}
static DEVICE_ATTR_RW(check_period);

static ssize_t rpm_table_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
			container_of(fan_cdev, struct oplus_fan_chip, cdev);
	int i;
	int count = 0;

	for (i = 0; i < MAX_LEVEL_DEFAULT; i++) {
		count += scnprintf(buf + count, PAGE_SIZE, "%u,%u,",
				chip->rpm_table[i].duty, chip->rpm_table[i].rpm);
		dev_err(chip->dev, "rpm_table[%d]:%u,%u\n", i,
				chip->rpm_table[i].duty, chip->rpm_table[i].rpm);
	}

	if (count > 0)
		buf[count - 1] = '\n';

	return count;
}

static ssize_t rpm_table_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct led_classdev *fan_cdev = dev_get_drvdata(dev);
	struct oplus_fan_chip *chip =
			container_of(fan_cdev, struct oplus_fan_chip, cdev);
	char buffer[128] = {0};
	u32 data[MAX_LEVEL_DEFAULT * 2] = {0};
	char *str = buffer;
	int val;
	int cnt = 0;
	int i;

	if (count > sizeof(buffer) - 1) {
		dev_err(chip->dev, "rpm_table data length out of range, count=%zu\n", count);
		return -EFAULT;
	}

	memmove(buffer, buf, count);
	dev_err(chip->dev, "rpm_table_store:%s\n", buffer);

	while (*str != '\0' && *str != '\n') {
		if (sscanf(str, "%d", &val) && val != 0) {
			data[cnt++] = val;
			str = strstr(str, ",");
			if (!str)
				break;
			else
				str++;
			if (cnt == MAX_LEVEL_DEFAULT * 2)
				break;
		} else {
			dev_err(chip->dev, "invalid rpm_table data, buffer=%s\n", buffer);
			return -EFAULT;
		}
	}

	if (cnt % 2) {
		dev_err(chip->dev, "invalid rpm_table data count, buffer=%s\n", buffer);
		return -EFAULT;
	} else {
		/* all duty param must be lower than MAX_DUTY */
		for (i = 0; i < cnt; i += 2) {
			if (data[i] > MAX_DUTY) {
				dev_err(chip->dev, "duty param %u is invalid, buffer=%s\n", data[i], buffer);
				return -EFAULT;
			}
		}

		/* all rpm param can't be zero */
		for (i = 1; i < cnt; i += 2) {
			if (data[i] == 0) {
				dev_err(chip->dev, "rpm param %u is invalid, buffer=%s\n", data[i], buffer);
				return -EFAULT;
			}
		}

		memset(chip->rpm_table, 0 , sizeof(chip->rpm_table));
		dev_err(chip->dev, "rpm_table update count = %d\n", cnt);
		for (i = 0; i < (cnt / 2); i++) {
			chip->rpm_table[i].duty = data[i * 2];
			chip->rpm_table[i].rpm = data[i * 2 + 1];
			dev_err(chip->dev, "rpm_table[%d]:%u,%u\n", i, data[i * 2], data[i * 2 + 1]);
		}
		chip->rpm_table_initialized = true;
	}

	return count;
}
static DEVICE_ATTR_RW(rpm_table);

static struct attribute *oplus_fan_attrs[] = {
	&dev_attr_speed.attr,
	&dev_attr_state.attr,
	&dev_attr_rpm.attr,
	&dev_attr_level.attr,
	&dev_attr_device_id.attr,
	&dev_attr_status.attr,
	&dev_attr_check_period.attr,
	&dev_attr_rpm_table.attr,
	NULL
};

static struct attribute_group oplus_fan_attrs_group = {
	.attrs = oplus_fan_attrs
};

static void oplus_fan_set_brightness(struct led_classdev *fan_cdev,
		enum led_brightness brightness)
{
	struct oplus_fan_chip *chip =
		container_of(fan_cdev, struct oplus_fan_chip, cdev);

	dev_err(chip->dev, "fan class set brightness, brightness=%d\n", brightness);
}

static enum led_brightness oplus_fan_get_brightness(
			struct led_classdev *fan_cdev)
{
	struct oplus_fan_chip *chip =
		container_of(fan_cdev, struct oplus_fan_chip, cdev);

	dev_err(chip->dev, "fan class get brightness, brightness=%d\n", fan_cdev->brightness);

	return fan_cdev->brightness;
}

static int oplus_fan_cdev_register(struct oplus_fan_chip *chip)
{
	int rc;

	chip->cdev.name = "fan";
	chip->cdev.max_brightness = LED_FULL;
	chip->cdev.brightness_set = oplus_fan_set_brightness;
	chip->cdev.brightness_get = oplus_fan_get_brightness;
	chip->cdev.brightness = 0;
	rc = devm_led_classdev_register(chip->dev, &chip->cdev);
	if (rc < 0) {
		dev_err(chip->dev, "failed to register fan class, rc=%d\n", rc);
		return rc;
	}

	rc = sysfs_create_group(&chip->cdev.dev->kobj,
				 &oplus_fan_attrs_group);
	if (rc < 0) {
		dev_err(chip->dev, "failed to create sysfs attrs, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static void oplus_fan_cleanup(void *__ctx)
{
	struct oplus_fan_chip *chip = __ctx;

	del_timer_sync(&chip->rpm_timer);
	/* Switch off everything */
	oplus_fan_enable(chip, false);
}

static int oplus_fan_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct oplus_fan_chip *chip;
	int ret;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	mutex_init(&chip->lock);
	mutex_init(&chip->tach.irq_lock);
	chip->dev = &pdev->dev;
	platform_set_drvdata(pdev, chip);

	ret = oplus_fan_parse_dt(chip);
	if (ret)
		return ret;

	ret = oplus_fan_parse_hw_config(chip);
	if (ret) {
		dev_err(chip->dev, "use default hw config\n");
		chip->device_count = 1;
		chip->hw_config = devm_kzalloc(chip->dev,
				sizeof(struct fan_hw_config), GFP_KERNEL);
		if (!chip->hw_config) {
			dev_err(chip->dev, "failed to kzalloc memory\n");
			return ret;
		}
		memmove(chip->hw_config, &default_hw_config, sizeof(struct fan_hw_config));
	}
	chip->device_id = DEVICE_ID_HONGYING;

	INIT_DELAYED_WORK(&chip->fan_status_work, oplus_fan_status_work);
	INIT_DELAYED_WORK(&chip->fan_retry_work, oplus_fan_retry_work);
	timer_setup(&chip->rpm_timer, sample_timer, 0);
	ret = devm_add_action_or_reset(dev, oplus_fan_cleanup, chip);
	if (ret)
		return ret;

	oplus_fan_cdev_register(chip);
	oplus_fan_init(chip);

	dev_err(chip->dev, "probe complete!\n");
	return 0;
}

static int oplus_fan_remove(struct platform_device *pdev)
{
	struct oplus_fan_chip *chip = platform_get_drvdata(pdev);

	if (gpio_is_valid(chip->reg_en_gpio))
		gpio_free(chip->reg_en_gpio);

	if (gpio_is_valid(chip->tach.fg_irq_gpio))
		free_irq(gpio_to_irq(chip->tach.fg_irq_gpio), &chip->tach);

	return 0;
}

static void oplus_fan_shutdown(struct platform_device *pdev)
{
	struct oplus_fan_chip *chip = platform_get_drvdata(pdev);

	oplus_fan_cleanup(chip);
}

static int oplus_fan_suspend(struct device *dev)
{
	struct oplus_fan_chip *chip = dev_get_drvdata(dev);

	dev_err(chip->dev, "fan suspend\n");

	return 0;
}

static int oplus_fan_resume(struct device *dev)
{
	struct oplus_fan_chip *chip = dev_get_drvdata(dev);

	dev_err(chip->dev, "fan resume\n");

	return 0;
}

static const struct dev_pm_ops oplus_fan_pm_ops = {
	.suspend = oplus_fan_suspend,
	.resume = oplus_fan_resume,
};

static const struct of_device_id of_oplus_fan_match[] = {
	{ .compatible = "oplus,pwm-fan", },
	{},
};
MODULE_DEVICE_TABLE(of, of_oplus_fan_match);

static struct platform_driver oplus_fan_driver = {
	.probe		= oplus_fan_probe,
	.remove		= oplus_fan_remove,
	.shutdown	= oplus_fan_shutdown,
	.driver	= {
		.name		= "pwm-fan",
		.pm		= &oplus_fan_pm_ops,
		.of_match_table	= of_oplus_fan_match,
	},
};

module_platform_driver(oplus_fan_driver);

MODULE_ALIAS("platform:pwm-fan");
MODULE_DESCRIPTION("PWM FAN driver");
MODULE_LICENSE("GPL");
