/* SPDX-License-Identifier: GPL-2.0
 * aw882xx_init.h
 *
 * Copyright (c) 2024 AWINIC Technology CO., LTD
 *
 * Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef __AW882XX_INIT_H__
#define __AW882XX_INIT_H__

#include "aw882xx_pid_1852_reg.h"
#include "aw882xx_pid_2013_reg.h"
#include "aw882xx_pid_2032_reg.h"
#include "aw882xx_pid_2055_reg.h"
#include "aw882xx_pid_2071_reg.h"
#include "aw882xx_pid_2113_reg.h"
#include "aw882xx_pid_2116_reg.h"
#include "aw882xx_pid_2308_reg.h"
#include "aw882xx_pid_2329_reg.h"
#include "aw882xx_pid_2330_reg.h"
#include "aw882xx_pid_2407_reg.h"
#include "aw882xx_pid_2418_reg.h"

#define AW_SOFT_RST_REG (0x00)
#define AW_SYSST_REG (0x01)
#define AW_SYSINTM_REG (0x03)
#define AW_SYSINT_REG  (0x02)
#define AW_SYSCTRL_REG (0x04)
#define AW_SYSCTRL2_REG	(0x05)
#define AW_I2SCTRL1_REG (0x06)
#define AW_DBGCTRL_REG (0x1F)
#define AW_VBAT_REG	(0x21)
#define AW_TEMP_REG (0x22)
#define AW_TESTIN_REG (0x38)
#define AW_TESTOUT_REG (0x39)
#define AW_NGCTRL3_REG		(0x5E)
#define AW_BSTCTRL2_REG		(0x61)
#define AW_LOCKH_REG (0x78)
#define AW_LOCKL_REG (0x7C)

#define AW_SYSINTM_DEFAULT (0xFFFF)
#define AW_SOFT_RST_VAL (0x55aa)

enum aw_chipid {
	PID_1852_ID = 0x1852,
	PID_2013_ID = 0x2013,
	PID_2032_ID = 0x2032,
	PID_2055_ID = 0x2055,
	PID_2055A_ID = 0x2055A,
	PID_2071_ID = 0x2071,
	PID_2113_ID = 0x2113,
	PID_2116_ID = 0x2116,
	PID_2308_ID = 0x2308,
	PID_2329_ID = 0x2329,
	PID_2330_ID = 0x2330,
	PID_2407_ID = 0x2407,
	PID_2418_ID = 0x2418,
};

#define AW_DEV_ATTR(dev_pid, dev_init_ptr) \
{								\
	.pid = (dev_pid), 			\
	.dev_init = (dev_init_ptr)	\
}

/********************************************
 * Volume Coefficient
 *******************************************/
#define AW_VOL_STEP		(64)
#define AW_MUTE_VOL		(1023)
#define AW_6_0P5_VOL_STEP_DB  (6 * 2)
#define AW_6_0P125_VOL_STEP_DB  (6 * 8)

/*monitor reg*/
#define AW_MONITOR_VBAT_RANGE		(6025)
#define AW_MONITOR_INT_10BIT		(1023)  // 1 << 10 -1
#define AW_MONITOR_TEMP_SIGN_MASK	(~(1<<9))
#define AW_MONITOR_TEMP_NEG_MASK	(0xFC00)
#define AW_MONITOR_TEMP_SCALE_DEFAULT (1)

/* SYSST (0x01) detail */
/* UVLS bit 14 (SYSST 0x01) */
#define AW_UVLS_START_BIT		(14)
#define AW_UVLS_UVLO			(1)
#define AW_UVLS_UVLO_VALUE		\
	(AW_UVLS_UVLO << AW_UVLS_START_BIT)

/* BSTOCS bit 11 (SYSST 0x01) */
#define AW_BSTOCS_START_BIT	(11)
#define AW_BSTOCS_OVER_CURRENT	(1)
#define AW_BSTOCS_OVER_CURRENT_VALUE	\
	(AW_BSTOCS_OVER_CURRENT << AW_BSTOCS_START_BIT)

/* BSTS bit 9 (SYSST 0x01) */
#define AW_BSTS_START_BIT		(9)
#define AW_BSTS_FINISHED		(1)
#define AW_BSTS_FINISHED_VALUE	\
	(AW_BSTS_FINISHED << AW_BSTS_START_BIT)

/* SWS bit 8 (SYSST 0x01) */
#define AW_SWS_START_BIT		(8)
#define AW_SWS_SWITCHING		(1)
#define AW_SWS_SWITCHING_VALUE	\
	(AW_SWS_SWITCHING << AW_SWS_START_BIT)

/* NOCLKS bit 5 (SYSST 0x01) */
#define AW_NOCLKS_START_BIT	(5)
#define AW_NOCLKS_NO_CLOCK		(1)
#define AW_NOCLKS_NO_CLOCK_VALUE	\
	(AW_NOCLKS_NO_CLOCK << AW_NOCLKS_START_BIT)

/* CLKS bit 4 (SYSST 0x01) */
#define AW_CLKS_START_BIT		(4)
#define AW_CLKS_STABLE			(1)
#define AW_CLKS_STABLE_VALUE	\
	(AW_CLKS_STABLE << AW_CLKS_START_BIT)

/* OCDS bit 3 (SYSST 0x01) */
#define AW_OCDS_START_BIT		(3)
#define AW_OCDS_OC				(1)
#define AW_OCDS_OC_VALUE		\
	(AW_OCDS_OC << AW_OCDS_START_BIT)

/* OTHS bit 1 (SYSST 0x01) */
#define AW_OTHS_START_BIT		(1)
#define AW_OTHS_OT				(1)
#define AW_OTHS_OT_VALUE		\
	(AW_OTHS_OT << AW_OTHS_START_BIT)

/* PLLS bit 0 (SYSST 0x01) */
#define AW_PLLS_START_BIT		(0)
#define AW_PLLS_LOCKED			(1)
#define AW_PLLS_LOCKED_VALUE	\
	(AW_PLLS_LOCKED << AW_PLLS_START_BIT)

#define AW_SYSST_CHECK_MASK \
	(~(AW_UVLS_UVLO_VALUE | \
	AW_BSTOCS_OVER_CURRENT_VALUE | \
	AW_BSTS_FINISHED_VALUE | \
	AW_SWS_SWITCHING_VALUE | \
	AW_NOCLKS_NO_CLOCK_VALUE | \
	AW_CLKS_STABLE_VALUE | \
	AW_OCDS_OC_VALUE | \
	AW_OTHS_OT_VALUE | \
	AW_PLLS_LOCKED_VALUE))

#define AW_SWS_SYSST_CHECK \
	(AW_BSTS_FINISHED_VALUE | \
	AW_SWS_SWITCHING_VALUE | \
	AW_CLKS_STABLE_VALUE | \
	AW_PLLS_LOCKED_VALUE)

#define AW_NO_SWS_SYSST_CHECK \
		(AW_BSTS_FINISHED_VALUE | \
		AW_CLKS_STABLE_VALUE | \
		AW_PLLS_LOCKED_VALUE)

#define AW_IIS_CHECK \
	(AW_CLKS_STABLE_VALUE | \
	AW_PLLS_LOCKED_VALUE)

/* UVLI bit 14 (SYSINT 0x02) */
#define AW_UVLI_START_BIT		(14)
#define AW_UVLI_BITS_LEN		(1)
#define AW_UVLI_MASK			\
	(~(((1<<AW_UVLI_BITS_LEN)-1) << AW_UVLI_START_BIT))

/* BSTOCI bit 11 (SYSINT 0x02) */
#define AW_BSTOCI_START_BIT	(11)
#define AW_BSTOCI_BITS_LEN		(1)
#define AW_BSTOCI_MASK			\
	(~(((1<<AW_BSTOCI_BITS_LEN)-1) << AW_BSTOCI_START_BIT))

/* OCDI bit 3 (SYSINT 0x02) */
#define AW_OCDI_START_BIT		(3)
#define AW_OCDI_BITS_LEN		(1)
#define AW_OCDI_MASK			\
	(~(((1<<AW_OCDI_BITS_LEN)-1) << AW_OCDI_START_BIT))

/* OTHI bit 1 (SYSINT 0x02) */
#define AW_OTHI_START_BIT		(1)
#define AW_OTHI_BITS_LEN		(1)
#define AW_OTHI_MASK			\
	(~(((1<<AW_OTHI_BITS_LEN)-1) << AW_OTHI_START_BIT))

/* RCV_MODE bit 7 (SYSCTRL 0x04) */
#define AW_RCV_MODE_START_BIT	(7)
#define AW_RCV_MODE_BITS_LEN	(1)
#define AW_RCV_MODE_MASK		\
			(~(((1<<AW_RCV_MODE_BITS_LEN)-1) << AW_RCV_MODE_START_BIT))

#define AW_RCV_MODE_SPEAKER (0)
#define AW_RCV_MODE_SPEAKER_VALUE	\
			(AW_RCV_MODE_SPEAKER << AW_RCV_MODE_START_BIT)

#define AW_RCV_MODE_RECEIVER	(1)
#define AW_RCV_MODE_RECEIVER_VALUE	\
			(AW_RCV_MODE_RECEIVER << AW_RCV_MODE_START_BIT)

/* PWDN bit 0 (SYSCTRL 0x04) */
#define AW_PWDN_START_BIT		(0)
#define AW_PWDN_BITS_LEN		(1)
#define AW_PWDN_MASK			\
	(~(((1<<AW_PWDN_BITS_LEN)-1) << AW_PWDN_START_BIT))

#define AW_PWDN_DISABLE		(0)
#define AW_PWDN_DISABLE_VALUE	\
	(AW_PWDN_DISABLE << AW_PWDN_START_BIT)

#define AW_PWDN_ENABLE		(1)
#define AW_PWDN_ENABLE_VALUE	\
	(AW_PWDN_ENABLE << AW_PWDN_START_BIT)

/* AMPPD bit 1 (SYSCTRL 0x04) */
#define AW_AMPPD_START_BIT		(1)
#define AW_AMPPD_BITS_LEN		(1)
#define AW_AMPPD_MASK			\
		(~(((1<<AW_AMPPD_BITS_LEN)-1) << AW_AMPPD_START_BIT))

#define AW_AMPPD_ENABLE		(0)
#define AW_AMPPD_WORK_VALUE	\
		(AW_AMPPD_ENABLE << AW_AMPPD_START_BIT)

#define AW_AMPPD_DISABLE		(1)
#define AW_AMPPD_POWERDONE_VALUE	\
			(AW_AMPPD_DISABLE << AW_AMPPD_START_BIT)

/* HMUTE bit 8 (SYSCTRL 0x04) */
#define AW_HMUTE_START_BIT		(8)
#define AW_HMUTE_BITS_LEN		(1)
#define AW_HMUTE_MASK			\
			(~(((1<<AW_HMUTE_BITS_LEN)-1) << AW_HMUTE_START_BIT))

#define AW_HMUTE_ENABLE		(1)
#define AW_HMUTE_ENABLE_VALUE	\
			(AW_HMUTE_ENABLE << AW_HMUTE_START_BIT)

#define AW_HMUTE_DISABLE		(0)
#define AW_HMUTE_DISABLE_VALUE	\
				(AW_HMUTE_DISABLE << AW_HMUTE_START_BIT)

/* BOP_EN bit 12 (SYSCTRL 0x04) */
#define AW_BOP_EN_START_BIT	(12)
#define AW_BOP_EN_BITS_LEN		(1)
#define AW_BOP_EN_MASK			\
	(~(((1<<AW_BOP_EN_BITS_LEN)-1) << AW_BOP_EN_START_BIT))

#define AW_BOP_EN_DISABLE		(0)
#define AW_BOP_EN_DISABLE_VALUE	\
	(AW_BOP_EN_DISABLE << AW_BOP_EN_START_BIT)

#define AW_BOP_EN_ENABLE		(1)
#define AW_BOP_EN_ENABLE_VALUE	\
	(AW_BOP_EN_ENABLE << AW_BOP_EN_START_BIT)

/* ULS_HMUTE bit 15 (SYSCTRL 0x04) */
#define AW_ULS_HMUTE_START_BIT		(15)
#define AW_ULS_HMUTE_BITS_LEN		(1)
#define AW_ULS_HMUTE_MASK			\
				(~(((1<<AW_ULS_HMUTE_BITS_LEN)-1) << AW_ULS_HMUTE_START_BIT))

#define AW_ULS_HMUTE_ENABLE		(1)
#define AW_ULS_HMUTE_ENABLE_VALUE	\
				(AW_ULS_HMUTE_ENABLE << AW_ULS_HMUTE_START_BIT)

#define AW_ULS_HMUTE_DISABLE		(0)
#define AW_ULS_HMUTE_DISABLE_VALUE	\
					(AW_ULS_HMUTE_DISABLE << AW_ULS_HMUTE_START_BIT)

/* I2STXEN bit 13 (SYSCTRL 0x04) */
#define AW_I2STXEN_START_BIT	(13)
#define AW_I2STXEN_BITS_LEN	(1)
#define AW_I2STXEN_MASK		\
	(~(((1<<AW_I2STXEN_BITS_LEN)-1) << AW_I2STXEN_START_BIT))

#define AW_I2STXEN_DISABLE		(0)
#define AW_I2STXEN_DISABLE_VALUE	\
	(AW_I2STXEN_DISABLE << AW_I2STXEN_START_BIT)

#define AW_I2STXEN_ENABLE		(1)
#define AW_I2STXEN_ENABLE_VALUE	\
	(AW_I2STXEN_ENABLE << AW_I2STXEN_START_BIT)

/* VOL bit 9:0 (SYSCTRL2 0x05) */
#define FADE_DEFAULT_VOL_STEP (64)
#define AW_VOL_START_BIT	(0)
#define AW_VOL_BITS_LEN		(10)
#define AW_VOL_MASK			\
	(~(((1<<AW_VOL_BITS_LEN)-1) << AW_VOL_START_BIT))

/* CHSEL bit 11:10 (I2SCTRL1 0x06) */
#define AW_CHSEL_START_BIT		(10)
#define AW_CHSEL_BITS_LEN		(2)
#define AW_CHSEL_MASK			\
	(~(((1<<AW_CHSEL_BITS_LEN)-1) << AW_CHSEL_START_BIT))

#define AW_CHSEL_RESERVED		(0)
#define AW_CHSEL_RESERVED_VALUE	\
	(AW_CHSEL_RESERVED << AW_CHSEL_START_BIT)

#define AW_CHSEL_LEFT			(1)
#define AW_CHSEL_LEFT_VALUE	\
	(AW_CHSEL_LEFT << AW_CHSEL_START_BIT)

#define AW_CHSEL_RIGHT			(2)
#define AW_CHSEL_RIGHT_VALUE	\
	(AW_CHSEL_RIGHT << AW_CHSEL_START_BIT)

/* CCO_MUX bit 13 (DBGCTRL 0x1F) */
#define AW_CCO_MUX_START_BIT	(13)
#define AW_CCO_MUX_BITS_LEN	(1)
#define AW_CCO_MUX_MASK		\
	(~(((1<<AW_CCO_MUX_BITS_LEN)-1) << AW_CCO_MUX_START_BIT))

#define AW_CCO_MUX_DIVIDED		(0)
#define AW_CCO_MUX_DIVIDED_VALUE	\
	(AW_CCO_MUX_DIVIDED << AW_CCO_MUX_START_BIT)

#define AW_CCO_MUX_BYPASS		(1)
#define AW_CCO_MUX_BYPASS_VALUE	\
	(AW_CCO_MUX_BYPASS << AW_CCO_MUX_START_BIT)

#define AW_VOL_DEFAULT			(0)
#define AW_VOL_DEFAULT_VALUE	\
	(AW_VOL_DEFAULT << AW_VOL_START_BIT)

/* NOISE_GATE_EN bit 0 (NGCTRL3 0x5E) */
#define AW_NOISE_GATE_EN_START_BIT	(0)
#define AW_NOISE_GATE_EN_BITS_LEN	(1)
#define AW_NOISE_GATE_EN_MASK	\
	(~(((1<<AW_NOISE_GATE_EN_BITS_LEN)-1) << AW_NOISE_GATE_EN_START_BIT))

#define AW_NOISE_GATE_EN_DISABLE	(0)
#define AW_NOISE_GATE_EN_DISABLE_VALUE	\
		(AW_NOISE_GATE_EN_DISABLE << AW_NOISE_GATE_EN_START_BIT)

#define AW_NOISE_GATE_EN_ENABLE	(1)
#define AW_NOISE_GATE_EN_ENABLE_VALUE	\
		(AW_NOISE_GATE_EN_ENABLE << AW_NOISE_GATE_EN_START_BIT)

/* BST_IPEAK bit 15:12 (BSTCTRL2 0x61) */
#define AW_BST_IPEAK_START_BIT	(12)
#define AW_BST_IPEAK_BITS_LEN	(4)
#define AW_BST_IPEAK_MASK		\
	(~(((1<<AW_BST_IPEAK_BITS_LEN)-1) << AW_BST_IPEAK_START_BIT))

/* EF_LOCK bit 15 */
#define AW_EF_LOCK_STRAT_BIT (15)
#define AW_EF_LOCK_BITS_LEN (1)
#define AW_EF_LOCK_MASK \
	(~(((1<<AW_EF_LOCK_BITS_LEN)-1) << AW_EF_LOCK_STRAT_BIT))
#define AW_EF_LOCK_ENABLE (1)
#define AW_EF_LOCK_ENABLE_VALUE \
	(AW_EF_LOCK_ENABLE << AW_EF_LOCK_STRAT_BIT)

#endif /*init end*/
