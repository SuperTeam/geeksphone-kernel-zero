/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/io.h>
#include <linux/usb/mass_storage_function.h>
#include <linux/mfd/pmic8058.h>
#include <linux/regulator/pmic8058-regulator.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/ofn_atlab.h>
#include <linux/power_supply.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>

#include <mach/mpp.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/memory.h>
#include <mach/msm_iomap.h>
#include <mach/dma.h>
#include <mach/sirc.h>
#include <linux/input/msm_ts.h>
#include <mach/pmic.h>
#include <mach/rpc_pmapp.h>

#include <asm/mach/mmc.h>
#include <asm/mach/flash.h>
#include <mach/vreg.h>
#include "devices.h"
#include "timer.h"
#include "socinfo.h"
#include "pm.h"
#include "spm.h"
#include <mach/dal_axi.h>
#include <mach/msm_serial_hs.h>
#include <mach/msm_reqs.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include "tlmm-fsm9xxx.h"


#define PMIC_GPIO_INT		144
#define PMIC_VREG_WLAN_LEVEL	2900
#define PMIC_GPIO_SD_DET	165

#define GPIO_MAC_RST_N		37

#define GPIO_GRFC_FTR0_0	136 /* GRFC 20 */
#define GPIO_GRFC_FTR0_1	137 /* GRFC 21 */
#define GPIO_GRFC_FTR1_0	145 /* GRFC 22 */
#define GPIO_GRFC_FTR1_1	93 /* GRFC 19 */
#define GPIO_GRFC_2		110
#define GPIO_GRFC_3		109
#define GPIO_GRFC_4		108
#define GPIO_GRFC_5		107
#define GPIO_GRFC_6		106
#define GPIO_GRFC_7		105
#define GPIO_GRFC_8		104
#define GPIO_GRFC_9		103
#define GPIO_GRFC_10		102
#define GPIO_GRFC_11		101
#define GPIO_GRFC_13		99
#define GPIO_GRFC_14		98
#define GPIO_GRFC_15		97
#define GPIO_GRFC_16		96
#define GPIO_GRFC_17		95
#define GPIO_GRFC_18		94
#define GPIO_GRFC_24		150
#define GPIO_GRFC_25		151
#define GPIO_GRFC_26		152
#define GPIO_GRFC_27		153
#define GPIO_GRFC_28		154
#define GPIO_GRFC_29		155

#define FPGA_SDCC_STATUS       0x8E0001A8

/* Macros assume PMIC GPIOs start at 0 */
#define PM8058_GPIO_PM_TO_SYS(pm_gpio)	   (pm_gpio + NR_MSM_GPIOS)
#define PM8058_GPIO_SYS_TO_PM(sys_gpio)    (sys_gpio - NR_MSM_GPIOS)

#define PMIC_GPIO_5V_PA_PWR	21	/* PMIC GPIO Number 22 */
#define PMIC_GPIO_4_2V_PA_PWR	22	/* PMIC GPIO Number 23 */
#define PMIC_MPP_3		2	/* PMIC MPP Number 3 */
#define PMIC_MPP_6		5	/* PMIC MPP Number 6 */
#define PMIC_MPP_7		6	/* PMIC MPP Number 7 */
#define PMIC_MPP_10		9	/* PMIC MPP Number 10 */

static int pm8058_gpios_init(void)
{
	int i;
	int rc;
	struct pm8058_gpio_cfg {
		int gpio;
		struct pm8058_gpio cfg;
	};

	struct pm8058_gpio_cfg gpio_cfgs[] = {
		{				/* 5V PA Power */
			PMIC_GPIO_5V_PA_PWR,
			{
				.vin_sel = 0,
				.direction = PM_GPIO_DIR_BOTH,
				.output_value = 1,
				.output_buffer = PM_GPIO_OUT_BUF_CMOS,
				.pull = PM_GPIO_PULL_DN,
				.out_strength = PM_GPIO_STRENGTH_HIGH,
				.function = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol = 0,
			},
		},
		{				/* 4.2V PA Power */
			PMIC_GPIO_4_2V_PA_PWR,
			{
				.vin_sel = 0,
				.direction = PM_GPIO_DIR_BOTH,
				.output_value = 1,
				.output_buffer = PM_GPIO_OUT_BUF_CMOS,
				.pull = PM_GPIO_PULL_DN,
				.out_strength = PM_GPIO_STRENGTH_HIGH,
				.function = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol = 0,
			},
		},
	};

	for (i = 0; i < ARRAY_SIZE(gpio_cfgs); ++i) {
		rc = pm8058_gpio_config(gpio_cfgs[i].gpio, &gpio_cfgs[i].cfg);
		if (rc < 0) {
			pr_err("%s pmic gpio config failed\n", __func__);
			return rc;
		}
	}

	return 0;
}

static int pm8058_mpps_init(void)
{
	/* Set up MPP 7 and 10 as analog inputs,
	 * MPP 3 and 6 as analog outputs at 1.25V
	 */
	pm8058_mpp_config_analog_input(PMIC_MPP_7, PM_MPP_AIN_AMUX_CH5, 0);
	pm8058_mpp_config_analog_input(PMIC_MPP_10, PM_MPP_AIN_AMUX_CH6, 0);
	pm8058_mpp_config_analog_output(PMIC_MPP_3, PM_MPP_AOUT_LVL_1V25_2,
		PM_MPP_AOUT_CTL_ENABLE);
	pm8058_mpp_config_analog_output(PMIC_MPP_6, PM_MPP_AOUT_LVL_1V25_2,
		PM_MPP_AOUT_CTL_ENABLE);
	pr_info("MPP initialization complete\n");
	return 0;
}

static struct pm8058_gpio_platform_data pm8058_gpio_data = {
	.gpio_base = PM8058_GPIO_PM_TO_SYS(0),
	.irq_base = PM8058_GPIO_IRQ(PMIC8058_IRQ_BASE, 0),
	.init = pm8058_gpios_init,
};

  static struct pm8058_gpio_platform_data pm8058_mpp_data = {
	.gpio_base = PM8058_GPIO_PM_TO_SYS(PM8058_GPIOS),
	.irq_base = PM8058_MPP_IRQ(PMIC8058_IRQ_BASE, 0),
	.init = pm8058_mpps_init,
};

static struct regulator_consumer_supply pm8058_vreg_supply[PM8058_VREG_MAX] = {
	[PM8058_VREG_ID_L3] = REGULATOR_SUPPLY("8058_l3", NULL),
	[PM8058_VREG_ID_L8] = REGULATOR_SUPPLY("8058_l8", NULL),
	[PM8058_VREG_ID_L11] = REGULATOR_SUPPLY("8058_l11", NULL),
	[PM8058_VREG_ID_L14] = REGULATOR_SUPPLY("8058_l14", NULL),
	[PM8058_VREG_ID_L15] = REGULATOR_SUPPLY("8058_l15", NULL),
	[PM8058_VREG_ID_L18] = REGULATOR_SUPPLY("8058_l18", NULL),
	[PM8058_VREG_ID_S4] = REGULATOR_SUPPLY("8058_s4", NULL),
};

#define PM8058_VREG_INIT(_id, _min_uV, _max_uV, _modes, _ops, _apply_uV) \
	[_id] = { \
		.constraints = { \
			.valid_modes_mask = _modes, \
			.valid_ops_mask = _ops, \
			.min_uV = _min_uV, \
			.max_uV = _max_uV, \
			.apply_uV = _apply_uV, \
			.always_on = 1, \
		}, \
		.num_consumer_supplies = 1, \
		.consumer_supplies = &pm8058_vreg_supply[_id], \
	}

#define PM8058_VREG_INIT_LDO(_id, _min_uV, _max_uV) \
	PM8058_VREG_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL | \
			REGULATOR_MODE_IDLE | REGULATOR_MODE_STANDBY, \
			REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS | \
			REGULATOR_CHANGE_MODE, 1)

#define PM8058_VREG_INIT_SMPS(_id, _min_uV, _max_uV) \
	PM8058_VREG_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL | \
			REGULATOR_MODE_IDLE | REGULATOR_MODE_STANDBY, \
			REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS | \
			REGULATOR_CHANGE_MODE, 1)

static struct regulator_init_data pm8058_vreg_init[PM8058_VREG_MAX] = {
	PM8058_VREG_INIT_LDO(PM8058_VREG_ID_L3, 1800000, 1800000),
	PM8058_VREG_INIT_LDO(PM8058_VREG_ID_L8, 2200000, 2200000),
	PM8058_VREG_INIT_LDO(PM8058_VREG_ID_L11, 2850000, 2850000),
	PM8058_VREG_INIT_LDO(PM8058_VREG_ID_L14, 2850000, 2850000),
	PM8058_VREG_INIT_LDO(PM8058_VREG_ID_L15, 2200000, 2200000),
	PM8058_VREG_INIT_LDO(PM8058_VREG_ID_L18, 2200000, 2200000),
	PM8058_VREG_INIT_SMPS(PM8058_VREG_ID_S4, 1300000, 1300000),
  };

#define PM8058_VREG(_id) { \
	.name = "pm8058-regulator", \
	.id = _id, \
	.platform_data = &pm8058_vreg_init[_id], \
	.data_size = sizeof(pm8058_vreg_init[_id]), \
}

/* Put sub devices with fixed location first in sub_devices array */
static struct mfd_cell pm8058_subdevs[] = {
	{	.name = "pm8058-mpp",
		.platform_data	= &pm8058_mpp_data,
		.data_size	= sizeof(pm8058_mpp_data),
	},
	{
		.name = "pm8058-gpio",
		.id = -1,
		.platform_data = &pm8058_gpio_data,
		.data_size = sizeof(pm8058_gpio_data),
	},
	PM8058_VREG(PM8058_VREG_ID_L3),
	PM8058_VREG(PM8058_VREG_ID_L6),
	PM8058_VREG(PM8058_VREG_ID_L8),
	PM8058_VREG(PM8058_VREG_ID_L11),
	PM8058_VREG(PM8058_VREG_ID_L14),
	PM8058_VREG(PM8058_VREG_ID_L15),
	PM8058_VREG(PM8058_VREG_ID_L18),
	PM8058_VREG(PM8058_VREG_ID_S4),
	{
		.name = "pm8058-femto",
		.id = -1,
	},
  };

static struct pm8058_platform_data pm8058_fsm9xxx_data = {
	.irq_base = PMIC8058_IRQ_BASE,

	.num_subdevs = ARRAY_SIZE(pm8058_subdevs),
	.sub_devices = pm8058_subdevs,
};

static struct i2c_board_info pm8058_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO("pm8058-core", 0),
		.irq = MSM_GPIO_TO_INT(PMIC_GPIO_INT),
		.platform_data = &pm8058_fsm9xxx_data,
	},
};

static int __init buses_init(void)
{
	i2c_register_board_info(0 /* I2C_SSBI ID */, pm8058_boardinfo,
				ARRAY_SIZE(pm8058_boardinfo));

	return 0;
}


static struct msm_pm_platform_data msm_pm_data[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].supported = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].latency = 8594,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].residency = 23740,

	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].supported = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].latency = 4594,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].residency = 23740,

	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE].supported = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE].suspend_enabled = 0,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE].latency = 500,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE].residency = 6000,

	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].supported = 1,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].suspend_enabled
		= 1,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].idle_enabled = 0,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency = 443,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].residency = 1098,

	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].supported = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].latency = 2,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].residency = 0,
};

# define QFEC_MAC_IRQ           28
# define QFEC_MAC_BASE          0x40000000
# define QFEC_CLK_BASE          0x94020000

# define QFEC_MAC_SIZE          0x2000
# define QFEC_CLK_SIZE          0x18100

# define QFEC_MAC_FUSE_BASE     0x80004210
# define QFEC_MAC_FUSE_SIZE     16

static struct resource qfec_resources[] = {
	[0] = {
		.start = QFEC_MAC_BASE,
		.end   = QFEC_MAC_BASE + QFEC_MAC_SIZE,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = QFEC_MAC_IRQ,
		.end   = QFEC_MAC_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = QFEC_CLK_BASE,
		.end   = QFEC_CLK_BASE + QFEC_CLK_SIZE,
		.flags = IORESOURCE_IO,
	},
	[3] = {
		.start = QFEC_MAC_FUSE_BASE,
		.end   = QFEC_MAC_FUSE_BASE + QFEC_MAC_FUSE_SIZE,
		.flags = IORESOURCE_DMA,
	},
};

static struct platform_device qfec_device = {
	.name           = "qfec",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(qfec_resources),
	.resource       = qfec_resources,
};

#define QCE_SIZE		0x10000

#define QCE_0_BASE		0x80C00000
#define QCE_1_BASE		0x80E00000
#define QCE_2_BASE		0x81000000

static struct resource qcrypto_resources[] = {
	[0] = {
		.start = QCE_0_BASE,
		.end = QCE_0_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE1_IN_CHAN,
		.end = DMOV_CE1_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE1_IN_CRCI,
		.end = DMOV_CE1_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE1_OUT_CRCI,
		.end = DMOV_CE1_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[4] = {
		.name = "crypto_crci_hash",
		.start = DMOV_CE1_HASH_CRCI,
		.end = DMOV_CE1_HASH_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

static struct resource ota_qcrypto_resources[] = {
	[0] = {
		.start = QCE_1_BASE,
		.end = QCE_1_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE2_IN_CHAN,
		.end = DMOV_CE2_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE2_IN_CRCI,
		.end = DMOV_CE2_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE2_OUT_CRCI,
		.end = DMOV_CE2_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[4] = {
		.name = "crypto_crci_hash",
		.start = DMOV_CE2_HASH_CRCI,
		.end = DMOV_CE2_HASH_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

static struct platform_device qcrypto_device = {
	.name		= "qcrypto",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qcrypto_resources),
	.resource	= qcrypto_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct platform_device ota_qcrypto_device = {
	.name		= "qcota",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(ota_qcrypto_resources),
	.resource	= ota_qcrypto_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct msm_gpio phy_config_data[] = {
	{ GPIO_CFG(GPIO_MAC_RST_N, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "MAC_RST_N"},
};

static int __init phy_init(void)
{
	msm_gpios_request_enable(phy_config_data, ARRAY_SIZE(phy_config_data));
	gpio_direction_output(GPIO_MAC_RST_N, 0);
	udelay(100);
	gpio_set_value(GPIO_MAC_RST_N, 1);

	return 0;
}

static struct msm_gpio grfc_config_data[] = {
	{ GPIO_CFG(GPIO_GRFC_FTR0_0, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "HH_RFMODE1_0"},
	{ GPIO_CFG(GPIO_GRFC_FTR0_1, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "HH_RFMODE1_1"},
	{ GPIO_CFG(GPIO_GRFC_FTR1_0, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "HH_RFMODE2_0"},
	{ GPIO_CFG(GPIO_GRFC_FTR1_1, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "HH_RFMODE2_1"},
	{ GPIO_CFG(GPIO_GRFC_2, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_2"},
	{ GPIO_CFG(GPIO_GRFC_3, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_3"},
	{ GPIO_CFG(GPIO_GRFC_4, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_4"},
	{ GPIO_CFG(GPIO_GRFC_5, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_5"},
	{ GPIO_CFG(GPIO_GRFC_6, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_6"},
	{ GPIO_CFG(GPIO_GRFC_7, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_7"},
	{ GPIO_CFG(GPIO_GRFC_8, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_8"},
	{ GPIO_CFG(GPIO_GRFC_9, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_9"},
	{ GPIO_CFG(GPIO_GRFC_10, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_10"},
	{ GPIO_CFG(GPIO_GRFC_11, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_11"},
	{ GPIO_CFG(GPIO_GRFC_13, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_13"},
	{ GPIO_CFG(GPIO_GRFC_14, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_14"},
	{ GPIO_CFG(GPIO_GRFC_15, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_15"},
	{ GPIO_CFG(GPIO_GRFC_16, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_16"},
	{ GPIO_CFG(GPIO_GRFC_17, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_17"},
	{ GPIO_CFG(GPIO_GRFC_18, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_18"},
	{ GPIO_CFG(GPIO_GRFC_24, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_24"},
	{ GPIO_CFG(GPIO_GRFC_25, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_25"},
	{ GPIO_CFG(GPIO_GRFC_26, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_26"},
	{ GPIO_CFG(GPIO_GRFC_27, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_27"},
	{ GPIO_CFG(GPIO_GRFC_28, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_28"},
	{ GPIO_CFG(GPIO_GRFC_29, 7, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_29"},
};

static int __init grfc_init(void)
{
	msm_gpios_request_enable(grfc_config_data,
		ARRAY_SIZE(grfc_config_data));

	return 0;
}

static struct platform_device *devices[] __initdata = {
#ifdef CONFIG_GPIOLIB
	&msm_gpio_devices[0],
	&msm_gpio_devices[1],
	&msm_gpio_devices[2],
	&msm_gpio_devices[3],
	&msm_gpio_devices[4],
	&msm_gpio_devices[5],
#endif
	&msm_device_smd,
	&msm_device_dmov,
	&msm_device_nand,
#ifdef CONFIG_I2C_SSBI
	&msm_device_ssbi0,
	&msm_device_ssbi1,
	&msm_device_ssbi2,
#endif
#ifdef NOTNOW
	&msm_device_i2c,
#endif
#if defined(CONFIG_SERIAL_MSM) || defined(CONFIG_MSM_SERIAL_DEBUGGER)
	&msm_device_uart1,
#endif
	&qfec_device,
	&qcrypto_device,
	&ota_qcrypto_device,
};

#ifdef NOTNOW
static struct msm_gpio msm_i2c_gpios_hw[] = {
	{ GPIO_CFG(85, 1, GPIO_CFG_INPUT, GPIO_NO_PULL, GPIO_16MA),
		"i2c_scl" },
	{ GPIO_CFG(86, 1, GPIO_CFG_INPUT, GPIO_NO_PULL, GPIO_16MA),
		"i2c_sda" },
};

static struct msm_gpio msm_i2c_gpios_io[] = {
	{ GPIO_CFG(85, 0, GPIO_CFG_OUTPUT, GPIO_NO_PULL, GPIO_16MA),
		"i2c_scl" },
	{ GPIO_CFG(86, 0, GPIO_CFG_OUTPUT, GPIO_NO_PULL, GPIO_16MA),
		"i2c_sda" },
};

static void
msm_i2c_gpio_config(int adap_id, int config_type)
{
	struct msm_gpio *msm_i2c_table;

	/* Each adapter gets 2 lines from the table */
	if (adap_id > 0)
		return;
	if (config_type)
		msm_i2c_table = &msm_i2c_gpios_hw[adap_id*2];
	else
		msm_i2c_table = &msm_i2c_gpios_io[adap_id*2];
	msm_gpios_enable(msm_i2c_table, 2);
}

static struct msm_i2c_platform_data msm_i2c_pdata = {
	.clk_freq = 100000,
	.pri_clk = 70,
	.pri_dat = 71,
	.rmutex  = 1,
	.rsl_id = "D:I2C02000021",
	.msm_i2c_config_gpio = msm_i2c_gpio_config,
};

static void __init msm_device_i2c_init(void)
{
	if (msm_gpios_request(msm_i2c_gpios_hw, ARRAY_SIZE(msm_i2c_gpios_hw)))
		pr_err("failed to request I2C gpios\n");

	msm_device_i2c.dev.platform_data = &msm_i2c_pdata;
}
#endif

#ifdef CONFIG_I2C_SSBI
static struct msm_ssbi_platform_data msm_i2c_ssbi0_pdata = {
	.controller_type = FSM_SBI_CTRL_SSBI,
};

static struct msm_ssbi_platform_data msm_i2c_ssbi1_pdata = {
	.controller_type = FSM_SBI_CTRL_SSBI,
};

static struct msm_ssbi_platform_data msm_i2c_ssbi2_pdata = {
	.controller_type = FSM_SBI_CTRL_SSBI,
};
#endif

static struct msm_acpu_clock_platform_data fsm9xxx_clock_data = {
	.acpu_switch_time_us = 50,
	.vdd_switch_time_us = 62,
};

static void __init fsm9xxx_init_irq(void)
{
	msm_init_irq();
	msm_init_sirc();
}

#ifdef NOTNOW
static struct msm_gpio msm_nand_ebi2_cfg_data[] = {
	{GPIO_CFG(86, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_8MA),
		"ebi2_cs1"},
	{GPIO_CFG(115, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_8MA),
		"ebi2_busy1"},
};

struct vreg *vreg_mmc;

#if (defined(CONFIG_MMC_MSM_SDC1_SUPPORT)

struct sdcc_gpio {
	struct msm_gpio *cfg_data;
	uint32_t size;
};

static struct msm_gpio sdc1_lvlshft_cfg_data[] = {
	{GPIO_CFG(35, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_16MA),
		"sdc1_lvlshft"},
};


static struct msm_gpio sdc1_cfg_data[] = {
	{GPIO_CFG(83, 1, GPIO_CFG_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
		"sdc1_clk"},
	{GPIO_CFG(82, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_8MA),
		"sdc1_cmd"},
	{GPIO_CFG(78, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_8MA),
		"sdc1_dat_3"},
	{GPIO_CFG(79, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_8MA),
		"sdc1_dat_2"},
	{GPIO_CFG(80, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_8MA),
		"sdc1_dat_1"},
	{GPIO_CFG(81, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_8MA),
		"sdc1_dat_0"},
};


static struct sdcc_gpio sdcc_cfg_data[] = {
	{
		.cfg_data = sdc1_cfg_data,
		.size = ARRAY_SIZE(sdc1_cfg_data),
	},
};

struct sdcc_vreg {
	struct vreg *vreg_data;
	unsigned level;
};

static struct sdcc_vreg sdcc_vreg_data[4];

static unsigned long vreg_sts, gpio_sts;

static uint32_t msm_sdcc_setup_gpio(int dev_id, unsigned int enable)
{
	int rc = 0;
	struct sdcc_gpio *curr;

	curr = &sdcc_cfg_data[dev_id - 1];

	if (!(test_bit(dev_id, &gpio_sts)^enable))
		return rc;

	if (enable) {
		set_bit(dev_id, &gpio_sts);
		rc = msm_gpios_request_enable(curr->cfg_data, curr->size);
		if (rc)
			printk(KERN_ERR "%s: Failed to turn on GPIOs for slot %d\n",
				__func__,  dev_id);
	} else {
		clear_bit(dev_id, &gpio_sts);
		msm_gpios_disable_free(curr->cfg_data, curr->size);
	}

	return rc;
}

static uint32_t msm_sdcc_setup_vreg(int dev_id, unsigned int enable)
{
	int rc = 0;
	struct sdcc_vreg *curr;
	static int enabled_once[] = {0, 0, 0, 0};

	curr = &sdcc_vreg_data[dev_id - 1];

	if (!(test_bit(dev_id, &vreg_sts)^enable))
		return rc;

	if (!enable || enabled_once[dev_id - 1])
		return 0;

	if (enable) {
		set_bit(dev_id, &vreg_sts);
		rc = vreg_set_level(curr->vreg_data, curr->level);
		if (rc) {
			printk(KERN_ERR "%s: vreg_set_level() = %d\n",
					__func__, rc);
		}
		rc = vreg_enable(curr->vreg_data);
		if (rc) {
			printk(KERN_ERR "%s: vreg_enable() = %d\n",
					__func__, rc);
		}
		enabled_once[dev_id - 1] = 1;
	} else {
		clear_bit(dev_id, &vreg_sts);
		rc = vreg_disable(curr->vreg_data);
		if (rc) {
			printk(KERN_ERR "%s: vreg_disable() = %d\n",
					__func__, rc);
		}
	}
	return rc;
}

static uint32_t msm_sdcc_setup_power(struct device *dv, unsigned int vdd)
{
	int rc = 0;
	struct platform_device *pdev;

	pdev = container_of(dv, struct platform_device, dev);
	rc = msm_sdcc_setup_gpio(pdev->id, (vdd ? 1 : 0));
	if (rc)
		goto out;

	if (pdev->id == 4) /* S3 is always ON and cannot be disabled */
		rc = msm_sdcc_setup_vreg(pdev->id, (vdd ? 1 : 0));
out:
	return rc;
}
#endif

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static struct mmc_platform_data fsm9xxx_sdc1_data = {
	.ocr_mask	= MMC_VDD_165_195,
	.translate_vdd	= msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
#ifdef CONFIG_MMC_MSM_SDC1_DUMMY52_REQUIRED
	.dummy52_required = 1,
#endif
};
#endif

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static void msm_sdc1_lvlshft_enable(void)
{
	int rc;

	/* Enable LDO5, an input to the FET that powers slot 1 */
	rc = vreg_set_level(vreg_mmc, 2850);
	if (rc)
		printk(KERN_ERR "%s: vreg_set_level() = %d\n",	__func__, rc);

	rc = vreg_enable(vreg_mmc);
	if (rc)
		printk(KERN_ERR "%s: vreg_enable() = %d\n", __func__, rc);

	/* Enable GPIO 35, to turn on the FET that powers slot 1 */
	rc = msm_gpios_request_enable(sdc1_lvlshft_cfg_data,
				ARRAY_SIZE(sdc1_lvlshft_cfg_data));
	if (rc)
		printk(KERN_ERR "%s: Failed to enable GPIO 35\n", __func__);

	rc = gpio_direction_output(GPIO_PIN(sdc1_lvlshft_cfg_data[0].gpio_cfg),
				1);
	if (rc)
		printk(KERN_ERR "%s: Failed to turn on GPIO 35\n", __func__);
}
#endif


static void __init fsm9xxx_init_nand(void)
{
	char *build_id;
	struct flash_platform_data *plat_data;

	build_id = socinfo_get_build_id();
	if (build_id == NULL) {
		pr_err("%s: Build ID not available from socinfo\n", __func__);
		return;
	}

	if (build_id[8] == 'C' &&
			!msm_gpios_request_enable(msm_nand_ebi2_cfg_data,
			ARRAY_SIZE(msm_nand_ebi2_cfg_data))) {
		plat_data = msm_device_nand.dev.platform_data;
		plat_data->interleave = 1;
		printk(KERN_INFO "%s: Interleave mode Build ID found\n",
			__func__);
	}
}
#endif /* NOTNOW */

#ifdef CONFIG_SERIAL_MSM_CONSOLE
static struct msm_gpio uart1_config_data[] = {
	{ GPIO_CFG(138, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"UART1_Rx"},
	{ GPIO_CFG(139, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"UART1_Tx"},
};

static void fsm9xxx_init_uart1(void)
{
	msm_gpios_request_enable(uart1_config_data,
			ARRAY_SIZE(uart1_config_data));

}
#endif

/* Intialize GPIO configuration for SSBI */
static struct msm_gpio ssbi_gpio_config_data[] = {
	{ GPIO_CFG(140, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
		"SSBI_0"},
	{ GPIO_CFG(141, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
		"SSBI_1"},
	{ GPIO_CFG(92, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
		"SSBI_2"},
};

static void
fsm9xxx_init_ssbi_gpio(void)
{
	msm_gpios_request_enable(ssbi_gpio_config_data,
		ARRAY_SIZE(ssbi_gpio_config_data));

}

#ifdef CONFIG_MSM_SPM
static struct msm_spm_platform_data msm_spm_data __initdata = {
	.reg_base_addr = MSM_SAW_BASE,

	.reg_init_values[MSM_SPM_REG_SAW_CFG] = 0x05,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_CTL] = 0x18,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_SLP_TMR_DLY] = 0x00006666,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_WAKE_TMR_DLY] = 0xFF000666,

	.reg_init_values[MSM_SPM_REG_SAW_SPM_PMIC_CTL] = 0xE0F272,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_CLK_EN] = 0x01,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_PRECLMP_EN] = 0x03,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_POSTCLMP_EN] = 0x00,

	.reg_init_values[MSM_SPM_REG_SAW_SLP_CLMP_EN] = 0x01,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_RST_EN] = 0x00,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_MPM_CFG] = 0x00,

	.awake_vlevel = 0xF2,
	.retention_vlevel = 0xE0,
	.collapse_vlevel = 0x72,
	.retention_mid_vlevel = 0xE0,
	.collapse_mid_vlevel = 0xE0,
};
#endif

static void __init fsm9xxx_init(void)
{
	if (socinfo_init() < 0)
		printk(KERN_ERR "%s: socinfo_init() failed!\n",
		       __func__);
	msm_acpu_clock_init(&fsm9xxx_clock_data);

	regulator_has_full_constraints();

	platform_add_devices(devices, ARRAY_SIZE(devices));

	/* rmt_storage_add_ramfs(); */

#ifdef NOTNOW
	fsm9xxx_init_nand();
#endif
#ifdef CONFIG_MSM_SPM
	msm_spm_init(&msm_spm_data, 1);
#endif
	msm_pm_set_platform_data(msm_pm_data, ARRAY_SIZE(msm_pm_data));
#ifdef NOTNOW
	msm_device_i2c_init();
#endif
	buses_init();
	phy_init();
	grfc_init();

#ifdef CONFIG_SERIAL_MSM_CONSOLE
	fsm9xxx_init_uart1();
#endif
#ifdef CONFIG_I2C_SSBI
	fsm9xxx_init_ssbi_gpio();
	msm_device_ssbi0.dev.platform_data = &msm_i2c_ssbi0_pdata;
	msm_device_ssbi1.dev.platform_data = &msm_i2c_ssbi1_pdata;
	msm_device_ssbi2.dev.platform_data = &msm_i2c_ssbi2_pdata;
#endif
}

static void __init fsm9xxx_allocate_memory_regions(void)
{

}

static void __init fsm9xxx_map_io(void)
{
	msm_shared_ram_phys = 0x00100000;
	msm_map_fsm9xxx_io();
	fsm9xxx_allocate_memory_regions();
	msm_clock_init(msm_clocks_fsm9xxx, msm_num_clocks_fsm9xxx);
}

MACHINE_START(FSM9XXX_SURF, "QCT FSM9XXX SURF")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params = PHYS_OFFSET + 0x100,
	.map_io = fsm9xxx_map_io,
	.init_irq = fsm9xxx_init_irq,
	.init_machine = fsm9xxx_init,
	.timer = &msm_timer,
MACHINE_END

MACHINE_START(FSM9XXX_FFA, "QCT FSM9XXX FFA")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params = PHYS_OFFSET + 0x100,
	.map_io = fsm9xxx_map_io,
	.init_irq = fsm9xxx_init_irq,
	.init_machine = fsm9xxx_init,
	.timer = &msm_timer,
MACHINE_END


