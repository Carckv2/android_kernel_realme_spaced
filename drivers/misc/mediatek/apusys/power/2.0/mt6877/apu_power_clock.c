/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>

#include "apusys_power_reg.h"
#include "apu_power_api.h"
#include "power_clock.h"
#include "apu_log.h"
#include "apusys_power_ctl.h"
#ifdef CONFIG_MTK_FREQ_HOPPING
#include "mtk_freqhopping_drv.h"
#endif

static DEFINE_SPINLOCK(meter_lock);
#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)

/************** IMPORTANT !! *******************
 * The following name of each clock struct
 * MUST mapping to clock-names @ mt6877.dts
 **********************************************/

/* for dvfs clock source */
static struct clk *clk_apupll_apupll;  /* MDLA  */
static struct clk *clk_apupll_npupll;  /* VPU   */
static struct clk *clk_apupll_apupll1; /* CONN  */
static struct clk *clk_apupll_apupll2; /* IOMMU */

static struct clk *mtcmos_scp_sys_vpu;		// mtcmos for apu conn/vcore

enum APUPLL {
	APUPLL,
	NPUPLL,
	APUPLL1,
	APUPLL2,
	APUPLL_MAX
};

static void _init_acc(enum DVFS_VOLTAGE_DOMAIN domain);

/**************************************************
 * The following functions are apu dedicated usate
 **************************************************/

int enable_apu_mtcmos(int enable)
{
	int ret = 0;
	int ret_all = 0;

	if (enable)
		ENABLE_CLK(mtcmos_scp_sys_vpu)
	else
		DISABLE_CLK(mtcmos_scp_sys_vpu)


	LOG_DBG("%s enable var = %d, ret = %d\n", __func__, enable, ret_all);
	return ret_all;
}

/* Prepare and Set apupll to default freq */
int prepare_apu_clock(struct device *dev)
{
	int ret = 0;
#if 0  //APUSYS_POWER_BRINGUP
	int ret_all = 0;
#endif

	PREPARE_CLK(mtcmos_scp_sys_vpu);

	PREPARE_CLK(clk_apupll_apupll);
	PREPARE_CLK(clk_apupll_npupll);
	PREPARE_CLK(clk_apupll_apupll1);
	PREPARE_CLK(clk_apupll_apupll2);

#if 0 //APUSYS_POWER_BRINGUP
	ENABLE_CLK(clk_apupll_apupll1);
	ENABLE_CLK(clk_apupll_apupll2);
	ENABLE_CLK(clk_apupll_npupll);
	ENABLE_CLK(clk_apupll_apupll);

	//_init_acc(V_APU_CONN);
	_init_acc(V_VPU0);
	_init_acc(V_VPU1);
	_init_acc(V_MDLA0);
	//_init_acc(V_TOP_IOMMU);

	/* Deault ACC will raise APU_ DIV_2 */
	clk_set_rate(clk_apupll_apupll,
			(BUCK_VMDLA_DOMAIN_DEFAULT_FREQ * 2) * 1000);
	clk_set_rate(clk_apupll_npupll,
			(BUCK_VVPU_DOMAIN_DEFAULT_FREQ  * 2) * 1000);
	clk_set_rate(clk_apupll_apupll1,
			(BUCK_VCONN_DOMAIN_DEFAULT_FREQ * 2) * 1000);
	clk_set_rate(clk_apupll_apupll2,
			(BUCK_VIOMMU_DOMAIN_DEFAULT_FREQ * 2) * 1000);

	DISABLE_CLK(clk_apupll_apupll);
	DISABLE_CLK(clk_apupll_npupll);
	DISABLE_CLK(clk_apupll_apupll2);
	DISABLE_CLK(clk_apupll_apupll1);
#endif

	return ret;
}

void unprepare_apu_clock(void)
{
	LOG_DBG("%s bypass\n", __func__);
}

int enable_apupll(enum APUPLL apupll)
{
	int ret = 0;
	int ret_all = 0;

	switch (apupll) {
	case APUPLL:
		ENABLE_CLK(clk_apupll_apupll);
		break;
	case NPUPLL:
		ENABLE_CLK(clk_apupll_npupll);
		break;
	case APUPLL1:
		ENABLE_CLK(clk_apupll_apupll1);
		break;
	case APUPLL2:
		ENABLE_CLK(clk_apupll_apupll2);
		break;
	default:
		LOG_ERR("[%s][%d] Invaild apupll = %d\n",
			__func__, __LINE__, apupll);
		return -1;
	}

	return ret_all;
}

void disable_apupll(enum APUPLL apupll)
{
	switch (apupll) {
	case APUPLL:
		DISABLE_CLK(clk_apupll_apupll);
		break;
	case NPUPLL:
		DISABLE_CLK(clk_apupll_npupll);
		break;
	case APUPLL1:
		DISABLE_CLK(clk_apupll_apupll1);
		break;
	case APUPLL2:
		DISABLE_CLK(clk_apupll_apupll2);
		break;
	default:
		LOG_ERR("[%s][%d] Invaild apupll = %d\n",
			__func__, __LINE__, apupll);
	}
}

/* acc_clk_enable/disable: Turn on/off CG_APU */
static int _enable_acc(enum DVFS_VOLTAGE_DOMAIN domain, bool enable)
{
	void *acc_set = NULL;
	void *acc_clr = NULL;
	void *aacc_set[APUSYS_BUCK_DOMAIN_NUM] = {APU_ACC_CONFG_SET1,
		APU_ACC_CONFG_SET2, APU_ACC_CONFG_SET4, APU_ACC_CONFG_SET0,
		APU_ACC_CONFG_SET7, NULL};
	void *aacc_clr[APUSYS_BUCK_DOMAIN_NUM] = {APU_ACC_CONFG_CLR1,
		APU_ACC_CONFG_CLR2, APU_ACC_CONFG_CLR4, APU_ACC_CONFG_CLR0,
		APU_ACC_CONFG_CLR7, NULL};

	switch (domain) {
	case V_VPU1:
	case V_VPU0:
	case V_MDLA0:
	case V_APU_CONN:
	case V_TOP_IOMMU:
		acc_set = aacc_set[domain];
		acc_clr = aacc_clr[domain];
		break;
	default:
		return -1;
	}

	if (enable)
		writel(BIT(BIT_CGEN_APU), acc_set);
	else
		writel(BIT(BIT_CGEN_APU), acc_clr);

	return 0;
}

int enable_apuacc(enum DVFS_VOLTAGE_DOMAIN domain)
{
	return _enable_acc(domain, true);
}

void disable_apuacc(enum DVFS_VOLTAGE_DOMAIN domain)
{
	_enable_acc(domain, false);
}

int enable_apu_vcore_clksrc(void)
{
	int ret_all = 0;

	LOG_WRN("%s, ret = %d\n", __func__, ret_all);
	return ret_all;
}

int enable_apu_conn_clksrc(void)
{
	int ret_all = 0;

	// MNOC, uP
	enable_apupll(APUPLL1);
	enable_apuacc(V_APU_CONN);

	// IOMMU
	enable_apupll(APUPLL2);
	enable_apuacc(V_TOP_IOMMU);

	LOG_WRN("%s, ret = %d\n", __func__, ret_all);

	return ret_all;
}

/*
 * Initialize the ACCx
 * 1. Select clksrc from APU (default from SOC)
 * 2. Turn off all input CG (CG_SOC default on)
 * 3. Invert out for VPU1/MDLA1
 * 4. Set APU DIV2 for default freq out > 375M
 */
static void _init_acc(enum DVFS_VOLTAGE_DOMAIN domain)
{
	bool inverse = false;
	void *acc_set = NULL;
	void *acc_clr = NULL;
	void *aacc_set[APUSYS_BUCK_DOMAIN_NUM] = {APU_ACC_CONFG_SET1,
		APU_ACC_CONFG_SET2, APU_ACC_CONFG_SET4, APU_ACC_CONFG_SET0,
		APU_ACC_CONFG_SET7, NULL};
	void *aacc_clr[APUSYS_BUCK_DOMAIN_NUM] = {APU_ACC_CONFG_CLR1,
		APU_ACC_CONFG_CLR2, APU_ACC_CONFG_CLR4, APU_ACC_CONFG_CLR0,
		APU_ACC_CONFG_CLR7, NULL};

	switch (domain) {
	case V_VPU1:
		inverse = true;
	case V_VPU0:
	case V_MDLA0:
	case V_APU_CONN:
	case V_TOP_IOMMU:
		acc_set = aacc_set[domain];
		acc_clr = aacc_clr[domain];
		break;
	default:
		return;
	}

	if (inverse)
		writel(BIT(BIT_INVEN_OUT), acc_set);

	writel(BIT(BIT_SEL_APU), acc_set);
	writel(BIT(BIT_CGEN_SOC), acc_clr);
	writel(BIT(BIT_SEL_APU_DIV2), acc_set); /* default freq needed */
}

void acc_init(void)
{
	_init_acc(V_APU_CONN);
	_init_acc(V_VPU0);
	_init_acc(V_VPU1);
	_init_acc(V_MDLA0);
	_init_acc(V_TOP_IOMMU);
}

int enable_apu_device_clksrc(enum DVFS_USER user)
{
	int ret_all = 0;

	switch (user) {
	case VPU0:
	case VPU1:
		enable_apupll(NPUPLL);
		if (user == VPU0)
			enable_apuacc(V_VPU0);
		else
			enable_apuacc(V_VPU1);

		break;
	case MDLA0:
		enable_apupll(APUPLL);
		enable_apuacc(V_MDLA0);

		break;
	default:
		LOG_ERR("%s illegal DVFS_USER: %d\n", __func__, user);
		ret_all = -1;
	}

	LOG_DBG("%s for DVFS_USER: %d, ret = %d\n",
					__func__, user, ret_all);

	return ret_all;
}

int enable_apu_conn_vcore_clock(void)
{
	int ret_all = 0;

	DRV_WriteReg32(APU_VCORE_CG_CLR, 0xFFFFFFFF);
	DRV_WriteReg32(APU_CONN_CG_CLR,  0xFFFFFFFF);
	DRV_WriteReg32(APU_CONN1_CG_CLR, 0xFFFFFFFF);

	LOG_DBG("%s, ret = %d\n", __func__, ret_all);

	return ret_all;
}

//per user
int enable_apu_device_clock(enum DVFS_USER user)
{
	int ret_all = 0;

	switch (user) {
	case VPU0:
		DRV_WriteReg32(APU0_APU_CG_CLR, 0xFFFFFFFF);
		break;
	case VPU1:
		DRV_WriteReg32(APU1_APU_CG_CLR, 0xFFFFFFFF);
		break;
	case MDLA0:
		DRV_WriteReg32(APU_MDLA0_APU_MDLA_CG_CLR, 0xFFFFFFFF);
		break;
	default:
		LOG_ERR("%s illegal DVFS_USER: %d\n", __func__, user);
		ret_all = -1;
	}

	LOG_DBG("%s for DVFS_USER: %d, ret = %d\n",
					__func__, user, ret_all);

	return ret_all;
}

void disable_apu_conn_vcore_clock(void)
{
	LOG_DBG("%s\n", __func__);
}

//per user
void disable_apu_device_clock(enum DVFS_USER user)
{
	switch (user) {
	case VPU0:
	case VPU1:
	case MDLA0:
		break;
	default:
		LOG_ERR("%s illegal DVFS_USER: %d\n", __func__, user);
	}
	LOG_DBG("%s for DVFS_USER: %d\n", __func__, user);
}

void disable_apu_conn_clksrc(void)
{
	// IOMMU
	disable_apuacc(V_TOP_IOMMU);
	disable_apupll(APUPLL2);
	// MNOC, uP
	disable_apuacc(V_APU_CONN);
	disable_apupll(APUPLL1);

	LOG_DBG("%s\n", __func__);
}

void disable_apu_device_clksrc(enum DVFS_USER user)
{
	switch (user) {
	case VPU0:
		disable_apuacc(V_VPU0);
		disable_apupll(NPUPLL);
		break;
	case VPU1:
		disable_apuacc(V_VPU1);
		disable_apupll(NPUPLL);
		break;
	case MDLA0:
		disable_apuacc(V_MDLA0);
		disable_apupll(APUPLL);
		break;
	default:
		LOG_ERR("%s illegal DVFS_USER: %d\n", __func__, user);
	}
	LOG_DBG("%s for DVFS_USER: %d\n", __func__, user);
}

/*
 * acc_clk_set_parent:ACC MUX select
 * 0. freq parameters here, only ACC clksrc is valid
 * 1. Switch between APUPLL <=> Parking (F26M, PARK)
 * 2. Turn on/off CG_F26M, CG_PARK, CG_SOC, but no CG_APU
 * 3. Clear APU Div2 while Parking
 * 4. Only use clksrc of APUPLL while ACC CG_APU is on
 */
int set_apu_clock_source(enum DVFS_FREQ freq, enum DVFS_VOLTAGE_DOMAIN domain)
{
	void *acc_set = NULL;
	void *acc_clr = NULL;
	void *aacc_set[APUSYS_BUCK_DOMAIN_NUM] = {APU_ACC_CONFG_SET1,
		APU_ACC_CONFG_SET2, APU_ACC_CONFG_SET4, APU_ACC_CONFG_SET0,
		APU_ACC_CONFG_SET7, NULL};
	void *aacc_clr[APUSYS_BUCK_DOMAIN_NUM] = {APU_ACC_CONFG_CLR1,
		APU_ACC_CONFG_CLR2, APU_ACC_CONFG_CLR4, APU_ACC_CONFG_CLR0,
		APU_ACC_CONFG_CLR7, NULL};

	switch (domain) {
	case V_VPU1:
	case V_VPU0:
	case V_MDLA0:
	case V_APU_CONN:
	case V_TOP_IOMMU:
		acc_set = aacc_set[domain];
		acc_clr = aacc_clr[domain];
		break;
	default:
		return -1;
	}

	// Select park source
	switch (freq) {
	case DVFS_FREQ_ACC_PARKING:
		// Select park source
		DRV_WriteReg32(acc_set, BIT(BIT_SEL_PARK));
		DRV_WriteReg32(acc_clr, BIT(BIT_SEL_F26M));
		// Enable park cg
		DRV_WriteReg32(acc_set, BIT(BIT_CGEN_PARK));
		DRV_WriteReg32(acc_clr, BIT(BIT_CGEN_F26M) | BIT(BIT_CGEN_SOC));
		// Select park path
		DRV_WriteReg32(acc_clr, BIT(BIT_SEL_APU));
		// clear apu div 2
		DRV_WriteReg32(acc_clr, BIT(BIT_SEL_APU_DIV2));
		break;

	case DVFS_FREQ_ACC_APUPLL:
		// Select park path
		DRV_WriteReg32(acc_set, BIT(BIT_SEL_APU));
		// Clear park cg
		DRV_WriteReg32(acc_clr, BIT(BIT_CGEN_PARK) |
			BIT(BIT_CGEN_F26M) | BIT(BIT_CGEN_SOC));
		break;

	case DVFS_FREQ_ACC_26M:
	case DVFS_FREQ_NOT_SUPPORT:
	default:
		// Select park source
		DRV_WriteReg32(acc_set, BIT(BIT_SEL_F26M));
		DRV_WriteReg32(acc_clr, BIT(BIT_SEL_PARK));
		// Enable park cg
		DRV_WriteReg32(acc_set, BIT(BIT_CGEN_F26M));
		DRV_WriteReg32(acc_clr, BIT(BIT_CGEN_PARK) | BIT(BIT_CGEN_SOC));
		// Select park path
		DRV_WriteReg32(acc_clr, BIT(BIT_SEL_APU));
		// clear apu div 2
		DRV_WriteReg32(acc_clr, BIT(BIT_SEL_APU_DIV2));
		LOG_ERR("%s wrong freq : %d, force assign 26M\n",
							__func__, freq);
	}

	LOG_DBG("%s config domain %s to ACC %d\n",
		__func__, buck_domain_str[domain], freq);

	return 0;
}

static unsigned int apu_get_dds(enum DVFS_FREQ freq,
	enum DVFS_VOLTAGE_DOMAIN domain)
{
	int opp = 0;

	for (opp = 0; opp < APUSYS_MAX_NUM_OPPS; opp++) {
		if (apusys_opps.opps[opp][domain].freq == freq)
			return apusys_opps.opps[opp][domain].dds;
	}

	LOG_DBG("%s freq %d find no dds\n",
		__func__, freq);
	return 0;
}

static enum DVFS_FREQ_POSTDIV apu_get_posdiv_power(enum DVFS_FREQ freq,
	enum DVFS_VOLTAGE_DOMAIN domain)
{
	int opp = 0;

	for (opp = 0; opp < APUSYS_MAX_NUM_OPPS; opp++) {
		if (apusys_opps.opps[opp][domain].freq == freq)
			return apusys_opps.opps[opp][domain].post_divider;
	}

	LOG_DBG("%s freq %d find no post divider\n",
		__func__, freq);
	return POSDIV_4;
}

#if 0
static enum DVFS_FREQ_POSTDIV apu_get_curr_posdiv_power(
	enum DVFS_VOLTAGE_DOMAIN domain)
{
	unsigned long pll  = 0;
	enum DVFS_FREQ_POSTDIV real_posdiv_power = 0;

	if (domain == V_VPU0 || domain == V_VPU1)
		pll = DRV_Reg32(APU_PLL4H_PLL2_CON1);
	else if (domain == V_MDLA0)
		pll = DRV_Reg32(APU_PLL4H_PLL1_CON1);

	real_posdiv_power = (pll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	LOG_DBG("%s real_posdiv_power %d\n",
		__func__, real_posdiv_power);

	return real_posdiv_power;
}
#endif

static bool apu_get_div2(enum DVFS_FREQ freq, enum DVFS_VOLTAGE_DOMAIN domain)
{
	int opp = 0;

	for (opp = 0; opp < APUSYS_MAX_NUM_OPPS; opp++) {
		if (apusys_opps.opps[opp][domain].freq == freq)
			return apusys_opps.opps[opp][domain].div2;
	}

	LOG_DBG("%s freq %d find no div2\n",
		__func__, freq);
	return false;
}

//acc_clk_set_rate w/ vol domain, ex: V_VPU0
int config_apupll_freq(enum DVFS_FREQ freq, enum DVFS_VOLTAGE_DOMAIN domain)
{
	int ret = 0;
	void *acc_clr0 = NULL, *acc_clr1 = NULL;
	void *acc_set0 = NULL, *acc_set1 = NULL;
	void *pll_con1 = NULL;
	struct clk *clk_apupll = NULL;
	enum DVFS_FREQ_POSTDIV posdiv_power = 0;
	unsigned int dds;
	bool div2;

	posdiv_power = apu_get_posdiv_power(freq, domain);
	dds = apu_get_dds(freq, domain);
	div2 = apu_get_div2(freq, domain);

	/*
	 * Switch to Parking src
	 * 1. Need to switch out all ACCs sharing the same apupll
	 */
	switch (domain) {
	case V_MDLA0:
		acc_clr0 = APU_ACC_CONFG_CLR4;
		acc_set0 = APU_ACC_CONFG_SET4;
		pll_con1 = APU_PLL4H_PLL1_CON1;
		clk_apupll = clk_apupll_apupll;

		ret = set_apu_clock_source(DVFS_FREQ_ACC_PARKING, V_MDLA0);
		break;
	case V_VPU0:
	case V_VPU1:
		acc_clr0 = APU_ACC_CONFG_CLR1;
		acc_clr1 = APU_ACC_CONFG_CLR2;
		acc_set0 = APU_ACC_CONFG_SET1;
		acc_set1 = APU_ACC_CONFG_SET2;
		pll_con1 = APU_PLL4H_PLL2_CON1;
		clk_apupll = clk_apupll_npupll;

		ret = set_apu_clock_source(DVFS_FREQ_ACC_PARKING, V_VPU0);
		ret = set_apu_clock_source(DVFS_FREQ_ACC_PARKING, V_VPU1);
		break;
	case V_APU_CONN:
		acc_clr0 = APU_ACC_CONFG_CLR0;
		acc_set0 = APU_ACC_CONFG_SET0;
		pll_con1 = APU_PLL4H_PLL3_CON1;
		clk_apupll = clk_apupll_apupll1;

		ret = set_apu_clock_source(DVFS_FREQ_ACC_PARKING, V_APU_CONN);
		break;
	case V_TOP_IOMMU:
		acc_clr0 = APU_ACC_CONFG_CLR7;
		acc_set0 = APU_ACC_CONFG_SET7;
		pll_con1 = APU_PLL4H_PLL4_CON1;
		clk_apupll = clk_apupll_apupll2;

		ret = set_apu_clock_source(DVFS_FREQ_ACC_PARKING, V_TOP_IOMMU);
		break;
	default:
		pr_info("[%s][%d] invalid input domain (%d)\n",
			__func__, __LINE__, domain);
		return -1;
	}

	clk_set_rate(clk_apupll, (div2) ? (freq * 1000 * 2) : (freq * 1000));

	if (div2 == true)
		DRV_WriteReg32(acc_set0, BIT(BIT_SEL_APU_DIV2));
	if ((div2 == true) && (acc_set1))
		DRV_WriteReg32(acc_set1, BIT(BIT_SEL_APU_DIV2));

	/*
	 * Switch back to APUPLL
	 * Only switch back to APUPLL while CG_APU on
	 * And clksrc is not APUPLL
	 */
	switch (domain) {
	case V_VPU0:
	case V_VPU1:
		if ((DRV_Reg32(acc_set0) & BIT(BIT_CGEN_APU)) &&
			!(DRV_Reg32(acc_set0) & BIT(BIT_SEL_APU)))
			ret = set_apu_clock_source(DVFS_FREQ_ACC_APUPLL, V_VPU0);
		if ((DRV_Reg32(acc_set1) & BIT(BIT_CGEN_APU)) &&
			!(DRV_Reg32(acc_set1) & BIT(BIT_SEL_APU)))
			ret = set_apu_clock_source(DVFS_FREQ_ACC_APUPLL, V_VPU1);
		break;
	case V_MDLA0:
		if ((DRV_Reg32(acc_set0) & BIT(BIT_CGEN_APU)) &&
			!(DRV_Reg32(acc_set0) & BIT(BIT_SEL_APU)))
			ret = set_apu_clock_source(DVFS_FREQ_ACC_APUPLL, V_MDLA0);
		break;
	case V_APU_CONN:
	case V_TOP_IOMMU:
		if ((DRV_Reg32(acc_set0) & BIT(BIT_CGEN_APU)) &&
			!(DRV_Reg32(acc_set0) & BIT(BIT_SEL_APU)))
			ret = set_apu_clock_source(DVFS_FREQ_ACC_APUPLL, domain);
		break;
	default:
		pr_info("[%s][%d] invalid input domain (%d)\n",
			__func__, __LINE__, domain);
		return -1;
	}

	return ret;
}

#if FMETER_CHK
#define FM_PLL1_CK				0
#define FM_PLL2_CK				1
#define FM_PLL3_CK				2
#define FM_PLL4_CK				3
static unsigned int pll_freqmeter_get(unsigned int pll_sel)
{
	int output = 0, i = 0;
	unsigned int temp, pll4h_fqmtr_con0, pll4h_fqmtr_con1;
	bool timeout = false;
	unsigned long flags;
	void *con0 = APU_PLL4H_FQMTR_CON0;
	void *con1 = APU_PLL4H_FQMTR_CON1;

	fmeter_lock(flags);
	pll4h_fqmtr_con0 = DRV_Reg32(con0);
	pll4h_fqmtr_con1 = DRV_Reg32(con1);

	/* PLL4H_FQMTR_CON1[15]: rst 1 -> 0 */
	DRV_WriteReg32(con0, DRV_Reg32(con0) & 0xFFFF7FFF);
	/* PLL4H_FQMTR_CON1[15]: rst 0 -> 1 */
	DRV_WriteReg32(con0, DRV_Reg32(con0) | 0x000080000);
	/* sel fqmtr_cksel */
	DRV_WriteReg32(con0, (pll4h_fqmtr_con0 & 0x00FFFFF8) | (pll_sel << 0));

	/* fqmtr_en set to 1, fqmtr_exc set to 0, fqmtr_start set to 0 */
	DRV_WriteReg32(con0, ((DRV_Reg32(con0) | 0x00001000) & 0xFFFFFEEF));
	/*fqmtr_start set to 1 */
	DRV_WriteReg32(con0, DRV_Reg32(con0) | 0x00000010);

	/* wait frequency meter finish */
	while (DRV_Reg32(con0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30) {
			timeout = true;
			LOG_DBG("timeout! [PLL%d]con0: 0x%x, con1: 0x%x\n",
				pll_sel, DRV_Reg32(con0), DRV_Reg32(con1));
			break;
		}
	}

	if (!timeout) {
		temp = DRV_Reg32(con1) & 0xFFFF;
		output = ((temp * 26000)) / (256 * 1000); // MHz
	} else {
		output = 0;
	}

	DRV_WriteReg32(con0, pll4h_fqmtr_con0);
	DRV_WriteReg32(con1, pll4h_fqmtr_con1);
	fmeter_unlock(flags);

	return output;
}
#endif

#if FMETER_CHK
#define FM_ACC0		0x0
#define FM_ACC1		0x1
#define FM_ACC2		0x10
#define FM_ACC4		0x40
#define FM_ACC5		0x44
#define FM_ACC7		0x68
#define FM_ACC0_Pout		0x8000
#define FM_ACC7_Pout		0xE800
static unsigned int acc_freqmeter_get(unsigned int acc_sel)
{
	int output = 0, i = 0;
	unsigned int tempValue = 0;
	bool timeout = false;
	unsigned int loop_ref = 0;  // 0 for Max freq  ~ 1074MHz
	unsigned long flags;
	void *fm_sel = APU_ACC_FM_SEL;
	void *confg_set = APU_ACC_FM_CONFG_SET;
	void *confg_clr = APU_ACC_FM_CONFG_CLR;

	fmeter_lock(flags);
	/* reset */
	DRV_WriteReg32(fm_sel, 0x0);
	DRV_WriteReg32(fm_sel, DRV_Reg32(fm_sel) | acc_sel);
	DRV_WriteReg32(fm_sel, DRV_Reg32(fm_sel) | (loop_ref << D_FM_LOOP_REF_OFFSET));
	DRV_WriteReg32(confg_set, BIT(D_FM_CLK_EN));
	DRV_WriteReg32(confg_set, BIT(D_FM_FUN_EN));

	/* wait frequency meter finish */
	while (!(DRV_Reg32(confg_set) & BIT(D_FM_FM_DONE))) {
		udelay(10);
		i++;
		if (i > 30) {
			timeout = true;
			LOG_DBG("timeout! [ACC%d]fm_sel: 0x%x, confg_set: 0x%x\n",
				acc_sel, DRV_Reg32(fm_sel), DRV_Reg32(confg_set));
			break;
		}
	}

	if ((!timeout) &&
		!(DRV_Reg32(confg_set) & BIT(D_FM_FM_OVERFLOW))) {
		tempValue = DRV_Reg32(APU_ACC_FM_CNT);
		tempValue = tempValue & 0xFFFF;
		output = tempValue * 16384 / ((loop_ref + 1) * 1000000);  //MHz
	} else {
		output = 0;
	}

	DRV_WriteReg32(confg_clr, BIT(D_FM_FM_DONE));
	DRV_WriteReg32(confg_clr, BIT(D_FM_FUN_EN));
	DRV_WriteReg32(confg_clr, BIT(D_FM_CLK_EN));
	fmeter_unlock(flags);

	return output;
}
#endif

// dump related frequencies of APUsys
void dump_frequency(struct apu_power_info *info)
{
	int apupll_freq = 0;
	int npupll_freq = 0;
	int apupll1_freq = 0;
	int apupll2_freq = 0;
	int dump_div = 1;
#if FMETER_CHK
	int acc0_fmeter = 0, acc1_fmeter = 0, acc2_fmeter = 0;
	int acc4_fmeter = 0, acc7_fmeter = 0;
	int acc0_pout_fmeter = 0, acc7_pout_fmeter = 0;
	int apupll_fmeter = 0, npupll_fmeter = 0;
	int apupll1_fmeter = 0, apupll2_fmeter = 0;
#else
	uint8_t opp_index = 0;
#endif

	info->acc_status[0] = DRV_Reg32(APU_ACC_CONFG_SET0);
	info->acc_status[1] = DRV_Reg32(APU_ACC_CONFG_SET1);
	info->acc_status[2] = DRV_Reg32(APU_ACC_CONFG_SET2);
	info->acc_status[4] = DRV_Reg32(APU_ACC_CONFG_SET4);
	info->acc_status[7] = DRV_Reg32(APU_ACC_CONFG_SET7);

#if FMETER_CHK
	if (info->acc_status[0] & BIT(BIT_CGEN_APU)) {
		apupll1_fmeter = pll_freqmeter_get(FM_PLL3_CK);
		acc0_fmeter = acc_freqmeter_get(FM_ACC0);
		acc0_pout_fmeter = acc_freqmeter_get(FM_ACC0_Pout);
	}
	if (info->acc_status[1] & BIT(BIT_CGEN_APU)) {
		npupll_fmeter = pll_freqmeter_get(FM_PLL2_CK);
		acc1_fmeter = acc_freqmeter_get(FM_ACC1);
	}
	if (info->acc_status[2] & BIT(BIT_CGEN_APU)) {
		npupll_fmeter = pll_freqmeter_get(FM_PLL2_CK);
		acc2_fmeter = acc_freqmeter_get(FM_ACC2);
	}
	if (info->acc_status[4] & BIT(BIT_CGEN_APU)) {
		apupll_fmeter = pll_freqmeter_get(FM_PLL1_CK);
		acc4_fmeter = acc_freqmeter_get(FM_ACC4);
	}
	if (info->acc_status[7] & BIT(BIT_CGEN_APU)) {
		apupll2_fmeter = pll_freqmeter_get(FM_PLL4_CK);
		acc7_fmeter = acc_freqmeter_get(FM_ACC7);
		acc7_pout_fmeter = acc_freqmeter_get(FM_ACC7_Pout);
	}
#endif

	apupll_freq = clk_get_rate(clk_apupll_apupll);
	npupll_freq = clk_get_rate(clk_apupll_npupll);
	apupll1_freq = clk_get_rate(clk_apupll_apupll1);
	apupll2_freq = clk_get_rate(clk_apupll_apupll2);

	info->apupll_freq = apupll_freq / dump_div;
	info->npupll_freq = npupll_freq / dump_div;
	info->apupll1_freq = apupll1_freq / dump_div;
	info->apupll2_freq = apupll2_freq / dump_div;

#if FMETER_CHK
	if (info->acc_status[0] & BIT(BIT_SEL_APU))
		info->conn_freq = acc0_fmeter;
	else
		info->conn_freq = acc7_pout_fmeter;

	if (info->acc_status[1] & BIT(BIT_SEL_APU))
		info->vpu0_freq = acc1_fmeter;
	else
		info->vpu0_freq = acc0_pout_fmeter;

	if (info->acc_status[2] & BIT(BIT_SEL_APU))
		info->vpu1_freq = acc2_fmeter;
	else
		info->vpu1_freq = acc0_pout_fmeter;

	if (info->acc_status[4] & BIT(BIT_SEL_APU))
		info->mdla0_freq = acc4_fmeter;
	else
		info->mdla0_freq = acc0_pout_fmeter;

	if (info->acc_status[7] & BIT(BIT_SEL_APU))
		info->iommu_freq = acc7_fmeter;
	else
		info->iommu_freq = acc0_pout_fmeter;

	dump_div = info->dump_div ? info->dump_div : 1;
	do_div(info->apupll_freq, dump_div * KHZ);
	do_div(info->npupll_freq, dump_div * KHZ);
	do_div(info->apupll2_freq, dump_div * KHZ);
	do_div(info->apupll1_freq, dump_div * KHZ);
#else

	opp_index = apusys_opps.cur_opp_index[V_VPU0];
	info->vpu0_freq = (apusys_opps.opps[opp_index][V_VPU0].div2) ?
				npupll_freq / 2 : npupll_freq;

	opp_index = apusys_opps.cur_opp_index[V_VPU1];
	info->vpu1_freq = (apusys_opps.opps[opp_index][V_VPU1].div2) ?
				npupll_freq / 2 : npupll_freq;

	opp_index = apusys_opps.cur_opp_index[V_MDLA0];
	info->mdla0_freq = (apusys_opps.opps[opp_index][V_MDLA0].div2) ?
				apupll_freq / 2 : apupll_freq;

	opp_index = apusys_opps.cur_opp_index[V_APU_CONN];
	info->conn_freq = (apusys_opps.opps[opp_index][V_APU_CONN].div2) ?
				apupll1_freq / 2 : apupll1_freq;

	opp_index = apusys_opps.cur_opp_index[V_TOP_IOMMU];
	info->iommu_freq = (apusys_opps.opps[opp_index][V_TOP_IOMMU].div2) ?
				apupll2_freq / 2 : apupll2_freq;

	dump_div = info->dump_div ? info->dump_div : 1;
	do_div(info->apupll_freq, dump_div * KHZ);
	do_div(info->npupll_freq, dump_div * KHZ);
	do_div(info->apupll2_freq, dump_div * KHZ);
	do_div(info->apupll1_freq, dump_div * KHZ);

	do_div(info->vpu0_freq, dump_div * KHZ);
	do_div(info->vpu1_freq, dump_div * KHZ);
	do_div(info->mdla0_freq, dump_div * KHZ);
	do_div(info->conn_freq, dump_div * KHZ);
	do_div(info->iommu_freq, dump_div * KHZ);
#endif

#if 0
#if FMETER_CHK
	LOG_DBG("apupll_fmeter = %d\n", apupll_fmeter);
	LOG_DBG("npupll_fmeter = %d\n", npupll_fmeter);
	LOG_DBG("apupll1_fmeter = %d\n", apupll1_fmeter);
	LOG_DBG("apupll2_fmeter = %d\n", apupll2_fmeter);
	LOG_DBG("acc0_fmeter = %d\n", acc0_fmeter);
	LOG_DBG("acc1_fmeter = %d\n", acc1_fmeter);
	LOG_DBG("acc2_fmeter = %d\n", acc2_fmeter);
	LOG_DBG("acc4_fmeter = %d\n", acc4_fmeter);
	LOG_DBG("acc7_fmeter = %d\n", acc7_fmeter);
	LOG_DBG("acc0_pout_fmeter = %d\n", acc0_pout_fmeter);
	LOG_DBG("acc7_pout_fmeter = %d\n", acc7_pout_fmeter);
#endif

	LOG_DBG("apupll_freq = %d\n", apupll_freq);
	LOG_DBG("npupll_freq = %d\n", npupll_freq);
	LOG_DBG("apupll1_freq = %d\n", apupll1_freq);
	LOG_DBG("apupll2_freq = %d\n", apupll2_freq);
	LOG_DBG("vpu0_freq = %d, acc1_status = 0x%x\n", info->vpu0_freq, info->acc_status[1]);
	LOG_DBG("vpu1_freq = %d, acc2_status = 0x%x\n", info->vpu1_freq, info->acc_status[2]);
	LOG_DBG("mdla0_freq = %d, acc4_status = 0x%x\n", info->mdla0_freq, info->acc_status[4]);
	LOG_DBG("conn_freq = %d, acc0_status = 0x%x\n", info->conn_freq, info->acc_status[0]);
	LOG_DBG("iommu_freq = %d, acc7_status = 0x%x\n", info->iommu_freq, info->acc_status[7]);
#endif
}
