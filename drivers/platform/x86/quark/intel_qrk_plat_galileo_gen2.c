/*
 * Intel Quark Legacy Platform Data Layout.conf accessor
 *
 * Simple Legacy SPI flash access layer
 *
 * Copyright(c) 2013 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * Author : Bryan O'Donoghue <bryan.odonoghue@linux.intel.com> 2013
 */

#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c/at24.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/mfd/intel_qrk_gip_pdata.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/spi/pxa2xx_spi.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/platform_data/pca953x.h>

#define DRIVER_NAME		"GalileoGen2"
#define GPIO_RESTRICT_NAME	"qrk-gpio-restrict-sc"
#define LPC_SCH_SPINAME		"spi-lpc-sch"

#define GPIO_PCAL9555A_EXP2_INT		9

/* Option to allow GPIO 10 to be used for SPI1 chip-select */
static int gpio_cs;

module_param(gpio_cs, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(gpio_cs, "Enable GPIO chip-select for SPI channel 1");

/* Galileo Gen2 boards require i2c master to operate @400kHz 'fast mode' */
static struct intel_qrk_gip_pdata gip_pdata = {
	.i2c_std_mode = 0,
};
static struct intel_qrk_gip_pdata *galileo_gen2_gip_get_pdata(void)
{
	return &gip_pdata;
}

/******************************************************************************
 *             Texas Instruments ADC1x8S102 SPI Device Platform Data
 ******************************************************************************/
#include "linux/platform_data/adc1x8s102.h"

/* Maximum input voltage allowed for each ADC input, in milliVolts */
#define ADC1x8S102_MAX_EXT_VIN 5000

static const struct adc1x8s102_platform_data adc1x8s102_platform_data = {
	.ext_vin = ADC1x8S102_MAX_EXT_VIN
};

#define PCAL9555A_GPIO_BASE_OFFSET 16

static struct pca953x_platform_data pcal9555a_platform_data_exp0 = {
	.gpio_base = PCAL9555A_GPIO_BASE_OFFSET,
	.irq_base = -1,
};

static struct pca953x_platform_data pcal9555a_platform_data_exp1 = {
	.gpio_base = PCAL9555A_GPIO_BASE_OFFSET + 16,
	.irq_base = -1,
};

static struct pca953x_platform_data pcal9555a_platform_data_exp2 = {
	.gpio_base = PCAL9555A_GPIO_BASE_OFFSET + 32,
};

#include "linux/platform_data/pca9685.h"

static struct pca9685_pdata pca9685_platform_data = {
	.chan_mapping = {
		PWM_CH_GPIO, PWM_CH_PWM,
		PWM_CH_GPIO, PWM_CH_PWM,
		PWM_CH_GPIO, PWM_CH_PWM,
		PWM_CH_GPIO, PWM_CH_PWM,
		PWM_CH_GPIO, PWM_CH_PWM,
		PWM_CH_GPIO, PWM_CH_PWM,
		PWM_CH_GPIO, PWM_CH_GPIO,
		PWM_CH_GPIO, PWM_CH_GPIO,
		PWM_CH_DISABLED /* ALL_LED disabled */
	},
	.gpio_base = PCAL9555A_GPIO_BASE_OFFSET + 48,
};

/******************************************************************************
 *                        Intel Galileo Gen2 i2c clients
 ******************************************************************************/
#define EEPROM_ADDR				0x54
#define PCAL9555A_EXP0_ADDR			0x25
#define PCAL9555A_EXP1_ADDR			0x26
#define PCAL9555A_EXP2_ADDR			0x27
#define PCA9685_ADDR				0x47

static struct i2c_board_info probed_i2c_eeprom;
static struct i2c_board_info probed_i2c_pcal9555a_exp0 = {
	.platform_data = &pcal9555a_platform_data_exp0,
};
static struct i2c_board_info probed_i2c_pcal9555a_exp1 = {
	.platform_data = &pcal9555a_platform_data_exp1,
};
static struct i2c_board_info probed_i2c_pcal9555a_exp2 = {
	.platform_data = &pcal9555a_platform_data_exp2,
};
static struct i2c_board_info probed_i2c_pca9685 = {
	.platform_data = &pca9685_platform_data,
};

static const unsigned short eeprom_i2c_addr[] = {
	EEPROM_ADDR, I2C_CLIENT_END
};
static const unsigned short pcal9555a_exp0_i2c_addr[] = {
	PCAL9555A_EXP0_ADDR, I2C_CLIENT_END
};
static const unsigned short pcal9555a_exp1_i2c_addr[] = {
	PCAL9555A_EXP1_ADDR, I2C_CLIENT_END
};
static const unsigned short pcal9555a_exp2_i2c_addr[] = {
	PCAL9555A_EXP2_ADDR, I2C_CLIENT_END
};
static const unsigned short pca9685_i2c_addr[] = {
	PCA9685_ADDR, I2C_CLIENT_END
};

static int i2c_probe(struct i2c_adapter *adap, unsigned short addr)
{
	/* Always return success: the I2C clients are already known.  */
	return 1;
}

/******************************************************************************
 *                 Intel Quark SPI Controller Data
 ******************************************************************************/
static struct pxa2xx_spi_chip qrk_ffrd_spi_0_cs_0 = {
	.gpio_cs = 8,
};

static struct pxa2xx_spi_chip qrk_ffrd_spi_1_cs_0 = {
	.gpio_cs = 10,
};

#define LPC_SCH_SPI_BUS_ID 0x03

/* TODO: extract this data from layout.conf encoded in flash */
struct mtd_partition galileo_gen2_ilb_partitions[] = {
	{
		.name		= "grub",
		.size		= 4096,
		.offset		= 0,
	},
	{
		.name		= "grub.conf",
		.size		= 0xA00,
		.offset		= 0x50500,
	},
	{
		.name		= "layout.conf",
		.size		= 4096,
		.offset		= 0x708000,
	},
	{
		.name		= "sketch",
		.size		= 0x40000,
		.offset		= 0x750000,
	},
	{
		.name		= "raw",
		.size		= 8192000,
		.offset		= 0,

	},
};

static struct flash_platform_data ilb_flash = {
	.type = "s25fl064k",
	.parts = galileo_gen2_ilb_partitions,
	.nr_parts = ARRAY_SIZE(galileo_gen2_ilb_partitions),
};

static struct spi_board_info spi0_onboard_devs[] = {
	{
		.modalias = "m25p80",
		.platform_data = &ilb_flash,
		.bus_num = LPC_SCH_SPI_BUS_ID,
		.chip_select = 0,
	},
	{
		.modalias = "adc1x8s102",
		.max_speed_hz = 16667000,
		.platform_data = &adc1x8s102_platform_data,
		.mode = SPI_MODE_3,
		.bus_num = 0,
		.chip_select = 0,
		.controller_data = &qrk_ffrd_spi_0_cs_0,
	},
};

static struct spi_board_info spi1_onboard_devs_gpiocs[] = {
	{
		.modalias = "spidev",
		.chip_select = 0,
		.controller_data = NULL,
		.max_speed_hz = 50000000,
		.bus_num = 1,
		.controller_data = &qrk_ffrd_spi_1_cs_0,
	},
};

static struct spi_board_info spi1_onboard_devs[] = {
	{
		.modalias = "spidev",
		.chip_select = 0,
		.controller_data = NULL,
		.max_speed_hz = 50000000,
		.bus_num = 1,
	},
};


/**
 * intel_qrk_spi_add_onboard_devs
 *
 * @return 0 on success or standard errnos on failure
 *
 * Registers onboard SPI device(s) present on the Izmir platform
 */
static int intel_qrk_spi_add_onboard_devs(void)
{
	int ret = 0;

	ret = spi_register_board_info(spi0_onboard_devs,
				      ARRAY_SIZE(spi0_onboard_devs));
	if (ret)
		return ret;

	if (gpio_cs)
		return spi_register_board_info(spi1_onboard_devs_gpiocs,
					ARRAY_SIZE(spi1_onboard_devs_gpiocs));
	else
		return spi_register_board_info(spi1_onboard_devs,
					ARRAY_SIZE(spi1_onboard_devs));
}


static struct gpio reserved_gpios[] = {
	{
		GPIO_PCAL9555A_EXP2_INT,
		GPIOF_IN,
		"pcal9555a-exp2-int",
	},
};

/**
 * intel_qrk_gpio_restrict_probe
 *
 * Make GPIOs pertaining to Firmware inaccessible by requesting them.  The
 * GPIOs are never released nor accessed by this driver.
 *
 * Registers devices which are dependent on this GPIO driver
 */
static int intel_qrk_gpio_restrict_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct i2c_adapter *i2c_adap = NULL;
	struct i2c_client *client = NULL;

	/* Need to tell the PCA953X driver which GPIO IRQ to use for signalling
	 * interrupts.  We can't get the IRQ until the GPIO driver is loaded.
	 * Hence, we defer registration of the I2C devices until now
	 */
	i2c_adap = i2c_get_adapter(0);
	if (NULL == i2c_adap) {
		pr_info("%s: i2c adapter not ready yet. Deferring..\n",
			__func__);
		return -EPROBE_DEFER;
	}

	ret = gpio_request_array(reserved_gpios, ARRAY_SIZE(reserved_gpios));
	if (ret) {
		dev_err(&client->dev, "failed to request reserved gpios\n");
		goto end;
	}

	strlcpy(probed_i2c_eeprom.type, "24c08", I2C_NAME_SIZE);
	client = i2c_new_probed_device(i2c_adap, &probed_i2c_eeprom,
				       eeprom_i2c_addr, i2c_probe);
	if (client == NULL) {
		pr_err("%s: Failed to probe 24c08 I2C device\n", __func__);
		ret = -ENODEV;
		goto end;
	}

	strlcpy(probed_i2c_pcal9555a_exp0.type, "pcal9555a", I2C_NAME_SIZE);
	client = i2c_new_probed_device(i2c_adap, &probed_i2c_pcal9555a_exp0,
				       pcal9555a_exp0_i2c_addr, i2c_probe);
	if (client == NULL) {
		pr_err("%s: Failed to probe pcal9555a I2C device\n", __func__);
		ret = -ENODEV;
		goto end;
	}

	strlcpy(probed_i2c_pcal9555a_exp1.type, "pcal9555a", I2C_NAME_SIZE);
	client = i2c_new_probed_device(i2c_adap, &probed_i2c_pcal9555a_exp1,
				       pcal9555a_exp1_i2c_addr, i2c_probe);
	if (client == NULL) {
		pr_err("%s: Failed to probe pcal9555a I2C device\n", __func__);
		ret = -ENODEV;
		goto end;
	}

	strlcpy(probed_i2c_pcal9555a_exp2.type, "pcal9555a", I2C_NAME_SIZE);
	probed_i2c_pcal9555a_exp2.irq = gpio_to_irq(GPIO_PCAL9555A_EXP2_INT);
	client = i2c_new_probed_device(i2c_adap, &probed_i2c_pcal9555a_exp2,
				       pcal9555a_exp2_i2c_addr, i2c_probe);
	if (client == NULL) {
		pr_err("%s: Failed to probe pcal9555a I2C device\n", __func__);
		ret = -ENODEV;
		goto end;
	}

	strlcpy(probed_i2c_pca9685.type, "pca9685", I2C_NAME_SIZE);
	client = i2c_new_probed_device(i2c_adap, &probed_i2c_pca9685,
				       pca9685_i2c_addr, i2c_probe);
	if (client == NULL) {
		pr_err("%s: Failed to probe pca9685 I2C device\n", __func__);
		ret = -ENODEV;
		goto end;
	}

	ret = intel_qrk_spi_add_onboard_devs();

end:
	i2c_put_adapter(i2c_adap);

	return ret;
}

static struct platform_driver gpio_restrict_pdriver = {
	.driver		= {
		.name	= GPIO_RESTRICT_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= intel_qrk_gpio_restrict_probe,
};

static int intel_qrk_plat_galileo_gen2_probe(struct platform_device *pdev)
{
	/* Assign GIP driver handle for board-specific settings */
	intel_qrk_gip_get_pdata = galileo_gen2_gip_get_pdata;

	/* gpio */
	return platform_driver_register(&gpio_restrict_pdriver);
}

static int intel_qrk_plat_galileo_gen2_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver qrk_galileo_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= intel_qrk_plat_galileo_gen2_probe,
	.remove		= intel_qrk_plat_galileo_gen2_remove,
};

module_platform_driver(qrk_galileo_driver);

MODULE_AUTHOR("Bryan O'Donoghue <bryan.odonoghue@intel.com>");
MODULE_DESCRIPTION("Galileo Gen2 BSP Data");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("platform:"DRIVER_NAME);
