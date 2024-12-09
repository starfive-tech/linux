// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include "sc8989x_reg.h"

enum sc89890h_vbus_type {
    SC89890H_VBUS_NONE,
    SC89890H_VBUS_USB_SDP,
    SC89890H_VBUS_USB_CDP,
    SC89890H_VBUS_USB_DCP,
    SC89890H_VBUS_HVDCP,
    SC89890H_VBUS_UNKNOWN,
    SC89890H_VBUS_NONSTAND,
    SC89890H_VBUS_OTG,
    SC89890H_VBUS_TYPE_NUM,
};

enum sc89890h_part_no {
    SC89890H = 0x04,
};


#define SC89890H_STATUS_PLUGIN          0x0001
#define SC89890H_STATUS_PG              0x0002
#define	SC89890H_STATUS_CHARGE_ENABLE   0x0004
#define SC89890H_STATUS_FAULT           0x0008

struct sc89890h_config {
    bool enable_auto_dpdm;
/*	bool enable_12v;*/

    int input_limit_current;

    int charge_voltage;
    int charge_current;

    bool enable_term;
    int term_current;

    bool enable_ico;
    bool use_absolute_vindpm;
};

struct pe_ctrl {
    bool enable;
    bool tune_up_volt;
    bool tune_down_volt;
    bool tune_done;
    bool tune_fail;
    int  tune_count;
    int  target_volt;
    int	 high_volt_level;/* vbus volt > this threshold means tune up successfully */
    int  low_volt_level; /* vbus volt < this threshold means tune down successfully */
    int  vbat_min_volt;  /* to tune up voltage only when vbat > this threshold */
};

struct sc89890h {
    struct device *dev;
    struct i2c_client *client;
    enum sc89890h_part_no part_no;
    struct pe_ctrl pe;

    unsigned int status;
    int vbus_type;

    bool enabled;

    int vbus_volt;
    int vbat_volt;

    int rsoc;

    struct mutex sc89890h_i2c_lock;

    struct sc89890h_config	cfg;
    struct work_struct irq_work;
    struct work_struct adapter_in_work;
    struct work_struct adapter_out_work;
    struct delayed_work monitor_work;
    struct delayed_work ico_work;
    struct delayed_work pe_volt_tune_work;
    struct delayed_work check_pe_tuneup_work;

    struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
    struct power_supply *usb;
    struct power_supply *wall;
    struct power_supply *batt_psy;
};



static int sc89890h_read_byte(struct sc89890h *sc, u8 *data, u8 reg)
{
    int ret;

    mutex_lock(&sc->sc89890h_i2c_lock);
    ret = i2c_smbus_read_byte_data(sc->client, reg);
    if (ret < 0) {
        dev_err(sc->dev, "failed to read 0x%.2x\n", reg);
        mutex_unlock(&sc->sc89890h_i2c_lock);
        return ret;
    }

    *data = (u8)ret;
    mutex_unlock(&sc->sc89890h_i2c_lock);

    return 0;
}

static int sc89890h_write_byte(struct sc89890h *sc, u8 reg, u8 data)
{
    int ret;
    mutex_lock(&sc->sc89890h_i2c_lock);
    ret = i2c_smbus_write_byte_data(sc->client, reg, data);
    mutex_unlock(&sc->sc89890h_i2c_lock);
    return ret;
}

static int sc89890h_update_bits(struct sc89890h *sc, u8 reg, u8 mask, u8 data)
{
    int ret;
    u8 tmp;

    ret = sc89890h_read_byte(sc, &tmp, reg);

    if (ret)
        return ret;

    tmp &= ~mask;
    tmp |= data & mask;

    return sc89890h_write_byte(sc, reg, tmp);
}


static enum sc89890h_vbus_type sc89890h_get_vbus_type(struct sc89890h *sc)
{
    u8 val = 0;
    int ret;

    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_0B);
    if (ret < 0)
        return 0;
    val &= SC89890H_VBUS_STAT_MASK;
    val >>= SC89890H_VBUS_STAT_SHIFT;

    return val;
}


static int sc89890h_enable_otg(struct sc89890h *sc)
{
    u8 val = SC89890H_OTG_ENABLE << SC89890H_OTG_CONFIG_SHIFT;

    return sc89890h_update_bits(sc, SC89890H_REG_03,
                            SC89890H_OTG_CONFIG_MASK, val);

}

int sc89890h_disable_otg(struct sc89890h *sc)
{
    u8 val = SC89890H_OTG_DISABLE << SC89890H_OTG_CONFIG_SHIFT;

    return sc89890h_update_bits(sc, SC89890H_REG_03,
                            SC89890H_OTG_CONFIG_MASK, val);

}
EXPORT_SYMBOL_GPL(sc89890h_disable_otg);

int sc89890h_set_otg_volt(struct sc89890h *sc, int volt)
{
    u8 val = 0;

    if (volt < SC89890H_BOOSTV_BASE)
        volt = SC89890H_BOOSTV_BASE;
    if (volt > SC89890H_BOOSTV_BASE + (SC89890H_BOOSTV_MASK >> SC89890H_BOOSTV_SHIFT) * SC89890H_BOOSTV_LSB)
        volt = SC89890H_BOOSTV_BASE + (SC89890H_BOOSTV_MASK >> SC89890H_BOOSTV_SHIFT) * SC89890H_BOOSTV_LSB;

    val = ((volt - SC89890H_BOOSTV_BASE) / SC89890H_BOOSTV_LSB) << SC89890H_BOOSTV_SHIFT;

    return sc89890h_update_bits(sc, SC89890H_REG_0A, SC89890H_BOOSTV_MASK, val);

}
EXPORT_SYMBOL_GPL(sc89890h_set_otg_volt);

int sc89890h_set_otg_current(struct sc89890h *sc, int curr)
{
    u8 temp;

    if (curr == 500)
        temp = SC89890H_BOOST_LIM_500MA;
    else if (curr == 750)
        temp = SC89890H_BOOST_LIM_750MA;
    else if (curr == 1200)
        temp = SC89890H_BOOST_LIM_1200MA;
    else if (curr == 1650)
        temp = SC89890H_BOOST_LIM_1650MA;
    else if (curr == 1875)
        temp = SC89890H_BOOST_LIM_1875MA;
    else if (curr == 2150)
        temp = SC89890H_BOOST_LIM_2150MA;
    else if (curr == 2450)
        temp = SC89890H_BOOST_LIM_2450MA;
    else
        temp = SC89890H_BOOST_LIM_1400MA;

    return sc89890h_update_bits(sc, SC89890H_REG_0A, SC89890H_BOOST_LIM_MASK, temp << SC89890H_BOOST_LIM_SHIFT);
}
EXPORT_SYMBOL_GPL(sc89890h_set_otg_current);

static int sc89890h_enable_charger(struct sc89890h *sc)
{
    int ret;
    u8 val = SC89890H_CHG_ENABLE << SC89890H_CHG_CONFIG_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_03, SC89890H_CHG_CONFIG_MASK, val);
    if (ret == 0)
        sc->status |= SC89890H_STATUS_CHARGE_ENABLE;
    return ret;
}

int sc89890h_disable_charger(struct sc89890h *sc)
{
    int ret;
    u8 val = SC89890H_CHG_DISABLE << SC89890H_CHG_CONFIG_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_03, SC89890H_CHG_CONFIG_MASK, val);
    if (ret == 0)
        sc->status &= ~SC89890H_STATUS_CHARGE_ENABLE;
    return ret;
}
EXPORT_SYMBOL_GPL(sc89890h_disable_charger);


/* interfaces that can be called by other module */
int sc89890h_adc_start(struct sc89890h *sc, bool oneshot)
{
    u8 val;
    int ret;

    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_02);
    if (ret < 0) {
        dev_err(sc->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
        return ret;
    }

    if (((val & SC89890H_CONV_RATE_MASK) >> SC89890H_CONV_RATE_SHIFT) == SC89890H_ADC_CONTINUE_ENABLE)
        return 0; /*is doing continuous scan*/
    if (oneshot)
        ret = sc89890h_update_bits(sc, SC89890H_REG_02, SC89890H_CONV_START_MASK, SC89890H_CONV_START << SC89890H_CONV_START_SHIFT);
    else
        ret = sc89890h_update_bits(sc, SC89890H_REG_02, SC89890H_CONV_RATE_MASK,  SC89890H_ADC_CONTINUE_ENABLE << SC89890H_CONV_RATE_SHIFT);
    return ret;
}
EXPORT_SYMBOL_GPL(sc89890h_adc_start);

int sc89890h_adc_stop(struct sc89890h *sc)
{
    return sc89890h_update_bits(sc, SC89890H_REG_02, SC89890H_CONV_RATE_MASK, SC89890H_ADC_CONTINUE_DISABLE << SC89890H_CONV_RATE_SHIFT);
}
EXPORT_SYMBOL_GPL(sc89890h_adc_stop);


int sc89890h_adc_read_battery_volt(struct sc89890h *sc)
{
    uint8_t val;
    int volt;
    int ret;
    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_0E);
    if (ret < 0) {
        dev_err(sc->dev, "read battery voltage failed :%d\n", ret);
        return ret;
    } else{
        volt = SC89890H_BATV_BASE + ((val & SC89890H_BATV_MASK) >> SC89890H_BATV_SHIFT) * SC89890H_BATV_LSB ;
        return volt;
    }
}
EXPORT_SYMBOL_GPL(sc89890h_adc_read_battery_volt);


int sc89890h_adc_read_sys_volt(struct sc89890h *sc)
{
    uint8_t val;
    int volt;
    int ret;
    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_0F);
    if (ret < 0) {
        dev_err(sc->dev, "read system voltage failed :%d\n", ret);
        return ret;
    } else{
        volt = SC89890H_SYSV_BASE + ((val & SC89890H_SYSV_MASK) >> SC89890H_SYSV_SHIFT) * SC89890H_SYSV_LSB ;
        return volt;
    }
}
EXPORT_SYMBOL_GPL(sc89890h_adc_read_sys_volt);

int sc89890h_adc_read_vbus_volt(struct sc89890h *sc)
{
    uint8_t val;
    int volt;
    int ret;
    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_11);
    if (ret < 0) {
        dev_err(sc->dev, "read vbus voltage failed :%d\n", ret);
        return ret;
    } else{
        volt = SC89890H_VBUSV_BASE + ((val & SC89890H_VBUSV_MASK) >> SC89890H_VBUSV_SHIFT) * SC89890H_VBUSV_LSB ;
        return volt;
    }
}
EXPORT_SYMBOL_GPL(sc89890h_adc_read_vbus_volt);

int sc89890h_adc_read_temperature(struct sc89890h *sc)
{
    uint8_t val;
    int temp;
    int ret;
    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_10);
    if (ret < 0) {
        dev_err(sc->dev, "read temperature failed :%d\n", ret);
        return ret;
    } else{
        temp = SC89890H_TSPCT_BASE + ((val & SC89890H_TSPCT_MASK) >> SC89890H_TSPCT_SHIFT) * SC89890H_TSPCT_LSB;
        return temp;
    }
}
EXPORT_SYMBOL_GPL(sc89890h_adc_read_temperature);

int sc89890h_adc_read_charge_current(struct sc89890h *sc)
{
    uint8_t val;
    int volt;
    int ret;
    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_12);
    if (ret < 0) {
        dev_err(sc->dev, "read charge current failed :%d\n", ret);
        return ret;
    } else{
        volt = (int)(SC89890H_ICHGR_BASE + ((val & SC89890H_ICHGR_MASK) >> SC89890H_ICHGR_SHIFT) * SC89890H_ICHGR_LSB) ;
        return volt;
    }
}
EXPORT_SYMBOL_GPL(sc89890h_adc_read_charge_current);

int sc89890h_set_chargecurrent(struct sc89890h *sc, int curr)
{
    u8 ichg;

    ichg = (curr - SC89890H_ICHG_BASE)/SC89890H_ICHG_LSB;
    return sc89890h_update_bits(sc, SC89890H_REG_04, SC89890H_ICHG_MASK, ichg << SC89890H_ICHG_SHIFT);

}
EXPORT_SYMBOL_GPL(sc89890h_set_chargecurrent);

int sc89890h_set_term_current(struct sc89890h *sc, int curr)
{
    u8 iterm;

    iterm = (curr - SC89890H_ITERM_BASE) / SC89890H_ITERM_LSB;

    return sc89890h_update_bits(sc, SC89890H_REG_05, SC89890H_ITERM_MASK, iterm << SC89890H_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(sc89890h_set_term_current);


int sc89890h_set_prechg_current(struct sc89890h *sc, int curr)
{
    u8 iprechg;

    iprechg = (curr - SC89890H_IPRECHG_BASE) / SC89890H_IPRECHG_LSB;

    return sc89890h_update_bits(sc, SC89890H_REG_05, SC89890H_IPRECHG_MASK, iprechg << SC89890H_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(sc89890h_set_prechg_current);

int sc89890h_set_chargevoltage(struct sc89890h *sc, int volt)
{
    u8 val;

    val = (volt - SC89890H_VREG_BASE)/SC89890H_VREG_LSB;
    return sc89890h_update_bits(sc, SC89890H_REG_06, SC89890H_VREG_MASK, val << SC89890H_VREG_SHIFT);
}
EXPORT_SYMBOL_GPL(sc89890h_set_chargevoltage);


int sc89890h_set_input_volt_limit(struct sc89890h *sc, int volt)
{
    u8 val;
    val = (volt - SC89890H_VINDPM_BASE) / SC89890H_VINDPM_LSB;
    return sc89890h_update_bits(sc, SC89890H_REG_0D, SC89890H_VINDPM_MASK, val << SC89890H_VINDPM_SHIFT);
}
EXPORT_SYMBOL_GPL(sc89890h_set_input_volt_limit);

int sc89890h_set_input_current_limit(struct sc89890h *sc, int curr)
{
    u8 val;

    val = (curr - SC89890H_IINLIM_BASE) / SC89890H_IINLIM_LSB;
    return sc89890h_update_bits(sc, SC89890H_REG_00, SC89890H_IINLIM_MASK, val << SC89890H_IINLIM_SHIFT);
}
EXPORT_SYMBOL_GPL(sc89890h_set_input_current_limit);

int sc89890h_enable_ilimt(struct sc89890h *sc)
{
    u8 val = SC89890H_ENILIM_ENABLE << SC89890H_ENILIM_SHIFT;

    return sc89890h_update_bits(sc, SC89890H_REG_00,
                            SC89890H_ENILIM_MASK, val);
}
EXPORT_SYMBOL_GPL(sc89890h_enable_ilimt);

int sc89890h_disable_ilimt(struct sc89890h *sc)
{
    u8 val = SC89890H_ENILIM_DISABLE << SC89890H_ENILIM_SHIFT;

    return sc89890h_update_bits(sc, SC89890H_REG_00,
                            SC89890H_ENILIM_MASK, val);
}
EXPORT_SYMBOL_GPL(sc89890h_disable_ilimt);

int sc89890h_set_vbus(struct sc89890h *sc, int vbus)
{
    u8 val;

    if (vbus == 9000) {
        val = SC89890H_HVDCP_9V << SC89890H_DM_DRIVE_SHIFT;
    } else if (vbus == 12000) {
        val = SC89890H_HVDCP_12V << SC89890H_DM_DRIVE_SHIFT;
    } else {
        val = SC89890H_HVDCP_5V << SC89890H_DM_DRIVE_SHIFT;
    }
    
    return sc89890h_update_bits(sc, SC89890H_REG_01,
                            SC89890H_DP_DRIVE_MASK | SC89890H_DM_DRIVE_MASK, val);
}

int sc89890h_set_vindpm_offset(struct sc89890h *sc, int offset)
{
    u8 val;

    if (offset < 500) {
        val = SC89890H_VINDPMOS_400MV;
    } else {
        val = SC89890H_VINDPMOS_600MV;
    }
    return sc89890h_update_bits(sc, SC89890H_REG_01, SC89890H_VINDPMOS_MASK, val << SC89890H_VINDPMOS_SHIFT);
}
EXPORT_SYMBOL_GPL(sc89890h_set_vindpm_offset);

int sc89890h_get_charging_status(struct sc89890h *sc)
{
    u8 val = 0;
    int ret;

    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_0B);
    if (ret < 0) {
        dev_err(sc->dev, "%s Failed to read register 0x0b:%d\n", __func__, ret);
        return ret;
    }
    val &= SC89890H_CHRG_STAT_MASK;
    val >>= SC89890H_CHRG_STAT_SHIFT;
    return val;
}
EXPORT_SYMBOL_GPL(sc89890h_get_charging_status);

void sc89890h_set_otg(struct sc89890h *sc, int enable)
{
    int ret;

    if (enable) {
        ret = sc89890h_disable_charger(sc);
        ret |= sc89890h_enable_otg(sc);
        if (ret < 0) {
            dev_err(sc->dev, "%s:Failed to enable otg-%d\n", __func__, ret);
            return;
        }
    } else{
        ret = sc89890h_disable_otg(sc);
        ret |= sc89890h_enable_charger(sc);
        if (ret < 0)
            dev_err(sc->dev, "%s:Failed to disable otg-%d\n", __func__, ret);
    }
}
EXPORT_SYMBOL_GPL(sc89890h_set_otg);

int sc89890h_set_watchdog_timer(struct sc89890h *sc, u8 timeout)
{
    return sc89890h_update_bits(sc, SC89890H_REG_07, SC89890H_WDT_MASK, (u8)((timeout - SC89890H_WDT_BASE) / SC89890H_WDT_LSB) << SC89890H_WDT_SHIFT);
}
EXPORT_SYMBOL_GPL(sc89890h_set_watchdog_timer);

int sc89890h_disable_watchdog_timer(struct sc89890h *sc)
{
    u8 val = SC89890H_WDT_DISABLE << SC89890H_WDT_SHIFT;

    return sc89890h_update_bits(sc, SC89890H_REG_07, SC89890H_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(sc89890h_disable_watchdog_timer);

int sc89890h_reset_watchdog_timer(struct sc89890h *sc)
{
    u8 val = SC89890H_WDT_RESET << SC89890H_WDT_RESET_SHIFT;

    return sc89890h_update_bits(sc, SC89890H_REG_03, SC89890H_WDT_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(sc89890h_reset_watchdog_timer);

int sc89890h_force_dpdm(struct sc89890h *sc)
{
    int ret;
    u8 val = SC89890H_FORCE_DPDM << SC89890H_FORCE_DPDM_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_02, SC89890H_FORCE_DPDM_MASK, val);
    if (ret)
        return ret;

    msleep(20);/*TODO: how much time needed to finish dpdm detect?*/
    return 0;

}
EXPORT_SYMBOL_GPL(sc89890h_force_dpdm);

int sc89890h_reset_chip(struct sc89890h *sc)
{
    int ret;
    u8 val = SC89890H_RESET << SC89890H_RESET_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_14, SC89890H_RESET_MASK, val);
    return ret;
}
EXPORT_SYMBOL_GPL(sc89890h_reset_chip);

int sc89890h_enter_ship_mode(struct sc89890h *sc)
{
    int ret;
    u8 val = SC89890H_BATFET_OFF << SC89890H_BATFET_DIS_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_09, SC89890H_BATFET_DIS_MASK, val);
    return ret;

}
EXPORT_SYMBOL_GPL(sc89890h_enter_ship_mode);

int sc89890h_enter_hiz_mode(struct sc89890h *sc)
{
    u8 val = SC89890H_HIZ_ENABLE << SC89890H_ENHIZ_SHIFT;

    return sc89890h_update_bits(sc, SC89890H_REG_00, SC89890H_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sc89890h_enter_hiz_mode);

int sc89890h_exit_hiz_mode(struct sc89890h *sc)
{

    u8 val = SC89890H_HIZ_DISABLE << SC89890H_ENHIZ_SHIFT;

    return sc89890h_update_bits(sc, SC89890H_REG_00, SC89890H_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sc89890h_exit_hiz_mode);

int sc89890h_get_hiz_mode(struct sc89890h *sc, u8 *state)
{
    u8 val;
    int ret;

    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_00);
    if (ret)
        return ret;
    *state = (val & SC89890H_ENHIZ_MASK) >> SC89890H_ENHIZ_SHIFT;

    return 0;
}
EXPORT_SYMBOL_GPL(sc89890h_get_hiz_mode);


int sc89890h_pumpx_enable(struct sc89890h *sc, int enable)
{
    u8 val;
    int ret;

    if (enable)
        val = SC89890H_PUMPX_ENABLE << SC89890H_EN_PUMPX_SHIFT;
    else
        val = SC89890H_PUMPX_DISABLE << SC89890H_EN_PUMPX_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_04, SC89890H_EN_PUMPX_MASK, val);

    return ret;
}
EXPORT_SYMBOL_GPL(sc89890h_pumpx_enable);

int sc89890h_pumpx_increase_volt(struct sc89890h *sc)
{
    u8 val;
    int ret;

    val = SC89890H_PUMPX_UP << SC89890H_PUMPX_UP_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_09, SC89890H_PUMPX_UP_MASK, val);

    return ret;

}
EXPORT_SYMBOL_GPL(sc89890h_pumpx_increase_volt);

int sc89890h_pumpx_increase_volt_done(struct sc89890h *sc)
{
    u8 val;
    int ret;

    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_09);
    if (ret)
        return ret;

    if (val & SC89890H_PUMPX_UP_MASK)
        return 1;   /* not finished*/
    else
        return 0;   /* pumpx up finished*/

}
EXPORT_SYMBOL_GPL(sc89890h_pumpx_increase_volt_done);

int sc89890h_pumpx_decrease_volt(struct sc89890h *sc)
{
    u8 val;
    int ret;

    val = SC89890H_PUMPX_DOWN << SC89890H_PUMPX_DOWN_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_09, SC89890H_PUMPX_DOWN_MASK, val);

    return ret;

}
EXPORT_SYMBOL_GPL(sc89890h_pumpx_decrease_volt);

int sc89890h_pumpx_decrease_volt_done(struct sc89890h *sc)
{
    u8 val;
    int ret;

    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_09);
    if (ret)
        return ret;

    if (val & SC89890H_PUMPX_DOWN_MASK)
        return 1;   /* not finished*/
    else
        return 0;   /* pumpx down finished*/

}
EXPORT_SYMBOL_GPL(sc89890h_pumpx_decrease_volt_done);

int sc89890h_force_ico(struct sc89890h *sc)
{
    u8 val;
    int ret;

    val = SC89890H_FORCE_ICO << SC89890H_FORCE_ICO_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_09, SC89890H_FORCE_ICO_MASK, val);

    return ret;
}
EXPORT_SYMBOL_GPL(sc89890h_force_ico);

int sc89890h_check_force_ico_done(struct sc89890h *sc)
{
    u8 val;
    int ret;

    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_14);
    if (ret)
        return ret;

    if (val & SC89890H_ICO_OPTIMIZED_MASK)
        return 1;  /*finished*/
    else
        return 0;   /* in progress*/
}
EXPORT_SYMBOL_GPL(sc89890h_check_force_ico_done);

int sc89890h_enable_term(struct sc89890h* sc, bool enable)
{
    u8 val;
    int ret;

    if (enable)
        val = SC89890H_TERM_ENABLE << SC89890H_EN_TERM_SHIFT;
    else
        val = SC89890H_TERM_DISABLE << SC89890H_EN_TERM_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_07, SC89890H_EN_TERM_MASK, val);

    return ret;
}
EXPORT_SYMBOL_GPL(sc89890h_enable_term);

int sc89890h_enable_auto_dpdm(struct sc89890h* sc, bool enable)
{
    u8 val;
    int ret;
    
    if (enable)
        val = SC89890H_AUTO_DPDM_ENABLE << SC89890H_AUTO_DPDM_EN_SHIFT;
    else
        val = SC89890H_AUTO_DPDM_DISABLE << SC89890H_AUTO_DPDM_EN_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_02, SC89890H_AUTO_DPDM_EN_MASK, val);

    return ret;

}
EXPORT_SYMBOL_GPL(sc89890h_enable_auto_dpdm);

int sc89890h_use_absolute_vindpm(struct sc89890h* sc, bool enable)
{
    u8 val;
    int ret;
    
    if (enable)
        val = SC89890H_FORCE_VINDPM_ENABLE << SC89890H_FORCE_VINDPM_SHIFT;
    else
        val = SC89890H_FORCE_VINDPM_DISABLE << SC89890H_FORCE_VINDPM_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_0D, SC89890H_FORCE_VINDPM_MASK, val);

    return ret;

}
EXPORT_SYMBOL_GPL(sc89890h_use_absolute_vindpm);

int sc89890h_enable_ico(struct sc89890h* sc, bool enable)
{
    u8 val;
    int ret;
    
    if (enable)
        val = SC89890H_ICO_ENABLE << SC89890H_ICOEN_SHIFT;
    else
        val = SC89890H_ICO_DISABLE << SC89890H_ICOEN_SHIFT;

    ret = sc89890h_update_bits(sc, SC89890H_REG_02, SC89890H_ICOEN_MASK, val);

    return ret;

}
EXPORT_SYMBOL_GPL(sc89890h_enable_ico);


int sc89890h_read_idpm_limit(struct sc89890h *sc)
{
    uint8_t val;
    int curr;
    int ret;

    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_13);
    if (ret < 0) {
        dev_err(sc->dev, "read vbus voltage failed :%d\n", ret);
        return ret;
    } else{
        curr = SC89890H_IDPM_LIM_BASE + ((val & SC89890H_IDPM_LIM_MASK) >> SC89890H_IDPM_LIM_SHIFT) * SC89890H_IDPM_LIM_LSB ;
        return curr;
    }
}
EXPORT_SYMBOL_GPL(sc89890h_read_idpm_limit);

bool sc89890h_is_charge_done(struct sc89890h *sc)
{
    int ret;
    u8 val;

    ret = sc89890h_read_byte(sc, &val, SC89890H_REG_0B);
    if (ret < 0) {
        dev_err(sc->dev, "%s:read REG0B failed :%d\n", __func__, ret);
        return false;
    }
    val &= SC89890H_CHRG_STAT_MASK;
    val >>= SC89890H_CHRG_STAT_SHIFT;

    return (val == SC89890H_CHRG_STAT_CHGDONE);
}
EXPORT_SYMBOL_GPL(sc89890h_is_charge_done);

static int sc89890h_init_device(struct sc89890h *sc)
{
    int ret;

    /*common initialization*/
    sc89890h_reset_chip(sc);

    sc89890h_disable_watchdog_timer(sc);

    sc89890h_enable_auto_dpdm(sc, sc->cfg.enable_auto_dpdm);
    // sc89890h_enable_term(sc, sc->cfg.enable_term);
    sc89890h_enable_ico(sc, sc->cfg.enable_ico);
    /*force use absolute vindpm if auto_dpdm not enabled*/
    if (!sc->cfg.enable_auto_dpdm)
        sc->cfg.use_absolute_vindpm = true;
    sc89890h_use_absolute_vindpm(sc, sc->cfg.use_absolute_vindpm);

    ret = sc89890h_set_vindpm_offset(sc, 600);
    if (ret < 0) {
        dev_err(sc->dev, "%s:Failed to set vindpm offset:%d\n", __func__, ret);
        return ret;
    }

    ret = sc89890h_set_term_current(sc, sc->cfg.term_current);
    if (ret < 0) {
        dev_err(sc->dev, "%s:Failed to set termination current:%d\n", __func__, ret);
        return ret;
    }

    ret = sc89890h_set_chargevoltage(sc, sc->cfg.charge_voltage);
    if (ret < 0) {
        dev_err(sc->dev, "%s:Failed to set charge voltage:%d\n", __func__, ret);
        return ret;
    }

    ret = sc89890h_set_chargecurrent(sc, sc->cfg.charge_current);
    if (ret < 0) {
        dev_err(sc->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
        return ret;
    }

    ret = sc89890h_set_input_current_limit(sc, sc->cfg.input_limit_current);
    if (ret < 0) {
        dev_err(sc->dev, "%s:Failed to set input current limit:%d\n", __func__, ret);
        return ret;
    }

    ret = sc89890h_enable_charger(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s:Failed to enable charger:%d\n", __func__, ret);
        return ret;
    }

    sc89890h_adc_start(sc, false);

    // ret = sc89890h_pumpx_enable(sc, 1);
    // if (ret) {
    //     dev_err(sc->dev, "%s:Failed to enable pumpx:%d\n", __func__, ret);
    //     return ret;
    // }

    sc89890h_set_watchdog_timer(sc, 160);

    sc89890h_disable_ilimt(sc);

    return ret;
}


static int sc89890h_charge_status(struct sc89890h *sc)
{
    u8 val = 0;

    sc89890h_read_byte(sc, &val, SC89890H_REG_0B);
    val &= SC89890H_CHRG_STAT_MASK;
    val >>= SC89890H_CHRG_STAT_SHIFT;
    switch (val) {
    case SC89890H_CHRG_STAT_IDLE:
        return POWER_SUPPLY_CHARGE_TYPE_NONE;
    case SC89890H_CHRG_STAT_PRECHG:
        return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
    case SC89890H_CHRG_STAT_FASTCHG:
        return POWER_SUPPLY_CHARGE_TYPE_FAST;
    case SC89890H_CHRG_STAT_CHGDONE:
        printk("Charge Termination \n");
    default:
        return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
    }
}

static enum power_supply_property sc89890h_charger_props[] = {
    POWER_SUPPLY_PROP_CHARGE_TYPE, /* Charger status output */
    POWER_SUPPLY_PROP_ONLINE, /* External power source */
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};


static int sc89890h_usb_get_property(struct power_supply *psy,
            enum power_supply_property psp,
            union power_supply_propval *val)
{

    struct sc89890h *sc = power_supply_get_drvdata(psy);
    u8 type = sc89890h_get_vbus_type(sc);

    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        if (type == SC89890H_VBUS_USB_SDP || type == SC89890H_VBUS_USB_DCP)
            val->intval = 1;
        else
            val->intval = 0;
        break;
    case POWER_SUPPLY_PROP_CHARGE_TYPE:
        val->intval = sc89890h_charge_status(sc);
        break;

    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        val->intval = sc89890h_adc_read_battery_volt(sc);
		break;

    case POWER_SUPPLY_PROP_CURRENT_NOW:
        val->intval = sc89890h_adc_read_charge_current(sc);
		break;

    default:
        return -EINVAL;
    }

    return 0;
}


static int sc89890h_wall_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{

    struct sc89890h *sc = power_supply_get_drvdata(psy);
    u8 type = sc89890h_get_vbus_type(sc);

    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        if (type == SC89890H_VBUS_HVDCP || type == SC89890H_VBUS_UNKNOWN || type == SC89890H_VBUS_NONSTAND)
            val->intval = 1;
        else
            val->intval = 0;
        break;
    case POWER_SUPPLY_PROP_CHARGE_TYPE:
        val->intval = sc89890h_charge_status(sc);
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int sc89890h_psy_register(struct sc89890h *sc)
{
    sc->psy_cfg.drv_data = sc;
	sc->psy_cfg.of_node = sc->dev->of_node;

    sc->psy_desc.name = "sc89890h-usb";
    sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
    sc->psy_desc.properties = sc89890h_charger_props;
    sc->psy_desc.num_properties = ARRAY_SIZE(sc89890h_charger_props);
    sc->psy_desc.get_property = sc89890h_usb_get_property;
    sc->psy_desc.external_power_changed = NULL;

    sc->usb = devm_power_supply_register(sc->dev, 
			&sc->psy_desc, &sc->psy_cfg);
	if (IS_ERR(sc->usb)) {
		dev_err(sc->dev, "failed to register usb\n");
        power_supply_unregister(sc->usb);
        return -ENODEV;
	}

    // sc->psy_desc.name = "sc89890h-Wall";
    // sc->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
    // sc->psy_desc.properties = sc89890h_charger_props;
    // sc->psy_desc.num_properties = ARRAY_SIZE(sc89890h_charger_props);
    // sc->psy_desc.get_property = sc89890h_wall_get_property;
    // sc->psy_desc.external_power_changed = NULL;

    // sc->wall = devm_power_supply_register(sc->dev, 
	// 		&sc->psy_desc, &sc->psy_cfg);
	// if (IS_ERR(sc->wall)) {
	// 	dev_err(sc->dev, "failed to register wall\n");
    //     power_supply_unregister(sc->wall);
    //     return -ENODEV;
	// }

    return 0;
}

static void sc89890h_psy_unregister(struct sc89890h *sc)
{
    power_supply_unregister(sc->usb);
    // power_supply_unregister(sc->wall);
}

static ssize_t sc89890h_show_registers(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    struct sc89890h *sc = dev_get_drvdata(dev);
    u8 addr;
    u8 val;
    u8 tmpbuf[300];
    int len;
    int idx = 0;
    int ret ;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "Charger");
    for (addr = 0x0; addr <= 0x14; addr++) {
        ret = sc89890h_read_byte(sc, &val, addr);
        if (ret == 0) {
            len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[0x%.2x] = 0x%.2x\n", addr, val);
            memcpy(&buf[idx], tmpbuf, len);
            idx += len;
        }
    }

    return idx;
}

static DEVICE_ATTR(registers, 0400, sc89890h_show_registers, NULL);

static void sc89890h_create_device_node(struct device *dev)
{
	device_create_file(dev, &dev_attr_registers);
}

static int sc89890h_parse_dt(struct device *dev, struct sc89890h *sc)
{
    int ret, value;
    struct device_node *np = dev->of_node;

    ret = of_property_read_u32(np, 
            "sc,sc89890h,vbus-volt-high-level", &sc->pe.high_volt_level);
    // if (ret)
    //     return ret;

    ret = of_property_read_u32(np, 
            "sc,sc89890h,vbus-volt-low-level", &sc->pe.low_volt_level);
    // if (ret)
    //     return ret;

    ret = of_property_read_u32(np, 
            "sc,sc89890h,vbat-min-volt-to-tuneup", &sc->pe.vbat_min_volt);
    // if (ret)
    //     return ret;

    sc->cfg.enable_auto_dpdm = of_property_read_bool(np, 
            "sc,sc89890h,enable-auto-dpdm");
    sc->cfg.enable_term = of_property_read_bool(np, 
            "sc,sc89890h,enable-termination");
    sc->cfg.enable_ico = of_property_read_bool(np, 
            "sc,sc89890h,enable-ico");
    sc->cfg.use_absolute_vindpm = of_property_read_bool(np, 
            "sc,sc89890h,use-absolute-vindpm");

    ret = of_property_read_u32(np, 
            "sc,sc89890h,charge-voltage",&sc->cfg.charge_voltage);
    // if (ret)
    //     return ret;

    ret = of_property_read_u32(np, 
            "sc,sc89890h,charge-current",&sc->cfg.charge_current);
    // if (ret)
    //     return ret;

    ret = of_property_read_u32(np, 
            "sc,sc89890h,term-current",&sc->cfg.term_current);
    // if (ret)
    //     return ret;

    ret = of_property_read_u32(np, 
            "sc,sc89890h,usb-ilim",&sc->cfg.input_limit_current);
    // if (ret)
    //     return ret;

    return 0;
}

static int sc89890h_detect_device(struct sc89890h *sc)
{
    int ret;
    u8 data;

    ret = sc89890h_read_byte(sc, &data, SC89890H_REG_14);
    if (ret == 0) {
        sc->part_no = (data & SC89890H_PN_MASK) >> SC89890H_PN_SHIFT;
    }

    return ret;
}

static int sc89890h_read_batt_rsoc(struct sc89890h *sc)
{
    union power_supply_propval ret = {0,};

    if (!sc->batt_psy) 
        sc->batt_psy = power_supply_get_by_name("battery");

    if (sc->batt_psy) {
        power_supply_get_property(sc->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &ret);
        return ret.intval;
    } else {
        return 50;
    }
}

static void sc89890h_adjust_absolute_vindpm(struct sc89890h *sc)
{
    u16 vbus_volt;
    u16 vindpm_volt;
    int ret;

    ret = sc89890h_disable_charger(sc);	
    if (ret < 0) {
        dev_err(sc->dev,"%s:failed to disable charger\n",__func__);
        /*return;*/
    }
    /* wait for new adc data */
    msleep(1000);
    vbus_volt = sc89890h_adc_read_vbus_volt(sc);
    ret = sc89890h_enable_charger(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s:failed to enable charger\n",__func__);
        return;
    }

    if (vbus_volt < 6000)
        vindpm_volt = vbus_volt - 600;
    else
        vindpm_volt = vbus_volt - 1200;
    ret = sc89890h_set_input_volt_limit(sc, vindpm_volt);
    if (ret < 0)
        dev_err(sc->dev, "%s:Set absolute vindpm threshold %d Failed:%d\n", 
            __func__, vindpm_volt, ret);
    else
        dev_info(sc->dev, "%s:Set absolute vindpm threshold %d successfully\n", 
            __func__, vindpm_volt);

}

static void sc89890h_adapter_in_workfunc(struct work_struct *work)
{
    struct sc89890h *sc = container_of(work, struct sc89890h, adapter_in_work);
    int ret;

    if (sc->vbus_type == SC89890H_VBUS_HVDCP) {
        dev_info(sc->dev, "%s:HVDCP adapter plugged in\n", __func__);
        //Vbus 9V
        ret = sc89890h_set_vbus(sc, 9000);
        ret = sc89890h_set_chargecurrent(sc, sc->cfg.charge_current);
        if (ret < 0) 
            dev_err(sc->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
        else
            dev_info(sc->dev, "%s: Set charge current to %dmA successfully\n",
                    __func__,sc->cfg.charge_current);
        schedule_delayed_work(&sc->ico_work, 0);
    } else if (sc->vbus_type == SC89890H_VBUS_USB_DCP) {/* DCP, let's check if it is PE adapter*/
        dev_info(sc->dev, "%s:usb dcp adapter plugged in\n", __func__);
        ret = sc89890h_set_chargecurrent(sc, sc->cfg.charge_current);
        if (ret < 0) 
            dev_err(sc->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
        else
            dev_info(sc->dev, "%s: Set charge current to %dmA successfully\n",
                    __func__,sc->cfg.charge_current);
        schedule_delayed_work(&sc->check_pe_tuneup_work, 0);
    } else if (sc->vbus_type == SC89890H_VBUS_USB_SDP 
            || sc->vbus_type == SC89890H_VBUS_UNKNOWN) {
        if (sc->vbus_type == SC89890H_VBUS_USB_SDP)
            dev_info(sc->dev, "%s:host SDP plugged in\n", __func__);
        else
            dev_info(sc->dev, "%s:unknown adapter plugged in\n", __func__);

        ret = sc89890h_set_chargecurrent(sc, 500);
        if (ret < 0) 
            dev_err(sc->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
        else
            dev_info(sc->dev, "%s: Set charge current to %dmA successfully\n",
                    __func__,500);
    }
    else {	
        dev_info(sc->dev, "%s:other adapter plugged in,vbus_type is %d\n", 
                __func__, sc->vbus_type);
        ret = sc89890h_set_chargecurrent(sc, 1000);
        if (ret < 0) 
            dev_err(sc->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
        else
            dev_info(sc->dev, "%s: Set charge current to %dmA successfully\n",
                    __func__, 1000);
        schedule_delayed_work(&sc->ico_work, 0);
    }

    if (sc->cfg.use_absolute_vindpm)
        sc89890h_adjust_absolute_vindpm(sc);

    schedule_delayed_work(&sc->monitor_work, 0);
}

static void sc89890h_adapter_out_workfunc(struct work_struct *work)
{
    struct sc89890h *sc = container_of(work, struct sc89890h, adapter_out_work);
    int ret;

    ret = sc89890h_set_input_volt_limit(sc, 4400);
    if (ret < 0)
        dev_err(sc->dev,"%s:reset vindpm threshold to 4400 failed:%d\n",__func__,ret);
    else
        dev_info(sc->dev,"%s:reset vindpm threshold to 4400 successfully\n",__func__);

    cancel_delayed_work_sync(&sc->monitor_work);
}

static void sc89890h_ico_workfunc(struct work_struct *work)
{
    struct sc89890h *sc = container_of(work, struct sc89890h, ico_work.work);
    int ret;
    int idpm;
    u8 status;
    static bool ico_issued;

    if (!ico_issued) {
        ret = sc89890h_force_ico(sc);
        if (ret < 0) {
            schedule_delayed_work(&sc->ico_work, HZ); /* retry 1 second later*/
            dev_info(sc->dev, "%s:ICO command issued failed:%d\n", __func__, ret);
        } else {
            ico_issued = true;
            schedule_delayed_work(&sc->ico_work, 3 * HZ);
            dev_info(sc->dev, "%s:ICO command issued successfully\n", __func__);
        }
    } else {
        ico_issued = false;
        ret = sc89890h_check_force_ico_done(sc);
        if (ret) {/*ico done*/
            ret = sc89890h_read_byte(sc, &status, SC89890H_REG_13);
            if (ret == 0) {
                idpm = (((status & SC89890H_IDPM_LIM_MASK) >> SC89890H_IDPM_LIM_SHIFT) 
                    * SC89890H_IDPM_LIM_LSB) + SC89890H_IDPM_LIM_BASE;
                dev_info(sc->dev, "%s:ICO done, result is:%d mA\n", __func__, idpm);
            }
        }
    }
}

static void sc89890h_check_pe_tuneup_workfunc(struct work_struct *work)
{
    struct sc89890h *sc = container_of(work, struct sc89890h, check_pe_tuneup_work.work);

    if (!sc->pe.enable) {
        schedule_delayed_work(&sc->ico_work, 0);
        return;
    }

    sc->vbat_volt = sc89890h_adc_read_battery_volt(sc);
    sc->rsoc = sc89890h_read_batt_rsoc(sc); 

    if (sc->vbat_volt > sc->pe.vbat_min_volt && sc->rsoc < 95) {
        dev_info(sc->dev, "%s:trying to tune up vbus voltage\n", __func__);
        sc->pe.target_volt = sc->pe.high_volt_level;
        sc->pe.tune_up_volt = true;
        sc->pe.tune_down_volt = false;
        sc->pe.tune_done = false;
        sc->pe.tune_count = 0;
        sc->pe.tune_fail = false;
        schedule_delayed_work(&sc->pe_volt_tune_work, 0);
    } else if (sc->rsoc >= 95) {
        schedule_delayed_work(&sc->ico_work, 0);
    } else {
        /* wait battery voltage up enough to check again */
        schedule_delayed_work(&sc->check_pe_tuneup_work, 2*HZ);
    }
}

static void sc89890h_tune_volt_workfunc(struct work_struct *work)
{
    struct sc89890h *sc = container_of(work, struct sc89890h, pe_volt_tune_work.work);
    int ret = 0;
    static bool pumpx_cmd_issued;

    sc->vbus_volt = sc89890h_adc_read_vbus_volt(sc);

    dev_info(sc->dev, "%s:vbus voltage:%d, Tune Target Volt:%d\n", 
            __func__, sc->vbus_volt, sc->pe.target_volt);

    if ((sc->pe.tune_up_volt && sc->vbus_volt > sc->pe.target_volt) ||
        (sc->pe.tune_down_volt && sc->vbus_volt < sc->pe.target_volt)) {
        dev_info(sc->dev, "%s:voltage tune successfully\n", __func__);
        sc->pe.tune_done = true;
        sc89890h_adjust_absolute_vindpm(sc);
        if (sc->pe.tune_up_volt)
            schedule_delayed_work(&sc->ico_work, 0);
        return;
    }

    if (sc->pe.tune_count > 10) {
        dev_info(sc->dev, "%s:voltage tune failed,reach max retry count\n", __func__);
        sc->pe.tune_fail = true;
        sc89890h_adjust_absolute_vindpm(sc);

        if (sc->pe.tune_up_volt)
            schedule_delayed_work(&sc->ico_work, 0);
        return;
    }

    if (!pumpx_cmd_issued) {
        if (sc->pe.tune_up_volt)
            ret = sc89890h_pumpx_increase_volt(sc);
        else if (sc->pe.tune_down_volt)
            ret =  sc89890h_pumpx_decrease_volt(sc);
        if (ret) {
            schedule_delayed_work(&sc->pe_volt_tune_work, HZ);
        } else {
            dev_info(sc->dev, "%s:pumpx command issued.\n", __func__);
            pumpx_cmd_issued = true;
            sc->pe.tune_count++;
            schedule_delayed_work(&sc->pe_volt_tune_work, 3*HZ);
        }
    } else {
        if (sc->pe.tune_up_volt)
            ret = sc89890h_pumpx_increase_volt_done(sc);
        else if (sc->pe.tune_down_volt)
            ret = sc89890h_pumpx_decrease_volt_done(sc);
        if (ret == 0) {
            dev_info(sc->dev, "%s:pumpx command finishedd!\n", __func__);
            sc89890h_adjust_absolute_vindpm(sc);
            pumpx_cmd_issued = 0;
        }
        schedule_delayed_work(&sc->pe_volt_tune_work, HZ);
    }
}


static void sc89890h_monitor_workfunc(struct work_struct *work)
{
    struct sc89890h *sc = container_of(work, struct sc89890h, monitor_work.work);
    u8 status = 0;
    int ret;
    int chg_current;

    sc89890h_reset_watchdog_timer(sc);

    sc->rsoc = sc89890h_read_batt_rsoc(sc);

    sc->vbus_volt = sc89890h_adc_read_vbus_volt(sc);
    sc->vbat_volt = sc89890h_adc_read_battery_volt(sc);
    chg_current = sc89890h_adc_read_charge_current(sc);

    // dev_info(sc->dev, "%s:vbus volt:%d,vbat volt:%d,charge current:%d\n", 
    //         __func__,sc->vbus_volt,sc->vbat_volt,chg_current);

    ret = sc89890h_read_byte(sc, &status, SC89890H_REG_13);
    if (ret == 0 && (status & SC89890H_VDPM_STAT_MASK))
        dev_info(sc->dev, "%s:VINDPM occurred\n", __func__);
    if (ret == 0 && (status & SC89890H_IDPM_STAT_MASK))
        dev_info(sc->dev, "%s:IINDPM occurred\n", __func__);
        
    if (sc->vbus_type == SC89890H_VBUS_USB_DCP && sc->vbus_volt > sc->pe.high_volt_level 
            && sc->rsoc > 95 && !sc->pe.tune_down_volt) {
        sc->pe.tune_down_volt = true;
        sc->pe.tune_up_volt = false;
        sc->pe.target_volt = sc->pe.low_volt_level;
        sc->pe.tune_done = false;
        sc->pe.tune_count = 0;
        sc->pe.tune_fail = false;
        schedule_delayed_work(&sc->pe_volt_tune_work, 0);
    }

    /* read temperature,or any other check if need to decrease charge current*/

    schedule_delayed_work(&sc->monitor_work, 10 * HZ);
}



static void sc89890h_charger_irq_workfunc(struct work_struct *work)
{
    struct sc89890h *sc = container_of(work, struct sc89890h, irq_work);
    u8 status = 0;
    u8 fault = 0;
    u8 charge_status = 0;
    int ret;

    msleep(100);

    /* Read STATUS and FAULT registers */
    ret = sc89890h_read_byte(sc, &status, SC89890H_REG_0B);
    if (ret)
        return;

    ret = sc89890h_read_byte(sc, &fault, SC89890H_REG_0C);
    if (ret)
        return;
    
    sc->vbus_type = (status & SC89890H_VBUS_STAT_MASK) >> SC89890H_VBUS_STAT_SHIFT;

    if (((sc->vbus_type == SC89890H_VBUS_NONE) || (sc->vbus_type == SC89890H_VBUS_OTG)) 
            && (sc->status & SC89890H_STATUS_PLUGIN)) {
        dev_info(sc->dev, "%s:adapter removed\n", __func__);
        sc->status &= ~SC89890H_STATUS_PLUGIN;
        schedule_work(&sc->adapter_out_work);
    } else if (sc->vbus_type != SC89890H_VBUS_NONE && (sc->vbus_type != SC89890H_VBUS_OTG)
            && !(sc->status & SC89890H_STATUS_PLUGIN)) {
        dev_info(sc->dev, "%s:adapter plugged in\n", __func__);
        sc->status |= SC89890H_STATUS_PLUGIN;
        schedule_work(&sc->adapter_in_work);
    }


    if ((status & SC89890H_PG_STAT_MASK) && !(sc->status & SC89890H_STATUS_PG))
        sc->status |= SC89890H_STATUS_PG;
    else if (!(status & SC89890H_PG_STAT_MASK) && (sc->status & SC89890H_STATUS_PG))
        sc->status &= ~SC89890H_STATUS_PG;

    if (fault && !(sc->status & SC89890H_STATUS_FAULT))
        sc->status |= SC89890H_STATUS_FAULT;
    else if (!fault && (sc->status & SC89890H_STATUS_FAULT))
        sc->status &= ~SC89890H_STATUS_FAULT;

    charge_status = (status & SC89890H_CHRG_STAT_MASK) >> SC89890H_CHRG_STAT_SHIFT;
    // if (charge_status == SC89890H_CHRG_STAT_IDLE)
    //     dev_info(sc->dev, "%s:not charging\n", __func__);
    // else if (charge_status == SC89890H_CHRG_STAT_PRECHG)
    //     dev_info(sc->dev, "%s:precharging\n", __func__);
    // else if (charge_status == SC89890H_CHRG_STAT_FASTCHG)
    //     dev_info(sc->dev, "%s:fast charging\n", __func__);
    // else if (charge_status == SC89890H_CHRG_STAT_CHGDONE)
    //     dev_info(sc->dev, "%s:charge done!\n", __func__);
    
    // if (fault)
    //     dev_info(sc->dev, "%s:charge fault:%02x\n", __func__,fault);
}


static irqreturn_t sc89890h_charger_interrupt(int irq, void *data)
{
    struct sc89890h *sc = data;

    schedule_work(&sc->irq_work);
    return IRQ_HANDLED;
}

static int sc89890h_charger_probe(struct i2c_client *client)
{
    struct sc89890h *sc;
    struct device *dev = &client->dev;
    struct gpio_desc *gpiod;
    int ret = 0;

    sc = devm_kzalloc(&client->dev, sizeof(struct sc89890h), GFP_KERNEL);
    if (!sc) {
        dev_err(&client->dev, "%s: out of memory\n", __func__);
        return -ENOMEM;
    }

    sc->dev = &client->dev;
    sc->client = client;

    mutex_init(&sc->sc89890h_i2c_lock);

    i2c_set_clientdata(client, sc);
    sc89890h_create_device_node(&(client->dev));


    ret = sc89890h_detect_device(sc);
    if (!ret && sc->part_no == SC89890H) {
        dev_info(sc->dev, "%s: charger device sc89890h detected\n",
                 __func__);
    } else {
        dev_info(sc->dev, "%s: no sc89890h charger device found:%d\n", __func__, ret);
        return -ENODEV;
    }

    sc->batt_psy = power_supply_get_by_name("battery");

    if (client->dev.of_node)
        sc89890h_parse_dt(&client->dev, sc);

    ret = sc89890h_init_device(sc);
    if (ret) {
        dev_err(sc->dev, "device init failure: %d\n", ret);
        goto err_0;
    }

    ret = sc89890h_psy_register(sc);
    if (ret)
        goto err_0;


    INIT_WORK(&sc->irq_work, sc89890h_charger_irq_workfunc);
    INIT_WORK(&sc->adapter_in_work, sc89890h_adapter_in_workfunc);
    INIT_WORK(&sc->adapter_out_work, sc89890h_adapter_out_workfunc);
    INIT_DELAYED_WORK(&sc->monitor_work, sc89890h_monitor_workfunc);
    INIT_DELAYED_WORK(&sc->ico_work, sc89890h_ico_workfunc);
    INIT_DELAYED_WORK(&sc->pe_volt_tune_work, sc89890h_tune_volt_workfunc);
    INIT_DELAYED_WORK(&sc->check_pe_tuneup_work, sc89890h_check_pe_tuneup_workfunc);

    gpiod = devm_gpiod_get_optional(dev, "irq", GPIOD_IN);
	client->irq = gpiod_to_irq(gpiod);

    if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
				NULL, sc89890h_charger_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"sc89890h_charger_irq", sc);
		if (ret < 0) {
			dev_err(sc->dev, "request irq for irq=%d failed, ret =%d\n",
							client->irq, ret);
			goto err_0;
		}
		enable_irq_wake(client->irq);
	}

    sc->pe.enable = false;
    schedule_work(&sc->irq_work);/*in case of adapter has been in when power off*/
    
    dev_err(sc->dev, "sc89890h probe successfully, Part Num:%d\n!",
				sc->part_no);
    return ret;

err_0:
    cancel_work_sync(&sc->irq_work);
    cancel_work_sync(&sc->adapter_in_work);
    cancel_work_sync(&sc->adapter_out_work);
    cancel_delayed_work_sync(&sc->monitor_work);
    cancel_delayed_work_sync(&sc->ico_work);
    cancel_delayed_work_sync(&sc->check_pe_tuneup_work);
    cancel_delayed_work_sync(&sc->pe_volt_tune_work);

    return ret;
}

static void sc89890h_charger_shutdown(struct i2c_client *client)
{
    struct sc89890h *sc = i2c_get_clientdata(client);

    dev_info(sc->dev, "%s: shutdown\n", __func__);

    sc89890h_psy_unregister(sc);

    mutex_destroy(&sc->sc89890h_i2c_lock);

    cancel_work_sync(&sc->irq_work);
    cancel_work_sync(&sc->adapter_in_work);
    cancel_work_sync(&sc->adapter_out_work);
    cancel_delayed_work_sync(&sc->monitor_work);
    cancel_delayed_work_sync(&sc->ico_work);
    cancel_delayed_work_sync(&sc->check_pe_tuneup_work);
    cancel_delayed_work_sync(&sc->pe_volt_tune_work);

    // free_irq(sc->client->irq, NULL);
}

static struct of_device_id sc89890h_charger_match_table[] = {
    {.compatible = "sc,sc89890h",},
    {},
};


static const struct i2c_device_id sc89890h_charger_id[] = {
    { "sc89890h", SC89890H },
    {},
};

MODULE_DEVICE_TABLE(i2c, sc89890h_charger_id);

static struct i2c_driver sc89890h_charger_driver = {
    .driver		= {
        .name	= "sc89890h",
        .of_match_table = sc89890h_charger_match_table,
    },
    .id_table	= sc89890h_charger_id,

    .probe		= sc89890h_charger_probe,
    .shutdown   = sc89890h_charger_shutdown,
};

module_i2c_driver(sc89890h_charger_driver);

MODULE_DESCRIPTION("SC SC89890H Charger Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("South Chip");
