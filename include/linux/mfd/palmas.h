/*
 * TI Palmas
 *
 * Copyright 2011 Texas Instruments Inc.
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 */

#ifndef __LINUX_MFD_PALMAS_H
#define __LINUX_MFD_PALMAS_H

#include <linux/usb/otg.h>
#include <linux/leds.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/kthread.h>
#include <linux/iio/machine.h>

#define PALMAS_NUM_CLIENTS	4

/* Fuel Gauge Constatnts */
#define MAX_CAPACITY	0x7fff
#define MAX_SOC		100
#define MAX_PERCENTAGE	100

/* Num, cycles with no Learning, after this many cycles, the gauge
   start adjusting FCC, based on Estimated Cell Degradation */
#define NO_LEARNING_CYCLES	25

/* Size of the OCV Lookup table */
#define OCV_TABLE_SIZE	21

/* OCV Configuration */
struct ocv_config {
	unsigned char voltage_diff;
	unsigned char current_diff;

	unsigned short sleep_enter_current;
	unsigned char sleep_enter_samples;

	unsigned short sleep_exit_current;
	unsigned char sleep_exit_samples;

	unsigned short long_sleep_current;

	unsigned int ocv_period;
	unsigned int relax_period;

	unsigned char flat_zone_low;
	unsigned char flat_zone_high;

	unsigned short max_ocv_discharge;

	unsigned short table[OCV_TABLE_SIZE];
};

/* EDV Point */
struct edv_point {
	short voltage;
	unsigned char percent;
};

/* EDV Point tracking data */
struct edv_state {
	short voltage;
	unsigned char percent;
	short min_capacity;
	unsigned char edv_cmp;
};

/* EDV Configuration */
struct edv_config {
	bool averaging;

	unsigned char seq_edv;

	unsigned char filter_light;
	unsigned char filter_heavy;
	short overload_current;

	struct edv_point edv[3];
};

/* General Battery Cell Configuration */
struct cell_config {
	int technology;
	bool cc_polarity;
	bool cc_out;
	bool ocv_below_edv1;

	short cc_voltage;
	short cc_current;
	unsigned char cc_capacity;
	unsigned char seq_cc;

	unsigned short design_capacity;
	short design_qmax;

	unsigned char r_sense;

	unsigned char qmax_adjust;
	unsigned char fcc_adjust;

	unsigned short max_overcharge;
	unsigned short electronics_load; /* *10uAh */

	short max_increment;
	short max_decrement;
	unsigned char low_temp;
	unsigned short deep_dsg_voltage;
	unsigned short max_dsg_estimate;
	unsigned char light_load;
	unsigned short near_full;
	unsigned short cycle_threshold;
	unsigned short recharge;

	unsigned char mode_switch_capacity;

	unsigned char call_period;

	struct ocv_config *ocv;
	struct edv_config *edv;
};

/* Cell State */
struct cell_state {
	short soc;

	short nac;

	short fcc;
	short qmax;

	short voltage;
	short av_voltage;
	short cur;
	short av_current;

	short temperature;
	short cycle_count;

	bool sleep;
	bool relax;

	bool chg;
	bool dsg;

	bool edv0;
	bool edv1;
	bool edv2;
	bool ocv;
	bool cc;
	bool full;

	bool vcq;
	bool vdq;
	bool init;

	struct timeval last_correction;
	struct timeval last_ocv;
	struct timeval sleep_timer;
	struct timeval el_timer;
	unsigned int cumulative_sleep;

	short prev_soc;
	short learn_q;
	unsigned short dod_eoc;
	short learn_offset;
	unsigned short learned_cycle;
	short new_fcc;
	short ocv_total_q;
	short ocv_enter_q;
	short negative_q;
	short overcharge_q;
	short charge_cycle_q;
	short discharge_cycle_q;
	short cycle_q;
	short top_off_q;
	unsigned char seq_cc_voltage;
	unsigned char seq_cc_current;
	unsigned char sleep_samples;
	unsigned char seq_edvs;

	unsigned int electronics_load;
	unsigned short cycle_dsg_estimate;

	struct edv_state edv;

	bool updated;
	bool calibrate;

	struct cell_config *config;
	struct device *dev;

	int *charge_status;
};

struct palmas_pmic;
struct palmas_rtc;
struct palmas_battery_info;

#define palmas_rails(_name) "palmas_"#_name

struct palmas {
	struct device *dev;

	struct i2c_client *i2c_clients[PALMAS_NUM_CLIENTS];
	struct regmap *regmap[PALMAS_NUM_CLIENTS];

	/* Stored chip id */
	int id;

	/* IRQ Data */
	int irq;
	u32 irq_mask;
	struct palmas_irq_chip_data *irq_chip_data;

	/* Child Devices */
	struct palmas_pmic *pmic;
	struct palmas_rtc *rtc;
	struct palmas_battery_info *battery;

	/* GPIO MUXing */
	u8 ngpio;
	u16 gpio_muxed;
	u8 led_muxed;
	u8 pwm_muxed;

	int design_revision;
	int sw_otp_version;
	int es_minor_version;
	int es_major_version;
};

struct palmas_reg_init {
	/* warm_rest controls the voltage levels after a warm reset
	 *
	 * 0: reload default values from OTP on warm reset
	 * 1: maintain voltage from VSEL on warm reset
	 */
	int warm_reset;

	/* roof_floor controls whether the regulator uses the i2c style
	 * of DVS or uses the method where a GPIO or other control method is
	 * attached to the NSLEEP/ENABLE1/ENABLE2 pins
	 *
	 * For SMPS
	 *
	 * 0: i2c selection of voltage
	 * 1: pin selection of voltage.
	 *
	 * For LDO unused
	 */
	int roof_floor;

	/* sleep_mode is the mode loaded to MODE_SLEEP bits as defined in
	 * the data sheet.
	 *
	 * For SMPS
	 *
	 * 0: Off
	 * 1: AUTO
	 * 2: ECO
	 * 3: Forced PWM
	 *
	 * For LDO
	 *
	 * 0: Off
	 * 1: On
	 */
	int mode_sleep;

	/* voltage_sel is the bitfield loaded onto the SMPSX_VOLTAGE
	 * register. Set this is the default voltage set in OTP needs
	 * to be overridden.
	 */
	u8 vsel;

	/* Configuration flags */
	unsigned int config_flags;

	/*
	 * tracking_regulator will tell which regulator will be tracked
	 * This will be used when regulator tracking is enabled and
	 * device supports.
	 */
	int tracking_regulator;
};

enum palmas_regulators {
	/* SMPS regulators */
	PALMAS_REG_SMPS12,
	PALMAS_REG_SMPS123,
	PALMAS_REG_SMPS3,
	PALMAS_REG_SMPS45,
	PALMAS_REG_SMPS457,
	PALMAS_REG_SMPS6,
	PALMAS_REG_SMPS7,
	PALMAS_REG_SMPS8,
	PALMAS_REG_SMPS9,
	PALMAS_REG_SMPS10,
	/* LDO regulators */
	PALMAS_REG_LDO1,
	PALMAS_REG_LDO2,
	PALMAS_REG_LDO3,
	PALMAS_REG_LDO4,
	PALMAS_REG_LDO5,
	PALMAS_REG_LDO6,
	PALMAS_REG_LDO7,
	PALMAS_REG_LDO8,
	PALMAS_REG_LDO9,
	PALMAS_REG_LDO10,
	PALMAS_REG_LDO11,
	PALMAS_REG_LDO12,
	PALMAS_REG_LDO13,
	PALMAS_REG_LDO14,
	PALMAS_REG_LDOLN,
	PALMAS_REG_LDOUSB,
	/* External regulators */
	PALMAS_REG_REGEN1,
	PALMAS_REG_REGEN2,
	PALMAS_REG_REGEN3,
	PALMAS_REG_REGEN4,
	PALMAS_REG_REGEN5,
	PALMAS_REG_REGEN7,
	PALMAS_REG_SYSEN1,
	PALMAS_REG_SYSEN2,
	PALMAS_REG_CHARGER_PUMP,
	/* Total number of regulators */
	PALMAS_NUM_REGS,
};

enum palmas_chip_id {
	PALMAS,
	TWL6035,
	TWL6037,
	TPS65913,
	TPS80036,
};

enum PALMAS_CLOCK32K {
	PALMAS_CLOCK32KG,
	PALMAS_CLOCK32KG_AUDIO,

	/* Last entry */
	PALMAS_CLOCK32K_NR,
};

struct palmas_clk32k_init_data {
	int clk32k_id;
	bool enable;
	int sleep_control;
};

struct palmas_dvfs_init_data {
	bool 	en_pwm;
	int	ext_ctrl;
	int	reg_id;
	bool	step_20mV;
	int	base_voltage_uV;
	int 	max_voltage_uV;
	bool	smps3_ctrl;
};

struct palmas_pmic_platform_data {
	/* An array of pointers to regulator init data indexed by regulator
	 * ID
	 */
	struct regulator_init_data *reg_data[PALMAS_NUM_REGS];

	/* An array of pointers to structures containing sleep mode and DVS
	 * configuration for regulators indexed by ID
	 */
	struct palmas_reg_init *reg_init[PALMAS_NUM_REGS];

	/* CL DVFS init data */
	struct palmas_dvfs_init_data *dvfs_init_data;
	int dvfs_init_data_size;

	/* use LDO6 for vibrator control */
	int ldo6_vibrator;

	bool disable_smps10_boost_suspend;

};

struct palmas_vbus_platform_data {
	int num_consumer_supplies;
	struct regulator_consumer_supply *consumer_supplies;
};

struct palmas_bcharger_platform_data {
	const char *battery_tz_name;
	int max_charge_volt_mV;
	int max_charge_current_mA;
	int charging_term_current_mA;
	int wdt_timeout;
	int rtc_alarm_time;
	int num_consumer_supplies;
	struct regulator_consumer_supply *consumer_supplies;
	int chg_restart_time;
	int temperature_poll_period_secs;
};

struct palmas_charger_platform_data {
	struct palmas_vbus_platform_data *vbus_pdata;
	struct palmas_bcharger_platform_data *bcharger_pdata;
};


struct palmas_rtc_platform_data {
	unsigned enable_charging:1;
	unsigned charging_current_ua;
};

/*
 * ADC wakeup property: Wakup the system from suspend when threshold crossed.
 * @adc_channel_number: ADC channel number for monitoring.
 * @adc_high_threshold: ADC High raw data for upper threshold to generate int.
 * @adc_low_threshold: ADC low raw data for lower threshold to generate int.
 */
struct palmas_adc_wakeup_property {
	int adc_channel_number;
	int adc_high_threshold;
	int adc_low_threshold;
};

struct palmas_gpadc_platform_data {
	int channel0_current_uA;
	int channel3_current_uA;

	struct iio_map *iio_maps;
	int auto_conversion_period_ms;
	struct palmas_adc_wakeup_property *adc_wakeup1_data;
	struct palmas_adc_wakeup_property *adc_wakeup2_data;
};

struct palmas_pinctrl_config {
	int pin_name;
	int pin_mux_option;
	int open_drain_state;
	int pin_pull_up_dn;
};

struct palmas_pinctrl_platform_data {
	struct palmas_pinctrl_config *pincfg;
	int num_pinctrl;
	bool dvfs1_enable;
	bool dvfs2_enable;
};

struct palmas_extcon_platform_data {
	const char *connection_name;
	bool enable_vbus_detection;
	bool enable_id_pin_detection;
};

struct palmas_battery_platform_data {
	const char *therm_zone_name;
	/* Battery Values */
	int battery_soldered; /* if battery detection should not be used */
	int battery_status_interval; /* time in ms for charge status polling */
	int *battery_temperature_chart;
	int battery_temperature_chart_size;
	int gpadc_retry_count;


	/* Fuelgauge Config */
	int current_avg_interval;
	struct cell_config *cell_cfg;
	int is_battery_present;
	bool enable_ovc_alarm;
	int ovc_period;
	int ovc_threshold;
};

struct palmas_sim_platform_data {
	unsigned dbcnt:5;
	unsigned pwrdncnt:5;
	unsigned pwrdnen1:1;
	unsigned pwrdnen2:1;
	unsigned det_polarity:1;
	unsigned det1_pu:1;
	unsigned det1_pd:1;
	unsigned det2_pu:1;
	unsigned det2_pd:1;
};

struct palmas_platform_data {
	int gpio_base;
	int irq_base;
	int irq_type;

	/* bit value to be loaded to the POWER_CTRL register */
	u8 power_ctrl;

	struct palmas_pmic_platform_data *pmic_pdata;
	struct palmas_rtc_platform_data *rtc_pdata;
	struct palmas_gpadc_platform_data *adc_pdata;
	struct palmas_battery_platform_data *battery_pdata;
	struct palmas_sim_platform_data *sim_pdata;

	struct palmas_clk32k_init_data  *clk32k_init_data;
	int clk32k_init_data_size;
	bool use_power_off;
	/* LDOUSB is enabled or disabled on VBUS detection */
	bool auto_ldousb_en;

	struct palmas_pinctrl_platform_data *pinctrl_pdata;
	struct palmas_extcon_platform_data *extcon_pdata;
	struct palmas_charger_platform_data *charger_pdata;

	int watchdog_timer_initial_period;

	/* Long press delay for hard shutdown */
	int long_press_delay;

	/* system off type by long press key */
	int poweron_lpk;
};

/* Define the palmas IRQ numbers */
enum palmas_irqs {
	/* INT1 registers */
	PALMAS_CHARG_DET_N_VBUS_OVV_IRQ,
	PALMAS_PWRON_IRQ,
	PALMAS_LONG_PRESS_KEY_IRQ,
	PALMAS_RPWRON_IRQ,
	PALMAS_PWRDOWN_IRQ,
	PALMAS_HOTDIE_IRQ,
	PALMAS_VSYS_MON_IRQ,
	PALMAS_VBAT_MON_IRQ,
	/* INT2 registers */
	PALMAS_RTC_ALARM_IRQ,
	PALMAS_RTC_TIMER_IRQ,
	PALMAS_WDT_IRQ,
	PALMAS_BATREMOVAL_IRQ,
	PALMAS_RESET_IN_IRQ,
	PALMAS_FBI_BB_IRQ,
	PALMAS_SHORT_IRQ,
	PALMAS_VAC_ACOK_IRQ,
	/* INT3 registers */
	PALMAS_GPADC_AUTO_0_IRQ,
	PALMAS_GPADC_AUTO_1_IRQ,
	PALMAS_GPADC_EOC_SW_IRQ,
	PALMAS_GPADC_EOC_RT_IRQ,
	PALMAS_ID_OTG_IRQ,
	PALMAS_ID_IRQ,
	PALMAS_VBUS_OTG_IRQ,
	PALMAS_VBUS_IRQ,
	/* INT4 registers */
	PALMAS_GPIO_0_IRQ,
	PALMAS_GPIO_1_IRQ,
	PALMAS_GPIO_2_IRQ,
	PALMAS_GPIO_3_IRQ,
	PALMAS_GPIO_4_IRQ,
	PALMAS_GPIO_5_IRQ,
	PALMAS_GPIO_6_IRQ,
	PALMAS_GPIO_7_IRQ,
	/* INT5 registers */
	PALMAS_GPIO_8_IRQ,
	PALMAS_GPIO_9_IRQ,
	PALMAS_GPIO_10_IRQ,
	PALMAS_GPIO_11_IRQ,
	PALMAS_GPIO_12_IRQ,
	PALMAS_GPIO_13_IRQ,
	PALMAS_GPIO_14_IRQ,
	PALMAS_GPIO_15_IRQ,
	/* INT6 interrupts */
	PALMAS_CHARGER_IRQ,
	PALMAS_SIM1_IRQ,
	PALMAS_SIM2_IRQ,
	/* INT7 interrupts */
	PALMAS_BAT_TEMP_FAULT_IRQ,
	/* Total Number IRQs */
	PALMAS_NUM_IRQ,
};

struct palmas_pmic {
	struct palmas *palmas;
	struct device *dev;
	struct regulator_desc desc[PALMAS_NUM_REGS];
	struct regulator_dev *rdev[PALMAS_NUM_REGS];
	struct mutex mutex;

	int smps123;
	int smps457;
	bool smps10_regulator_enabled;
	int ldo_vref0p425;

	unsigned int ramp_delay[PALMAS_NUM_REGS];
	bool ramp_delay_support[PALMAS_NUM_REGS];
	unsigned int current_mode_reg[PALMAS_NUM_REGS];

	int range[PALMAS_REG_SMPS10];
	unsigned long roof_floor[PALMAS_NUM_REGS];
	unsigned long config_flags[PALMAS_NUM_REGS];
};

/* defines so we can store the mux settings */
#define PALMAS_GPIO_0_MUXED					(1 << 0)
#define PALMAS_GPIO_1_MUXED					(1 << 1)
#define PALMAS_GPIO_2_MUXED					(1 << 2)
#define PALMAS_GPIO_3_MUXED					(1 << 3)
#define PALMAS_GPIO_4_MUXED					(1 << 4)
#define PALMAS_GPIO_5_MUXED					(1 << 5)
#define PALMAS_GPIO_6_MUXED					(1 << 6)
#define PALMAS_GPIO_7_MUXED					(1 << 7)
#define PALMAS_GPIO_8_MUXED					(1 << 8)
#define PALMAS_GPIO_9_MUXED					(1 << 9)
#define PALMAS_GPIO_10_MUXED					(1 << 10)
#define PALMAS_GPIO_11_MUXED					(1 << 11)
#define PALMAS_GPIO_12_MUXED					(1 << 12)
#define PALMAS_GPIO_13_MUXED					(1 << 13)
#define PALMAS_GPIO_14_MUXED					(1 << 14)
#define PALMAS_GPIO_15_MUXED					(1 << 15)

#define PALMAS_LED1_MUXED					(1 << 0)
#define PALMAS_LED2_MUXED					(1 << 1)

#define PALMAS_PWM1_MUXED					(1 << 0)
#define PALMAS_PWM2_MUXED					(1 << 1)

/* helper macro to get correct slave number */
#define PALMAS_BASE_TO_SLAVE(x)		((x >> 8) - 1)
#define PALMAS_BASE_TO_REG(x, y)	((x & 0xff) + y)
#define RTC_SLAVE			0

/* Base addresses of IP blocks in Palmas */
#define PALMAS_SMPS_DVS_BASE					0x20
#define PALMAS_RTC_BASE						0x100
#define PALMAS_VALIDITY_BASE					0x118
#define PALMAS_SMPS_BASE					0x120
#define PALMAS_LDO_BASE						0x150
#define PALMAS_DVFS_BASE					0x180
#define PALMAS_SIMCARD_BASE					0X19E
#define PALMAS_PMU_CONTROL_BASE					0x1A0
#define PALMAS_RESOURCE_BASE					0x1D4
#define PALMAS_PU_PD_OD_BASE					0x1F0
#define PALMAS_LED_BASE						0x200
#define PALMAS_INTERRUPT_BASE					0x210
#define PALMAS_FUEL_GAUGE_BASE					0x230
#define PALMAS_USB_OTG_BASE					0x250
#define PALMAS_VIBRATOR_BASE					0x270
#define PALMAS_GPIO_BASE					0x280
#define PALMAS_USB_BASE						0x290
#define PALMAS_GPADC_BASE					0x2C0
#define PALMAS_TRIM_GPADC_BASE					0x3CD
#define PALMAS_PAGE3_BASE					0x300
#define PALMAS_CHARGER_BASE					0x400

#define PALMAS_CHARGE_PUMP_CTRL					0x7C
/* Bit definitions for CHARGE_PUMP_CTRL */
#define  PALMAS_PALMAS_CHARGE_PUMP_CTRL_STATUS			0x10
#define PALMAS_CHARGE_PUMP_CTRL_STATUS_SHIFT			4
#define PALMAS_CHARGE_PUMP_CTRL_MODE_SLEEP			0x04
#define PALMAS_CHARGE_PUMP_CTRL_MODE_SLEEP_SHIFT		2
#define PALMAS_CHARGE_PUMP_CTRL_MODE_ACTIVE			0x01
#define PALMAS_CHARGE_PUMP_CTRL_MODE_ACTIVE_SHIFT		0

/* Registers for function RTC */
#define PALMAS_SECONDS_REG					0x0
#define PALMAS_MINUTES_REG					0x1
#define PALMAS_HOURS_REG					0x2
#define PALMAS_DAYS_REG						0x3
#define PALMAS_MONTHS_REG					0x4
#define PALMAS_YEARS_REG					0x5
#define PALMAS_WEEKS_REG					0x6
#define PALMAS_ALARM_SECONDS_REG				0x8
#define PALMAS_ALARM_MINUTES_REG				0x9
#define PALMAS_ALARM_HOURS_REG					0xA
#define PALMAS_ALARM_DAYS_REG					0xB
#define PALMAS_ALARM_MONTHS_REG					0xC
#define PALMAS_ALARM_YEARS_REG					0xD
#define PALMAS_RTC_CTRL_REG					0x10
#define PALMAS_RTC_STATUS_REG					0x11
#define PALMAS_RTC_INTERRUPTS_REG				0x12
#define PALMAS_RTC_COMP_LSB_REG					0x13
#define PALMAS_RTC_COMP_MSB_REG					0x14
#define PALMAS_RTC_RES_PROG_REG					0x15
#define PALMAS_RTC_RESET_STATUS_REG				0x16

/* Bit definitions for SECONDS_REG */
#define PALMAS_SECONDS_REG_SEC1_MASK				0x70
#define PALMAS_SECONDS_REG_SEC1_SHIFT				4
#define PALMAS_SECONDS_REG_SEC0_MASK				0x0f
#define PALMAS_SECONDS_REG_SEC0_SHIFT				0

/* Bit definitions for MINUTES_REG */
#define PALMAS_MINUTES_REG_MIN1_MASK				0x70
#define PALMAS_MINUTES_REG_MIN1_SHIFT				4
#define PALMAS_MINUTES_REG_MIN0_MASK				0x0f
#define PALMAS_MINUTES_REG_MIN0_SHIFT				0

/* Bit definitions for HOURS_REG */
#define PALMAS_HOURS_REG_PM_NAM					0x80
#define PALMAS_HOURS_REG_PM_NAM_SHIFT				7
#define PALMAS_HOURS_REG_HOUR1_MASK				0x30
#define PALMAS_HOURS_REG_HOUR1_SHIFT				4
#define PALMAS_HOURS_REG_HOUR0_MASK				0x0f
#define PALMAS_HOURS_REG_HOUR0_SHIFT				0

/* Bit definitions for DAYS_REG */
#define PALMAS_DAYS_REG_DAY1_MASK				0x30
#define PALMAS_DAYS_REG_DAY1_SHIFT				4
#define PALMAS_DAYS_REG_DAY0_MASK				0x0f
#define PALMAS_DAYS_REG_DAY0_SHIFT				0

/* Bit definitions for MONTHS_REG */
#define PALMAS_MONTHS_REG_MONTH1				0x10
#define PALMAS_MONTHS_REG_MONTH1_SHIFT				4
#define PALMAS_MONTHS_REG_MONTH0_MASK				0x0f
#define PALMAS_MONTHS_REG_MONTH0_SHIFT				0

/* Bit definitions for YEARS_REG */
#define PALMAS_YEARS_REG_YEAR1_MASK				0xf0
#define PALMAS_YEARS_REG_YEAR1_SHIFT				4
#define PALMAS_YEARS_REG_YEAR0_MASK				0x0f
#define PALMAS_YEARS_REG_YEAR0_SHIFT				0

/* Bit definitions for WEEKS_REG */
#define PALMAS_WEEKS_REG_WEEK_MASK				0x07
#define PALMAS_WEEKS_REG_WEEK_SHIFT				0

/* Bit definitions for ALARM_SECONDS_REG */
#define PALMAS_ALARM_SECONDS_REG_ALARM_SEC1_MASK		0x70
#define PALMAS_ALARM_SECONDS_REG_ALARM_SEC1_SHIFT		4
#define PALMAS_ALARM_SECONDS_REG_ALARM_SEC0_MASK		0x0f
#define PALMAS_ALARM_SECONDS_REG_ALARM_SEC0_SHIFT		0

/* Bit definitions for ALARM_MINUTES_REG */
#define PALMAS_ALARM_MINUTES_REG_ALARM_MIN1_MASK		0x70
#define PALMAS_ALARM_MINUTES_REG_ALARM_MIN1_SHIFT		4
#define PALMAS_ALARM_MINUTES_REG_ALARM_MIN0_MASK		0x0f
#define PALMAS_ALARM_MINUTES_REG_ALARM_MIN0_SHIFT		0

/* Bit definitions for ALARM_HOURS_REG */
#define PALMAS_ALARM_HOURS_REG_ALARM_PM_NAM			0x80
#define PALMAS_ALARM_HOURS_REG_ALARM_PM_NAM_SHIFT		7
#define PALMAS_ALARM_HOURS_REG_ALARM_HOUR1_MASK			0x30
#define PALMAS_ALARM_HOURS_REG_ALARM_HOUR1_SHIFT		4
#define PALMAS_ALARM_HOURS_REG_ALARM_HOUR0_MASK			0x0f
#define PALMAS_ALARM_HOURS_REG_ALARM_HOUR0_SHIFT		0

/* Bit definitions for ALARM_DAYS_REG */
#define PALMAS_ALARM_DAYS_REG_ALARM_DAY1_MASK			0x30
#define PALMAS_ALARM_DAYS_REG_ALARM_DAY1_SHIFT			4
#define PALMAS_ALARM_DAYS_REG_ALARM_DAY0_MASK			0x0f
#define PALMAS_ALARM_DAYS_REG_ALARM_DAY0_SHIFT			0

/* Bit definitions for ALARM_MONTHS_REG */
#define PALMAS_ALARM_MONTHS_REG_ALARM_MONTH1			0x10
#define PALMAS_ALARM_MONTHS_REG_ALARM_MONTH1_SHIFT		4
#define PALMAS_ALARM_MONTHS_REG_ALARM_MONTH0_MASK		0x0f
#define PALMAS_ALARM_MONTHS_REG_ALARM_MONTH0_SHIFT		0

/* Bit definitions for ALARM_YEARS_REG */
#define PALMAS_ALARM_YEARS_REG_ALARM_YEAR1_MASK			0xf0
#define PALMAS_ALARM_YEARS_REG_ALARM_YEAR1_SHIFT		4
#define PALMAS_ALARM_YEARS_REG_ALARM_YEAR0_MASK			0x0f
#define PALMAS_ALARM_YEARS_REG_ALARM_YEAR0_SHIFT		0

/* Bit definitions for RTC_CTRL_REG */
#define PALMAS_RTC_CTRL_REG_RTC_V_OPT				0x80
#define PALMAS_RTC_CTRL_REG_RTC_V_OPT_SHIFT			7
#define PALMAS_RTC_CTRL_REG_GET_TIME				0x40
#define PALMAS_RTC_CTRL_REG_GET_TIME_SHIFT			6
#define PALMAS_RTC_CTRL_REG_SET_32_COUNTER			0x20
#define PALMAS_RTC_CTRL_REG_SET_32_COUNTER_SHIFT		5
#define PALMAS_RTC_CTRL_REG_TEST_MODE				0x10
#define PALMAS_RTC_CTRL_REG_TEST_MODE_SHIFT			4
#define PALMAS_RTC_CTRL_REG_MODE_12_24				0x08
#define PALMAS_RTC_CTRL_REG_MODE_12_24_SHIFT			3
#define PALMAS_RTC_CTRL_REG_AUTO_COMP				0x04
#define PALMAS_RTC_CTRL_REG_AUTO_COMP_SHIFT			2
#define PALMAS_RTC_CTRL_REG_ROUND_30S				0x02
#define PALMAS_RTC_CTRL_REG_ROUND_30S_SHIFT			1
#define PALMAS_RTC_CTRL_REG_STOP_RTC				0x01
#define PALMAS_RTC_CTRL_REG_STOP_RTC_SHIFT			0

/* Bit definitions for RTC_STATUS_REG */
#define PALMAS_RTC_STATUS_REG_POWER_UP				0x80
#define PALMAS_RTC_STATUS_REG_POWER_UP_SHIFT			7
#define PALMAS_RTC_STATUS_REG_ALARM				0x40
#define PALMAS_RTC_STATUS_REG_ALARM_SHIFT			6
#define PALMAS_RTC_STATUS_REG_EVENT_1D				0x20
#define PALMAS_RTC_STATUS_REG_EVENT_1D_SHIFT			5
#define PALMAS_RTC_STATUS_REG_EVENT_1H				0x10
#define PALMAS_RTC_STATUS_REG_EVENT_1H_SHIFT			4
#define PALMAS_RTC_STATUS_REG_EVENT_1M				0x08
#define PALMAS_RTC_STATUS_REG_EVENT_1M_SHIFT			3
#define PALMAS_RTC_STATUS_REG_EVENT_1S				0x04
#define PALMAS_RTC_STATUS_REG_EVENT_1S_SHIFT			2
#define PALMAS_RTC_STATUS_REG_RUN				0x02
#define PALMAS_RTC_STATUS_REG_RUN_SHIFT				1

/* Bit definitions for RTC_INTERRUPTS_REG */
#define PALMAS_RTC_INTERRUPTS_REG_IT_SLEEP_MASK_EN		0x10
#define PALMAS_RTC_INTERRUPTS_REG_IT_SLEEP_MASK_EN_SHIFT	4
#define PALMAS_RTC_INTERRUPTS_REG_IT_ALARM			0x08
#define PALMAS_RTC_INTERRUPTS_REG_IT_ALARM_SHIFT		3
#define PALMAS_RTC_INTERRUPTS_REG_IT_TIMER			0x04
#define PALMAS_RTC_INTERRUPTS_REG_IT_TIMER_SHIFT		2
#define PALMAS_RTC_INTERRUPTS_REG_EVERY_MASK			0x03
#define PALMAS_RTC_INTERRUPTS_REG_EVERY_SHIFT			0

/* Bit definitions for RTC_COMP_LSB_REG */
#define PALMAS_RTC_COMP_LSB_REG_RTC_COMP_LSB_MASK		0xff
#define PALMAS_RTC_COMP_LSB_REG_RTC_COMP_LSB_SHIFT		0

/* Bit definitions for RTC_COMP_MSB_REG */
#define PALMAS_RTC_COMP_MSB_REG_RTC_COMP_MSB_MASK		0xff
#define PALMAS_RTC_COMP_MSB_REG_RTC_COMP_MSB_SHIFT		0

/* Bit definitions for RTC_RES_PROG_REG */
#define PALMAS_RTC_RES_PROG_REG_SW_RES_PROG_MASK		0x3f
#define PALMAS_RTC_RES_PROG_REG_SW_RES_PROG_SHIFT		0

/* Bit definitions for RTC_RESET_STATUS_REG */
#define PALMAS_RTC_RESET_STATUS_REG_RESET_STATUS		0x01
#define PALMAS_RTC_RESET_STATUS_REG_RESET_STATUS_SHIFT		0

/* Registers for function BACKUP */
#define PALMAS_BACKUP0						0x0
#define PALMAS_BACKUP1						0x1
#define PALMAS_BACKUP2						0x2
#define PALMAS_BACKUP3						0x3
#define PALMAS_BACKUP4						0x4
#define PALMAS_BACKUP5						0x5
#define PALMAS_BACKUP6						0x6
#define PALMAS_BACKUP7						0x7

/* Bit definitions for BACKUP0 */
#define PALMAS_BACKUP0_BACKUP_MASK				0xff
#define PALMAS_BACKUP0_BACKUP_SHIFT				0

/* Bit definitions for BACKUP1 */
#define PALMAS_BACKUP1_BACKUP_MASK				0xff
#define PALMAS_BACKUP1_BACKUP_SHIFT				0

/* Bit definitions for BACKUP2 */
#define PALMAS_BACKUP2_BACKUP_MASK				0xff
#define PALMAS_BACKUP2_BACKUP_SHIFT				0

/* Bit definitions for BACKUP3 */
#define PALMAS_BACKUP3_BACKUP_MASK				0xff
#define PALMAS_BACKUP3_BACKUP_SHIFT				0

/* Bit definitions for BACKUP4 */
#define PALMAS_BACKUP4_BACKUP_MASK				0xff
#define PALMAS_BACKUP4_BACKUP_SHIFT				0

/* Bit definitions for BACKUP5 */
#define PALMAS_BACKUP5_BACKUP_MASK				0xff
#define PALMAS_BACKUP5_BACKUP_SHIFT				0

/* Bit definitions for BACKUP6 */
#define PALMAS_BACKUP6_BACKUP_MASK				0xff
#define PALMAS_BACKUP6_BACKUP_SHIFT				0

/* Bit definitions for BACKUP7 */
#define PALMAS_BACKUP7_BACKUP_MASK				0xff
#define PALMAS_BACKUP7_BACKUP_SHIFT				0

/* Registers for function SMPS */
#define PALMAS_SMPS12_CTRL					0x0
#define PALMAS_SMPS12_TSTEP					0x1
#define PALMAS_SMPS12_FORCE					0x2
#define PALMAS_SMPS12_VOLTAGE					0x3
#define PALMAS_SMPS3_CTRL					0x4
#define PALMAS_SMPS3_TSTEP					0x5
#define PALMAS_SMPS3_FORCE					0x6
#define PALMAS_SMPS3_VOLTAGE					0x7
#define PALMAS_SMPS45_CTRL					0x8
#define PALMAS_SMPS45_TSTEP					0x9
#define PALMAS_SMPS45_FORCE					0xA
#define PALMAS_SMPS45_VOLTAGE					0xB
#define PALMAS_SMPS6_CTRL					0xC
#define PALMAS_SMPS6_TSTEP					0xD
#define PALMAS_SMPS6_FORCE					0xE
#define PALMAS_SMPS6_VOLTAGE					0xF
#define PALMAS_SMPS7_CTRL					0x10
#define PALMAS_SMPS7_VOLTAGE					0x13
#define PALMAS_SMPS8_CTRL					0x14
#define PALMAS_SMPS8_TSTEP					0x15
#define PALMAS_SMPS8_FORCE					0x16
#define PALMAS_SMPS8_VOLTAGE					0x17
#define PALMAS_SMPS9_CTRL					0x18
#define PALMAS_SMPS9_VOLTAGE					0x1B
#define PALMAS_SMPS10_CTRL					0x1C
#define PALMAS_SMPS10_STATUS					0x1F
#define PALMAS_SMPS_CTRL					0x24
#define PALMAS_SMPS_PD_CTRL					0x25
#define PALMAS_SMPS_DITHER_EN					0x26
#define PALMAS_SMPS_THERMAL_EN					0x27
#define PALMAS_SMPS_THERMAL_STATUS				0x28
#define PALMAS_SMPS_SHORT_STATUS				0x29
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN			0x2A
#define PALMAS_SMPS_POWERGOOD_MASK1				0x2B
#define PALMAS_SMPS_POWERGOOD_MASK2				0x2C

/* Bit definitions for SMPS12_CTRL */
#define PALMAS_SMPS12_CTRL_WR_S					0x80
#define PALMAS_SMPS12_CTRL_WR_S_SHIFT				7
#define PALMAS_SMPS12_CTRL_ROOF_FLOOR_EN			0x40
#define PALMAS_SMPS12_CTRL_ROOF_FLOOR_EN_SHIFT			6
#define PALMAS_SMPS12_CTRL_STATUS_MASK				0x30
#define PALMAS_SMPS12_CTRL_STATUS_SHIFT				4
#define PALMAS_SMPS12_CTRL_MODE_SLEEP_MASK			0x0c
#define PALMAS_SMPS12_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_SMPS12_CTRL_MODE_ACTIVE_MASK			0x03
#define PALMAS_SMPS12_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for SMPS12_TSTEP */
#define PALMAS_SMPS12_TSTEP_TSTEP_MASK				0x03
#define PALMAS_SMPS12_TSTEP_TSTEP_SHIFT				0

/* Bit definitions for SMPS12_FORCE */
#define PALMAS_SMPS12_FORCE_CMD					0x80
#define PALMAS_SMPS12_FORCE_CMD_SHIFT				7
#define PALMAS_SMPS12_FORCE_VSEL_MASK				0x7f
#define PALMAS_SMPS12_FORCE_VSEL_SHIFT				0

/* Bit definitions for SMPS12_VOLTAGE */
#define PALMAS_SMPS12_VOLTAGE_RANGE				0x80
#define PALMAS_SMPS12_VOLTAGE_RANGE_SHIFT			7
#define PALMAS_SMPS12_VOLTAGE_VSEL_MASK				0x7f
#define PALMAS_SMPS12_VOLTAGE_VSEL_SHIFT			0

/* Bit definitions for SMPS3_CTRL */
#define PALMAS_SMPS3_CTRL_WR_S					0x80
#define PALMAS_SMPS3_CTRL_WR_S_SHIFT				7
#define PALMAS_SMPS3_CTRL_STATUS_MASK				0x30
#define PALMAS_SMPS3_CTRL_STATUS_SHIFT				4
#define PALMAS_SMPS3_CTRL_MODE_SLEEP_MASK			0x0c
#define PALMAS_SMPS3_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_SMPS3_CTRL_MODE_ACTIVE_MASK			0x03
#define PALMAS_SMPS3_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for SMPS3_VOLTAGE */
#define PALMAS_SMPS3_VOLTAGE_RANGE				0x80
#define PALMAS_SMPS3_VOLTAGE_RANGE_SHIFT			7
#define PALMAS_SMPS3_VOLTAGE_VSEL_MASK				0x7f
#define PALMAS_SMPS3_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for SMPS45_CTRL */
#define PALMAS_SMPS45_CTRL_WR_S					0x80
#define PALMAS_SMPS45_CTRL_WR_S_SHIFT				7
#define PALMAS_SMPS45_CTRL_ROOF_FLOOR_EN			0x40
#define PALMAS_SMPS45_CTRL_ROOF_FLOOR_EN_SHIFT			6
#define PALMAS_SMPS45_CTRL_STATUS_MASK				0x30
#define PALMAS_SMPS45_CTRL_STATUS_SHIFT				4
#define PALMAS_SMPS45_CTRL_MODE_SLEEP_MASK			0x0c
#define PALMAS_SMPS45_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_SMPS45_CTRL_MODE_ACTIVE_MASK			0x03
#define PALMAS_SMPS45_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for SMPS45_TSTEP */
#define PALMAS_SMPS45_TSTEP_TSTEP_MASK				0x03
#define PALMAS_SMPS45_TSTEP_TSTEP_SHIFT				0

/* Bit definitions for SMPS45_FORCE */
#define PALMAS_SMPS45_FORCE_CMD					0x80
#define PALMAS_SMPS45_FORCE_CMD_SHIFT				7
#define PALMAS_SMPS45_FORCE_VSEL_MASK				0x7f
#define PALMAS_SMPS45_FORCE_VSEL_SHIFT				0

/* Bit definitions for SMPS45_VOLTAGE */
#define PALMAS_SMPS45_VOLTAGE_RANGE				0x80
#define PALMAS_SMPS45_VOLTAGE_RANGE_SHIFT			7
#define PALMAS_SMPS45_VOLTAGE_VSEL_MASK				0x7f
#define PALMAS_SMPS45_VOLTAGE_VSEL_SHIFT			0

/* Bit definitions for SMPS6_CTRL */
#define PALMAS_SMPS6_CTRL_WR_S					0x80
#define PALMAS_SMPS6_CTRL_WR_S_SHIFT				7
#define PALMAS_SMPS6_CTRL_ROOF_FLOOR_EN				0x40
#define PALMAS_SMPS6_CTRL_ROOF_FLOOR_EN_SHIFT			6
#define PALMAS_SMPS6_CTRL_STATUS_MASK				0x30
#define PALMAS_SMPS6_CTRL_STATUS_SHIFT				4
#define PALMAS_SMPS6_CTRL_MODE_SLEEP_MASK			0x0c
#define PALMAS_SMPS6_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_SMPS6_CTRL_MODE_ACTIVE_MASK			0x03
#define PALMAS_SMPS6_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for SMPS6_TSTEP */
#define PALMAS_SMPS6_TSTEP_TSTEP_MASK				0x03
#define PALMAS_SMPS6_TSTEP_TSTEP_SHIFT				0

/* Bit definitions for SMPS6_FORCE */
#define PALMAS_SMPS6_FORCE_CMD					0x80
#define PALMAS_SMPS6_FORCE_CMD_SHIFT				7
#define PALMAS_SMPS6_FORCE_VSEL_MASK				0x7f
#define PALMAS_SMPS6_FORCE_VSEL_SHIFT				0

/* Bit definitions for SMPS6_VOLTAGE */
#define PALMAS_SMPS6_VOLTAGE_RANGE				0x80
#define PALMAS_SMPS6_VOLTAGE_RANGE_SHIFT			7
#define PALMAS_SMPS6_VOLTAGE_VSEL_MASK				0x7f
#define PALMAS_SMPS6_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for SMPS7_CTRL */
#define PALMAS_SMPS7_CTRL_WR_S					0x80
#define PALMAS_SMPS7_CTRL_WR_S_SHIFT				7
#define PALMAS_SMPS7_CTRL_STATUS_MASK				0x30
#define PALMAS_SMPS7_CTRL_STATUS_SHIFT				4
#define PALMAS_SMPS7_CTRL_MODE_SLEEP_MASK			0x0c
#define PALMAS_SMPS7_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_SMPS7_CTRL_MODE_ACTIVE_MASK			0x03
#define PALMAS_SMPS7_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for SMPS7_VOLTAGE */
#define PALMAS_SMPS7_VOLTAGE_RANGE				0x80
#define PALMAS_SMPS7_VOLTAGE_RANGE_SHIFT			7
#define PALMAS_SMPS7_VOLTAGE_VSEL_MASK				0x7f
#define PALMAS_SMPS7_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for SMPS8_CTRL */
#define PALMAS_SMPS8_CTRL_WR_S					0x80
#define PALMAS_SMPS8_CTRL_WR_S_SHIFT				7
#define PALMAS_SMPS8_CTRL_ROOF_FLOOR_EN				0x40
#define PALMAS_SMPS8_CTRL_ROOF_FLOOR_EN_SHIFT			6
#define PALMAS_SMPS8_CTRL_STATUS_MASK				0x30
#define PALMAS_SMPS8_CTRL_STATUS_SHIFT				4
#define PALMAS_SMPS8_CTRL_MODE_SLEEP_MASK			0x0c
#define PALMAS_SMPS8_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_SMPS8_CTRL_MODE_ACTIVE_MASK			0x03
#define PALMAS_SMPS8_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for SMPS8_TSTEP */
#define PALMAS_SMPS8_TSTEP_TSTEP_MASK				0x03
#define PALMAS_SMPS8_TSTEP_TSTEP_SHIFT				0

/* Bit definitions for SMPS8_FORCE */
#define PALMAS_SMPS8_FORCE_CMD					0x80
#define PALMAS_SMPS8_FORCE_CMD_SHIFT				7
#define PALMAS_SMPS8_FORCE_VSEL_MASK				0x7f
#define PALMAS_SMPS8_FORCE_VSEL_SHIFT				0

/* Bit definitions for SMPS8_VOLTAGE */
#define PALMAS_SMPS8_VOLTAGE_RANGE				0x80
#define PALMAS_SMPS8_VOLTAGE_RANGE_SHIFT			7
#define PALMAS_SMPS8_VOLTAGE_VSEL_MASK				0x7f
#define PALMAS_SMPS8_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for SMPS9_CTRL */
#define PALMAS_SMPS9_CTRL_WR_S					0x80
#define PALMAS_SMPS9_CTRL_WR_S_SHIFT				7
#define PALMAS_SMPS9_CTRL_STATUS_MASK				0x30
#define PALMAS_SMPS9_CTRL_STATUS_SHIFT				4
#define PALMAS_SMPS9_CTRL_MODE_SLEEP_MASK			0x0c
#define PALMAS_SMPS9_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_SMPS9_CTRL_MODE_ACTIVE_MASK			0x03
#define PALMAS_SMPS9_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for SMPS9_VOLTAGE */
#define PALMAS_SMPS9_VOLTAGE_RANGE				0x80
#define PALMAS_SMPS9_VOLTAGE_RANGE_SHIFT			7
#define PALMAS_SMPS9_VOLTAGE_VSEL_MASK				0x7f
#define PALMAS_SMPS9_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for SMPS10_CTRL */
#define PALMAS_SMPS10_CTRL_MODE_SLEEP_MASK			0xf0
#define PALMAS_SMPS10_CTRL_MODE_SLEEP_SHIFT			4
#define PALMAS_SMPS10_CTRL_MODE_ACTIVE_MASK			0x0f
#define PALMAS_SMPS10_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for SMPS10_STATUS */
#define PALMAS_SMPS10_STATUS_STATUS_MASK			0x0f
#define PALMAS_SMPS10_STATUS_STATUS_SHIFT			0

/* Bit definitions for SMPS_CTRL */
#define PALMAS_SMPS_CTRL_SMPS45_SMPS457_EN			0x20
#define PALMAS_SMPS_CTRL_SMPS45_SMPS457_EN_SHIFT		5
#define PALMAS_SMPS_CTRL_SMPS12_SMPS123_EN			0x10
#define PALMAS_SMPS_CTRL_SMPS12_SMPS123_EN_SHIFT		4
#define PALMAS_SMPS_CTRL_SMPS45_PHASE_CTRL_MASK			0x0c
#define PALMAS_SMPS_CTRL_SMPS45_PHASE_CTRL_SHIFT		2
#define PALMAS_SMPS_CTRL_SMPS123_PHASE_CTRL_MASK		0x03
#define PALMAS_SMPS_CTRL_SMPS123_PHASE_CTRL_SHIFT		0

/* Bit definitions for SMPS_PD_CTRL */
#define PALMAS_SMPS_PD_CTRL_SMPS9				0x40
#define PALMAS_SMPS_PD_CTRL_SMPS9_SHIFT				6
#define PALMAS_SMPS_PD_CTRL_SMPS8				0x20
#define PALMAS_SMPS_PD_CTRL_SMPS8_SHIFT				5
#define PALMAS_SMPS_PD_CTRL_SMPS7				0x10
#define PALMAS_SMPS_PD_CTRL_SMPS7_SHIFT				4
#define PALMAS_SMPS_PD_CTRL_SMPS6				0x08
#define PALMAS_SMPS_PD_CTRL_SMPS6_SHIFT				3
#define PALMAS_SMPS_PD_CTRL_SMPS45				0x04
#define PALMAS_SMPS_PD_CTRL_SMPS45_SHIFT			2
#define PALMAS_SMPS_PD_CTRL_SMPS3				0x02
#define PALMAS_SMPS_PD_CTRL_SMPS3_SHIFT				1
#define PALMAS_SMPS_PD_CTRL_SMPS12				0x01
#define PALMAS_SMPS_PD_CTRL_SMPS12_SHIFT			0

/* Bit definitions for SMPS_THERMAL_EN */
#define PALMAS_SMPS_THERMAL_EN_SMPS9				0x40
#define PALMAS_SMPS_THERMAL_EN_SMPS9_SHIFT			6
#define PALMAS_SMPS_THERMAL_EN_SMPS8				0x20
#define PALMAS_SMPS_THERMAL_EN_SMPS8_SHIFT			5
#define PALMAS_SMPS_THERMAL_EN_SMPS6				0x08
#define PALMAS_SMPS_THERMAL_EN_SMPS6_SHIFT			3
#define PALMAS_SMPS_THERMAL_EN_SMPS457				0x04
#define PALMAS_SMPS_THERMAL_EN_SMPS457_SHIFT			2
#define PALMAS_SMPS_THERMAL_EN_SMPS123				0x01
#define PALMAS_SMPS_THERMAL_EN_SMPS123_SHIFT			0

/* Bit definitions for SMPS_THERMAL_STATUS */
#define PALMAS_SMPS_THERMAL_STATUS_SMPS9			0x40
#define PALMAS_SMPS_THERMAL_STATUS_SMPS9_SHIFT			6
#define PALMAS_SMPS_THERMAL_STATUS_SMPS8			0x20
#define PALMAS_SMPS_THERMAL_STATUS_SMPS8_SHIFT			5
#define PALMAS_SMPS_THERMAL_STATUS_SMPS6			0x08
#define PALMAS_SMPS_THERMAL_STATUS_SMPS6_SHIFT			3
#define PALMAS_SMPS_THERMAL_STATUS_SMPS457			0x04
#define PALMAS_SMPS_THERMAL_STATUS_SMPS457_SHIFT		2
#define PALMAS_SMPS_THERMAL_STATUS_SMPS123			0x01
#define PALMAS_SMPS_THERMAL_STATUS_SMPS123_SHIFT		0

/* Bit definitions for SMPS_SHORT_STATUS */
#define PALMAS_SMPS_SHORT_STATUS_SMPS10				0x80
#define PALMAS_SMPS_SHORT_STATUS_SMPS10_SHIFT			7
#define PALMAS_SMPS_SHORT_STATUS_SMPS9				0x40
#define PALMAS_SMPS_SHORT_STATUS_SMPS9_SHIFT			6
#define PALMAS_SMPS_SHORT_STATUS_SMPS8				0x20
#define PALMAS_SMPS_SHORT_STATUS_SMPS8_SHIFT			5
#define PALMAS_SMPS_SHORT_STATUS_SMPS7				0x10
#define PALMAS_SMPS_SHORT_STATUS_SMPS7_SHIFT			4
#define PALMAS_SMPS_SHORT_STATUS_SMPS6				0x08
#define PALMAS_SMPS_SHORT_STATUS_SMPS6_SHIFT			3
#define PALMAS_SMPS_SHORT_STATUS_SMPS45				0x04
#define PALMAS_SMPS_SHORT_STATUS_SMPS45_SHIFT			2
#define PALMAS_SMPS_SHORT_STATUS_SMPS3				0x02
#define PALMAS_SMPS_SHORT_STATUS_SMPS3_SHIFT			1
#define PALMAS_SMPS_SHORT_STATUS_SMPS12				0x01
#define PALMAS_SMPS_SHORT_STATUS_SMPS12_SHIFT			0

/* Bit definitions for SMPS_NEGATIVE_CURRENT_LIMIT_EN */
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS9		0x40
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS9_SHIFT	6
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS8		0x20
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS8_SHIFT	5
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS7		0x10
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS7_SHIFT	4
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS6		0x08
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS6_SHIFT	3
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS45		0x04
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS45_SHIFT	2
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS3		0x02
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS3_SHIFT	1
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS12		0x01
#define PALMAS_SMPS_NEGATIVE_CURRENT_LIMIT_EN_SMPS12_SHIFT	0

/* Bit definitions for SMPS_POWERGOOD_MASK1 */
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS10			0x80
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS10_SHIFT		7
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS9			0x40
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS9_SHIFT			6
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS8			0x20
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS8_SHIFT			5
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS7			0x10
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS7_SHIFT			4
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS6			0x08
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS6_SHIFT			3
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS45			0x04
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS45_SHIFT		2
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS3			0x02
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS3_SHIFT			1
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS12			0x01
#define PALMAS_SMPS_POWERGOOD_MASK1_SMPS12_SHIFT		0

/* Bit definitions for SMPS_POWERGOOD_MASK2 */
#define PALMAS_SMPS_POWERGOOD_MASK2_POWERGOOD_TYPE_SELECT	0x80
#define PALMAS_SMPS_POWERGOOD_MASK2_POWERGOOD_TYPE_SELECT_SHIFT	7
#define PALMAS_SMPS_POWERGOOD_MASK2_OVC_ALARM			0x10
#define PALMAS_SMPS_POWERGOOD_MASK2_GPIO_7			0x04
#define PALMAS_SMPS_POWERGOOD_MASK2_GPIO_7_SHIFT		2
#define PALMAS_SMPS_POWERGOOD_MASK2_VBUS			0x02
#define PALMAS_SMPS_POWERGOOD_MASK2_VBUS_SHIFT			1
#define PALMAS_SMPS_POWERGOOD_MASK2_ACOK			0x01
#define PALMAS_SMPS_POWERGOOD_MASK2_ACOK_SHIFT			0

/* Registers for function LDO */
#define PALMAS_LDO1_CTRL					0x0
#define PALMAS_LDO1_VOLTAGE					0x1
#define PALMAS_LDO2_CTRL					0x2
#define PALMAS_LDO2_VOLTAGE					0x3
#define PALMAS_LDO3_CTRL					0x4
#define PALMAS_LDO3_VOLTAGE					0x5
#define PALMAS_LDO4_CTRL					0x6
#define PALMAS_LDO4_VOLTAGE					0x7
#define PALMAS_LDO5_CTRL					0x8
#define PALMAS_LDO5_VOLTAGE					0x9
#define PALMAS_LDO6_CTRL					0xA
#define PALMAS_LDO6_VOLTAGE					0xB
#define PALMAS_LDO7_CTRL					0xC
#define PALMAS_LDO7_VOLTAGE					0xD
#define PALMAS_LDO8_CTRL					0xE
#define PALMAS_LDO8_VOLTAGE					0xF
#define PALMAS_LDO9_CTRL					0x10
#define PALMAS_LDO9_VOLTAGE					0x11
#define PALMAS_LDOLN_CTRL					0x12
#define PALMAS_LDOLN_VOLTAGE					0x13
#define PALMAS_LDOUSB_CTRL					0x14
#define PALMAS_LDOUSB_VOLTAGE					0x15
#define PALMAS_LDO10_CTRL					0x16
#define PALMAS_LDO10_VOLTAGE					0x17
#define PALMAS_LDO11_CTRL					0x18
#define PALMAS_LDO11_VOLTAGE					0x19
#define PALMAS_LDO12_CTRL					0x1F
#define PALMAS_LDO12_VOLTAGE					0x20
#define PALMAS_LDO13_CTRL					0x21
#define PALMAS_LDO13_VOLTAGE					0x22
#define PALMAS_LDO14_CTRL					0x23
#define PALMAS_LDO14_VOLTAGE					0x24
#define PALMAS_LDO_CTRL						0x1A
#define PALMAS_LDO_PD_CTRL1					0x1B
#define PALMAS_LDO_PD_CTRL2					0x1C
#define PALMAS_LDO_SHORT_STATUS1				0x1D
#define PALMAS_LDO_SHORT_STATUS2				0x1E

/* Bit definitions for LDO1_CTRL */
#define PALMAS_LDO1_CTRL_WR_S					0x80
#define PALMAS_LDO1_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO1_CTRL_LDO_BYPASS_EN				0x40
#define PALMAS_LDO1_CTRL_LDO_BYPASS_EN_SHIFT			6
#define PALMAS_LDO1_CTRL_STATUS					0x10
#define PALMAS_LDO1_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO1_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO1_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO1_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO1_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO1_VOLTAGE */
#define PALMAS_LDO1_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO1_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO2_CTRL */
#define PALMAS_LDO2_CTRL_WR_S					0x80
#define PALMAS_LDO2_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO2_CTRL_LDO_BYPASS_EN				0x40
#define PALMAS_LDO2_CTRL_LDO_BYPASS_EN_SHIFT			6
#define PALMAS_LDO2_CTRL_STATUS					0x10
#define PALMAS_LDO2_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO2_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO2_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO2_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO2_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO2_VOLTAGE */
#define PALMAS_LDO2_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO2_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO3_CTRL */
#define PALMAS_LDO3_CTRL_WR_S					0x80
#define PALMAS_LDO3_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO3_CTRL_STATUS					0x10
#define PALMAS_LDO3_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO3_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO3_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO3_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO3_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO3_VOLTAGE */
#define PALMAS_LDO3_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO3_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO4_CTRL */
#define PALMAS_LDO4_CTRL_WR_S					0x80
#define PALMAS_LDO4_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO4_CTRL_LDO_BYPASS_EN				0x40
#define PALMAS_LDO4_CTRL_LDO_BYPASS_EN_SHIFT			6
#define PALMAS_LDO4_CTRL_STATUS					0x10
#define PALMAS_LDO4_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO4_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO4_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO4_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO4_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO4_VOLTAGE */
#define PALMAS_LDO4_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO4_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO5_CTRL */
#define PALMAS_LDO5_CTRL_WR_S					0x80
#define PALMAS_LDO5_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO5_CTRL_STATUS					0x10
#define PALMAS_LDO5_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO5_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO5_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO5_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO5_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO5_VOLTAGE */
#define PALMAS_LDO5_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO5_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO6_CTRL */
#define PALMAS_LDO6_CTRL_WR_S					0x80
#define PALMAS_LDO6_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO6_CTRL_LDO_BYPASS_EN				0x40
#define PALMAS_LDO6_CTRL_LDO_BYPASS_EN_SHIFT			6
#define PALMAS_LDO6_CTRL_LDO_VIB_EN				0x40
#define PALMAS_LDO6_CTRL_LDO_VIB_EN_SHIFT			6
#define PALMAS_LDO6_CTRL_STATUS					0x10
#define PALMAS_LDO6_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO6_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO6_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO6_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO6_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO6_VOLTAGE */
#define PALMAS_LDO6_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO6_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO7_CTRL */
#define PALMAS_LDO7_CTRL_WR_S					0x80
#define PALMAS_LDO7_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO7_CTRL_STATUS					0x10
#define PALMAS_LDO7_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO7_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO7_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO7_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO7_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO7_VOLTAGE */
#define PALMAS_LDO7_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO7_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO8_CTRL */
#define PALMAS_LDO8_CTRL_WR_S					0x80
#define PALMAS_LDO8_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO8_CTRL_LDO_TRACKING_EN			0x40
#define PALMAS_LDO8_CTRL_LDO_TRACKING_EN_SHIFT			6
#define PALMAS_LDO8_CTRL_STATUS					0x10
#define PALMAS_LDO8_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO8_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO8_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO8_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO8_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO8_VOLTAGE */
#define PALMAS_LDO8_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO8_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO9_CTRL */
#define PALMAS_LDO9_CTRL_WR_S					0x80
#define PALMAS_LDO9_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO9_CTRL_LDO_BYPASS_EN				0x40
#define PALMAS_LDO9_CTRL_LDO_BYPASS_EN_SHIFT			6
#define PALMAS_LDO9_CTRL_STATUS					0x10
#define PALMAS_LDO9_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO9_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO9_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO9_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO9_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO9_VOLTAGE */
#define PALMAS_LDO9_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO9_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO10_CTRL */
#define PALMAS_LDO10_CTRL_WR_S					0x80
#define PALMAS_LDO10_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO10_CTRL_LDO_BYPASS_EN				0x40
#define PALMAS_LDO10_CTRL_LDO_BYPASS_EN_SHIFT			6
#define PALMAS_LDO10_CTRL_STATUS				0x10
#define PALMAS_LDO10_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO10_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO10_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO10_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO10_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO10_VOLTAGE */
#define PALMAS_LDO10_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO10_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO11_CTRL */
#define PALMAS_LDO11_CTRL_WR_S					0x80
#define PALMAS_LDO11_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO11_CTRL_LDO_BYPASS_EN				0x40
#define PALMAS_LDO11_CTRL_LDO_BYPASS_EN_SHIFT			6
#define PALMAS_LDO11_CTRL_STATUS				0x10
#define PALMAS_LDO11_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO11_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO11_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO11_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO11_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO11_VOLTAGE */
#define PALMAS_LDO11_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO11_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO12_CTRL */
#define PALMAS_LDO12_CTRL_WR_S					0x80
#define PALMAS_LDO12_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO12_CTRL_LDO_BYPASS_EN				0x40
#define PALMAS_LDO12_CTRL_LDO_BYPASS_EN_SHIFT			6
#define PALMAS_LDO12_CTRL_STATUS				0x10
#define PALMAS_LDO12_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO12_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO12_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO12_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO12_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO12_VOLTAGE */
#define PALMAS_LDO12_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO12_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO13_CTRL */
#define PALMAS_LDO13_CTRL_WR_S					0x80
#define PALMAS_LDO13_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO13_CTRL_LDO_BYPASS_EN				0x40
#define PALMAS_LDO13_CTRL_LDO_BYPASS_EN_SHIFT			6
#define PALMAS_LDO13_CTRL_STATUS				0x10
#define PALMAS_LDO13_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO13_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO13_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO13_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO13_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO13_VOLTAGE */
#define PALMAS_LDO13_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO13_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDO14_CTRL */
#define PALMAS_LDO14_CTRL_WR_S					0x80
#define PALMAS_LDO14_CTRL_WR_S_SHIFT				7
#define PALMAS_LDO14_CTRL_LDO_BYPASS_EN				0x40
#define PALMAS_LDO14_CTRL_LDO_BYPASS_EN_SHIFT			6
#define PALMAS_LDO14_CTRL_STATUS				0x10
#define PALMAS_LDO14_CTRL_STATUS_SHIFT				4
#define PALMAS_LDO14_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDO14_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDO14_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDO14_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDO14_VOLTAGE */
#define PALMAS_LDO14_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDO14_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDOLN_CTRL */
#define PALMAS_LDOLN_CTRL_WR_S					0x80
#define PALMAS_LDOLN_CTRL_WR_S_SHIFT				7
#define PALMAS_LDOLN_CTRL_STATUS				0x10
#define PALMAS_LDOLN_CTRL_STATUS_SHIFT				4
#define PALMAS_LDOLN_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDOLN_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDOLN_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDOLN_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDOLN_VOLTAGE */
#define PALMAS_LDOLN_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDOLN_VOLTAGE_VSEL_SHIFT				0

/* Bit definitions for LDOUSB_CTRL */
#define PALMAS_LDOUSB_CTRL_WR_S					0x80
#define PALMAS_LDOUSB_CTRL_WR_S_SHIFT				7
#define PALMAS_LDOUSB_CTRL_STATUS				0x10
#define PALMAS_LDOUSB_CTRL_STATUS_SHIFT				4
#define PALMAS_LDOUSB_CTRL_MODE_SLEEP				0x04
#define PALMAS_LDOUSB_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_LDOUSB_CTRL_MODE_ACTIVE				0x01
#define PALMAS_LDOUSB_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for LDOUSB_VOLTAGE */
#define PALMAS_LDOUSB_VOLTAGE_VSEL_MASK				0x3f
#define PALMAS_LDOUSB_VOLTAGE_VSEL_SHIFT			0

/* Bit definitions for LDO_CTRL */
#define PALMAS_LDO_CTRL_VREF_425				0x08
#define PALMAS_LDO_CTRL_VREF_425_SHIFT				3
#define PALMAS_LDO_CTRL_LDO5_BYPASS_SRC_SEL_MASK		0x6
#define PALMAS_LDO_CTRL_LDO5_BYPASS_SRC_SEL_DISABLE		0x0
#define PALMAS_LDO_CTRL_LDO5_BYPASS_SRC_SEL_SMPS12		0x2
#define PALMAS_LDO_CTRL_LDO5_BYPASS_SRC_SEL_SMPS3		0x4
#define PALMAS_LDO_CTRL_LDO5_BYPASS_SRC_SEL_SMPS6		0x6
#define PALMAS_LDO_CTRL_LDOUSB_ON_VBUS_VSYS			0x01
#define PALMAS_LDO_CTRL_LDOUSB_ON_VBUS_VSYS_SHIFT		0

/* Bit definitions for LDO_PD_CTRL1 */
#define PALMAS_LDO_PD_CTRL1_LDO8				0x80
#define PALMAS_LDO_PD_CTRL1_LDO8_SHIFT				7
#define PALMAS_LDO_PD_CTRL1_LDO7				0x40
#define PALMAS_LDO_PD_CTRL1_LDO7_SHIFT				6
#define PALMAS_LDO_PD_CTRL1_LDO6				0x20
#define PALMAS_LDO_PD_CTRL1_LDO6_SHIFT				5
#define PALMAS_LDO_PD_CTRL1_LDO5				0x10
#define PALMAS_LDO_PD_CTRL1_LDO5_SHIFT				4
#define PALMAS_LDO_PD_CTRL1_LDO4				0x08
#define PALMAS_LDO_PD_CTRL1_LDO4_SHIFT				3
#define PALMAS_LDO_PD_CTRL1_LDO3				0x04
#define PALMAS_LDO_PD_CTRL1_LDO3_SHIFT				2
#define PALMAS_LDO_PD_CTRL1_LDO2				0x02
#define PALMAS_LDO_PD_CTRL1_LDO2_SHIFT				1
#define PALMAS_LDO_PD_CTRL1_LDO1				0x01
#define PALMAS_LDO_PD_CTRL1_LDO1_SHIFT				0

/* Bit definitions for LDO_PD_CTRL2 */
#define PALMAS_LDO_PD_CTRL2_LDO14				0x80
#define PALMAS_LDO_PD_CTRL2_LDO14_SHIFT				7
#define PALMAS_LDO_PD_CTRL2_LDO13				0x40
#define PALMAS_LDO_PD_CTRL2_LDO13_SHIFT				6
#define PALMAS_LDO_PD_CTRL2_LDO12				0x20
#define PALMAS_LDO_PD_CTRL2_LDO12_SHIFT				5
#define PALMAS_LDO_PD_CTRL2_LDO11				0x10
#define PALMAS_LDO_PD_CTRL2_LDO11_SHIFT				4
#define PALMAS_LDO_PD_CTRL2_LDO10				0x08
#define PALMAS_LDO_PD_CTRL2_LDO10_SHIFT				3
#define PALMAS_LDO_PD_CTRL2_LDOUSB				0x04
#define PALMAS_LDO_PD_CTRL2_LDOUSB_SHIFT			2
#define PALMAS_LDO_PD_CTRL2_LDOLN				0x02
#define PALMAS_LDO_PD_CTRL2_LDOLN_SHIFT				1
#define PALMAS_LDO_PD_CTRL2_LDO9				0x01
#define PALMAS_LDO_PD_CTRL2_LDO9_SHIFT				0

/* Bit definitions for LDO_SHORT_STATUS1 */
#define PALMAS_LDO_SHORT_STATUS1_LDO8				0x80
#define PALMAS_LDO_SHORT_STATUS1_LDO8_SHIFT			7
#define PALMAS_LDO_SHORT_STATUS1_LDO7				0x40
#define PALMAS_LDO_SHORT_STATUS1_LDO7_SHIFT			6
#define PALMAS_LDO_SHORT_STATUS1_LDO6				0x20
#define PALMAS_LDO_SHORT_STATUS1_LDO6_SHIFT			5
#define PALMAS_LDO_SHORT_STATUS1_LDO5				0x10
#define PALMAS_LDO_SHORT_STATUS1_LDO5_SHIFT			4
#define PALMAS_LDO_SHORT_STATUS1_LDO4				0x08
#define PALMAS_LDO_SHORT_STATUS1_LDO4_SHIFT			3
#define PALMAS_LDO_SHORT_STATUS1_LDO3				0x04
#define PALMAS_LDO_SHORT_STATUS1_LDO3_SHIFT			2
#define PALMAS_LDO_SHORT_STATUS1_LDO2				0x02
#define PALMAS_LDO_SHORT_STATUS1_LDO2_SHIFT			1
#define PALMAS_LDO_SHORT_STATUS1_LDO1				0x01
#define PALMAS_LDO_SHORT_STATUS1_LDO1_SHIFT			0

/* Bit definitions for LDO_SHORT_STATUS2 */
#define PALMAS_LDO_SHORT_STATUS2_LDO14				0x80
#define PALMAS_LDO_SHORT_STATUS2_LDO14_SHIFT			7
#define PALMAS_LDO_SHORT_STATUS2_LDO13				0x40
#define PALMAS_LDO_SHORT_STATUS2_LDO13_SHIFT			6
#define PALMAS_LDO_SHORT_STATUS2_LDO12				0x20
#define PALMAS_LDO_SHORT_STATUS2_LDO12_SHIFT			5
#define PALMAS_LDO_SHORT_STATUS2_LDO11				0x10
#define PALMAS_LDO_SHORT_STATUS2_LDO11_SHIFT			4
#define PALMAS_LDO_SHORT_STATUS2_LDO10				0x08
#define PALMAS_LDO_SHORT_STATUS2_LDO10_SHIFT			3
#define PALMAS_LDO_SHORT_STATUS2_LDOVANA			0x08
#define PALMAS_LDO_SHORT_STATUS2_LDOVANA_SHIFT			3
#define PALMAS_LDO_SHORT_STATUS2_LDOUSB				0x04
#define PALMAS_LDO_SHORT_STATUS2_LDOUSB_SHIFT			2
#define PALMAS_LDO_SHORT_STATUS2_LDOLN				0x02
#define PALMAS_LDO_SHORT_STATUS2_LDOLN_SHIFT			1
#define PALMAS_LDO_SHORT_STATUS2_LDO9				0x01
#define PALMAS_LDO_SHORT_STATUS2_LDO9_SHIFT			0

/* Registers for function DVFS Func */
#define PALMAS_SMPS_DVFS1_CTRL					0x0
#define PALMAS_SMPS_DVFS1_ENABLE_SHIFT				0
#define PALMAS_SMPS_DVFS1_OFFSET_STEP_SHIFT			1
#define PALMAS_SMPS_DVFS1_ENABLE_RST_SHIFT			2
#define PALMAS_SMPS_DVFS1_RESTORE_VALUE_SHIFT			3
#define PALMAS_SMPS_DVFS1_SMPS_SELECT_SHIFT			4
#define PALMAS_SMPS_DVFS1_VOLTAGE_MAX				0x1
#define PALMAS_SMPS_DVFS1_STATUS				0x2

#define DVFS_BASE_VOLTAGE_UV					500000
#define DVFS_MAX_VOLTAGE_UV					1650000
#define DVFS_VOLTAGE_STEP_UV					10000

/* Registers for function SIMCARD Func */
#define PALMAS_SIM_DEBOUNCE					0x0
#define PALMAS_SIM_PWR_DOWN					0x1

/* Bit definitions for SIM_DEBOUNCE */
#define PALMAS_SIM_DEBOUNCE_SIM2_IR				0x80
#define PALMAS_SIM_DEBOUNCE_SIM2_IR_SHIFT			7
#define PALMAS_SIM_DEBOUNCE_SIM1_IR				0x40
#define PALMAS_SIM_DEBOUNCE_SIM1_IR_SHIFT			6
#define PALMAS_SIM_DEBOUNCE_SIM_DET1_PIN_STATE			0x20
#define PALMAS_SIM_DEBOUNCE_SIM_DET1_PIN_STATE_SHIFT		5
#define PALMAS_SIM_DEBOUNCE_DBCNT_MASK				0x1F
#define PALMAS_SIM_DEBOUNCE_DBCNT_SHIFT				0

/* Bit definitions for SIM_PWR_DOWN */
#define PALMAS_SIM_PWR_DOWN_PWRDNEN2				0x80
#define PALMAS_SIM_PWR_DOWN_PWRDNEN2_SHIFT			7
#define PALMAS_SIM_PWR_DOWN_PWRDNEN1				0x40
#define PALMAS_SIM_PWR_DOWN_PWRDNEN1_SHIFT			6
#define PALMAS_SIM_PWR_DOWN_SIM_DET2_PIN_STATE			0x20
#define PALMAS_SIM_PWR_DOWN_SIM_DET2_PIN_STATE_SHIFT		5
#define PALMAS_SIM_PWR_DOWN_PWRDNCNT_MASK			0x1F
#define PALMAS_SIM_PWR_DOWN_PWRDNCNT_SHIFT			0

/* Registers for function PMU_CONTROL */
#define PALMAS_DEV_CTRL						0x0
#define PALMAS_POWER_CTRL					0x1
#define PALMAS_VSYS_LO						0x2
#define PALMAS_VSYS_MON						0x3
#define PALMAS_VBAT_MON						0x4
#define PALMAS_WATCHDOG						0x5
#define PALMAS_BOOT_STATUS					0x6
#define PALMAS_BATTERY_BOUNCE					0x7
#define PALMAS_BACKUP_BATTERY_CTRL				0x8
#define PALMAS_LONG_PRESS_KEY					0x9
#define PALMAS_OSC_THERM_CTRL					0xA
#define PALMAS_BATDEBOUNCING					0xB
#define PALMAS_SWOFF_HWRST					0xF
#define PALMAS_SWOFF_COLDRST					0x10
#define PALMAS_SWOFF_STATUS					0x11
#define PALMAS_PMU_CONFIG					0x12
#define PALMAS_SPARE						0x14
#define PALMAS_PMU_SECONDARY_INT				0x15
#define PALMAS_SW_REVISION					0x17
#define PALMAS_EXT_CHRG_CTRL					0x18
#define PALMAS_PMU_SECONDARY_INT2				0x19
#define PALMAS_USB_CHGCTL1					0x1A
#define PALMAS_USB_CHGCTL2					0x1B

/* Bit definitions for DEV_CTRL */
#define PALMAS_DEV_CTRL_DEV_STATUS_MASK				0x0c
#define PALMAS_DEV_CTRL_DEV_STATUS_SHIFT			2
#define PALMAS_DEV_CTRL_SW_RST					0x02
#define PALMAS_DEV_CTRL_SW_RST_SHIFT				1
#define PALMAS_DEV_CTRL_DEV_ON					0x01
#define PALMAS_DEV_CTRL_DEV_ON_SHIFT				0

/* Bit definitions for POWER_CTRL */
#define PALMAS_POWER_CTRL_ENABLE2_MASK				0x04
#define PALMAS_POWER_CTRL_ENABLE2_MASK_SHIFT			2
#define PALMAS_POWER_CTRL_ENABLE1_MASK				0x02
#define PALMAS_POWER_CTRL_ENABLE1_MASK_SHIFT			1
#define PALMAS_POWER_CTRL_NSLEEP_MASK				0x01
#define PALMAS_POWER_CTRL_NSLEEP_MASK_SHIFT			0

/* Bit definitions for VSYS_LO */
#define PALMAS_VSYS_LO_THRESHOLD_MASK				0x1f
#define PALMAS_VSYS_LO_THRESHOLD_SHIFT				0

/* Bit definitions for VSYS_MON */
#define PALMAS_VSYS_MON_ENABLE					0x80
#define PALMAS_VSYS_MON_ENABLE_SHIFT				7
#define PALMAS_VSYS_MON_THRESHOLD_MASK				0x3f
#define PALMAS_VSYS_MON_THRESHOLD_SHIFT				0

/* Bit definitions for VBAT_MON */
#define PALMAS_VBAT_MON_ENABLE					0x80
#define PALMAS_VBAT_MON_ENABLE_SHIFT				7
#define PALMAS_VBAT_MON_THRESHOLD_MASK				0x3f
#define PALMAS_VBAT_MON_THRESHOLD_SHIFT				0

/* Bit definitions for WATCHDOG */
#define PALMAS_WATCHDOG_LOCK					0x20
#define PALMAS_WATCHDOG_LOCK_SHIFT				5
#define PALMAS_WATCHDOG_ENABLE					0x10
#define PALMAS_WATCHDOG_ENABLE_SHIFT				4
#define PALMAS_WATCHDOG_MODE					0x08
#define PALMAS_WATCHDOG_MODE_SHIFT				3
#define PALMAS_WATCHDOG_TIMER_MASK				0x07
#define PALMAS_WATCHDOG_TIMER_SHIFT				0

/* Bit definitions for BOOT_STATUS */
#define PALMAS_BOOT_STATUS_BOOT1				0x02
#define PALMAS_BOOT_STATUS_BOOT1_SHIFT				1
#define PALMAS_BOOT_STATUS_BOOT0				0x01
#define PALMAS_BOOT_STATUS_BOOT0_SHIFT				0

/* Bit definitions for BATTERY_BOUNCE */
#define PALMAS_BATTERY_BOUNCE_BB_DELAY_MASK			0x3f
#define PALMAS_BATTERY_BOUNCE_BB_DELAY_SHIFT			0

/* Bit definitions for BACKUP_BATTERY_CTRL */
#define PALMAS_BACKUP_BATTERY_CTRL_VRTC_18_15			0x80
#define PALMAS_BACKUP_BATTERY_CTRL_VRTC_18_15_SHIFT		7
#define PALMAS_BACKUP_BATTERY_CTRL_VRTC_EN_SLP			0x40
#define PALMAS_BACKUP_BATTERY_CTRL_VRTC_EN_SLP_SHIFT		6
#define PALMAS_BACKUP_BATTERY_CTRL_VRTC_EN_OFF			0x20
#define PALMAS_BACKUP_BATTERY_CTRL_VRTC_EN_OFF_SHIFT		5
#define PALMAS_BACKUP_BATTERY_CTRL_VRTC_PWEN			0x10
#define PALMAS_BACKUP_BATTERY_CTRL_VRTC_PWEN_SHIFT		4
#define PALMAS_BACKUP_BATTERY_CTRL_BBS_BBC_LOW_ICHRG		0x08
#define PALMAS_BACKUP_BATTERY_CTRL_BBS_BBC_LOW_ICHRG_SHIFT	3
#define PALMAS_BACKUP_BATTERY_CTRL_BB_SEL_MASK			0x06
#define PALMAS_BACKUP_BATTERY_CTRL_BB_SEL_SHIFT			1
#define PALMAS_BACKUP_BATTERY_CTRL_BB_CHG_EN			0x01
#define PALMAS_BACKUP_BATTERY_CTRL_BB_CHG_EN_SHIFT		0

/* Bit definitions for LONG_PRESS_KEY */
#define PALMAS_LONG_PRESS_KEY_LPK_LOCK				0x80
#define PALMAS_LONG_PRESS_KEY_LPK_LOCK_SHIFT			7
#define PALMAS_LONG_PRESS_KEY_LPK_INT_CLR			0x10
#define PALMAS_LONG_PRESS_KEY_LPK_INT_CLR_SHIFT			4
#define PALMAS_LONG_PRESS_KEY_LPK_TIME_MASK			0x0c
#define PALMAS_LONG_PRESS_KEY_LPK_TIME_SHIFT			2
#define PALMAS_LONG_PRESS_KEY_PWRON_DEBOUNCE_MASK		0x03
#define PALMAS_LONG_PRESS_KEY_PWRON_DEBOUNCE_SHIFT		0

/* Register bit values for various Long_Press_key durations */
#define PALMAS_LONG_PRESS_KEY_TIME_DEFAULT	-1
#define PALMAS_LONG_PRESS_KEY_TIME_6SECONDS	0
#define PALMAS_LONG_PRESS_KEY_TIME_8SECONDS	1
#define PALMAS_LONG_PRESS_KEY_TIME_10SECONDS	2
#define PALMAS_LONG_PRESS_KEY_TIME_12SECONDS	3

/* Bit definitions for OSC_THERM_CTRL */
#define PALMAS_OSC_THERM_CTRL_VANA_ON_IN_SLEEP			0x80
#define PALMAS_OSC_THERM_CTRL_VANA_ON_IN_SLEEP_SHIFT		7
#define PALMAS_OSC_THERM_CTRL_INT_MASK_IN_SLEEP			0x40
#define PALMAS_OSC_THERM_CTRL_INT_MASK_IN_SLEEP_SHIFT		6
#define PALMAS_OSC_THERM_CTRL_RC15MHZ_ON_IN_SLEEP		0x20
#define PALMAS_OSC_THERM_CTRL_RC15MHZ_ON_IN_SLEEP_SHIFT		5
#define PALMAS_OSC_THERM_CTRL_THERM_OFF_IN_SLEEP		0x10
#define PALMAS_OSC_THERM_CTRL_THERM_OFF_IN_SLEEP_SHIFT		4
#define PALMAS_OSC_THERM_CTRL_THERM_HD_SEL_MASK			0x0c
#define PALMAS_OSC_THERM_CTRL_THERM_HD_SEL_SHIFT		2
#define PALMAS_OSC_THERM_CTRL_OSC_BYPASS			0x02
#define PALMAS_OSC_THERM_CTRL_OSC_BYPASS_SHIFT			1
#define PALMAS_OSC_THERM_CTRL_OSC_HPMODE			0x01
#define PALMAS_OSC_THERM_CTRL_OSC_HPMODE_SHIFT			0

/* Bit definitions for BATDEBOUNCING */
#define PALMAS_BATDEBOUNCING_BAT_DEB_BYPASS			0x80
#define PALMAS_BATDEBOUNCING_BAT_DEB_BYPASS_SHIFT		7
#define PALMAS_BATDEBOUNCING_BINS_DEB_MASK			0x78
#define PALMAS_BATDEBOUNCING_BINS_DEB_SHIFT			3
#define PALMAS_BATDEBOUNCING_BEXT_DEB_MASK			0x07
#define PALMAS_BATDEBOUNCING_BEXT_DEB_SHIFT			0

/* Bit definitions for SWOFF_HWRST */
#define PALMAS_SWOFF_HWRST_PWRON_LPK				0x80
#define PALMAS_SWOFF_HWRST_PWRON_LPK_SHIFT			7
#define PALMAS_SWOFF_HWRST_PWRDOWN				0x40
#define PALMAS_SWOFF_HWRST_PWRDOWN_SHIFT			6
#define PALMAS_SWOFF_HWRST_WTD					0x20
#define PALMAS_SWOFF_HWRST_WTD_SHIFT				5
#define PALMAS_SWOFF_HWRST_TSHUT				0x10
#define PALMAS_SWOFF_HWRST_TSHUT_SHIFT				4
#define PALMAS_SWOFF_HWRST_RESET_IN				0x08
#define PALMAS_SWOFF_HWRST_RESET_IN_SHIFT			3
#define PALMAS_SWOFF_HWRST_SW_RST				0x04
#define PALMAS_SWOFF_HWRST_SW_RST_SHIFT				2
#define PALMAS_SWOFF_HWRST_VSYS_LO				0x02
#define PALMAS_SWOFF_HWRST_VSYS_LO_SHIFT			1
#define PALMAS_SWOFF_HWRST_GPADC_SHUTDOWN			0x01
#define PALMAS_SWOFF_HWRST_GPADC_SHUTDOWN_SHIFT			0

/* Register bit values for poweron_lpk */
#define PALMAS_SWOFF_COLDRST_PWRON_LPK_DEFAULT		-1
#define PALMAS_SWOFF_COLDRST_PWRON_LPK_SHUTDOWN	0
#define PALMAS_SWOFF_COLDRST_PWRON_LPK_RESTART		1

/* Bit definitions for SWOFF_COLDRST */
#define PALMAS_SWOFF_COLDRST_PWRON_LPK				0x80
#define PALMAS_SWOFF_COLDRST_PWRON_LPK_SHIFT			7
#define PALMAS_SWOFF_COLDRST_PWRDOWN				0x40
#define PALMAS_SWOFF_COLDRST_PWRDOWN_SHIFT			6
#define PALMAS_SWOFF_COLDRST_WTD				0x20
#define PALMAS_SWOFF_COLDRST_WTD_SHIFT				5
#define PALMAS_SWOFF_COLDRST_TSHUT				0x10
#define PALMAS_SWOFF_COLDRST_TSHUT_SHIFT			4
#define PALMAS_SWOFF_COLDRST_RESET_IN				0x08
#define PALMAS_SWOFF_COLDRST_RESET_IN_SHIFT			3
#define PALMAS_SWOFF_COLDRST_SW_RST				0x04
#define PALMAS_SWOFF_COLDRST_SW_RST_SHIFT			2
#define PALMAS_SWOFF_COLDRST_VSYS_LO				0x02
#define PALMAS_SWOFF_COLDRST_VSYS_LO_SHIFT			1
#define PALMAS_SWOFF_COLDRST_GPADC_SHUTDOWN			0x01
#define PALMAS_SWOFF_COLDRST_GPADC_SHUTDOWN_SHIFT		0

/* Bit definitions for SWOFF_STATUS */
#define PALMAS_SWOFF_STATUS_PWRON_LPK				0x80
#define PALMAS_SWOFF_STATUS_PWRON_LPK_SHIFT			7
#define PALMAS_SWOFF_STATUS_PWRDOWN				0x40
#define PALMAS_SWOFF_STATUS_PWRDOWN_SHIFT			6
#define PALMAS_SWOFF_STATUS_WTD					0x20
#define PALMAS_SWOFF_STATUS_WTD_SHIFT				5
#define PALMAS_SWOFF_STATUS_TSHUT				0x10
#define PALMAS_SWOFF_STATUS_TSHUT_SHIFT				4
#define PALMAS_SWOFF_STATUS_RESET_IN				0x08
#define PALMAS_SWOFF_STATUS_RESET_IN_SHIFT			3
#define PALMAS_SWOFF_STATUS_SW_RST				0x04
#define PALMAS_SWOFF_STATUS_SW_RST_SHIFT			2
#define PALMAS_SWOFF_STATUS_VSYS_LO				0x02
#define PALMAS_SWOFF_STATUS_VSYS_LO_SHIFT			1
#define PALMAS_SWOFF_STATUS_GPADC_SHUTDOWN			0x01
#define PALMAS_SWOFF_STATUS_GPADC_SHUTDOWN_SHIFT		0

/* Bit definitions for PMU_CONFIG */
#define PALMAS_PMU_CONFIG_MULTI_CELL_EN				0x40
#define PALMAS_PMU_CONFIG_MULTI_CELL_EN_SHIFT			6
#define PALMAS_PMU_CONFIG_SPARE_MASK				0x30
#define PALMAS_PMU_CONFIG_SPARE_SHIFT				4
#define PALMAS_PMU_CONFIG_SWOFF_DLY_MASK			0x0c
#define PALMAS_PMU_CONFIG_SWOFF_DLY_SHIFT			2
#define PALMAS_PMU_CONFIG_GATE_RESET_OUT			0x02
#define PALMAS_PMU_CONFIG_GATE_RESET_OUT_SHIFT			1
#define PALMAS_PMU_CONFIG_AUTODEVON				0x01
#define PALMAS_PMU_CONFIG_AUTODEVON_SHIFT			0

/* Bit definitions for SPARE */
#define PALMAS_SPARE_SPARE_MASK					0xf8
#define PALMAS_SPARE_SPARE_SHIFT				3
#define PALMAS_SPARE_REGEN3_OD					0x04
#define PALMAS_SPARE_REGEN3_OD_SHIFT				2
#define PALMAS_SPARE_REGEN2_OD					0x02
#define PALMAS_SPARE_REGEN2_OD_SHIFT				1
#define PALMAS_SPARE_REGEN1_OD					0x01
#define PALMAS_SPARE_REGEN1_OD_SHIFT				0

/* Bit definitions for PMU_SECONDARY_INT */
#define PALMAS_PMU_SECONDARY_INT_VBUS_OVV_INT_SRC		0x80
#define PALMAS_PMU_SECONDARY_INT_VBUS_OVV_INT_SRC_SHIFT		7
#define PALMAS_PMU_SECONDARY_INT_CHARG_DET_N_INT_SRC		0x40
#define PALMAS_PMU_SECONDARY_INT_CHARG_DET_N_INT_SRC_SHIFT	6
#define PALMAS_PMU_SECONDARY_INT_BB_INT_SRC			0x20
#define PALMAS_PMU_SECONDARY_INT_BB_INT_SRC_SHIFT		5
#define PALMAS_PMU_SECONDARY_INT_FBI_INT_SRC			0x10
#define PALMAS_PMU_SECONDARY_INT_FBI_INT_SRC_SHIFT		4
#define PALMAS_PMU_SECONDARY_INT_VBUS_OVV_MASK			0x08
#define PALMAS_PMU_SECONDARY_INT_VBUS_OVV_MASK_SHIFT		3
#define PALMAS_PMU_SECONDARY_INT_CHARG_DET_N_MASK		0x04
#define PALMAS_PMU_SECONDARY_INT_CHARG_DET_N_MASK_SHIFT		2
#define PALMAS_PMU_SECONDARY_INT_BB_MASK			0x02
#define PALMAS_PMU_SECONDARY_INT_BB_MASK_SHIFT			1
#define PALMAS_PMU_SECONDARY_INT_FBI_MASK			0x01
#define PALMAS_PMU_SECONDARY_INT_FBI_MASK_SHIFT			0

/* Bit definitions for SW_REVISION */
#define PALMAS_SW_REVISION_SW_REVISION_MASK			0xff
#define PALMAS_SW_REVISION_SW_REVISION_SHIFT			0

/* Bit definitions for EXT_CHRG_CTRL */
#define PALMAS_EXT_CHRG_CTRL_VBUS_OVV_STATUS			0x80
#define PALMAS_EXT_CHRG_CTRL_VBUS_OVV_STATUS_SHIFT		7
#define PALMAS_EXT_CHRG_CTRL_CHARG_DET_N_STATUS			0x40
#define PALMAS_EXT_CHRG_CTRL_CHARG_DET_N_STATUS_SHIFT		6
#define PALMAS_EXT_CHRG_CTRL_VSYS_DEBOUNCE_DELAY		0x08
#define PALMAS_EXT_CHRG_CTRL_VSYS_DEBOUNCE_DELAY_SHIFT		3
#define PALMAS_EXT_CHRG_CTRL_CHRG_DET_N				0x04
#define PALMAS_EXT_CHRG_CTRL_CHRG_DET_N_SHIFT			2
#define PALMAS_EXT_CHRG_CTRL_AUTO_ACA_EN			0x02
#define PALMAS_EXT_CHRG_CTRL_AUTO_ACA_EN_SHIFT			1
#define PALMAS_EXT_CHRG_CTRL_AUTO_LDOUSB_EN			0x01
#define PALMAS_EXT_CHRG_CTRL_AUTO_LDOUSB_EN_SHIFT		0

/* Bit definitions for PMU_SECONDARY_INT2 */
#define PALMAS_PMU_SECONDARY_INT2_DVFS2_INT_SRC			0x20
#define PALMAS_PMU_SECONDARY_INT2_DVFS2_INT_SRC_SHIFT		5
#define PALMAS_PMU_SECONDARY_INT2_DVFS1_INT_SRC			0x10
#define PALMAS_PMU_SECONDARY_INT2_DVFS1_INT_SRC_SHIFT		4
#define PALMAS_PMU_SECONDARY_INT2_DVFS2_MASK			0x02
#define PALMAS_PMU_SECONDARY_INT2_DVFS2_MASK_SHIFT		1
#define PALMAS_PMU_SECONDARY_INT2_DVFS1_MASK			0x01
#define PALMAS_PMU_SECONDARY_INT2_DVFS1_MASK_SHIFT		0

/* Bit definitions for USB_CHGCTL1 */
#define PALMAS_USB_CHGCTL1_USB_SUSPEND				0x04

/* Bit definitions for USB_CHGCTL2 */
#define PALMAS_USB_CHGCTL2_BOOST_EN				0x08

/* Registers for function RESOURCE */
#define PALMAS_CLK32KG_CTRL					0x0
#define PALMAS_CLK32KGAUDIO_CTRL				0x1
#define PALMAS_REGEN1_CTRL					0x2
#define PALMAS_REGEN2_CTRL					0x3
#define PALMAS_SYSEN1_CTRL					0x4
#define PALMAS_SYSEN2_CTRL					0x5
#define PALMAS_NSLEEP_RES_ASSIGN				0x6
#define PALMAS_NSLEEP_SMPS_ASSIGN				0x7
#define PALMAS_NSLEEP_LDO_ASSIGN1				0x8
#define PALMAS_NSLEEP_LDO_ASSIGN2				0x9
#define PALMAS_ENABLE1_RES_ASSIGN				0xA
#define PALMAS_ENABLE1_SMPS_ASSIGN				0xB
#define PALMAS_ENABLE1_LDO_ASSIGN1				0xC
#define PALMAS_ENABLE1_LDO_ASSIGN2				0xD
#define PALMAS_ENABLE2_RES_ASSIGN				0xE
#define PALMAS_ENABLE2_SMPS_ASSIGN				0xF
#define PALMAS_ENABLE2_LDO_ASSIGN1				0x10
#define PALMAS_ENABLE2_LDO_ASSIGN2				0x11
#define PALMAS_REGEN3_CTRL					0x12
#define PALMAS_REGEN4_CTRL					0x13
#define PALMAS_REGEN5_CTRL					0x14
#define PALMAS_REGEN7_CTRL					0x16
#define PALMAS_NSLEEP_RES_ASSIGN2				0x18
#define PALMAS_ENABLE1_RES_ASSIGN2				0x19
#define PALMAS_ENABLE2_RES_ASSIGN2				0x1A

/* Bit definitions for CLK32KG_CTRL */
#define PALMAS_CLK32KG_CTRL_STATUS				0x10
#define PALMAS_CLK32KG_CTRL_STATUS_SHIFT			4
#define PALMAS_CLK32KG_CTRL_MODE_SLEEP				0x04
#define PALMAS_CLK32KG_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_CLK32KG_CTRL_MODE_ACTIVE				0x01
#define PALMAS_CLK32KG_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for CLK32KGAUDIO_CTRL */
#define PALMAS_CLK32KGAUDIO_CTRL_STATUS				0x10
#define PALMAS_CLK32KGAUDIO_CTRL_STATUS_SHIFT			4
#define PALMAS_CLK32KGAUDIO_CTRL_RESERVED3			0x08
#define PALMAS_CLK32KGAUDIO_CTRL_RESERVED3_SHIFT		3
#define PALMAS_CLK32KGAUDIO_CTRL_MODE_SLEEP			0x04
#define PALMAS_CLK32KGAUDIO_CTRL_MODE_SLEEP_SHIFT		2
#define PALMAS_CLK32KGAUDIO_CTRL_MODE_ACTIVE			0x01
#define PALMAS_CLK32KGAUDIO_CTRL_MODE_ACTIVE_SHIFT		0

/* Bit definitions for REGEN1_CTRL */
#define PALMAS_REGEN1_CTRL_STATUS				0x10
#define PALMAS_REGEN1_CTRL_STATUS_SHIFT				4
#define PALMAS_REGEN1_CTRL_MODE_SLEEP				0x04
#define PALMAS_REGEN1_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_REGEN1_CTRL_MODE_ACTIVE				0x01
#define PALMAS_REGEN1_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for REGEN2_CTRL */
#define PALMAS_REGEN2_CTRL_STATUS				0x10
#define PALMAS_REGEN2_CTRL_STATUS_SHIFT				4
#define PALMAS_REGEN2_CTRL_MODE_SLEEP				0x04
#define PALMAS_REGEN2_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_REGEN2_CTRL_MODE_ACTIVE				0x01
#define PALMAS_REGEN2_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for SYSEN1_CTRL */
#define PALMAS_SYSEN1_CTRL_STATUS				0x10
#define PALMAS_SYSEN1_CTRL_STATUS_SHIFT				4
#define PALMAS_SYSEN1_CTRL_MODE_SLEEP				0x04
#define PALMAS_SYSEN1_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_SYSEN1_CTRL_MODE_ACTIVE				0x01
#define PALMAS_SYSEN1_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for SYSEN2_CTRL */
#define PALMAS_SYSEN2_CTRL_STATUS				0x10
#define PALMAS_SYSEN2_CTRL_STATUS_SHIFT				4
#define PALMAS_SYSEN2_CTRL_MODE_SLEEP				0x04
#define PALMAS_SYSEN2_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_SYSEN2_CTRL_MODE_ACTIVE				0x01
#define PALMAS_SYSEN2_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for NSLEEP_RES_ASSIGN */
#define PALMAS_NSLEEP_RES_ASSIGN_REGEN4				0x80
#define PALMAS_NSLEEP_RES_ASSIGN_REGEN4_SHIFT			7
#define PALMAS_NSLEEP_RES_ASSIGN_REGEN3				0x40
#define PALMAS_NSLEEP_RES_ASSIGN_REGEN3_SHIFT			6
#define PALMAS_NSLEEP_RES_ASSIGN_CLK32KGAUDIO			0x20
#define PALMAS_NSLEEP_RES_ASSIGN_CLK32KGAUDIO_SHIFT		5
#define PALMAS_NSLEEP_RES_ASSIGN_CLK32KG			0x10
#define PALMAS_NSLEEP_RES_ASSIGN_CLK32KG_SHIFT			4
#define PALMAS_NSLEEP_RES_ASSIGN_SYSEN2				0x08
#define PALMAS_NSLEEP_RES_ASSIGN_SYSEN2_SHIFT			3
#define PALMAS_NSLEEP_RES_ASSIGN_SYSEN1				0x04
#define PALMAS_NSLEEP_RES_ASSIGN_SYSEN1_SHIFT			2
#define PALMAS_NSLEEP_RES_ASSIGN_REGEN2				0x02
#define PALMAS_NSLEEP_RES_ASSIGN_REGEN2_SHIFT			1
#define PALMAS_NSLEEP_RES_ASSIGN_REGEN1				0x01
#define PALMAS_NSLEEP_RES_ASSIGN_REGEN1_SHIFT			0

/* Bit definitions for NSLEEP_SMPS_ASSIGN */
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS10			0x80
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS10_SHIFT			7
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS9				0x40
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS9_SHIFT			6
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS8				0x20
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS8_SHIFT			5
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS7				0x10
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS7_SHIFT			4
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS6				0x08
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS6_SHIFT			3
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS45			0x04
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS45_SHIFT			2
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS3				0x02
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS3_SHIFT			1
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS12			0x01
#define PALMAS_NSLEEP_SMPS_ASSIGN_SMPS12_SHIFT			0

/* Bit definitions for NSLEEP_LDO_ASSIGN1 */
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO8				0x80
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO8_SHIFT			7
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO7				0x40
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO7_SHIFT			6
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO6				0x20
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO6_SHIFT			5
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO5				0x10
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO5_SHIFT			4
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO4				0x08
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO4_SHIFT			3
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO3				0x04
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO3_SHIFT			2
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO2				0x02
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO2_SHIFT			1
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO1				0x01
#define PALMAS_NSLEEP_LDO_ASSIGN1_LDO1_SHIFT			0

/* Bit definitions for NSLEEP_LDO_ASSIGN2 */
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDO14				0x80
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDO14_SHIFT			7
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDO13				0x40
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDO13_SHIFT			6
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDO12				0x20
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDO12_SHIFT			5
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDO11				0x10
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDO11_SHIFT			4
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDO10				0x08
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDO10_SHIFT			3
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDOUSB			0x04
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDOUSB_SHIFT			2
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDOLN				0x02
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDOLN_SHIFT			1
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDO9				0x01
#define PALMAS_NSLEEP_LDO_ASSIGN2_LDO9_SHIFT			0

/* Bit definitions for ENABLE1_RES_ASSIGN */
#define PALMAS_ENABLE1_RES_ASSIGN_REGEN4			0x80
#define PALMAS_ENABLE1_RES_ASSIGN_REGEN4_SHIFT			7
#define PALMAS_ENABLE1_RES_ASSIGN_REGEN3			0x40
#define PALMAS_ENABLE1_RES_ASSIGN_REGEN3_SHIFT			6
#define PALMAS_ENABLE1_RES_ASSIGN_CLK32KGAUDIO			0x20
#define PALMAS_ENABLE1_RES_ASSIGN_CLK32KGAUDIO_SHIFT		5
#define PALMAS_ENABLE1_RES_ASSIGN_CLK32KG			0x10
#define PALMAS_ENABLE1_RES_ASSIGN_CLK32KG_SHIFT			4
#define PALMAS_ENABLE1_RES_ASSIGN_SYSEN2			0x08
#define PALMAS_ENABLE1_RES_ASSIGN_SYSEN2_SHIFT			3
#define PALMAS_ENABLE1_RES_ASSIGN_SYSEN1			0x04
#define PALMAS_ENABLE1_RES_ASSIGN_SYSEN1_SHIFT			2
#define PALMAS_ENABLE1_RES_ASSIGN_REGEN2			0x02
#define PALMAS_ENABLE1_RES_ASSIGN_REGEN2_SHIFT			1
#define PALMAS_ENABLE1_RES_ASSIGN_REGEN1			0x01
#define PALMAS_ENABLE1_RES_ASSIGN_REGEN1_SHIFT			0

/* Bit definitions for ENABLE1_SMPS_ASSIGN */
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS10			0x80
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS10_SHIFT			7
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS9			0x40
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS9_SHIFT			6
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS8			0x20
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS8_SHIFT			5
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS7			0x10
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS7_SHIFT			4
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS6			0x08
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS6_SHIFT			3
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS45			0x04
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS45_SHIFT			2
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS3			0x02
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS3_SHIFT			1
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS12			0x01
#define PALMAS_ENABLE1_SMPS_ASSIGN_SMPS12_SHIFT			0

/* Bit definitions for ENABLE1_LDO_ASSIGN1 */
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO8				0x80
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO8_SHIFT			7
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO7				0x40
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO7_SHIFT			6
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO6				0x20
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO6_SHIFT			5
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO5				0x10
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO5_SHIFT			4
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO4				0x08
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO4_SHIFT			3
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO3				0x04
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO3_SHIFT			2
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO2				0x02
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO2_SHIFT			1
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO1				0x01
#define PALMAS_ENABLE1_LDO_ASSIGN1_LDO1_SHIFT			0

/* Bit definitions for ENABLE1_LDO_ASSIGN2 */
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDO14			0x80
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDO14_SHIFT			7
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDO13			0x40
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDO13_SHIFT			6
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDO12			0x20
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDO12_SHIFT			5
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDO11			0x10
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDO11_SHIFT			4
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDO10			0x08
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDO10_SHIFT			3
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDOUSB			0x04
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDOUSB_SHIFT			2
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDOLN			0x02
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDOLN_SHIFT			1
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDO9				0x01
#define PALMAS_ENABLE1_LDO_ASSIGN2_LDO9_SHIFT			0

/* Bit definitions for ENABLE2_RES_ASSIGN */
#define PALMAS_ENABLE2_RES_ASSIGN_REGEN4			0x80
#define PALMAS_ENABLE2_RES_ASSIGN_REGEN4_SHIFT			7
#define PALMAS_ENABLE2_RES_ASSIGN_REGEN3			0x40
#define PALMAS_ENABLE2_RES_ASSIGN_REGEN3_SHIFT			6
#define PALMAS_ENABLE2_RES_ASSIGN_CLK32KGAUDIO			0x20
#define PALMAS_ENABLE2_RES_ASSIGN_CLK32KGAUDIO_SHIFT		5
#define PALMAS_ENABLE2_RES_ASSIGN_CLK32KG			0x10
#define PALMAS_ENABLE2_RES_ASSIGN_CLK32KG_SHIFT			4
#define PALMAS_ENABLE2_RES_ASSIGN_SYSEN2			0x08
#define PALMAS_ENABLE2_RES_ASSIGN_SYSEN2_SHIFT			3
#define PALMAS_ENABLE2_RES_ASSIGN_SYSEN1			0x04
#define PALMAS_ENABLE2_RES_ASSIGN_SYSEN1_SHIFT			2
#define PALMAS_ENABLE2_RES_ASSIGN_REGEN2			0x02
#define PALMAS_ENABLE2_RES_ASSIGN_REGEN2_SHIFT			1
#define PALMAS_ENABLE2_RES_ASSIGN_REGEN1			0x01
#define PALMAS_ENABLE2_RES_ASSIGN_REGEN1_SHIFT			0

/* Bit definitions for ENABLE2_SMPS_ASSIGN */
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS10			0x80
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS10_SHIFT			7
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS9			0x40
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS9_SHIFT			6
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS8			0x20
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS8_SHIFT			5
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS7			0x10
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS7_SHIFT			4
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS6			0x08
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS6_SHIFT			3
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS45			0x04
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS45_SHIFT			2
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS3			0x02
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS3_SHIFT			1
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS12			0x01
#define PALMAS_ENABLE2_SMPS_ASSIGN_SMPS12_SHIFT			0

/* Bit definitions for ENABLE2_LDO_ASSIGN1 */
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO8				0x80
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO8_SHIFT			7
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO7				0x40
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO7_SHIFT			6
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO6				0x20
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO6_SHIFT			5
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO5				0x10
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO5_SHIFT			4
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO4				0x08
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO4_SHIFT			3
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO3				0x04
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO3_SHIFT			2
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO2				0x02
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO2_SHIFT			1
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO1				0x01
#define PALMAS_ENABLE2_LDO_ASSIGN1_LDO1_SHIFT			0

/* Bit definitions for ENABLE2_LDO_ASSIGN2 */
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDO14			0x80
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDO14_SHIFT			7
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDO13			0x40
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDO13_SHIFT			6
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDO12			0x20
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDO12_SHIFT			5
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDO11			0x10
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDO11_SHIFT			4
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDO10			0x08
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDO10_SHIFT			3
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDOUSB			0x04
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDOUSB_SHIFT			2
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDOLN			0x02
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDOLN_SHIFT			1
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDO9				0x01
#define PALMAS_ENABLE2_LDO_ASSIGN2_LDO9_SHIFT			0

/* Bit definitions for REGEN3_CTRL */
#define PALMAS_REGEN3_CTRL_STATUS				0x10
#define PALMAS_REGEN3_CTRL_STATUS_SHIFT				4
#define PALMAS_REGEN3_CTRL_MODE_SLEEP				0x04
#define PALMAS_REGEN3_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_REGEN3_CTRL_MODE_ACTIVE				0x01
#define PALMAS_REGEN3_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for REGEN4_CTRL */
#define PALMAS_REGEN4_CTRL_STATUS				0x10
#define PALMAS_REGEN4_CTRL_STATUS_SHIFT				4
#define PALMAS_REGEN4_CTRL_MODE_SLEEP				0x04
#define PALMAS_REGEN4_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_REGEN4_CTRL_MODE_ACTIVE				0x01
#define PALMAS_REGEN4_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for REGEN5_CTRL */
#define PALMAS_REGEN5_CTRL_STATUS				0x10
#define PALMAS_REGEN5_CTRL_STATUS_SHIFT				4
#define PALMAS_REGEN5_CTRL_MODE_SLEEP				0x04
#define PALMAS_REGEN5_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_REGEN5_CTRL_MODE_ACTIVE				0x01
#define PALMAS_REGEN5_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for REGEN7_CTRL */
#define PALMAS_REGEN7_CTRL_STATUS				0x10
#define PALMAS_REGEN7_CTRL_STATUS_SHIFT				4
#define PALMAS_REGEN7_CTRL_MODE_SLEEP				0x04
#define PALMAS_REGEN7_CTRL_MODE_SLEEP_SHIFT			2
#define PALMAS_REGEN7_CTRL_MODE_ACTIVE				0x01
#define PALMAS_REGEN7_CTRL_MODE_ACTIVE_SHIFT			0

/* Bit definitions for NSLEEP_RES_ASSIGN2 */
#define PALMAS_NSLEEP_RES_ASSIGN_REGEN7				0x04
#define PALMAS_NSLEEP_RES_ASSIGN_REGEN7_SHIFT			2
#define PALMAS_NSLEEP_RES_ASSIGN_REGEN5				0x01
#define PALMAS_NSLEEP_RES_ASSIGN_REGEN5_SHIFT			0

/* Bit definitions for ENABLE1_RES_ASSIGN2 */
#define PALMAS_ENABLE1_RES_ASSIGN_REGEN7			0x04
#define PALMAS_ENABLE1_RES_ASSIGN_REGEN7_SHIFT			2
#define PALMAS_ENABLE1_RES_ASSIGN_REGEN5			0x01
#define PALMAS_ENABLE1_RES_ASSIGN_REGEN5_SHIFT			0

/* Bit definitions for ENABLE2_RES_ASSIGN2 */
#define PALMAS_ENABLE2_RES_ASSIGN_REGEN7			0x04
#define PALMAS_ENABLE2_RES_ASSIGN_REGEN7_SHIFT			2
#define PALMAS_ENABLE2_RES_ASSIGN_REGEN5			0x01
#define PALMAS_ENABLE2_RES_ASSIGN_REGEN5_SHIFT			0

/* Registers for function PAD_CONTROL */
#define PALMAS_OD_OUTPUT_CTRL2					0x2
#define PALMAS_POLARITY_CTRL2					0x3
#define PALMAS_PU_PD_INPUT_CTRL1				0x4
#define PALMAS_PU_PD_INPUT_CTRL2				0x5
#define PALMAS_PU_PD_INPUT_CTRL3				0x6
#define PALMAS_PU_PD_INPUT_CTRL5				0x7
#define PALMAS_OD_OUTPUT_CTRL					0x8
#define PALMAS_POLARITY_CTRL					0x9
#define PALMAS_PRIMARY_SECONDARY_PAD1				0xA
#define PALMAS_PRIMARY_SECONDARY_PAD2				0xB
#define PALMAS_I2C_SPI						0xC
#define PALMAS_PU_PD_INPUT_CTRL4				0xD
#define PALMAS_PRIMARY_SECONDARY_PAD3				0xE
#define PALMAS_PRIMARY_SECONDARY_PAD4				0xF

/* Bit definitions for OD_OUTPUT_CTRL2 */
#define PALMAS_OD_OUTPUT_CTRL2_OD_REGEN7			0x40
#define PALMAS_OD_OUTPUT_CTRL2_OD_REGEN7_SHIFT			6
#define PALMAS_OD_OUTPUT_CTRL2_OD_REGEN5			0x10
#define PALMAS_OD_OUTPUT_CTRL2_OD_REGEN5_SHIFT			4
#define PALMAS_OD_OUTPUT_CTRL2_OD_REGEN4			0x08
#define PALMAS_OD_OUTPUT_CTRL2_OD_REGEN4_SHIFT			3
#define PALMAS_OD_OUTPUT_CTRL2_OD_REGEN2			0x02
#define PALMAS_OD_OUTPUT_CTRL2_OD_REGEN2_SHIFT			1
#define PALMAS_OD_OUTPUT_CTRL2_OD_REGEN1			0x01
#define PALMAS_OD_OUTPUT_CTRL2_OD_REGEN1_SHIFT			0

/* Bit definitions for POLARITY_CTRL2 */
#define PALMAS_POLARITY_CTRL2_DET_POLARITY			0x01
#define PALMAS_POLARITY_CTRL2_DET_POLARITY_SHIFT		0

/* Bit definitions for PU_PD_INPUT_CTRL1 */
#define PALMAS_PU_PD_INPUT_CTRL1_RESET_IN_PD			0x40
#define PALMAS_PU_PD_INPUT_CTRL1_RESET_IN_PD_SHIFT		6
#define PALMAS_PU_PD_INPUT_CTRL1_GPADC_START_PU			0x20
#define PALMAS_PU_PD_INPUT_CTRL1_GPADC_START_PU_SHIFT		5
#define PALMAS_PU_PD_INPUT_CTRL1_GPADC_START_PD			0x10
#define PALMAS_PU_PD_INPUT_CTRL1_GPADC_START_PD_SHIFT		4
#define PALMAS_PU_PD_INPUT_CTRL1_PWRDOWN_PD			0x04
#define PALMAS_PU_PD_INPUT_CTRL1_PWRDOWN_PD_SHIFT		2
#define PALMAS_PU_PD_INPUT_CTRL1_NRESWARM_PU			0x02
#define PALMAS_PU_PD_INPUT_CTRL1_NRESWARM_PU_SHIFT		1

/* Bit definitions for PU_PD_INPUT_CTRL2 */
#define PALMAS_PU_PD_INPUT_CTRL2_ENABLE2_PU			0x20
#define PALMAS_PU_PD_INPUT_CTRL2_ENABLE2_PU_SHIFT		5
#define PALMAS_PU_PD_INPUT_CTRL2_ENABLE2_PD			0x10
#define PALMAS_PU_PD_INPUT_CTRL2_ENABLE2_PD_SHIFT		4
#define PALMAS_PU_PD_INPUT_CTRL2_ENABLE1_PU			0x08
#define PALMAS_PU_PD_INPUT_CTRL2_ENABLE1_PU_SHIFT		3
#define PALMAS_PU_PD_INPUT_CTRL2_ENABLE1_PD			0x04
#define PALMAS_PU_PD_INPUT_CTRL2_ENABLE1_PD_SHIFT		2
#define PALMAS_PU_PD_INPUT_CTRL2_NSLEEP_PU			0x02
#define PALMAS_PU_PD_INPUT_CTRL2_NSLEEP_PU_SHIFT		1
#define PALMAS_PU_PD_INPUT_CTRL2_NSLEEP_PD			0x01
#define PALMAS_PU_PD_INPUT_CTRL2_NSLEEP_PD_SHIFT		0

/* Bit definitions for PU_PD_INPUT_CTRL3 */
#define PALMAS_PU_PD_INPUT_CTRL3_ACOK_PD			0x40
#define PALMAS_PU_PD_INPUT_CTRL3_ACOK_PD_SHIFT			6
#define PALMAS_PU_PD_INPUT_CTRL3_CHRG_DET_N_PD			0x10
#define PALMAS_PU_PD_INPUT_CTRL3_CHRG_DET_N_PD_SHIFT		4
#define PALMAS_PU_PD_INPUT_CTRL3_POWERHOLD_PD			0x04
#define PALMAS_PU_PD_INPUT_CTRL3_POWERHOLD_PD_SHIFT		2
#define PALMAS_PU_PD_INPUT_CTRL3_MSECURE_PD			0x01
#define PALMAS_PU_PD_INPUT_CTRL3_MSECURE_PD_SHIFT		0

/* Bit definitions for PU_PD_INPUT_CTRL5 */
#define PALMAS_PU_PD_INPUT_CTRL5_DET2_PU			0x80
#define PALMAS_PU_PD_INPUT_CTRL5_DET2_PU_SHIFT			7
#define PALMAS_PU_PD_INPUT_CTRL5_DET2_PD			0x40
#define PALMAS_PU_PD_INPUT_CTRL5_DET2_PD_SHIFT			6
#define PALMAS_PU_PD_INPUT_CTRL5_DET1_PU			0x20
#define PALMAS_PU_PD_INPUT_CTRL5_DET1_PU_SHIFT			5
#define PALMAS_PU_PD_INPUT_CTRL5_DET1_PD			0x10
#define PALMAS_PU_PD_INPUT_CTRL5_DET1_PD_SHIFT			4

/* Bit definitions for OD_OUTPUT_CTRL */
#define PALMAS_OD_OUTPUT_CTRL_PWM_2_OD				0x80
#define PALMAS_OD_OUTPUT_CTRL_PWM_2_OD_SHIFT			7
#define PALMAS_OD_OUTPUT_CTRL_VBUSDET_OD			0x40
#define PALMAS_OD_OUTPUT_CTRL_VBUSDET_OD_SHIFT			6
#define PALMAS_OD_OUTPUT_CTRL_PWM_1_OD				0x20
#define PALMAS_OD_OUTPUT_CTRL_PWM_1_OD_SHIFT			5
#define PALMAS_OD_OUTPUT_CTRL_INT_OD				0x08
#define PALMAS_OD_OUTPUT_CTRL_INT_OD_SHIFT			3

/* Bit definitions for POLARITY_CTRL */
#define PALMAS_POLARITY_CTRL_INT_POLARITY			0x80
#define PALMAS_POLARITY_CTRL_INT_POLARITY_SHIFT			7
#define PALMAS_POLARITY_CTRL_ENABLE2_POLARITY			0x40
#define PALMAS_POLARITY_CTRL_ENABLE2_POLARITY_SHIFT		6
#define PALMAS_POLARITY_CTRL_ENABLE1_POLARITY			0x20
#define PALMAS_POLARITY_CTRL_ENABLE1_POLARITY_SHIFT		5
#define PALMAS_POLARITY_CTRL_NSLEEP_POLARITY			0x10
#define PALMAS_POLARITY_CTRL_NSLEEP_POLARITY_SHIFT		4
#define PALMAS_POLARITY_CTRL_RESET_IN_POLARITY			0x08
#define PALMAS_POLARITY_CTRL_RESET_IN_POLARITY_SHIFT		3
#define PALMAS_POLARITY_CTRL_GPIO_3_CHRG_DET_N_POLARITY		0x04
#define PALMAS_POLARITY_CTRL_GPIO_3_CHRG_DET_N_POLARITY_SHIFT	2
#define PALMAS_POLARITY_CTRL_POWERGOOD_USB_PSEL_POLARITY	0x02
#define PALMAS_POLARITY_CTRL_POWERGOOD_USB_PSEL_POLARITY_SHIFT	1
#define PALMAS_POLARITY_CTRL_PWRDOWN_POLARITY			0x01
#define PALMAS_POLARITY_CTRL_PWRDOWN_POLARITY_SHIFT		0

/* Bit definitions for PRIMARY_SECONDARY_PAD1 */
#define PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_3			0x80
#define PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_3_SHIFT		7
#define PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_2_MASK		0x60
#define PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_2_SHIFT		5
#define PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_1_MASK		0x18
#define PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_1_SHIFT		3
#define PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_0			0x04
#define PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_0_SHIFT		2
#define PALMAS_PRIMARY_SECONDARY_PAD1_VAC			0x02
#define PALMAS_PRIMARY_SECONDARY_PAD1_VAC_SHIFT			1
#define PALMAS_PRIMARY_SECONDARY_PAD1_POWERGOOD			0x01
#define PALMAS_PRIMARY_SECONDARY_PAD1_POWERGOOD_SHIFT		0

/* Bit definitions for PRIMARY_SECONDARY_PAD2 */
#define PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_4_MSB		0x04
#define PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_4_MSB_SHIFT		6
#define PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_7_MASK		0x30
#define PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_7_SHIFT		4
#define PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_6			0x08
#define PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_6_SHIFT		3
#define PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_5_MASK		0x06
#define PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_5_SHIFT		1
#define PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_4			0x01
#define PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_4_SHIFT		0

/* Bit definitions for I2C_SPI */
#define PALMAS_I2C_SPI_I2C2OTP_EN				0x80
#define PALMAS_I2C_SPI_I2C2OTP_EN_SHIFT				7
#define PALMAS_I2C_SPI_I2C2OTP_PAGESEL				0x40
#define PALMAS_I2C_SPI_I2C2OTP_PAGESEL_SHIFT			6
#define PALMAS_I2C_SPI_ID_I2C2					0x20
#define PALMAS_I2C_SPI_ID_I2C2_SHIFT				5
#define PALMAS_I2C_SPI_I2C_SPI					0x10
#define PALMAS_I2C_SPI_I2C_SPI_SHIFT				4
#define PALMAS_I2C_SPI_ID_I2C1_MASK				0x0f
#define PALMAS_I2C_SPI_ID_I2C1_SHIFT				0

/* Bit definitions for PU_PD_INPUT_CTRL4 */
#define PALMAS_PU_PD_INPUT_CTRL4_DVFS2_DAT_PD			0x40
#define PALMAS_PU_PD_INPUT_CTRL4_DVFS2_DAT_PD_SHIFT		6
#define PALMAS_PU_PD_INPUT_CTRL4_DVFS2_CLK_PD			0x10
#define PALMAS_PU_PD_INPUT_CTRL4_DVFS2_CLK_PD_SHIFT		4
#define PALMAS_PU_PD_INPUT_CTRL4_DVFS1_DAT_PD			0x04
#define PALMAS_PU_PD_INPUT_CTRL4_DVFS1_DAT_PD_SHIFT		2
#define PALMAS_PU_PD_INPUT_CTRL4_DVFS1_CLK_PD			0x01
#define PALMAS_PU_PD_INPUT_CTRL4_DVFS1_CLK_PD_SHIFT		0

/* Bit definitions for PRIMARY_SECONDARY_PAD3 */
#define PALMAS_PRIMARY_SECONDARY_PAD3_DVFS2			0x02
#define PALMAS_PRIMARY_SECONDARY_PAD3_DVFS2_SHIFT		1
#define PALMAS_PRIMARY_SECONDARY_PAD3_DVFS1			0x01
#define PALMAS_PRIMARY_SECONDARY_PAD3_DVFS1_SHIFT		0

/* Bit definitions for PRIMARY_SECONDARY_PAD4 */
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_15_MASK		0x80
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_15_SHIFT		7
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_14_MASK		0x40
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_14_SHIFT		6
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_13_MASK		0x20
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_13_SHIFT		5
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_12_MASK		0x10
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_12_SHIFT		4
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_11_MASK		0x08
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_11_SHIFT		3
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_10_MASK		0x04
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_10_SHIFT		2
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_9_MASK		0x02
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_9_SHIFT		1
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_8_MASK		0x01
#define PALMAS_PRIMARY_SECONDARY_PAD4_GPIO_8_SHIFT		0

/* Registers for function LED_PWM */
#define PALMAS_LED_PERIOD_CTRL					0x0
#define PALMAS_LED_CTRL						0x1
#define PALMAS_PWM_CTRL1					0x2
#define PALMAS_PWM_CTRL2					0x3

/* Bit definitions for LED_PERIOD_CTRL */
#define PALMAS_LED_PERIOD_CTRL_LED_2_PERIOD_MASK		0x38
#define PALMAS_LED_PERIOD_CTRL_LED_2_PERIOD_SHIFT		3
#define PALMAS_LED_PERIOD_CTRL_LED_1_PERIOD_MASK		0x07
#define PALMAS_LED_PERIOD_CTRL_LED_1_PERIOD_SHIFT		0

/* Bit definitions for LED_CTRL */
#define PALMAS_LED_CTRL_LED_2_SEQ				0x20
#define PALMAS_LED_CTRL_LED_2_SEQ_SHIFT				5
#define PALMAS_LED_CTRL_LED_1_SEQ				0x10
#define PALMAS_LED_CTRL_LED_1_SEQ_SHIFT				4
#define PALMAS_LED_CTRL_LED_2_ON_TIME_MASK			0x0c
#define PALMAS_LED_CTRL_LED_2_ON_TIME_SHIFT			2
#define PALMAS_LED_CTRL_LED_1_ON_TIME_MASK			0x03
#define PALMAS_LED_CTRL_LED_1_ON_TIME_SHIFT			0

/* Bit definitions for PWM_CTRL1 */
#define PALMAS_PWM_CTRL1_PWM_FREQ_EN				0x02
#define PALMAS_PWM_CTRL1_PWM_FREQ_EN_SHIFT			1
#define PALMAS_PWM_CTRL1_PWM_FREQ_SEL				0x01
#define PALMAS_PWM_CTRL1_PWM_FREQ_SEL_SHIFT			0

/* Bit definitions for PWM_CTRL2 */
#define PALMAS_PWM_CTRL2_PWM_DUTY_SEL_MASK			0xff
#define PALMAS_PWM_CTRL2_PWM_DUTY_SEL_SHIFT			0

/* Maximum INT mask/edge regsiter */
#define PALMAS_MAX_INTERRUPT_MASK_REG				6
#define PALMAS_MAX_INTERRUPT_EDGE_REG				12

/* Registers for function INTERRUPT */
#define PALMAS_INT1_STATUS					0x0
#define PALMAS_INT1_MASK					0x1
#define PALMAS_INT1_LINE_STATE					0x2
#define PALMAS_INT1_EDGE_DETECT1_RESERVED			0x3
#define PALMAS_INT1_EDGE_DETECT2_RESERVED			0x4
#define PALMAS_INT2_STATUS					0x5
#define PALMAS_INT2_MASK					0x6
#define PALMAS_INT2_LINE_STATE					0x7
#define PALMAS_INT2_EDGE_DETECT1_RESERVED			0x8
#define PALMAS_INT2_EDGE_DETECT2_RESERVED			0x9
#define PALMAS_INT3_STATUS					0xA
#define PALMAS_INT3_MASK					0xB
#define PALMAS_INT3_LINE_STATE					0xC
#define PALMAS_INT3_EDGE_DETECT1_RESERVED			0xD
#define PALMAS_INT3_EDGE_DETECT2_RESERVED			0xE
#define PALMAS_INT4_STATUS					0xF
#define PALMAS_INT4_MASK					0x10
#define PALMAS_INT4_LINE_STATE					0x11
#define PALMAS_INT4_EDGE_DETECT1				0x12
#define PALMAS_INT4_EDGE_DETECT2				0x13
#define PALMAS_INT5_STATUS					0x15
#define PALMAS_INT5_MASK					0x16
#define PALMAS_INT5_LINE_STATE					0x17
#define PALMAS_INT5_EDGE_DETECT1				0x18
#define PALMAS_INT5_EDGE_DETECT2				0x19
#define PALMAS_INT_CTRL						0x14
#define PALMAS_INT5_STATUS                                      0x15
#define PALMAS_INT5_MASK                                        0x16
#define PALMAS_INT5_LINE_STATE                                  0x17
#define PALMAS_INT5_EDGE_DETECT1                                0x18
#define PALMAS_INT5_EDGE_DETECT2                                0x19
#define PALMAS_INT6_STATUS                                      0x1A
#define PALMAS_INT6_MASK                                        0x1B
#define PALMAS_INT6_LINE_STATE                                  0x1C
#define PALMAS_INT6_EDGE_DETECT1_RESERVED                       0x1D
#define PALMAS_INT6_EDGE_DETECT2_RESERVED                       0x1E

/* Bit definitions for INT1_STATUS */
#define PALMAS_INT1_STATUS_VBAT_MON				0x80
#define PALMAS_INT1_STATUS_VBAT_MON_SHIFT			7
#define PALMAS_INT1_STATUS_VSYS_MON				0x40
#define PALMAS_INT1_STATUS_VSYS_MON_SHIFT			6
#define PALMAS_INT1_STATUS_HOTDIE				0x20
#define PALMAS_INT1_STATUS_HOTDIE_SHIFT				5
#define PALMAS_INT1_STATUS_PWRDOWN				0x10
#define PALMAS_INT1_STATUS_PWRDOWN_SHIFT			4
#define PALMAS_INT1_STATUS_RPWRON				0x08
#define PALMAS_INT1_STATUS_RPWRON_SHIFT				3
#define PALMAS_INT1_STATUS_LONG_PRESS_KEY			0x04
#define PALMAS_INT1_STATUS_LONG_PRESS_KEY_SHIFT			2
#define PALMAS_INT1_STATUS_PWRON				0x02
#define PALMAS_INT1_STATUS_PWRON_SHIFT				1
#define PALMAS_INT1_STATUS_CHARG_DET_N_VBUS_OVV			0x01
#define PALMAS_INT1_STATUS_CHARG_DET_N_VBUS_OVV_SHIFT		0

/* Bit definitions for INT1_MASK */
#define PALMAS_INT1_MASK_VBAT_MON				0x80
#define PALMAS_INT1_MASK_VBAT_MON_SHIFT				7
#define PALMAS_INT1_MASK_VSYS_MON				0x40
#define PALMAS_INT1_MASK_VSYS_MON_SHIFT				6
#define PALMAS_INT1_MASK_HOTDIE					0x20
#define PALMAS_INT1_MASK_HOTDIE_SHIFT				5
#define PALMAS_INT1_MASK_PWRDOWN				0x10
#define PALMAS_INT1_MASK_PWRDOWN_SHIFT				4
#define PALMAS_INT1_MASK_RPWRON					0x08
#define PALMAS_INT1_MASK_RPWRON_SHIFT				3
#define PALMAS_INT1_MASK_LONG_PRESS_KEY				0x04
#define PALMAS_INT1_MASK_LONG_PRESS_KEY_SHIFT			2
#define PALMAS_INT1_MASK_PWRON					0x02
#define PALMAS_INT1_MASK_PWRON_SHIFT				1
#define PALMAS_INT1_MASK_CHARG_DET_N_VBUS_OVV			0x01
#define PALMAS_INT1_MASK_CHARG_DET_N_VBUS_OVV_SHIFT		0

/* Bit definitions for INT1_LINE_STATE */
#define PALMAS_INT1_LINE_STATE_VBAT_MON				0x80
#define PALMAS_INT1_LINE_STATE_VBAT_MON_SHIFT			7
#define PALMAS_INT1_LINE_STATE_VSYS_MON				0x40
#define PALMAS_INT1_LINE_STATE_VSYS_MON_SHIFT			6
#define PALMAS_INT1_LINE_STATE_HOTDIE				0x20
#define PALMAS_INT1_LINE_STATE_HOTDIE_SHIFT			5
#define PALMAS_INT1_LINE_STATE_PWRDOWN				0x10
#define PALMAS_INT1_LINE_STATE_PWRDOWN_SHIFT			4
#define PALMAS_INT1_LINE_STATE_RPWRON				0x08
#define PALMAS_INT1_LINE_STATE_RPWRON_SHIFT			3
#define PALMAS_INT1_LINE_STATE_LONG_PRESS_KEY			0x04
#define PALMAS_INT1_LINE_STATE_LONG_PRESS_KEY_SHIFT		2
#define PALMAS_INT1_LINE_STATE_PWRON				0x02
#define PALMAS_INT1_LINE_STATE_PWRON_SHIFT			1
#define PALMAS_INT1_LINE_STATE_CHARG_DET_N_VBUS_OVV		0x01
#define PALMAS_INT1_LINE_STATE_CHARG_DET_N_VBUS_OVV_SHIFT	0

/* Bit definitions for INT2_STATUS */
#define PALMAS_INT2_STATUS_VAC_ACOK				0x80
#define PALMAS_INT2_STATUS_VAC_ACOK_SHIFT			7
#define PALMAS_INT2_STATUS_SHORT				0x40
#define PALMAS_INT2_STATUS_SHORT_SHIFT				6
#define PALMAS_INT2_STATUS_FBI_BB				0x20
#define PALMAS_INT2_STATUS_FBI_BB_SHIFT				5
#define PALMAS_INT2_STATUS_RESET_IN				0x10
#define PALMAS_INT2_STATUS_RESET_IN_SHIFT			4
#define PALMAS_INT2_STATUS_BATREMOVAL				0x08
#define PALMAS_INT2_STATUS_BATREMOVAL_SHIFT			3
#define PALMAS_INT2_STATUS_WDT					0x04
#define PALMAS_INT2_STATUS_WDT_SHIFT				2
#define PALMAS_INT2_STATUS_RTC_TIMER				0x02
#define PALMAS_INT2_STATUS_RTC_TIMER_SHIFT			1
#define PALMAS_INT2_STATUS_RTC_ALARM				0x01
#define PALMAS_INT2_STATUS_RTC_ALARM_SHIFT			0

/* Bit definitions for INT2_MASK */
#define PALMAS_INT2_MASK_VAC_ACOK				0x80
#define PALMAS_INT2_MASK_VAC_ACOK_SHIFT				7
#define PALMAS_INT2_MASK_SHORT					0x40
#define PALMAS_INT2_MASK_SHORT_SHIFT				6
#define PALMAS_INT2_MASK_FBI_BB					0x20
#define PALMAS_INT2_MASK_FBI_BB_SHIFT				5
#define PALMAS_INT2_MASK_RESET_IN				0x10
#define PALMAS_INT2_MASK_RESET_IN_SHIFT				4
#define PALMAS_INT2_MASK_BATREMOVAL				0x08
#define PALMAS_INT2_MASK_BATREMOVAL_SHIFT			3
#define PALMAS_INT2_MASK_WDT					0x04
#define PALMAS_INT2_MASK_WDT_SHIFT				2
#define PALMAS_INT2_MASK_RTC_TIMER				0x02
#define PALMAS_INT2_MASK_RTC_TIMER_SHIFT			1
#define PALMAS_INT2_MASK_RTC_ALARM				0x01
#define PALMAS_INT2_MASK_RTC_ALARM_SHIFT			0

/* Bit definitions for INT2_LINE_STATE */
#define PALMAS_INT2_LINE_STATE_VAC_ACOK				0x80
#define PALMAS_INT2_LINE_STATE_VAC_ACOK_SHIFT			7
#define PALMAS_INT2_LINE_STATE_SHORT				0x40
#define PALMAS_INT2_LINE_STATE_SHORT_SHIFT			6
#define PALMAS_INT2_LINE_STATE_FBI_BB				0x20
#define PALMAS_INT2_LINE_STATE_FBI_BB_SHIFT			5
#define PALMAS_INT2_LINE_STATE_RESET_IN				0x10
#define PALMAS_INT2_LINE_STATE_RESET_IN_SHIFT			4
#define PALMAS_INT2_LINE_STATE_BATREMOVAL			0x08
#define PALMAS_INT2_LINE_STATE_BATREMOVAL_SHIFT			3
#define PALMAS_INT2_LINE_STATE_WDT				0x04
#define PALMAS_INT2_LINE_STATE_WDT_SHIFT			2
#define PALMAS_INT2_LINE_STATE_RTC_TIMER			0x02
#define PALMAS_INT2_LINE_STATE_RTC_TIMER_SHIFT			1
#define PALMAS_INT2_LINE_STATE_RTC_ALARM			0x01
#define PALMAS_INT2_LINE_STATE_RTC_ALARM_SHIFT			0

/* Bit definitions for INT3_STATUS */
#define PALMAS_INT3_STATUS_VBUS					0x80
#define PALMAS_INT3_STATUS_VBUS_SHIFT				7
#define PALMAS_INT3_STATUS_VBUS_OTG				0x40
#define PALMAS_INT3_STATUS_VBUS_OTG_SHIFT			6
#define PALMAS_INT3_STATUS_ID					0x20
#define PALMAS_INT3_STATUS_ID_SHIFT				5
#define PALMAS_INT3_STATUS_ID_OTG				0x10
#define PALMAS_INT3_STATUS_ID_OTG_SHIFT				4
#define PALMAS_INT3_STATUS_GPADC_EOC_RT				0x08
#define PALMAS_INT3_STATUS_GPADC_EOC_RT_SHIFT			3
#define PALMAS_INT3_STATUS_GPADC_EOC_SW				0x04
#define PALMAS_INT3_STATUS_GPADC_EOC_SW_SHIFT			2
#define PALMAS_INT3_STATUS_GPADC_AUTO_1				0x02
#define PALMAS_INT3_STATUS_GPADC_AUTO_1_SHIFT			1
#define PALMAS_INT3_STATUS_GPADC_AUTO_0				0x01
#define PALMAS_INT3_STATUS_GPADC_AUTO_0_SHIFT			0

/* Bit definitions for INT3_MASK */
#define PALMAS_INT3_MASK_VBUS					0x80
#define PALMAS_INT3_MASK_VBUS_SHIFT				7
#define PALMAS_INT3_MASK_VBUS_OTG				0x40
#define PALMAS_INT3_MASK_VBUS_OTG_SHIFT				6
#define PALMAS_INT3_MASK_ID					0x20
#define PALMAS_INT3_MASK_ID_SHIFT				5
#define PALMAS_INT3_MASK_ID_OTG					0x10
#define PALMAS_INT3_MASK_ID_OTG_SHIFT				4
#define PALMAS_INT3_MASK_GPADC_EOC_RT				0x08
#define PALMAS_INT3_MASK_GPADC_EOC_RT_SHIFT			3
#define PALMAS_INT3_MASK_GPADC_EOC_SW				0x04
#define PALMAS_INT3_MASK_GPADC_EOC_SW_SHIFT			2
#define PALMAS_INT3_MASK_GPADC_AUTO_1				0x02
#define PALMAS_INT3_MASK_GPADC_AUTO_1_SHIFT			1
#define PALMAS_INT3_MASK_GPADC_AUTO_0				0x01
#define PALMAS_INT3_MASK_GPADC_AUTO_0_SHIFT			0

/* Bit definitions for INT3_LINE_STATE */
#define PALMAS_INT3_LINE_STATE_VBUS				0x80
#define PALMAS_INT3_LINE_STATE_VBUS_SHIFT			7
#define PALMAS_INT3_LINE_STATE_VBUS_OTG				0x40
#define PALMAS_INT3_LINE_STATE_VBUS_OTG_SHIFT			6
#define PALMAS_INT3_LINE_STATE_ID				0x20
#define PALMAS_INT3_LINE_STATE_ID_SHIFT				5
#define PALMAS_INT3_LINE_STATE_ID_OTG				0x10
#define PALMAS_INT3_LINE_STATE_ID_OTG_SHIFT			4
#define PALMAS_INT3_LINE_STATE_GPADC_EOC_RT			0x08
#define PALMAS_INT3_LINE_STATE_GPADC_EOC_RT_SHIFT		3
#define PALMAS_INT3_LINE_STATE_GPADC_EOC_SW			0x04
#define PALMAS_INT3_LINE_STATE_GPADC_EOC_SW_SHIFT		2
#define PALMAS_INT3_LINE_STATE_GPADC_AUTO_1			0x02
#define PALMAS_INT3_LINE_STATE_GPADC_AUTO_1_SHIFT		1
#define PALMAS_INT3_LINE_STATE_GPADC_AUTO_0			0x01
#define PALMAS_INT3_LINE_STATE_GPADC_AUTO_0_SHIFT		0

/* Bit definitions for INT4_STATUS */
#define PALMAS_INT4_STATUS_GPIO_7				0x80
#define PALMAS_INT4_STATUS_GPIO_7_SHIFT				7
#define PALMAS_INT4_STATUS_GPIO_6				0x40
#define PALMAS_INT4_STATUS_GPIO_6_SHIFT				6
#define PALMAS_INT4_STATUS_GPIO_5				0x20
#define PALMAS_INT4_STATUS_GPIO_5_SHIFT				5
#define PALMAS_INT4_STATUS_GPIO_4				0x10
#define PALMAS_INT4_STATUS_GPIO_4_SHIFT				4
#define PALMAS_INT4_STATUS_GPIO_3				0x08
#define PALMAS_INT4_STATUS_GPIO_3_SHIFT				3
#define PALMAS_INT4_STATUS_GPIO_2				0x04
#define PALMAS_INT4_STATUS_GPIO_2_SHIFT				2
#define PALMAS_INT4_STATUS_GPIO_1				0x02
#define PALMAS_INT4_STATUS_GPIO_1_SHIFT				1
#define PALMAS_INT4_STATUS_GPIO_0				0x01
#define PALMAS_INT4_STATUS_GPIO_0_SHIFT				0

/* Bit definitions for INT4_MASK */
#define PALMAS_INT4_MASK_GPIO_7					0x80
#define PALMAS_INT4_MASK_GPIO_7_SHIFT				7
#define PALMAS_INT4_MASK_GPIO_6					0x40
#define PALMAS_INT4_MASK_GPIO_6_SHIFT				6
#define PALMAS_INT4_MASK_GPIO_5					0x20
#define PALMAS_INT4_MASK_GPIO_5_SHIFT				5
#define PALMAS_INT4_MASK_GPIO_4					0x10
#define PALMAS_INT4_MASK_GPIO_4_SHIFT				4
#define PALMAS_INT4_MASK_GPIO_3					0x08
#define PALMAS_INT4_MASK_GPIO_3_SHIFT				3
#define PALMAS_INT4_MASK_GPIO_2					0x04
#define PALMAS_INT4_MASK_GPIO_2_SHIFT				2
#define PALMAS_INT4_MASK_GPIO_1					0x02
#define PALMAS_INT4_MASK_GPIO_1_SHIFT				1
#define PALMAS_INT4_MASK_GPIO_0					0x01
#define PALMAS_INT4_MASK_GPIO_0_SHIFT				0

/* Bit definitions for INT4_LINE_STATE */
#define PALMAS_INT4_LINE_STATE_GPIO_7				0x80
#define PALMAS_INT4_LINE_STATE_GPIO_7_SHIFT			7
#define PALMAS_INT4_LINE_STATE_GPIO_6				0x40
#define PALMAS_INT4_LINE_STATE_GPIO_6_SHIFT			6
#define PALMAS_INT4_LINE_STATE_GPIO_5				0x20
#define PALMAS_INT4_LINE_STATE_GPIO_5_SHIFT			5
#define PALMAS_INT4_LINE_STATE_GPIO_4				0x10
#define PALMAS_INT4_LINE_STATE_GPIO_4_SHIFT			4
#define PALMAS_INT4_LINE_STATE_GPIO_3				0x08
#define PALMAS_INT4_LINE_STATE_GPIO_3_SHIFT			3
#define PALMAS_INT4_LINE_STATE_GPIO_2				0x04
#define PALMAS_INT4_LINE_STATE_GPIO_2_SHIFT			2
#define PALMAS_INT4_LINE_STATE_GPIO_1				0x02
#define PALMAS_INT4_LINE_STATE_GPIO_1_SHIFT			1
#define PALMAS_INT4_LINE_STATE_GPIO_0				0x01
#define PALMAS_INT4_LINE_STATE_GPIO_0_SHIFT			0

/* Bit definitions for INT4_EDGE_DETECT1 */
#define PALMAS_INT4_EDGE_DETECT1_GPIO_3_RISING			0x80
#define PALMAS_INT4_EDGE_DETECT1_GPIO_3_RISING_SHIFT		7
#define PALMAS_INT4_EDGE_DETECT1_GPIO_3_FALLING			0x40
#define PALMAS_INT4_EDGE_DETECT1_GPIO_3_FALLING_SHIFT		6
#define PALMAS_INT4_EDGE_DETECT1_GPIO_2_RISING			0x20
#define PALMAS_INT4_EDGE_DETECT1_GPIO_2_RISING_SHIFT		5
#define PALMAS_INT4_EDGE_DETECT1_GPIO_2_FALLING			0x10
#define PALMAS_INT4_EDGE_DETECT1_GPIO_2_FALLING_SHIFT		4
#define PALMAS_INT4_EDGE_DETECT1_GPIO_1_RISING			0x08
#define PALMAS_INT4_EDGE_DETECT1_GPIO_1_RISING_SHIFT		3
#define PALMAS_INT4_EDGE_DETECT1_GPIO_1_FALLING			0x04
#define PALMAS_INT4_EDGE_DETECT1_GPIO_1_FALLING_SHIFT		2
#define PALMAS_INT4_EDGE_DETECT1_GPIO_0_RISING			0x02
#define PALMAS_INT4_EDGE_DETECT1_GPIO_0_RISING_SHIFT		1
#define PALMAS_INT4_EDGE_DETECT1_GPIO_0_FALLING			0x01
#define PALMAS_INT4_EDGE_DETECT1_GPIO_0_FALLING_SHIFT		0

/* Bit definitions for INT4_EDGE_DETECT2 */
#define PALMAS_INT4_EDGE_DETECT2_GPIO_7_RISING			0x80
#define PALMAS_INT4_EDGE_DETECT2_GPIO_7_RISING_SHIFT		7
#define PALMAS_INT4_EDGE_DETECT2_GPIO_7_FALLING			0x40
#define PALMAS_INT4_EDGE_DETECT2_GPIO_7_FALLING_SHIFT		6
#define PALMAS_INT4_EDGE_DETECT2_GPIO_6_RISING			0x20
#define PALMAS_INT4_EDGE_DETECT2_GPIO_6_RISING_SHIFT		5
#define PALMAS_INT4_EDGE_DETECT2_GPIO_6_FALLING			0x10
#define PALMAS_INT4_EDGE_DETECT2_GPIO_6_FALLING_SHIFT		4
#define PALMAS_INT4_EDGE_DETECT2_GPIO_5_RISING			0x08
#define PALMAS_INT4_EDGE_DETECT2_GPIO_5_RISING_SHIFT		3
#define PALMAS_INT4_EDGE_DETECT2_GPIO_5_FALLING			0x04
#define PALMAS_INT4_EDGE_DETECT2_GPIO_5_FALLING_SHIFT		2
#define PALMAS_INT4_EDGE_DETECT2_GPIO_4_RISING			0x02
#define PALMAS_INT4_EDGE_DETECT2_GPIO_4_RISING_SHIFT		1
#define PALMAS_INT4_EDGE_DETECT2_GPIO_4_FALLING			0x01
#define PALMAS_INT4_EDGE_DETECT2_GPIO_4_FALLING_SHIFT		0

/* Bit definitions for INT5_STATUS */
#define PALMAS_INT5_STATUS_GPIO_15				0x80
#define PALMAS_INT5_STATUS_GPIO_15_SHIFT			7
#define PALMAS_INT5_STATUS_GPIO_14				0x40
#define PALMAS_INT5_STATUS_GPIO_14_SHIFT			6
#define PALMAS_INT5_STATUS_GPIO_13				0x20
#define PALMAS_INT5_STATUS_GPIO_13_SHIFT			5
#define PALMAS_INT5_STATUS_GPIO_12				0x10
#define PALMAS_INT5_STATUS_GPIO_12_SHIFT			4
#define PALMAS_INT5_STATUS_GPIO_11				0x08
#define PALMAS_INT5_STATUS_GPIO_11_SHIFT			3
#define PALMAS_INT5_STATUS_GPIO_10				0x04
#define PALMAS_INT5_STATUS_GPIO_10_SHIFT			2
#define PALMAS_INT5_STATUS_GPIO_9				0x02
#define PALMAS_INT5_STATUS_GPIO_9_SHIFT				1
#define PALMAS_INT5_STATUS_GPIO_8				0x01
#define PALMAS_INT5_STATUS_GPIO_8_SHIFT				0

/* Bit definitions for INT5_MASK */
#define PALMAS_INT5_MASK_GPIO_15				0x80
#define PALMAS_INT5_MASK_GPIO_15_SHIFT				7
#define PALMAS_INT5_MASK_GPIO_14				0x40
#define PALMAS_INT5_MASK_GPIO_14_SHIFT				6
#define PALMAS_INT5_MASK_GPIO_13				0x20
#define PALMAS_INT5_MASK_GPIO_13_SHIFT				5
#define PALMAS_INT5_MASK_GPIO_12				0x10
#define PALMAS_INT5_MASK_GPIO_12_SHIFT				4
#define PALMAS_INT5_MASK_GPIO_11				0x08
#define PALMAS_INT5_MASK_GPIO_11_SHIFT				3
#define PALMAS_INT5_MASK_GPIO_10				0x04
#define PALMAS_INT5_MASK_GPIO_10_SHIFT				2
#define PALMAS_INT5_MASK_GPIO_9					0x02
#define PALMAS_INT5_MASK_GPIO_9_SHIFT				1
#define PALMAS_INT5_MASK_GPIO_8					0x01
#define PALMAS_INT5_MASK_GPIO_8_SHIFT				0

/* Bit definitions for INT5_LINE_STATE */
#define PALMAS_INT5_LINE_STATE_GPIO_15				0x80
#define PALMAS_INT5_LINE_STATE_GPIO_15_SHIFT			7
#define PALMAS_INT5_LINE_STATE_GPIO_14				0x40
#define PALMAS_INT5_LINE_STATE_GPIO_14_SHIFT			6
#define PALMAS_INT5_LINE_STATE_GPIO_13				0x20
#define PALMAS_INT5_LINE_STATE_GPIO_13_SHIFT			5
#define PALMAS_INT5_LINE_STATE_GPIO_12				0x10
#define PALMAS_INT5_LINE_STATE_GPIO_12_SHIFT			4
#define PALMAS_INT5_LINE_STATE_GPIO_11				0x08
#define PALMAS_INT5_LINE_STATE_GPIO_11_SHIFT			3
#define PALMAS_INT5_LINE_STATE_GPIO_10				0x04
#define PALMAS_INT5_LINE_STATE_GPIO_10_SHIFT			2
#define PALMAS_INT5_LINE_STATE_GPIO_9				0x02
#define PALMAS_INT5_LINE_STATE_GPIO_9_SHIFT			1
#define PALMAS_INT5_LINE_STATE_GPIO_8				0x01
#define PALMAS_INT5_LINE_STATE_GPIO_8_SHIFT			0

/* Bit definitions for INT5_EDGE_DETECT1 */
#define PALMAS_INT5_EDGE_DETECT1_GPIO_11_RISING			0x80
#define PALMAS_INT5_EDGE_DETECT1_GPIO_11_RISING_SHIFT		7
#define PALMAS_INT5_EDGE_DETECT1_GPIO_11_FALLING		0x40
#define PALMAS_INT5_EDGE_DETECT1_GPIO_11_FALLING_SHIFT		6
#define PALMAS_INT5_EDGE_DETECT1_GPIO_10_RISING			0x20
#define PALMAS_INT5_EDGE_DETECT1_GPIO_10_RISING_SHIFT		5
#define PALMAS_INT5_EDGE_DETECT1_GPIO_10_FALLING		0x10
#define PALMAS_INT5_EDGE_DETECT1_GPIO_10_FALLING_SHIFT		4
#define PALMAS_INT5_EDGE_DETECT1_GPIO_9_RISING			0x08
#define PALMAS_INT5_EDGE_DETECT1_GPIO_9_RISING_SHIFT		3
#define PALMAS_INT5_EDGE_DETECT1_GPIO_9_FALLING			0x04
#define PALMAS_INT5_EDGE_DETECT1_GPIO_9_FALLING_SHIFT		2
#define PALMAS_INT5_EDGE_DETECT1_GPIO_8_RISING			0x02
#define PALMAS_INT5_EDGE_DETECT1_GPIO_8_RISING_SHIFT		1
#define PALMAS_INT5_EDGE_DETECT1_GPIO_8_FALLING			0x01
#define PALMAS_INT5_EDGE_DETECT1_GPIO_8_FALLING_SHIFT		0

/* Bit definitions for INT5_EDGE_DETECT2 */
#define PALMAS_INT5_EDGE_DETECT2_GPIO_15_RISING			0x80
#define PALMAS_INT5_EDGE_DETECT2_GPIO_15_RISING_SHIFT		7
#define PALMAS_INT5_EDGE_DETECT2_GPIO_15_FALLING		0x40
#define PALMAS_INT5_EDGE_DETECT2_GPIO_15_FALLING_SHIFT		6
#define PALMAS_INT5_EDGE_DETECT2_GPIO_14_RISING			0x20
#define PALMAS_INT5_EDGE_DETECT2_GPIO_14_RISING_SHIFT		5
#define PALMAS_INT5_EDGE_DETECT2_GPIO_14_FALLING		0x10
#define PALMAS_INT5_EDGE_DETECT2_GPIO_14_FALLING_SHIFT		4
#define PALMAS_INT5_EDGE_DETECT2_GPIO_13_RISING			0x08
#define PALMAS_INT5_EDGE_DETECT2_GPIO_13_RISING_SHIFT		3
#define PALMAS_INT5_EDGE_DETECT2_GPIO_13_FALLING		0x04
#define PALMAS_INT5_EDGE_DETECT2_GPIO_13_FALLING_SHIFT		2
#define PALMAS_INT5_EDGE_DETECT2_GPIO_12_RISING			0x02
#define PALMAS_INT5_EDGE_DETECT2_GPIO_12_RISING_SHIFT		1
#define PALMAS_INT5_EDGE_DETECT2_GPIO_12_FALLING		0x01
#define PALMAS_INT5_EDGE_DETECT2_GPIO_12_FALLING_SHIFT		0

/* Bit definitions for INT_CTRL */
#define PALMAS_INT_CTRL_INT_PENDING				0x04
#define PALMAS_INT_CTRL_INT_PENDING_SHIFT			2
#define PALMAS_INT_CTRL_INT_CLEAR				0x01
#define PALMAS_INT_CTRL_INT_CLEAR_SHIFT				0

/* Bit definitions for INT6_STATUS */
#define PALMAS_INT6_STATUS_SIM2                                 0x80
#define PALMAS_INT6_STATUS_SIM2_SHIFT                           7
#define PALMAS_INT6_STATUS_SIM1                                 0x40
#define PALMAS_INT6_STATUS_SIM1_SHIFT                           6
#define PALMAS_INT6_STATUS_CHARGER                              0x20
#define PALMAS_INT6_STATUS_CHARGER_SHIFT                        5
#define PALMAS_INT6_STATUS_CC_AUTOCAL                           0x10
#define PALMAS_INT6_STATUS_CC_AUTOCAL_SHIFT                     4
#define PALMAS_INT6_STATUS_CC_BAT_STABLE                        0x08
#define PALMAS_INT6_STATUS_CC_BAT_STABLE_SHIFT                  3
#define PALMAS_INT6_STATUS_CC_OVC_LIMIT                         0x04
#define PALMAS_INT6_STATUS_CC_OVC_LIMIT_SHIFT                   2
#define PALMAS_INT6_STATUS_CC_SYNC_EOC                          0x02
#define PALMAS_INT6_STATUS_CC_SYNC_EOC_SHIFT                    1
#define PALMAS_INT6_STATUS_CC_EOC                               0x01
#define PALMAS_INT6_STATUS_CC_EOC_SHIFT                         0

/* Bit definitions for INT6_MASK */
#define PALMAS_INT6_MASK_SIM2                                   0x80
#define PALMAS_INT6_MASK_SIM2_SHIFT                             7
#define PALMAS_INT6_MASK_SIM1                                   0x40
#define PALMAS_INT6_MASK_SIM1_SHIFT                             6
#define PALMAS_INT6_MASK_CHARGER                                0x20
#define PALMAS_INT6_MASK_CHARGER_SHIFT                          5
#define PALMAS_INT6_MASK_CC_AUTOCAL                             0x10
#define PALMAS_INT6_MASK_CC_AUTOCAL_SHIFT                       4
#define PALMAS_INT6_MASK_CC_BAT_STABLE                          0x08
#define PALMAS_INT6_MASK_CC_BAT_STABLE_SHIFT                    3
#define PALMAS_INT6_MASK_CC_OVC_LIMIT                           0x04
#define PALMAS_INT6_MASK_CC_OVC_LIMIT_SHIFT                     2
#define PALMAS_INT6_MASK_CC_SYNC_EOC                            0x02
#define PALMAS_INT6_MASK_CC_SYNC_EOC_SHIFT                      1
#define PALMAS_INT6_MASK_CC_EOC                                 0x01
#define PALMAS_INT6_MASK_CC_EOC_SHIFT                           0

/* Bit definitions for INT6_LINE_STATE */
#define PALMAS_INT6_LINE_STATE_SIM2                             0x80
#define PALMAS_INT6_LINE_STATE_SIM2_SHIFT                       7
#define PALMAS_INT6_LINE_STATE_SIM1                             0x40
#define PALMAS_INT6_LINE_STATE_SIM1_SHIFT                       6
#define PALMAS_INT6_LINE_STATE_CHARGER                          0x20
#define PALMAS_INT6_LINE_STATE_CHARGER_SHIFT                    5
#define PALMAS_INT6_LINE_STATE_CC_AUTOCAL                       0x10
#define PALMAS_INT6_LINE_STATE_CC_AUTOCAL_SHIFT                 4
#define PALMAS_INT6_LINE_STATE_CC_BAT_STABLE                    0x08
#define PALMAS_INT6_LINE_STATE_CC_BAT_STABLE_SHIFT              3
#define PALMAS_INT6_LINE_STATE_CC_OVC_LIMIT                     0x04
#define PALMAS_INT6_LINE_STATE_CC_OVC_LIMIT_SHIFT               2
#define PALMAS_INT6_LINE_STATE_CC_SYNC_EOC                      0x02
#define PALMAS_INT6_LINE_STATE_CC_SYNC_EOC_SHIFT                1
#define PALMAS_INT6_LINE_STATE_CC_EOC                           0x01
#define PALMAS_INT6_LINE_STATE_CC_EOC_SHIFT                     0

/* Registers for function USB_OTG */
#define PALMAS_USB_WAKEUP					0x3
#define PALMAS_USB_VBUS_CTRL_SET				0x4
#define PALMAS_USB_VBUS_CTRL_CLR				0x5
#define PALMAS_USB_ID_CTRL_SET					0x6
#define PALMAS_USB_ID_CTRL_CLEAR				0x7
#define PALMAS_USB_VBUS_INT_SRC					0x8
#define PALMAS_USB_VBUS_INT_LATCH_SET				0x9
#define PALMAS_USB_VBUS_INT_LATCH_CLR				0xA
#define PALMAS_USB_VBUS_INT_EN_LO_SET				0xB
#define PALMAS_USB_VBUS_INT_EN_LO_CLR				0xC
#define PALMAS_USB_VBUS_INT_EN_HI_SET				0xD
#define PALMAS_USB_VBUS_INT_EN_HI_CLR				0xE
#define PALMAS_USB_ID_INT_SRC					0xF
#define PALMAS_USB_ID_INT_LATCH_SET				0x10
#define PALMAS_USB_ID_INT_LATCH_CLR				0x11
#define PALMAS_USB_ID_INT_EN_LO_SET				0x12
#define PALMAS_USB_ID_INT_EN_LO_CLR				0x13
#define PALMAS_USB_ID_INT_EN_HI_SET				0x14
#define PALMAS_USB_ID_INT_EN_HI_CLR				0x15
#define PALMAS_USB_OTG_ADP_CTRL					0x16
#define PALMAS_USB_OTG_ADP_HIGH					0x17
#define PALMAS_USB_OTG_ADP_LOW					0x18
#define PALMAS_USB_OTG_ADP_RISE					0x19
#define PALMAS_USB_OTG_REVISION					0x1A

/* Bit definitions for USB_WAKEUP */
#define PALMAS_USB_WAKEUP_ID_WK_UP_COMP				0x01
#define PALMAS_USB_WAKEUP_ID_WK_UP_COMP_SHIFT			0

/* Bit definitions for USB_VBUS_CTRL_SET */
#define PALMAS_USB_VBUS_CTRL_SET_VBUS_CHRG_VSYS			0x80
#define PALMAS_USB_VBUS_CTRL_SET_VBUS_CHRG_VSYS_SHIFT		7
#define PALMAS_USB_VBUS_CTRL_SET_VBUS_DISCHRG			0x20
#define PALMAS_USB_VBUS_CTRL_SET_VBUS_DISCHRG_SHIFT		5
#define PALMAS_USB_VBUS_CTRL_SET_VBUS_IADP_SRC			0x10
#define PALMAS_USB_VBUS_CTRL_SET_VBUS_IADP_SRC_SHIFT		4
#define PALMAS_USB_VBUS_CTRL_SET_VBUS_IADP_SINK			0x08
#define PALMAS_USB_VBUS_CTRL_SET_VBUS_IADP_SINK_SHIFT		3
#define PALMAS_USB_VBUS_CTRL_SET_VBUS_ACT_COMP			0x04
#define PALMAS_USB_VBUS_CTRL_SET_VBUS_ACT_COMP_SHIFT		2

/* Bit definitions for USB_VBUS_CTRL_CLR */
#define PALMAS_USB_VBUS_CTRL_CLR_VBUS_CHRG_VSYS			0x80
#define PALMAS_USB_VBUS_CTRL_CLR_VBUS_CHRG_VSYS_SHIFT		7
#define PALMAS_USB_VBUS_CTRL_CLR_VBUS_DISCHRG			0x20
#define PALMAS_USB_VBUS_CTRL_CLR_VBUS_DISCHRG_SHIFT		5
#define PALMAS_USB_VBUS_CTRL_CLR_VBUS_IADP_SRC			0x10
#define PALMAS_USB_VBUS_CTRL_CLR_VBUS_IADP_SRC_SHIFT		4
#define PALMAS_USB_VBUS_CTRL_CLR_VBUS_IADP_SINK			0x08
#define PALMAS_USB_VBUS_CTRL_CLR_VBUS_IADP_SINK_SHIFT		3
#define PALMAS_USB_VBUS_CTRL_CLR_VBUS_ACT_COMP			0x04
#define PALMAS_USB_VBUS_CTRL_CLR_VBUS_ACT_COMP_SHIFT		2

/* Bit definitions for USB_ID_CTRL_SET */
#define PALMAS_USB_ID_CTRL_SET_ID_PU_220K			0x80
#define PALMAS_USB_ID_CTRL_SET_ID_PU_220K_SHIFT			7
#define PALMAS_USB_ID_CTRL_SET_ID_PU_100K			0x40
#define PALMAS_USB_ID_CTRL_SET_ID_PU_100K_SHIFT			6
#define PALMAS_USB_ID_CTRL_SET_ID_GND_DRV			0x20
#define PALMAS_USB_ID_CTRL_SET_ID_GND_DRV_SHIFT			5
#define PALMAS_USB_ID_CTRL_SET_ID_SRC_16U			0x10
#define PALMAS_USB_ID_CTRL_SET_ID_SRC_16U_SHIFT			4
#define PALMAS_USB_ID_CTRL_SET_ID_SRC_5U			0x08
#define PALMAS_USB_ID_CTRL_SET_ID_SRC_5U_SHIFT			3
#define PALMAS_USB_ID_CTRL_SET_ID_ACT_COMP			0x04
#define PALMAS_USB_ID_CTRL_SET_ID_ACT_COMP_SHIFT		2

/* Bit definitions for USB_ID_CTRL_CLEAR */
#define PALMAS_USB_ID_CTRL_CLEAR_ID_PU_220K			0x80
#define PALMAS_USB_ID_CTRL_CLEAR_ID_PU_220K_SHIFT		7
#define PALMAS_USB_ID_CTRL_CLEAR_ID_PU_100K			0x40
#define PALMAS_USB_ID_CTRL_CLEAR_ID_PU_100K_SHIFT		6
#define PALMAS_USB_ID_CTRL_CLEAR_ID_GND_DRV			0x20
#define PALMAS_USB_ID_CTRL_CLEAR_ID_GND_DRV_SHIFT		5
#define PALMAS_USB_ID_CTRL_CLEAR_ID_SRC_16U			0x10
#define PALMAS_USB_ID_CTRL_CLEAR_ID_SRC_16U_SHIFT		4
#define PALMAS_USB_ID_CTRL_CLEAR_ID_SRC_5U			0x08
#define PALMAS_USB_ID_CTRL_CLEAR_ID_SRC_5U_SHIFT		3
#define PALMAS_USB_ID_CTRL_CLEAR_ID_ACT_COMP			0x04
#define PALMAS_USB_ID_CTRL_CLEAR_ID_ACT_COMP_SHIFT		2

/* Bit definitions for USB_VBUS_INT_SRC */
#define PALMAS_USB_VBUS_INT_SRC_VOTG_SESS_VLD			0x80
#define PALMAS_USB_VBUS_INT_SRC_VOTG_SESS_VLD_SHIFT		7
#define PALMAS_USB_VBUS_INT_SRC_VADP_PRB			0x40
#define PALMAS_USB_VBUS_INT_SRC_VADP_PRB_SHIFT			6
#define PALMAS_USB_VBUS_INT_SRC_VADP_SNS			0x20
#define PALMAS_USB_VBUS_INT_SRC_VADP_SNS_SHIFT			5
#define PALMAS_USB_VBUS_INT_SRC_VA_VBUS_VLD			0x08
#define PALMAS_USB_VBUS_INT_SRC_VA_VBUS_VLD_SHIFT		3
#define PALMAS_USB_VBUS_INT_SRC_VA_SESS_VLD			0x04
#define PALMAS_USB_VBUS_INT_SRC_VA_SESS_VLD_SHIFT		2
#define PALMAS_USB_VBUS_INT_SRC_VB_SESS_VLD			0x02
#define PALMAS_USB_VBUS_INT_SRC_VB_SESS_VLD_SHIFT		1
#define PALMAS_USB_VBUS_INT_SRC_VB_SESS_END			0x01
#define PALMAS_USB_VBUS_INT_SRC_VB_SESS_END_SHIFT		0

/* Bit definitions for USB_VBUS_INT_LATCH_SET */
#define PALMAS_USB_VBUS_INT_LATCH_SET_VOTG_SESS_VLD		0x80
#define PALMAS_USB_VBUS_INT_LATCH_SET_VOTG_SESS_VLD_SHIFT	7
#define PALMAS_USB_VBUS_INT_LATCH_SET_VADP_PRB			0x40
#define PALMAS_USB_VBUS_INT_LATCH_SET_VADP_PRB_SHIFT		6
#define PALMAS_USB_VBUS_INT_LATCH_SET_VADP_SNS			0x20
#define PALMAS_USB_VBUS_INT_LATCH_SET_VADP_SNS_SHIFT		5
#define PALMAS_USB_VBUS_INT_LATCH_SET_ADP			0x10
#define PALMAS_USB_VBUS_INT_LATCH_SET_ADP_SHIFT			4
#define PALMAS_USB_VBUS_INT_LATCH_SET_VA_VBUS_VLD		0x08
#define PALMAS_USB_VBUS_INT_LATCH_SET_VA_VBUS_VLD_SHIFT		3
#define PALMAS_USB_VBUS_INT_LATCH_SET_VA_SESS_VLD		0x04
#define PALMAS_USB_VBUS_INT_LATCH_SET_VA_SESS_VLD_SHIFT		2
#define PALMAS_USB_VBUS_INT_LATCH_SET_VB_SESS_VLD		0x02
#define PALMAS_USB_VBUS_INT_LATCH_SET_VB_SESS_VLD_SHIFT		1
#define PALMAS_USB_VBUS_INT_LATCH_SET_VB_SESS_END		0x01
#define PALMAS_USB_VBUS_INT_LATCH_SET_VB_SESS_END_SHIFT		0

/* Bit definitions for USB_VBUS_INT_LATCH_CLR */
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VOTG_SESS_VLD		0x80
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VOTG_SESS_VLD_SHIFT	7
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VADP_PRB			0x40
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VADP_PRB_SHIFT		6
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VADP_SNS			0x20
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VADP_SNS_SHIFT		5
#define PALMAS_USB_VBUS_INT_LATCH_CLR_ADP			0x10
#define PALMAS_USB_VBUS_INT_LATCH_CLR_ADP_SHIFT			4
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VA_VBUS_VLD		0x08
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VA_VBUS_VLD_SHIFT		3
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VA_SESS_VLD		0x04
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VA_SESS_VLD_SHIFT		2
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VB_SESS_VLD		0x02
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VB_SESS_VLD_SHIFT		1
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VB_SESS_END		0x01
#define PALMAS_USB_VBUS_INT_LATCH_CLR_VB_SESS_END_SHIFT		0

/* Bit definitions for USB_VBUS_INT_EN_LO_SET */
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VOTG_SESS_VLD		0x80
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VOTG_SESS_VLD_SHIFT	7
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VADP_PRB			0x40
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VADP_PRB_SHIFT		6
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VADP_SNS			0x20
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VADP_SNS_SHIFT		5
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VA_VBUS_VLD		0x08
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VA_VBUS_VLD_SHIFT		3
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VA_SESS_VLD		0x04
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VA_SESS_VLD_SHIFT		2
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VB_SESS_VLD		0x02
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VB_SESS_VLD_SHIFT		1
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VB_SESS_END		0x01
#define PALMAS_USB_VBUS_INT_EN_LO_SET_VB_SESS_END_SHIFT		0

/* Bit definitions for USB_VBUS_INT_EN_LO_CLR */
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VOTG_SESS_VLD		0x80
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VOTG_SESS_VLD_SHIFT	7
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VADP_PRB			0x40
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VADP_PRB_SHIFT		6
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VADP_SNS			0x20
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VADP_SNS_SHIFT		5
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VA_VBUS_VLD		0x08
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VA_VBUS_VLD_SHIFT		3
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VA_SESS_VLD		0x04
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VA_SESS_VLD_SHIFT		2
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VB_SESS_VLD		0x02
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VB_SESS_VLD_SHIFT		1
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VB_SESS_END		0x01
#define PALMAS_USB_VBUS_INT_EN_LO_CLR_VB_SESS_END_SHIFT		0

/* Bit definitions for USB_VBUS_INT_EN_HI_SET */
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VOTG_SESS_VLD		0x80
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VOTG_SESS_VLD_SHIFT	7
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VADP_PRB			0x40
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VADP_PRB_SHIFT		6
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VADP_SNS			0x20
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VADP_SNS_SHIFT		5
#define PALMAS_USB_VBUS_INT_EN_HI_SET_ADP			0x10
#define PALMAS_USB_VBUS_INT_EN_HI_SET_ADP_SHIFT			4
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VA_VBUS_VLD		0x08
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VA_VBUS_VLD_SHIFT		3
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VA_SESS_VLD		0x04
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VA_SESS_VLD_SHIFT		2
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VB_SESS_VLD		0x02
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VB_SESS_VLD_SHIFT		1
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VB_SESS_END		0x01
#define PALMAS_USB_VBUS_INT_EN_HI_SET_VB_SESS_END_SHIFT		0

/* Bit definitions for USB_VBUS_INT_EN_HI_CLR */
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VOTG_SESS_VLD		0x80
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VOTG_SESS_VLD_SHIFT	7
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VADP_PRB			0x40
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VADP_PRB_SHIFT		6
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VADP_SNS			0x20
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VADP_SNS_SHIFT		5
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_ADP			0x10
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_ADP_SHIFT			4
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VA_VBUS_VLD		0x08
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VA_VBUS_VLD_SHIFT		3
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VA_SESS_VLD		0x04
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VA_SESS_VLD_SHIFT		2
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VB_SESS_VLD		0x02
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VB_SESS_VLD_SHIFT		1
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VB_SESS_END		0x01
#define PALMAS_USB_VBUS_INT_EN_HI_CLR_VB_SESS_END_SHIFT		0

/* Bit definitions for USB_ID_INT_SRC */
#define PALMAS_USB_ID_INT_SRC_ID_FLOAT				0x10
#define PALMAS_USB_ID_INT_SRC_ID_FLOAT_SHIFT			4
#define PALMAS_USB_ID_INT_SRC_ID_A				0x08
#define PALMAS_USB_ID_INT_SRC_ID_A_SHIFT			3
#define PALMAS_USB_ID_INT_SRC_ID_B				0x04
#define PALMAS_USB_ID_INT_SRC_ID_B_SHIFT			2
#define PALMAS_USB_ID_INT_SRC_ID_C				0x02
#define PALMAS_USB_ID_INT_SRC_ID_C_SHIFT			1
#define PALMAS_USB_ID_INT_SRC_ID_GND				0x01
#define PALMAS_USB_ID_INT_SRC_ID_GND_SHIFT			0

/* Bit definitions for USB_ID_INT_LATCH_SET */
#define PALMAS_USB_ID_INT_LATCH_SET_ID_FLOAT			0x10
#define PALMAS_USB_ID_INT_LATCH_SET_ID_FLOAT_SHIFT		4
#define PALMAS_USB_ID_INT_LATCH_SET_ID_A			0x08
#define PALMAS_USB_ID_INT_LATCH_SET_ID_A_SHIFT			3
#define PALMAS_USB_ID_INT_LATCH_SET_ID_B			0x04
#define PALMAS_USB_ID_INT_LATCH_SET_ID_B_SHIFT			2
#define PALMAS_USB_ID_INT_LATCH_SET_ID_C			0x02
#define PALMAS_USB_ID_INT_LATCH_SET_ID_C_SHIFT			1
#define PALMAS_USB_ID_INT_LATCH_SET_ID_GND			0x01
#define PALMAS_USB_ID_INT_LATCH_SET_ID_GND_SHIFT		0

/* Bit definitions for USB_ID_INT_LATCH_CLR */
#define PALMAS_USB_ID_INT_LATCH_CLR_ID_FLOAT			0x10
#define PALMAS_USB_ID_INT_LATCH_CLR_ID_FLOAT_SHIFT		4
#define PALMAS_USB_ID_INT_LATCH_CLR_ID_A			0x08
#define PALMAS_USB_ID_INT_LATCH_CLR_ID_A_SHIFT			3
#define PALMAS_USB_ID_INT_LATCH_CLR_ID_B			0x04
#define PALMAS_USB_ID_INT_LATCH_CLR_ID_B_SHIFT			2
#define PALMAS_USB_ID_INT_LATCH_CLR_ID_C			0x02
#define PALMAS_USB_ID_INT_LATCH_CLR_ID_C_SHIFT			1
#define PALMAS_USB_ID_INT_LATCH_CLR_ID_GND			0x01
#define PALMAS_USB_ID_INT_LATCH_CLR_ID_GND_SHIFT		0

/* Bit definitions for USB_ID_INT_EN_LO_SET */
#define PALMAS_USB_ID_INT_EN_LO_SET_ID_FLOAT			0x10
#define PALMAS_USB_ID_INT_EN_LO_SET_ID_FLOAT_SHIFT		4
#define PALMAS_USB_ID_INT_EN_LO_SET_ID_A			0x08
#define PALMAS_USB_ID_INT_EN_LO_SET_ID_A_SHIFT			3
#define PALMAS_USB_ID_INT_EN_LO_SET_ID_B			0x04
#define PALMAS_USB_ID_INT_EN_LO_SET_ID_B_SHIFT			2
#define PALMAS_USB_ID_INT_EN_LO_SET_ID_C			0x02
#define PALMAS_USB_ID_INT_EN_LO_SET_ID_C_SHIFT			1
#define PALMAS_USB_ID_INT_EN_LO_SET_ID_GND			0x01
#define PALMAS_USB_ID_INT_EN_LO_SET_ID_GND_SHIFT		0

/* Bit definitions for USB_ID_INT_EN_LO_CLR */
#define PALMAS_USB_ID_INT_EN_LO_CLR_ID_FLOAT			0x10
#define PALMAS_USB_ID_INT_EN_LO_CLR_ID_FLOAT_SHIFT		4
#define PALMAS_USB_ID_INT_EN_LO_CLR_ID_A			0x08
#define PALMAS_USB_ID_INT_EN_LO_CLR_ID_A_SHIFT			3
#define PALMAS_USB_ID_INT_EN_LO_CLR_ID_B			0x04
#define PALMAS_USB_ID_INT_EN_LO_CLR_ID_B_SHIFT			2
#define PALMAS_USB_ID_INT_EN_LO_CLR_ID_C			0x02
#define PALMAS_USB_ID_INT_EN_LO_CLR_ID_C_SHIFT			1
#define PALMAS_USB_ID_INT_EN_LO_CLR_ID_GND			0x01
#define PALMAS_USB_ID_INT_EN_LO_CLR_ID_GND_SHIFT		0

/* Bit definitions for USB_ID_INT_EN_HI_SET */
#define PALMAS_USB_ID_INT_EN_HI_SET_ID_FLOAT			0x10
#define PALMAS_USB_ID_INT_EN_HI_SET_ID_FLOAT_SHIFT		4
#define PALMAS_USB_ID_INT_EN_HI_SET_ID_A			0x08
#define PALMAS_USB_ID_INT_EN_HI_SET_ID_A_SHIFT			3
#define PALMAS_USB_ID_INT_EN_HI_SET_ID_B			0x04
#define PALMAS_USB_ID_INT_EN_HI_SET_ID_B_SHIFT			2
#define PALMAS_USB_ID_INT_EN_HI_SET_ID_C			0x02
#define PALMAS_USB_ID_INT_EN_HI_SET_ID_C_SHIFT			1
#define PALMAS_USB_ID_INT_EN_HI_SET_ID_GND			0x01
#define PALMAS_USB_ID_INT_EN_HI_SET_ID_GND_SHIFT		0

/* Bit definitions for USB_ID_INT_EN_HI_CLR */
#define PALMAS_USB_ID_INT_EN_HI_CLR_ID_FLOAT			0x10
#define PALMAS_USB_ID_INT_EN_HI_CLR_ID_FLOAT_SHIFT		4
#define PALMAS_USB_ID_INT_EN_HI_CLR_ID_A			0x08
#define PALMAS_USB_ID_INT_EN_HI_CLR_ID_A_SHIFT			3
#define PALMAS_USB_ID_INT_EN_HI_CLR_ID_B			0x04
#define PALMAS_USB_ID_INT_EN_HI_CLR_ID_B_SHIFT			2
#define PALMAS_USB_ID_INT_EN_HI_CLR_ID_C			0x02
#define PALMAS_USB_ID_INT_EN_HI_CLR_ID_C_SHIFT			1
#define PALMAS_USB_ID_INT_EN_HI_CLR_ID_GND			0x01
#define PALMAS_USB_ID_INT_EN_HI_CLR_ID_GND_SHIFT		0

/* Bit definitions for USB_OTG_ADP_CTRL */
#define PALMAS_USB_OTG_ADP_CTRL_ADP_EN				0x04
#define PALMAS_USB_OTG_ADP_CTRL_ADP_EN_SHIFT			2
#define PALMAS_USB_OTG_ADP_CTRL_ADP_MODE_MASK			0x03
#define PALMAS_USB_OTG_ADP_CTRL_ADP_MODE_SHIFT			0

/* Bit definitions for USB_OTG_ADP_HIGH */
#define PALMAS_USB_OTG_ADP_HIGH_T_ADP_HIGH_MASK			0xff
#define PALMAS_USB_OTG_ADP_HIGH_T_ADP_HIGH_SHIFT		0

/* Bit definitions for USB_OTG_ADP_LOW */
#define PALMAS_USB_OTG_ADP_LOW_T_ADP_LOW_MASK			0xff
#define PALMAS_USB_OTG_ADP_LOW_T_ADP_LOW_SHIFT			0

/* Bit definitions for USB_OTG_ADP_RISE */
#define PALMAS_USB_OTG_ADP_RISE_T_ADP_RISE_MASK			0xff
#define PALMAS_USB_OTG_ADP_RISE_T_ADP_RISE_SHIFT		0

/* Bit definitions for USB_OTG_REVISION */
#define PALMAS_USB_OTG_REVISION_OTG_REV				0x01
#define PALMAS_USB_OTG_REVISION_OTG_REV_SHIFT			0

/* Registers for function VIBRATOR */
#define PALMAS_VIBRA_CTRL					0x0

/* Bit definitions for VIBRA_CTRL */
#define PALMAS_VIBRA_CTRL_PWM_DUTY_SEL_MASK			0x06
#define PALMAS_VIBRA_CTRL_PWM_DUTY_SEL_SHIFT			1
#define PALMAS_VIBRA_CTRL_PWM_FREQ_SEL				0x01
#define PALMAS_VIBRA_CTRL_PWM_FREQ_SEL_SHIFT			0

/* Registers for function GPIO */
#define PALMAS_GPIO_DATA_IN					0x0
#define PALMAS_GPIO_DATA_DIR					0x1
#define PALMAS_GPIO_DATA_OUT					0x2
#define PALMAS_GPIO_DEBOUNCE_EN					0x3
#define PALMAS_GPIO_CLEAR_DATA_OUT				0x4
#define PALMAS_GPIO_SET_DATA_OUT				0x5
#define PALMAS_PU_PD_GPIO_CTRL1					0x6
#define PALMAS_PU_PD_GPIO_CTRL2					0x7
#define PALMAS_OD_OUTPUT_GPIO_CTRL				0x8
#define PALMAS_GPIO_DATA_IN2					0x9
#define PALMAS_GPIO_DATA_DIR2					0x0A
#define PALMAS_GPIO_DATA_OUT2					0x0B
#define PALMAS_GPIO_DEBOUNCE_EN2				0x0C
#define PALMAS_GPIO_CLEAR_DATA_OUT2				0x0D
#define PALMAS_GPIO_SET_DATA_OUT2				0x0E
#define PALMAS_PU_PD_GPIO_CTRL3					0x0F
#define PALMAS_PU_PD_GPIO_CTRL4					0x10
#define PALMAS_OD_OUTPUT_GPIO_CTRL2				0x11

/* Bit definitions for GPIO_DATA_IN */
#define PALMAS_GPIO_DATA_IN_GPIO_7_IN				0x80
#define PALMAS_GPIO_DATA_IN_GPIO_7_IN_SHIFT			7
#define PALMAS_GPIO_DATA_IN_GPIO_6_IN				0x40
#define PALMAS_GPIO_DATA_IN_GPIO_6_IN_SHIFT			6
#define PALMAS_GPIO_DATA_IN_GPIO_5_IN				0x20
#define PALMAS_GPIO_DATA_IN_GPIO_5_IN_SHIFT			5
#define PALMAS_GPIO_DATA_IN_GPIO_4_IN				0x10
#define PALMAS_GPIO_DATA_IN_GPIO_4_IN_SHIFT			4
#define PALMAS_GPIO_DATA_IN_GPIO_3_IN				0x08
#define PALMAS_GPIO_DATA_IN_GPIO_3_IN_SHIFT			3
#define PALMAS_GPIO_DATA_IN_GPIO_2_IN				0x04
#define PALMAS_GPIO_DATA_IN_GPIO_2_IN_SHIFT			2
#define PALMAS_GPIO_DATA_IN_GPIO_1_IN				0x02
#define PALMAS_GPIO_DATA_IN_GPIO_1_IN_SHIFT			1
#define PALMAS_GPIO_DATA_IN_GPIO_0_IN				0x01
#define PALMAS_GPIO_DATA_IN_GPIO_0_IN_SHIFT			0

/* Bit definitions for GPIO_DATA_DIR */
#define PALMAS_GPIO_DATA_DIR_GPIO_7_DIR				0x80
#define PALMAS_GPIO_DATA_DIR_GPIO_7_DIR_SHIFT			7
#define PALMAS_GPIO_DATA_DIR_GPIO_6_DIR				0x40
#define PALMAS_GPIO_DATA_DIR_GPIO_6_DIR_SHIFT			6
#define PALMAS_GPIO_DATA_DIR_GPIO_5_DIR				0x20
#define PALMAS_GPIO_DATA_DIR_GPIO_5_DIR_SHIFT			5
#define PALMAS_GPIO_DATA_DIR_GPIO_4_DIR				0x10
#define PALMAS_GPIO_DATA_DIR_GPIO_4_DIR_SHIFT			4
#define PALMAS_GPIO_DATA_DIR_GPIO_3_DIR				0x08
#define PALMAS_GPIO_DATA_DIR_GPIO_3_DIR_SHIFT			3
#define PALMAS_GPIO_DATA_DIR_GPIO_2_DIR				0x04
#define PALMAS_GPIO_DATA_DIR_GPIO_2_DIR_SHIFT			2
#define PALMAS_GPIO_DATA_DIR_GPIO_1_DIR				0x02
#define PALMAS_GPIO_DATA_DIR_GPIO_1_DIR_SHIFT			1
#define PALMAS_GPIO_DATA_DIR_GPIO_0_DIR				0x01
#define PALMAS_GPIO_DATA_DIR_GPIO_0_DIR_SHIFT			0

/* Bit definitions for GPIO_DATA_OUT */
#define PALMAS_GPIO_DATA_OUT_GPIO_7_OUT				0x80
#define PALMAS_GPIO_DATA_OUT_GPIO_7_OUT_SHIFT			7
#define PALMAS_GPIO_DATA_OUT_GPIO_6_OUT				0x40
#define PALMAS_GPIO_DATA_OUT_GPIO_6_OUT_SHIFT			6
#define PALMAS_GPIO_DATA_OUT_GPIO_5_OUT				0x20
#define PALMAS_GPIO_DATA_OUT_GPIO_5_OUT_SHIFT			5
#define PALMAS_GPIO_DATA_OUT_GPIO_4_OUT				0x10
#define PALMAS_GPIO_DATA_OUT_GPIO_4_OUT_SHIFT			4
#define PALMAS_GPIO_DATA_OUT_GPIO_3_OUT				0x08
#define PALMAS_GPIO_DATA_OUT_GPIO_3_OUT_SHIFT			3
#define PALMAS_GPIO_DATA_OUT_GPIO_2_OUT				0x04
#define PALMAS_GPIO_DATA_OUT_GPIO_2_OUT_SHIFT			2
#define PALMAS_GPIO_DATA_OUT_GPIO_1_OUT				0x02
#define PALMAS_GPIO_DATA_OUT_GPIO_1_OUT_SHIFT			1
#define PALMAS_GPIO_DATA_OUT_GPIO_0_OUT				0x01
#define PALMAS_GPIO_DATA_OUT_GPIO_0_OUT_SHIFT			0

/* Bit definitions for GPIO_DEBOUNCE_EN */
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_7_DEBOUNCE_EN		0x80
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_7_DEBOUNCE_EN_SHIFT	7
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_6_DEBOUNCE_EN		0x40
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_6_DEBOUNCE_EN_SHIFT	6
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_5_DEBOUNCE_EN		0x20
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_5_DEBOUNCE_EN_SHIFT	5
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_4_DEBOUNCE_EN		0x10
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_4_DEBOUNCE_EN_SHIFT	4
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_3_DEBOUNCE_EN		0x08
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_3_DEBOUNCE_EN_SHIFT	3
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_2_DEBOUNCE_EN		0x04
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_2_DEBOUNCE_EN_SHIFT	2
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_1_DEBOUNCE_EN		0x02
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_1_DEBOUNCE_EN_SHIFT	1
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_0_DEBOUNCE_EN		0x01
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_0_DEBOUNCE_EN_SHIFT	0

/* Bit definitions for GPIO_CLEAR_DATA_OUT */
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_7_CLEAR_DATA_OUT	0x80
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_7_CLEAR_DATA_OUT_SHIFT	7
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_6_CLEAR_DATA_OUT	0x40
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_6_CLEAR_DATA_OUT_SHIFT	6
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_5_CLEAR_DATA_OUT	0x20
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_5_CLEAR_DATA_OUT_SHIFT	5
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_4_CLEAR_DATA_OUT	0x10
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_4_CLEAR_DATA_OUT_SHIFT	4
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_3_CLEAR_DATA_OUT	0x08
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_3_CLEAR_DATA_OUT_SHIFT	3
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_2_CLEAR_DATA_OUT	0x04
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_2_CLEAR_DATA_OUT_SHIFT	2
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_1_CLEAR_DATA_OUT	0x02
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_1_CLEAR_DATA_OUT_SHIFT	1
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_0_CLEAR_DATA_OUT	0x01
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_0_CLEAR_DATA_OUT_SHIFT	0

/* Bit definitions for GPIO_SET_DATA_OUT */
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_7_SET_DATA_OUT		0x80
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_7_SET_DATA_OUT_SHIFT	7
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_6_SET_DATA_OUT		0x40
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_6_SET_DATA_OUT_SHIFT	6
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_5_SET_DATA_OUT		0x20
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_5_SET_DATA_OUT_SHIFT	5
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_4_SET_DATA_OUT		0x10
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_4_SET_DATA_OUT_SHIFT	4
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_3_SET_DATA_OUT		0x08
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_3_SET_DATA_OUT_SHIFT	3
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_2_SET_DATA_OUT		0x04
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_2_SET_DATA_OUT_SHIFT	2
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_1_SET_DATA_OUT		0x02
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_1_SET_DATA_OUT_SHIFT	1
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_0_SET_DATA_OUT		0x01
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_0_SET_DATA_OUT_SHIFT	0

/* Bit definitions for PU_PD_GPIO_CTRL1 */
#define PALMAS_PU_PD_GPIO_CTRL1_GPIO_3_PD			0x40
#define PALMAS_PU_PD_GPIO_CTRL1_GPIO_3_PD_SHIFT			6
#define PALMAS_PU_PD_GPIO_CTRL1_GPIO_2_PU			0x20
#define PALMAS_PU_PD_GPIO_CTRL1_GPIO_2_PU_SHIFT			5
#define PALMAS_PU_PD_GPIO_CTRL1_GPIO_2_PD			0x10
#define PALMAS_PU_PD_GPIO_CTRL1_GPIO_2_PD_SHIFT			4
#define PALMAS_PU_PD_GPIO_CTRL1_GPIO_1_PU			0x08
#define PALMAS_PU_PD_GPIO_CTRL1_GPIO_1_PU_SHIFT			3
#define PALMAS_PU_PD_GPIO_CTRL1_GPIO_1_PD			0x04
#define PALMAS_PU_PD_GPIO_CTRL1_GPIO_1_PD_SHIFT			2
#define PALMAS_PU_PD_GPIO_CTRL1_GPIO_0_PD			0x01
#define PALMAS_PU_PD_GPIO_CTRL1_GPIO_0_PD_SHIFT			0

/* Bit definitions for PU_PD_GPIO_CTRL2 */
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_7_PD			0x40
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_7_PD_SHIFT			6
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_6_PU			0x20
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_6_PU_SHIFT			5
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_6_PD			0x10
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_6_PD_SHIFT			4
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_5_PU			0x08
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_5_PU_SHIFT			3
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_5_PD			0x04
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_5_PD_SHIFT			2
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_4_PU			0x02
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_4_PU_SHIFT			1
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_4_PD			0x01
#define PALMAS_PU_PD_GPIO_CTRL2_GPIO_4_PD_SHIFT			0

/* Bit definitions for OD_OUTPUT_GPIO_CTRL */
#define PALMAS_OD_OUTPUT_GPIO_CTRL_GPIO_5_OD			0x20
#define PALMAS_OD_OUTPUT_GPIO_CTRL_GPIO_5_OD_SHIFT		5
#define PALMAS_OD_OUTPUT_GPIO_CTRL_GPIO_2_OD			0x04
#define PALMAS_OD_OUTPUT_GPIO_CTRL_GPIO_2_OD_SHIFT		2
#define PALMAS_OD_OUTPUT_GPIO_CTRL_GPIO_1_OD			0x02
#define PALMAS_OD_OUTPUT_GPIO_CTRL_GPIO_1_OD_SHIFT		1

/* Bit definitions for GPIO_DATA_IN2 */
#define PALMAS_GPIO_DATA_IN_GPIO_15_IN				0x80
#define PALMAS_GPIO_DATA_IN_GPIO_15_IN_SHIFT			7
#define PALMAS_GPIO_DATA_IN_GPIO_14_IN				0x40
#define PALMAS_GPIO_DATA_IN_GPIO_14_IN_SHIFT			6
#define PALMAS_GPIO_DATA_IN_GPIO_13_IN				0x20
#define PALMAS_GPIO_DATA_IN_GPIO_13_IN_SHIFT			5
#define PALMAS_GPIO_DATA_IN_GPIO_12_IN				0x10
#define PALMAS_GPIO_DATA_IN_GPIO_12_IN_SHIFT			4
#define PALMAS_GPIO_DATA_IN_GPIO_11_IN				0x08
#define PALMAS_GPIO_DATA_IN_GPIO_11_IN_SHIFT			3
#define PALMAS_GPIO_DATA_IN_GPIO_10_IN				0x04
#define PALMAS_GPIO_DATA_IN_GPIO_10_IN_SHIFT			2
#define PALMAS_GPIO_DATA_IN_GPIO_9_IN				0x02
#define PALMAS_GPIO_DATA_IN_GPIO_9_IN_SHIFT			1
#define PALMAS_GPIO_DATA_IN_GPIO_8_IN				0x01
#define PALMAS_GPIO_DATA_IN_GPIO_8_IN_SHIFT			0

/* Bit definitions for GPIO_DATA_DIR2 */
#define PALMAS_GPIO_DATA_DIR_GPIO_15_DIR			0x80
#define PALMAS_GPIO_DATA_DIR_GPIO_15_DIR_SHIFT			7
#define PALMAS_GPIO_DATA_DIR_GPIO_14_DIR			0x40
#define PALMAS_GPIO_DATA_DIR_GPIO_14_DIR_SHIFT			6
#define PALMAS_GPIO_DATA_DIR_GPIO_13_DIR			0x20
#define PALMAS_GPIO_DATA_DIR_GPIO_13_DIR_SHIFT			5
#define PALMAS_GPIO_DATA_DIR_GPIO_12_DIR			0x10
#define PALMAS_GPIO_DATA_DIR_GPIO_12_DIR_SHIFT			4
#define PALMAS_GPIO_DATA_DIR_GPIO_11_DIR			0x08
#define PALMAS_GPIO_DATA_DIR_GPIO_11_DIR_SHIFT			3
#define PALMAS_GPIO_DATA_DIR_GPIO_10_DIR			0x04
#define PALMAS_GPIO_DATA_DIR_GPIO_10_DIR_SHIFT			2
#define PALMAS_GPIO_DATA_DIR_GPIO_9_DIR				0x02
#define PALMAS_GPIO_DATA_DIR_GPIO_9_DIR_SHIFT			1
#define PALMAS_GPIO_DATA_DIR_GPIO_8_DIR				0x01
#define PALMAS_GPIO_DATA_DIR_GPIO_8_DIR_SHIFT			0

/* Bit definitions for GPIO_DATA_OUT2 */
#define PALMAS_GPIO_DATA_OUT_GPIO_15_OUT			0x80
#define PALMAS_GPIO_DATA_OUT_GPIO_15_OUT_SHIFT			7
#define PALMAS_GPIO_DATA_OUT_GPIO_14_OUT			0x40
#define PALMAS_GPIO_DATA_OUT_GPIO_14_OUT_SHIFT			6
#define PALMAS_GPIO_DATA_OUT_GPIO_13_OUT			0x20
#define PALMAS_GPIO_DATA_OUT_GPIO_13_OUT_SHIFT			5
#define PALMAS_GPIO_DATA_OUT_GPIO_12_OUT			0x10
#define PALMAS_GPIO_DATA_OUT_GPIO_12_OUT_SHIFT			4
#define PALMAS_GPIO_DATA_OUT_GPIO_11_OUT			0x08
#define PALMAS_GPIO_DATA_OUT_GPIO_11_OUT_SHIFT			3
#define PALMAS_GPIO_DATA_OUT_GPIO_10_OUT			0x04
#define PALMAS_GPIO_DATA_OUT_GPIO_10_OUT_SHIFT			2
#define PALMAS_GPIO_DATA_OUT_GPIO_9_OUT				0x02
#define PALMAS_GPIO_DATA_OUT_GPIO_9_OUT_SHIFT			1
#define PALMAS_GPIO_DATA_OUT_GPIO_8_OUT				0x01
#define PALMAS_GPIO_DATA_OUT_GPIO_8_OUT_SHIFT			0

/* Bit definitions for GPIO_DEBOUNCE_EN2 */
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_15_DEBOUNCE_EN		0x80
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_15_DEBOUNCE_EN_SHIFT	7
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_14_DEBOUNCE_EN		0x40
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_14_DEBOUNCE_EN_SHIFT	6
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_13_DEBOUNCE_EN		0x20
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_13_DEBOUNCE_EN_SHIFT	5
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_12_DEBOUNCE_EN		0x10
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_12_DEBOUNCE_EN_SHIFT	4
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_11_DEBOUNCE_EN		0x08
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_11_DEBOUNCE_EN_SHIFT	3
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_10_DEBOUNCE_EN		0x04
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_10_DEBOUNCE_EN_SHIFT	2
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_9_DEBOUNCE_EN		0x02
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_9_DEBOUNCE_EN_SHIFT	1
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_8_DEBOUNCE_EN		0x01
#define PALMAS_GPIO_DEBOUNCE_EN_GPIO_8_DEBOUNCE_EN_SHIFT	0

/* Bit definitions for GPIO_CLEAR_DATA_OUT2 */
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_15_CLEAR_DATA_OUT	0x80
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_15_CLEAR_DATA_OUT_SHIFT	7
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_14_CLEAR_DATA_OUT	0x40
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_14_CLEAR_DATA_OUT_SHIFT	6
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_13_CLEAR_DATA_OUT	0x20
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_13_CLEAR_DATA_OUT_SHIFT	5
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_12_CLEAR_DATA_OUT	0x10
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_12_CLEAR_DATA_OUT_SHIFT	4
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_11_CLEAR_DATA_OUT	0x08
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_11_CLEAR_DATA_OUT_SHIFT	3
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_10_CLEAR_DATA_OUT	0x04
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_10_CLEAR_DATA_OUT_SHIFT	2
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_9_CLEAR_DATA_OUT	0x02
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_9_CLEAR_DATA_OUT_SHIFT	1
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_8_CLEAR_DATA_OUT	0x01
#define PALMAS_GPIO_CLEAR_DATA_OUT_GPIO_8_CLEAR_DATA_OUT_SHIFT	0

/* Bit definitions for GPIO_SET_DATA_OUT2 */
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_15_SET_DATA_OUT		0x80
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_15_SET_DATA_OUT_SHIFT	7
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_14_SET_DATA_OUT		0x40
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_14_SET_DATA_OUT_SHIFT	6
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_13_SET_DATA_OUT		0x20
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_13_SET_DATA_OUT_SHIFT	5
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_12_SET_DATA_OUT		0x10
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_12_SET_DATA_OUT_SHIFT	4
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_11_SET_DATA_OUT		0x08
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_11_SET_DATA_OUT_SHIFT	3
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_10_SET_DATA_OUT		0x04
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_10_SET_DATA_OUT_SHIFT	2
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_9_SET_DATA_OUT		0x02
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_9_SET_DATA_OUT_SHIFT	1
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_8_SET_DATA_OUT		0x01
#define PALMAS_GPIO_SET_DATA_OUT_GPIO_8_SET_DATA_OUT_SHIFT	0

/* Bit definitions for PU_PD_GPIO_CTRL3 */
#define PALMAS_PU_PD_GPIO_CTRL3_GPIO_11_PD			0x40
#define PALMAS_PU_PD_GPIO_CTRL3_GPIO_11_PD_SHIFT		6
#define PALMAS_PU_PD_GPIO_CTRL3_GPIO_10_PU			0x20
#define PALMAS_PU_PD_GPIO_CTRL3_GPIO_10_PU_SHIFT		5
#define PALMAS_PU_PD_GPIO_CTRL3_GPIO_10_PD			0x10
#define PALMAS_PU_PD_GPIO_CTRL3_GPIO_10_PD_SHIFT		4
#define PALMAS_PU_PD_GPIO_CTRL3_GPIO_9_PU			0x08
#define PALMAS_PU_PD_GPIO_CTRL3_GPIO_9_PU_SHIFT			3
#define PALMAS_PU_PD_GPIO_CTRL3_GPIO_9_PD			0x04
#define PALMAS_PU_PD_GPIO_CTRL3_GPIO_9_PD_SHIFT			2

/* Bit definitions for PU_PD_GPIO_CTRL4 */
#define PALMAS_PU_PD_GPIO_CTRL4_GPIO_14_PU			0x20
#define PALMAS_PU_PD_GPIO_CTRL4_GPIO_14_PU_SHIFT		5
#define PALMAS_PU_PD_GPIO_CTRL4_GPIO_14_PD			0x10
#define PALMAS_PU_PD_GPIO_CTRL4_GPIO_14_PD_SHIFT		4

/* Bit definitions for OD_OUTPUT_GPIO_CTRL2 */
#define PALMAS_OD_OUTPUT_GPIO_CTRL_GPIO_10_OD			0x04
#define PALMAS_OD_OUTPUT_GPIO_CTRL_GPIO_10_OD_SHIFT		2

/* Registers for function GPADC */
#define PALMAS_GPADC_CTRL1					0x0
#define PALMAS_GPADC_CTRL2					0x1
#define PALMAS_GPADC_RT_CTRL					0x2
#define PALMAS_GPADC_AUTO_CTRL					0x3
#define PALMAS_GPADC_STATUS					0x4
#define PALMAS_GPADC_RT_SELECT					0x5
#define PALMAS_GPADC_RT_CONV0_LSB				0x6
#define PALMAS_GPADC_RT_CONV0_MSB				0x7
#define PALMAS_GPADC_AUTO_SELECT				0x8
#define PALMAS_GPADC_AUTO_CONV0_LSB				0x9
#define PALMAS_GPADC_AUTO_CONV0_MSB				0xA
#define PALMAS_GPADC_AUTO_CONV1_LSB				0xB
#define PALMAS_GPADC_AUTO_CONV1_MSB				0xC
#define PALMAS_GPADC_SW_SELECT					0xD
#define PALMAS_GPADC_SW_CONV0_LSB				0xE
#define PALMAS_GPADC_SW_CONV0_MSB				0xF
#define PALMAS_GPADC_THRES_CONV0_LSB				0x10
#define PALMAS_GPADC_THRES_CONV0_MSB				0x11
#define PALMAS_GPADC_THRES_CONV1_LSB				0x12
#define PALMAS_GPADC_THRES_CONV1_MSB				0x13
#define PALMAS_GPADC_SMPS_ILMONITOR_EN				0x14
#define PALMAS_GPADC_SMPS_VSEL_MONITORING			0x15

/* Bit definitions for GPADC_CTRL1 */
#define PALMAS_GPADC_CTRL1_RESERVED_MASK			0xc0
#define PALMAS_GPADC_CTRL1_RESERVED_SHIFT			6
#define PALMAS_GPADC_CTRL1_CURRENT_SRC_CH3_MASK			0x30
#define PALMAS_GPADC_CTRL1_CURRENT_SRC_CH3_SHIFT		4
#define PALMAS_GPADC_CTRL1_CURRENT_SRC_CH0_MASK			0x0c
#define PALMAS_GPADC_CTRL1_CURRENT_SRC_CH0_SHIFT		2
#define PALMAS_GPADC_CTRL1_BAT_REMOVAL_DET			0x02
#define PALMAS_GPADC_CTRL1_BAT_REMOVAL_DET_SHIFT		1
#define PALMAS_GPADC_CTRL1_GPADC_FORCE				0x01
#define PALMAS_GPADC_CTRL1_GPADC_FORCE_SHIFT			0

/* Bit definitions for GPADC_CTRL2 */
#define PALMAS_GPADC_CTRL2_RESERVED_MASK			0x06
#define PALMAS_GPADC_CTRL2_RESERVED_SHIFT			1

/* Bit definitions for GPADC_RT_CTRL */
#define PALMAS_GPADC_RT_CTRL_EXTEND_DELAY			0x02
#define PALMAS_GPADC_RT_CTRL_EXTEND_DELAY_SHIFT			1
#define PALMAS_GPADC_RT_CTRL_START_POLARITY			0x01
#define PALMAS_GPADC_RT_CTRL_START_POLARITY_SHIFT		0

/* Bit definitions for GPADC_AUTO_CTRL */
#define PALMAS_GPADC_AUTO_CTRL_SHUTDOWN_CONV1			0x80
#define PALMAS_GPADC_AUTO_CTRL_SHUTDOWN_CONV1_SHIFT		7
#define PALMAS_GPADC_AUTO_CTRL_SHUTDOWN_CONV0			0x40
#define PALMAS_GPADC_AUTO_CTRL_SHUTDOWN_CONV0_SHIFT		6
#define PALMAS_GPADC_AUTO_CTRL_AUTO_CONV1_EN			0x20
#define PALMAS_GPADC_AUTO_CTRL_AUTO_CONV1_EN_SHIFT		5
#define PALMAS_GPADC_AUTO_CTRL_AUTO_CONV0_EN			0x10
#define PALMAS_GPADC_AUTO_CTRL_AUTO_CONV0_EN_SHIFT		4
#define PALMAS_GPADC_AUTO_CTRL_COUNTER_CONV_MASK		0x0f
#define PALMAS_GPADC_AUTO_CTRL_COUNTER_CONV_SHIFT		0

/* Bit definitions for GPADC_STATUS */
#define PALMAS_GPADC_STATUS_GPADC_AVAILABLE			0x10
#define PALMAS_GPADC_STATUS_GPADC_AVAILABLE_SHIFT		4

/* Bit definitions for GPADC_RT_SELECT */
#define PALMAS_GPADC_RT_SELECT_RT_CONV_EN			0x80
#define PALMAS_GPADC_RT_SELECT_RT_CONV_EN_SHIFT			7
#define PALMAS_GPADC_RT_SELECT_RT_CONV0_SEL_MASK		0x0f
#define PALMAS_GPADC_RT_SELECT_RT_CONV0_SEL_SHIFT		0

/* Bit definitions for GPADC_RT_CONV0_LSB */
#define PALMAS_GPADC_RT_CONV0_LSB_RT_CONV0_LSB_MASK		0xff
#define PALMAS_GPADC_RT_CONV0_LSB_RT_CONV0_LSB_SHIFT		0

/* Bit definitions for GPADC_RT_CONV0_MSB */
#define PALMAS_GPADC_RT_CONV0_MSB_RT_CONV0_MSB_MASK		0x0f
#define PALMAS_GPADC_RT_CONV0_MSB_RT_CONV0_MSB_SHIFT		0

/* Bit definitions for GPADC_AUTO_SELECT */
#define PALMAS_GPADC_AUTO_SELECT_AUTO_CONV1_SEL_MASK		0xf0
#define PALMAS_GPADC_AUTO_SELECT_AUTO_CONV1_SEL_SHIFT		4
#define PALMAS_GPADC_AUTO_SELECT_AUTO_CONV0_SEL_MASK		0x0f
#define PALMAS_GPADC_AUTO_SELECT_AUTO_CONV0_SEL_SHIFT		0

/* Bit definitions for GPADC_AUTO_CONV0_LSB */
#define PALMAS_GPADC_AUTO_CONV0_LSB_AUTO_CONV0_LSB_MASK		0xff
#define PALMAS_GPADC_AUTO_CONV0_LSB_AUTO_CONV0_LSB_SHIFT	0

/* Bit definitions for GPADC_AUTO_CONV0_MSB */
#define PALMAS_GPADC_AUTO_CONV0_MSB_AUTO_CONV0_MSB_MASK		0x0f
#define PALMAS_GPADC_AUTO_CONV0_MSB_AUTO_CONV0_MSB_SHIFT	0

/* Bit definitions for GPADC_AUTO_CONV1_LSB */
#define PALMAS_GPADC_AUTO_CONV1_LSB_AUTO_CONV1_LSB_MASK		0xff
#define PALMAS_GPADC_AUTO_CONV1_LSB_AUTO_CONV1_LSB_SHIFT	0

/* Bit definitions for GPADC_AUTO_CONV1_MSB */
#define PALMAS_GPADC_AUTO_CONV1_MSB_AUTO_CONV1_MSB_MASK		0x0f
#define PALMAS_GPADC_AUTO_CONV1_MSB_AUTO_CONV1_MSB_SHIFT	0

/* Bit definitions for GPADC_SW_SELECT */
#define PALMAS_GPADC_SW_SELECT_SW_CONV_EN			0x80
#define PALMAS_GPADC_SW_SELECT_SW_CONV_EN_SHIFT			7
#define PALMAS_GPADC_SW_SELECT_SW_START_CONV0			0x10
#define PALMAS_GPADC_SW_SELECT_SW_START_CONV0_SHIFT		4
#define PALMAS_GPADC_SW_SELECT_SW_CONV0_SEL_MASK		0x0f
#define PALMAS_GPADC_SW_SELECT_SW_CONV0_SEL_SHIFT		0

/* Bit definitions for GPADC_SW_CONV0_LSB */
#define PALMAS_GPADC_SW_CONV0_LSB_SW_CONV0_LSB_MASK		0xff
#define PALMAS_GPADC_SW_CONV0_LSB_SW_CONV0_LSB_SHIFT		0

/* Bit definitions for GPADC_SW_CONV0_MSB */
#define PALMAS_GPADC_SW_CONV0_MSB_SW_CONV0_MSB_MASK		0x0f
#define PALMAS_GPADC_SW_CONV0_MSB_SW_CONV0_MSB_SHIFT		0

/* Bit definitions for GPADC_THRES_CONV0_LSB */
#define PALMAS_GPADC_THRES_CONV0_LSB_THRES_CONV0_LSB_MASK	0xff
#define PALMAS_GPADC_THRES_CONV0_LSB_THRES_CONV0_LSB_SHIFT	0

/* Bit definitions for GPADC_THRES_CONV0_MSB */
#define PALMAS_GPADC_THRES_CONV0_MSB_THRES_CONV0_POL		0x80
#define PALMAS_GPADC_THRES_CONV0_MSB_THRES_CONV0_POL_SHIFT	7
#define PALMAS_GPADC_THRES_CONV0_MSB_THRES_CONV0_MSB_MASK	0x0f
#define PALMAS_GPADC_THRES_CONV0_MSB_THRES_CONV0_MSB_SHIFT	0

/* Bit definitions for GPADC_THRES_CONV1_LSB */
#define PALMAS_GPADC_THRES_CONV1_LSB_THRES_CONV1_LSB_MASK	0xff
#define PALMAS_GPADC_THRES_CONV1_LSB_THRES_CONV1_LSB_SHIFT	0

/* Bit definitions for GPADC_THRES_CONV1_MSB */
#define PALMAS_GPADC_THRES_CONV1_MSB_THRES_CONV1_POL		0x80
#define PALMAS_GPADC_THRES_CONV1_MSB_THRES_CONV1_POL_SHIFT	7
#define PALMAS_GPADC_THRES_CONV1_MSB_THRES_CONV1_MSB_MASK	0x0f
#define PALMAS_GPADC_THRES_CONV1_MSB_THRES_CONV1_MSB_SHIFT	0

/* Bit definitions for GPADC_SMPS_ILMONITOR_EN */
#define PALMAS_GPADC_SMPS_ILMONITOR_EN_SMPS_ILMON_EN		0x20
#define PALMAS_GPADC_SMPS_ILMONITOR_EN_SMPS_ILMON_EN_SHIFT	5
#define PALMAS_GPADC_SMPS_ILMONITOR_EN_SMPS_ILMON_REXT		0x10
#define PALMAS_GPADC_SMPS_ILMONITOR_EN_SMPS_ILMON_REXT_SHIFT	4
#define PALMAS_GPADC_SMPS_ILMONITOR_EN_SMPS_ILMON_SEL_MASK	0x0f
#define PALMAS_GPADC_SMPS_ILMONITOR_EN_SMPS_ILMON_SEL_SHIFT	0

/* Bit definitions for GPADC_SMPS_VSEL_MONITORING */
#define PALMAS_GPADC_SMPS_VSEL_MONITORING_ACTIVE_PHASE		0x80
#define PALMAS_GPADC_SMPS_VSEL_MONITORING_ACTIVE_PHASE_SHIFT	7
#define PALMAS_GPADC_SMPS_VSEL_MONITORING_SMPS_VSEL_MONITORING_MASK	0x7f
#define PALMAS_GPADC_SMPS_VSEL_MONITORING_SMPS_VSEL_MONITORING_SHIFT	0

#define PALMAS_INTERNAL_DESIGNREV				0x57
#define PALMAS_INTERNAL_DESIGNREV_DESIGNREV(val)		((val) & 0xF)

/* Registers for function GPADC */
#define PALMAS_GPADC_TRIM1					0x0
#define PALMAS_GPADC_TRIM2					0x1
#define PALMAS_GPADC_TRIM3					0x2
#define PALMAS_GPADC_TRIM4					0x3
#define PALMAS_GPADC_TRIM5					0x4
#define PALMAS_GPADC_TRIM6					0x5
#define PALMAS_GPADC_TRIM7					0x6
#define PALMAS_GPADC_TRIM8					0x7
#define PALMAS_GPADC_TRIM9					0x8
#define PALMAS_GPADC_TRIM10					0x9
#define PALMAS_GPADC_TRIM11					0xA
#define PALMAS_GPADC_TRIM12					0xB
#define PALMAS_GPADC_TRIM13					0xC
#define PALMAS_GPADC_TRIM14					0xD
#define PALMAS_GPADC_TRIM15					0xE
#define PALMAS_GPADC_TRIM16					0xF
#define PALMAS_GPADC_TRIMINVALID				-1

/* Registers for function BQ24192 */
#define PALMAS_CHARGER_REG00					0x00
#define PALMAS_CHARGER_REG01					0x01
#define PALMAS_CHARGER_REG02					0x02
#define PALMAS_CHARGER_REG03					0x03
#define PALMAS_CHARGER_REG04					0x04
#define PALMAS_CHARGER_REG05					0x05
#define PALMAS_CHARGER_REG06					0x06
#define PALMAS_CHARGER_REG07					0x07
#define PALMAS_CHARGER_REG08					0x08
#define PALMAS_CHARGER_REG09					0x09
#define PALMAS_CHARGER_REG10					0x0a

#define BQ24190_IC_VER                  0x40
#define BQ24192_IC_VER                  0x28
#define BQ24192i_IC_VER                 0x18

#define PALMAS_ENABLE_CHARGE_MASK      0x30
#define PALMAS_DISABLE_CHARGE          0x00
#define PALMAS_ENABLE_CHARGE           0x10
#define PALMAS_ENABLE_VBUS             0x20
#define PALMAS_DISABLE_CHARGE		0x00

#define PALMAS_REG0                    0x0
#define PALMAS_EN_HIZ                  BIT(7)

#define PALMAS_CHRG_CTRL_REG_3A        0xC0
#define PALMAS_OTP_CURRENT_500MA       0x32

#define PALMAS_WD                      0x5
#define PALMAS_WD_MASK                 0x30
#define PALMAS_WD_DISABLE              0x00
#define PALMAS_WD_40ms                 0x10
#define PALMAS_WD_80ms                 0x20
#define PALMAS_WD_160ms                0x30

#define PALMAS_VBUS_STAT               0xc0
#define PALMAS_VBUS_UNKNOWN            0x00
#define PALMAS_VBUS_USB                0x40
#define PALMAS_VBUS_AC                 0x80

#define PALMAS_CHRG_STATE_MASK                 0x30
#define PALMAS_CHRG_STATE_NOTCHARGING          0x00
#define PALMAS_CHRG_STATE_PRE_CHARGE           0x10
#define PALMAS_CHRG_STATE_POST_CHARGE          0x20
#define PALMAS_CHRG_STATE_CHARGE_DONE          0x30

#define PALMAS_FAULT_WATCHDOG_FAULT            BIT(7)
#define PALMAS_FAULT_BOOST_FAULT               BIT(6)
#define PALMAS_FAULT_CHRG_FAULT_MASK           0x30
#define PALMAS_FAULT_CHRG_NORMAL               0x00
#define PALMAS_FAULT_CHRG_INPUT                0x10
#define PALMAS_FAULT_CHRG_THERMAL              0x20
#define PALMAS_FAULT_CHRG_SAFTY                0x30

#define PALMAS_FAULT_NTC_FAULT                 0x07

#define PALMAS_CONFIG_MASK             0x7
#define PALMAS_INPUT_VOLTAGE_MASK      0x78
#define PALMAS_NVCHARGER_INPUT_VOL_SEL 0x40
#define PALMAS_DEFAULT_INPUT_VOL_SEL   0x30

#define PALMAS_CHARGE_VOLTAGE_MASK		0xFC
#define PALMAS_CHARGE_VOLTAGE_4112MV		0x98
#define PALMAS_CHARGE_VOLTAGE_4048MV		0x88

#define PALMAS_MAX_REGS                (PALMAS_REVISION_REG + 1)

/* Registers for function FUEL_GAUGE */
#define PALMAS_FG_REG_00                                      0x0
#define PALMAS_FG_REG_01                                      0x1
#define PALMAS_FG_REG_02                                      0x2
#define PALMAS_FG_REG_03                                      0x3
#define PALMAS_FG_REG_04                                      0x4
#define PALMAS_FG_REG_05                                      0x5
#define PALMAS_FG_REG_06                                      0x6
#define PALMAS_FG_REG_07                                      0x7
#define PALMAS_FG_REG_08                                      0x8
#define PALMAS_FG_REG_09                                      0x9
#define PALMAS_FG_REG_10                                      0xA
#define PALMAS_FG_REG_11                                      0xB
#define PALMAS_FG_REG_12                                      0xC
#define PALMAS_FG_REG_13                                      0xD
#define PALMAS_FG_REG_14                                      0xE
#define PALMAS_FG_REG_15                                      0xF
#define PALMAS_FG_REG_16                                      0x10
#define PALMAS_FG_REG_17                                      0x11
#define PALMAS_FG_REG_18                                      0x12
#define PALMAS_FG_REG_19                                      0x13
#define PALMAS_FG_REG_20                                      0x14
#define PALMAS_FG_REG_21                                      0x15
#define PALMAS_FG_REG_22                                      0x16

/* Bit definitions for FG_REG_00 */
#define PALMAS_FG_REG_00_CC_ACTIVE_MODE_MASK                  0xc0
#define PALMAS_FG_REG_00_CC_ACTIVE_MODE_SHIFT                 6
#define PALMAS_FG_REG_00_CC_BAT_STABLE_EN                     0x20
#define PALMAS_FG_REG_00_CC_BAT_STABLE_EN_SHIFT                       5
#define PALMAS_FG_REG_00_CC_DITH_EN                           0x10
#define PALMAS_FG_REG_00_CC_DITH_EN_SHIFT                     4
#define PALMAS_FG_REG_00_CC_FG_EN                             0x08
#define PALMAS_FG_REG_00_CC_FG_EN_SHIFT                               3
#define PALMAS_FG_REG_00_CC_AUTOCLEAR                         0x04
#define PALMAS_FG_REG_00_CC_AUTOCLEAR_SHIFT                   2
#define PALMAS_FG_REG_00_CC_CAL_EN                            0x02
#define PALMAS_FG_REG_00_CC_CAL_EN_SHIFT                      1
#define PALMAS_FG_REG_00_CC_PAUSE                             0x01
#define PALMAS_FG_REG_00_CC_PAUSE_SHIFT                               0

/* Bit definitions for FG_REG_01 */
#define PALMAS_FG_REG_01_CC_SAMPLE_CNTR_MASK                  0xff
#define PALMAS_FG_REG_01_CC_SAMPLE_CNTR_SHIFT                 0

/* Bit definitions for FG_REG_02 */
#define PALMAS_FG_REG_02_CC_SAMPLE_CNTR_MASK                  0xff
#define PALMAS_FG_REG_02_CC_SAMPLE_CNTR_SHIFT                 0

/* Bit definitions for FG_REG_03 */
#define PALMAS_FG_REG_03_CC_SAMPLE_CNTR_MASK                  0xff
#define PALMAS_FG_REG_03_CC_SAMPLE_CNTR_SHIFT                 0

/* Bit definitions for FG_REG_04 */
#define PALMAS_FG_REG_04_CC_ACCUM_MASK                                0xff
#define PALMAS_FG_REG_04_CC_ACCUM_SHIFT                               0

/* Bit definitions for FG_REG_05 */
#define PALMAS_FG_REG_05_CC_ACCUM_MASK                                0xff
#define PALMAS_FG_REG_05_CC_ACCUM_SHIFT                               0

/* Bit definitions for FG_REG_06 */
#define PALMAS_FG_REG_06_CC_ACCUM_MASK                                0xff
#define PALMAS_FG_REG_06_CC_ACCUM_SHIFT                               0

/* Bit definitions for FG_REG_07 */
#define PALMAS_FG_REG_07_CC_ACCUM_MASK                                0xff
#define PALMAS_FG_REG_07_CC_ACCUM_SHIFT                               0

/* Bit definitions for FG_REG_08 */
#define PALMAS_FG_REG_08_CC_OFFSET_MASK                               0xff
#define PALMAS_FG_REG_08_CC_OFFSET_SHIFT                      0

/* Bit definitions for FG_REG_09 */
#define PALMAS_FG_REG_09_CC_OFFSET_MASK                               0x03
#define PALMAS_FG_REG_09_CC_OFFSET_SHIFT                      0

/* Bit definitions for FG_REG_10 */
#define PALMAS_FG_REG_10_CC_INTEG_MASK                                0xff
#define PALMAS_FG_REG_10_CC_INTEG_SHIFT                               0

/* Bit definitions for FG_REG_11 */
#define PALMAS_FG_REG_11_CC_INTEG_MASK                                0x3f
#define PALMAS_FG_REG_11_CC_INTEG_SHIFT                               0

/* Bit definitions for FG_REG_12 */
#define PALMAS_FG_REG_12_CC_VBAT_SYNC_MASK                    0xfc
#define PALMAS_FG_REG_12_CC_VBAT_SYNC_SHIFT                   2
#define PALMAS_FG_REG_12_CC_SYNC_EN                           0x02
#define PALMAS_FG_REG_12_CC_SYNC_EN_SHIFT                     1
#define PALMAS_FG_REG_12_CC_SYNC_RDY                          0x01
#define PALMAS_FG_REG_12_CC_SYNC_RDY_SHIFT                    0

/* Bit definitions for FG_REG_13 */
#define PALMAS_FG_REG_13_CC_VBAT_SYNC_MASK                    0x3f
#define PALMAS_FG_REG_13_CC_VBAT_SYNC_SHIFT                   0

/* Bit definitions for FG_REG_14 */
#define PALMAS_FG_REG_14_CC_VBAT_CNTR_MASK                    0xff
#define PALMAS_FG_REG_14_CC_VBAT_CNTR_SHIFT                   0

/* Bit definitions for FG_REG_15 */
#define PALMAS_FG_REG_15_CC_VBAT_CNTR_MASK                    0x03
#define PALMAS_FG_REG_15_CC_VBAT_CNTR_SHIFT                   0

/* Bit definitions for FG_REG_16 */
#define PALMAS_FG_REG_16_CC_VBAT_ACCUM_MASK                   0xff
#define PALMAS_FG_REG_16_CC_VBAT_ACCUM_SHIFT                  0

/* Bit definitions for FG_REG_17 */
#define PALMAS_FG_REG_17_CC_VBAT_ACCUM_MASK                   0xff
#define PALMAS_FG_REG_17_CC_VBAT_ACCUM_SHIFT                  0

/* Bit definitions for FG_REG_18 */
#define PALMAS_FG_REG_18_CC_VBAT_ACCUM_MASK                   0x3f
#define PALMAS_FG_REG_18_CC_VBAT_ACCUM_SHIFT                  0

/* Bit definitions for FG_REG_19 */
#define PALMAS_FG_REG_19_CC_CUR_LVL_MASK                      0x3f
#define PALMAS_FG_REG_19_CC_CUR_LVL_SHIFT                     0

/* Bit definitions for FG_REG_20 */
#define PALMAS_FG_REG_20_BAT_SLEEP_STATUS                     0x40
#define PALMAS_FG_REG_20_BAT_SLEEP_STATUS_SHIFT                       6
#define PALMAS_FG_REG_20_CC_BAT_SLEEP_PERIOD_MASK             0x30
#define PALMAS_FG_REG_20_CC_BAT_SLEEP_PERIOD_SHIFT            4
#define PALMAS_FG_REG_20_CC_BAT_SLEEP_EXIT_MASK                       0x0c
#define PALMAS_FG_REG_20_CC_BAT_SLEEP_EXIT_SHIFT              2
#define PALMAS_FG_REG_20_CC_BAT_SLEEP_ENTER_MASK              0x03
#define PALMAS_FG_REG_20_CC_BAT_SLEEP_ENTER_SHIFT             0

/* Bit definitions for FG_REG_21 */
#define PALMAS_FG_REG_21_CC_OVERCUR_THRES_MASK                        0x7f
#define PALMAS_FG_REG_21_CC_OVERCUR_THRES_SHIFT                       0

/* Bit definitions for FG_REG_22 */
#define PALMAS_FG_REG_22_CC_CHOPPER_DIS                               0x80
#define PALMAS_FG_REG_22_CC_CHOPPER_DIS_SHIFT                 7
#define PALMAS_FG_REG_22_CC_NSLEEP_GATE                               0x08
#define PALMAS_FG_REG_22_CC_NSLEEP_GATE_SHIFT                 3
#define PALMAS_FG_REG_22_CC_OVC_EN                            0x04
#define PALMAS_FG_REG_22_CC_OVC_EN_SHIFT                      2
#define PALMAS_FG_REG_22_CC_OVC_PER_MASK                      0x03
#define PALMAS_FG_REG_22_CC_OVC_PER_SHIFT                     0

enum {
	PALMAS_EXT_CONTROL_ENABLE1	= 0x1,
	PALMAS_EXT_CONTROL_ENABLE2	= 0x2,
	PALMAS_EXT_CONTROL_NSLEEP	= 0x4,
};

/**
 * Palmas regulator configs
 * PALMAS_REGULATOR_CONFIG_SUSPEND_FORCE_OFF: Force off on suspend
 * PALMAS_REGULATOR_CONFIG_TRACKING_ENABLE: Enable tracking of regualtor.
 * PALMAS_REGULATOR_CONFIG_SUSPEND_TRACKING_DISABLE: Disable tracking in
		suspend.
 */
enum {
	PALMAS_REGULATOR_CONFIG_SUSPEND_FORCE_OFF		= 0x1,
	PALMAS_REGULATOR_CONFIG_TRACKING_ENABLE			= 0x2,
	PALMAS_REGULATOR_CONFIG_SUSPEND_TRACKING_DISABLE	= 0x4,
};

/*
 *PALMAS GPIOs
 */
enum {
	PALMAS_GPIO0,
	PALMAS_GPIO1,
	PALMAS_GPIO2,
	PALMAS_GPIO3,
	PALMAS_GPIO4,
	PALMAS_GPIO5,
	PALMAS_GPIO6,
	PALMAS_GPIO7,
	PALMAS_GPIO8,
	PALMAS_GPIO9,
	PALMAS_GPIO10,
	PALMAS_GPIO11,
	PALMAS_GPIO12,
	PALMAS_GPIO13,
	PALMAS_GPIO14,
	PALMAS_GPIO15,

	PALMAS_GPIO_NR,
};

/* Palma GPADC Channels */
enum {
	PALMAS_ADC_CH_IN0,
	PALMAS_ADC_CH_IN1,
	PALMAS_ADC_CH_IN2,
	PALMAS_ADC_CH_IN3,
	PALMAS_ADC_CH_IN4,
	PALMAS_ADC_CH_IN5,
	PALMAS_ADC_CH_IN6,
	PALMAS_ADC_CH_IN7,
	PALMAS_ADC_CH_IN8,
	PALMAS_ADC_CH_IN9,
	PALMAS_ADC_CH_IN10,
	PALMAS_ADC_CH_IN11,
	PALMAS_ADC_CH_IN12,
	PALMAS_ADC_CH_IN13,
	PALMAS_ADC_CH_IN14,
	PALMAS_ADC_CH_IN15,

	PALMAS_ADC_CH_MAX,
};

/* Palma Sleep requestor IDs IDs */
enum {
	PALMAS_SLEEP_REQSTR_ID_REGEN1,
	PALMAS_SLEEP_REQSTR_ID_REGEN2,
	PALMAS_SLEEP_REQSTR_ID_SYSEN1,
	PALMAS_SLEEP_REQSTR_ID_SYSEN2,
	PALMAS_SLEEP_REQSTR_ID_CLK32KG,
	PALMAS_SLEEP_REQSTR_ID_CLK32KGAUDIO,
	PALMAS_SLEEP_REQSTR_ID_REGEN3,
	PALMAS_SLEEP_REQSTR_ID_SMPS12,
	PALMAS_SLEEP_REQSTR_ID_SMPS3,
	PALMAS_SLEEP_REQSTR_ID_SMPS45,
	PALMAS_SLEEP_REQSTR_ID_SMPS6,
	PALMAS_SLEEP_REQSTR_ID_SMPS7,
	PALMAS_SLEEP_REQSTR_ID_SMPS8,
	PALMAS_SLEEP_REQSTR_ID_SMPS9,
	PALMAS_SLEEP_REQSTR_ID_SMPS10,
	PALMAS_SLEEP_REQSTR_ID_LDO1,
	PALMAS_SLEEP_REQSTR_ID_LDO2,
	PALMAS_SLEEP_REQSTR_ID_LDO3,
	PALMAS_SLEEP_REQSTR_ID_LDO4,
	PALMAS_SLEEP_REQSTR_ID_LDO5,
	PALMAS_SLEEP_REQSTR_ID_LDO6,
	PALMAS_SLEEP_REQSTR_ID_LDO7,
	PALMAS_SLEEP_REQSTR_ID_LDO8,
	PALMAS_SLEEP_REQSTR_ID_LDO9,
	PALMAS_SLEEP_REQSTR_ID_LDOLN,
	PALMAS_SLEEP_REQSTR_ID_LDOUSB,
	PALMAS_SLEEP_REQSTR_ID_LDO10,
	PALMAS_SLEEP_REQSTR_ID_LDO11,
	PALMAS_SLEEP_REQSTR_ID_LDO12,
	PALMAS_SLEEP_REQSTR_ID_LDO13,
	PALMAS_SLEEP_REQSTR_ID_LDO14,
	PALMAS_SLEEP_REQSTR_ID_REGEN4,
	PALMAS_SLEEP_REQSTR_ID_REGEN5,
	PALMAS_SLEEP_REQSTR_ID_REGEN7,

	/* Last entry */
	PALMAS_SLEEP_REQSTR_ID_MAX,
};

/* Palmas Pinmux option */
enum {
	PALMAS_PINMUX_GPIO = 0,
	PALMAS_PINMUX_LED,
	PALMAS_PINMUX_PWM,
	PALMAS_PINMUX_REGEN,
	PALMAS_PINMUX_SYSEN,
	PALMAS_PINMUX_CLK32KGAUDIO,
	PALMAS_PINMUX_ID,
	PALMAS_PINMUX_VBUS_DET,
	PALMAS_PINMUX_CHRG_DET,
	PALMAS_PINMUX_VAC,
	PALMAS_PINMUX_VACOK,
	PALMAS_PINMUX_POWERGOOD,
	PALMAS_PINMUX_USB_PSEL,
	PALMAS_PINMUX_MSECURE,
	PALMAS_PINMUX_PWRHOLD,
	PALMAS_PINMUX_INT,
	PALMAS_PINMUX_DVFS2,
	PALMAS_PINMUX_DVFS1,
	PALMAS_PINMUX_NRESWARM,
	PALMAS_PINMUX_SIM1RSTO,
	PALMAS_PINMUX_SIM1RSTI,
	PALMAS_PINMUX_LOW_VBAT,
	PALMAS_PINMUX_WIRELESS_CHRG1,
	PALMAS_PINMUX_RCM,
	PALMAS_PINMUX_SIM2RSTO,
	PALMAS_PINMUX_SIM2RSTI,
	PALMAS_PINMUX_PWRDOWN,
	PALMAS_PINMUX_GPADC_START,
	PALMAS_PINMUX_RESET_IN,
	PALMAS_PINMUX_NSLEEP,
	PALMAS_PINMUX_ENABLE1,
	PALMAS_PINMUX_ENABLE2,
	PALMAS_PINMUX_RESVD = 0x2000,
	PALMAS_PINMUX_DEFAULT = 0x4000,
	PALMAS_PINMUX_INVALID = 0x8000,
};

/* Palmas Pinmux Pullup/pulldown/opendrain configuration. */
enum {
	PALMAS_PIN_CONFIG_DEFAULT,
	PALMAS_PIN_CONFIG_NORMAL,
	PALMAS_PIN_CONFIG_PULL_UP,
	PALMAS_PIN_CONFIG_PULL_DOWN,

	PALMAS_PIN_CONFIG_OD_DEFAULT,
	PALMAS_PIN_CONFIG_OD_ENABLE,
	PALMAS_PIN_CONFIG_OD_DISABLE,
};

/* Palmas Pins name */
enum {
	PALMAS_PIN_NAME_GPIO0,
	PALMAS_PIN_NAME_GPIO1,
	PALMAS_PIN_NAME_GPIO2,
	PALMAS_PIN_NAME_GPIO3,
	PALMAS_PIN_NAME_GPIO4,
	PALMAS_PIN_NAME_GPIO5,
	PALMAS_PIN_NAME_GPIO6,
	PALMAS_PIN_NAME_GPIO7,
	PALMAS_PIN_NAME_GPIO8,
	PALMAS_PIN_NAME_GPIO9,
	PALMAS_PIN_NAME_GPIO10,
	PALMAS_PIN_NAME_GPIO11,
	PALMAS_PIN_NAME_GPIO12,
	PALMAS_PIN_NAME_GPIO13,
	PALMAS_PIN_NAME_GPIO14,
	PALMAS_PIN_NAME_GPIO15,
	PALMAS_PIN_NAME_VAC,
	PALMAS_PIN_NAME_POWERGOOD,
	PALMAS_PIN_NAME_NRESWARM,
	PALMAS_PIN_NAME_PWRDOWN,
	PALMAS_PIN_NAME_GPADC_START,
	PALMAS_PIN_NAME_RESET_IN,
	PALMAS_PIN_NAME_NSLEEP,
	PALMAS_PIN_NAME_ENABLE1,
	PALMAS_PIN_NAME_ENABLE2,
	PALMAS_PIN_NAME_INT,
	PALMAS_PIN_NAME_MAX,
};

extern int palmas_ext_power_req_config(struct palmas *palmas,
		int id,  int ext_pwr_ctrl, bool enable);

static inline int palmas_read(struct palmas *palmas, unsigned int base,
		unsigned int reg, unsigned int *val)
{
	unsigned int addr =  PALMAS_BASE_TO_REG(base, reg);
	int slave_id = PALMAS_BASE_TO_SLAVE(base);

	return regmap_read(palmas->regmap[slave_id], addr, val);
}

static inline int palmas_write(struct palmas *palmas, unsigned int base,
		unsigned int reg, unsigned int value)
{
	unsigned int addr = PALMAS_BASE_TO_REG(base, reg);
	int slave_id = PALMAS_BASE_TO_SLAVE(base);

	return regmap_write(palmas->regmap[slave_id], addr, value);
}

static inline int palmas_bulk_write(struct palmas *palmas, unsigned int base,
	unsigned int reg, const void *val, size_t val_count)
{
	unsigned int addr = PALMAS_BASE_TO_REG(base, reg);
	int slave_id = PALMAS_BASE_TO_SLAVE(base);

	return regmap_bulk_write(palmas->regmap[slave_id], addr,
			val, val_count);
}

static inline int palmas_bulk_read(struct palmas *palmas, unsigned int base,
		unsigned int reg, void *val, size_t val_count)
{
	unsigned int addr = PALMAS_BASE_TO_REG(base, reg);
	int slave_id = PALMAS_BASE_TO_SLAVE(base);

	return regmap_bulk_read(palmas->regmap[slave_id], addr,
		val, val_count);
}

static inline int palmas_update_bits(struct palmas *palmas, unsigned int base,
	unsigned int reg, unsigned int mask, unsigned int val)
{
	unsigned int addr = PALMAS_BASE_TO_REG(base, reg);
	int slave_id = PALMAS_BASE_TO_SLAVE(base);

	return regmap_update_bits(palmas->regmap[slave_id], addr, mask, val);
}

extern int palmas_irq_get_virq(struct palmas *palmas, int irq);

static inline int palmas_is_es_version_or_less(struct palmas *palmas,
	int major, int minor)
{
	if (palmas->es_major_version < major)
		return true;

	if ((palmas->es_major_version == major) &&
		(palmas->es_minor_version <= minor))
		return true;

	return false;
}

#define PALMAS_DATASHEET_NAME(_name)	"palmas-gpadc-chan-"#_name

#define PALMAS_GPADC_IIO_MAP(chan, _consumer, _comsumer_channel_name)	\
{									\
	.adc_channel_label = PALMAS_DATASHEET_NAME(chan),		\
	.consumer_dev_name = _consumer,					\
	.consumer_channel = _comsumer_channel_name,			\
}

#endif /*  __LINUX_MFD_PALMAS_H */
