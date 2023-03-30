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

#include "apusys_power_cust.h"


//	| VPU0 |		| VPU1 |		| MDLA0 |  --> DVFS_USER
//	(VPU0_VPU)    (VPU1_VPU)     (VMDLA0_MDLA)
//	   |              |               |
//	   v	          v               v
//   |APU_CONN|     |APU_CONN|     |APU_CONN |
//   (VPU0_APU_CONN)(VPU1_APU_CONN)(VMDLA0_APU_CONN)

char *user_str[APUSYS_DVFS_USER_NUM] = {
	"VPU0",
	"VPU1",
	"MDLA0",
};


char *buck_domain_str[APUSYS_BUCK_DOMAIN_NUM] = {
	"V_VPU0",
	"V_VPU1",
	"V_MDLA0",
	"V_APU_CONN",
	"V_TOP_IOMMU",
	"V_VCORE",
};

char *buck_str[APUSYS_BUCK_NUM] = {
	"VPU_BUCK",
	"MDLA_BUCK",
	"VCORE_BUCK",
};


bool apusys_dvfs_user_support[APUSYS_DVFS_USER_NUM] = {
	true, true, true
};

#if VCORE_DVFS_SUPPORT
bool apusys_dvfs_buck_domain_support[APUSYS_BUCK_DOMAIN_NUM] = {
	true, true, true, true, true, true
};
#else
bool apusys_dvfs_buck_domain_support[APUSYS_BUCK_DOMAIN_NUM] = {
	true, true, true, true, true, false
};
#endif

enum DVFS_VOLTAGE_DOMAIN apusys_user_to_buck_domain[APUSYS_DVFS_USER_NUM] = {
	V_VPU0, V_VPU1, V_MDLA0
};


enum DVFS_BUCK apusys_user_to_buck[APUSYS_DVFS_USER_NUM] = {
	VPU_BUCK, VPU_BUCK, VPU_BUCK
};


enum DVFS_USER apusys_buck_domain_to_user[APUSYS_BUCK_DOMAIN_NUM] = {
	VPU0, VPU1, MDLA0,
	APUSYS_DVFS_USER_NUM,
	APUSYS_DVFS_USER_NUM,
	APUSYS_DVFS_USER_NUM  // APUSYS_DVFS_USER_NUM means invalid
};


enum DVFS_BUCK apusys_buck_domain_to_buck[APUSYS_BUCK_DOMAIN_NUM] = {
	VPU_BUCK, VPU_BUCK, VPU_BUCK,
	VPU_BUCK, VPU_BUCK, VCORE_BUCK
};


enum DVFS_VOLTAGE_DOMAIN apusys_buck_to_buck_domain[APUSYS_BUCK_NUM] = {
	V_VPU0, V_MDLA0, V_VCORE
};


// voltage for clk path
uint8_t dvfs_clk_path[APUSYS_DVFS_USER_NUM][APUSYS_PATH_USER_NUM] = {
	{VPU0_VPU, VPU0_APU_CONN, VPU0_TOP_IOMMU, VPU0_VCORE},
	{VPU1_VPU, VPU1_APU_CONN, VPU1_TOP_IOMMU, VPU1_VCORE},
	{MDLA0_MDLA, MDLA0_APU_CONN, MDLA0_TOP_IOMMU, MDLA0_VCORE},
};

enum DVFS_VOLTAGE
	dvfs_clk_path_max_vol[APUSYS_DVFS_USER_NUM][APUSYS_PATH_USER_NUM] = {
	{DVFS_VOLT_00_775000_V, DVFS_VOLT_00_775000_V,
		DVFS_VOLT_00_775000_V, DVFS_VOLT_00_725000_V},
	{DVFS_VOLT_00_775000_V, DVFS_VOLT_00_775000_V,
		DVFS_VOLT_00_775000_V, DVFS_VOLT_00_725000_V},
	{DVFS_VOLT_00_800000_V, DVFS_VOLT_00_775000_V,
		DVFS_VOLT_00_775000_V, DVFS_VOLT_00_725000_V},
};
#if 0
// buck for clk path
uint8_t dvfs_buck_for_clk_path[APUSYS_DVFS_USER_NUM][APUSYS_BUCK_NUM] = {
	{VPU_BUCK, MDLA_BUCK, VCORE_BUCK},
	{VPU_BUCK, MDLA_BUCK, VCORE_BUCK},
	{MDLA_BUCK, MDLA_BUCK, VCORE_BUCK},
};
#endif

// relation for dvfs_clk_path,
bool buck_shared[APUSYS_BUCK_NUM]
			[APUSYS_DVFS_USER_NUM][APUSYS_PATH_USER_NUM] = {
	// VPU BUCK
	{{true, true, true, false},
	{true, true, true, false},
	{true, true, true, false},
	},

	// MDLA BUCK
	{{false, false, false, false},
	{false, false, false, false},
	{false, false, false, false},
	},

	// VCORE
	{{false, false, false, true},
	 {false, false, false, true},
	 {false, false, false, true},
	},
};

// small voltage first
struct apusys_dvfs_constraint
	dvfs_constraint_table[APUSYS_DVFS_CONSTRAINT_NUM] = {
	{VCORE_BUCK, DVFS_VOLT_00_575000_V, VPU_BUCK, DVFS_VOLT_00_775000_V},
};

enum DVFS_VOLTAGE vcore_opp_mapping[] = {
	DVFS_VOLT_00_750000_V,	// VCORE_OPP_0: 0.75 for LPDDR5, no use
	DVFS_VOLT_00_725000_V,	// VCORE_OPP_1
	DVFS_VOLT_00_650000_V,	// VCORE_OPP_2
	DVFS_VOLT_00_600000_V,  // VCORE_OPP_3
	DVFS_VOLT_00_550000_V   // VCORE_OPP_4
};

/* 6877 only has ? segment */
#ifdef APUSYS_PARKOUT_OPP
struct apusys_dvfs_steps
	dvfs_table_0[APUSYS_MAX_NUM_OPPS + 1][APUSYS_BUCK_DOMAIN_NUM] = {
#else
struct apusys_dvfs_steps
	dvfs_table_0[APUSYS_MAX_NUM_OPPS][APUSYS_BUCK_DOMAIN_NUM] = {
#endif
	// opp 0
	{{DVFS_FREQ_00_832000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_832000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_950000_F, DVFS_VOLT_00_800000_V, POSDIV_4, 0x2489D8, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_600000_V, POSDIV_NO, 0x0, false} },

	// opp 1
	{{DVFS_FREQ_00_832000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_832000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_880000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x21D89D, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_600000_V, POSDIV_NO, 0x0, false} },

	// opp 2
	{{DVFS_FREQ_00_728000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x1C0000, false},
	 {DVFS_FREQ_00_728000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x1C0000, false},
	 {DVFS_FREQ_00_832000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_624000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x180000, false},
	 {DVFS_FREQ_00_624000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x180000, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

	// opp 3
	{{DVFS_FREQ_00_624000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x180000, false},
	 {DVFS_FREQ_00_624000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x180000, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_00_500000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x133b13, false},
	 {DVFS_FREQ_00_500000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x133b13, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

	// opp 4
	{{DVFS_FREQ_00_525000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x14313B, false},
	 {DVFS_FREQ_00_525000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x14313B, false},
	 {DVFS_FREQ_00_546000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x150000, false},
	 {DVFS_FREQ_00_343000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x1A6276, true},
	 {DVFS_FREQ_00_343000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x1A6276, true},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

	// opp 5
	{{DVFS_FREQ_00_360000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x1BB13B, true},
	 {DVFS_FREQ_00_360000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x1BB13B, true},
	 {DVFS_FREQ_00_425000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x10589D, false},
	 {DVFS_FREQ_00_250000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x1A6276, true},
	 {DVFS_FREQ_00_250000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x1A6276, true},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

	// opp 6
	{{DVFS_FREQ_00_273000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x150000, true},
	 {DVFS_FREQ_00_273000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x150000, true},
	 {DVFS_FREQ_00_360000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x1BB13B, true},
	 {DVFS_FREQ_00_208000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x100000, true},
	 {DVFS_FREQ_00_208000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x100000, true},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

#ifdef APUSYS_PARKOUT_OPP
	// opp 7
	{{DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false} },
#endif

};

#ifdef APUSYS_PARKOUT_OPP
struct apusys_dvfs_steps
	dvfs_table_1[APUSYS_MAX_NUM_OPPS + 1][APUSYS_BUCK_DOMAIN_NUM] = {
#else
struct apusys_dvfs_steps
	dvfs_table_1[APUSYS_MAX_NUM_OPPS][APUSYS_BUCK_DOMAIN_NUM] = {
#endif
	// opp 0
	{{DVFS_FREQ_00_832000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_832000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_950000_F, DVFS_VOLT_00_800000_V, POSDIV_4, 0x2489D8, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_600000_V, POSDIV_NO, 0x0, false} },

	// opp 1
	{{DVFS_FREQ_00_832000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_832000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_880000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x21D89D, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_600000_V, POSDIV_NO, 0x0, false} },

	// opp 2
	{{DVFS_FREQ_00_728000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x1C0000, false},
	 {DVFS_FREQ_00_728000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x1C0000, false},
	 {DVFS_FREQ_00_832000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_624000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x180000, false},
	 {DVFS_FREQ_00_624000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x180000, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

	// opp 3
	{{DVFS_FREQ_00_624000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x180000, false},
	 {DVFS_FREQ_00_624000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x180000, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_00_500000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x133b13, false},
	 {DVFS_FREQ_00_500000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x133b13, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

	// opp 4
	{{DVFS_FREQ_00_525000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x14313B, false},
	 {DVFS_FREQ_00_525000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x14313B, false},
	 {DVFS_FREQ_00_546000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x150000, false},
	 {DVFS_FREQ_00_343000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x1A6276, true},
	 {DVFS_FREQ_00_343000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x1A6276, true},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

	// opp 5
	{{DVFS_FREQ_00_360000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x1BB13B, true},
	 {DVFS_FREQ_00_360000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x1BB13B, true},
	 {DVFS_FREQ_00_425000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x10589D, false},
	 {DVFS_FREQ_00_250000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x1A6276, true},
	 {DVFS_FREQ_00_250000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x1A6276, true},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

	// opp 6
	{{DVFS_FREQ_00_273000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x150000, true},
	 {DVFS_FREQ_00_273000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x150000, true},
	 {DVFS_FREQ_00_360000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x1BB13B, true},
	 {DVFS_FREQ_00_208000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x100000, true},
	 {DVFS_FREQ_00_208000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x100000, true},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

#ifdef APUSYS_PARKOUT_OPP
	// opp 7
	{{DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false} },
#endif

};

#ifdef APUSYS_PARKOUT_OPP
struct apusys_dvfs_steps
	dvfs_table_2[APUSYS_MAX_NUM_OPPS + 1][APUSYS_BUCK_DOMAIN_NUM] = {
#else
struct apusys_dvfs_steps
	dvfs_table_2[APUSYS_MAX_NUM_OPPS][APUSYS_BUCK_DOMAIN_NUM] = {
#endif
	// opp 0
	{{DVFS_FREQ_00_832000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_832000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_950000_F, DVFS_VOLT_00_800000_V, POSDIV_4, 0x2489D8, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_600000_V, POSDIV_NO, 0x0, false} },

	// opp 1
	{{DVFS_FREQ_00_832000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_832000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_880000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x21D89D, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_775000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_600000_V, POSDIV_NO, 0x0, false} },

	// opp 2
	{{DVFS_FREQ_00_728000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x1C0000, false},
	 {DVFS_FREQ_00_728000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x1C0000, false},
	 {DVFS_FREQ_00_832000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x200000, false},
	 {DVFS_FREQ_00_624000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x180000, false},
	 {DVFS_FREQ_00_624000_F, DVFS_VOLT_00_750000_V, POSDIV_4, 0x180000, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

	// opp 3
	{{DVFS_FREQ_00_624000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x180000, false},
	 {DVFS_FREQ_00_624000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x180000, false},
	 {DVFS_FREQ_00_688000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x1A7627, false},
	 {DVFS_FREQ_00_500000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x133b13, false},
	 {DVFS_FREQ_00_500000_F, DVFS_VOLT_00_700000_V, POSDIV_4, 0x133b13, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

	// opp 4
	{{DVFS_FREQ_00_525000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x14313B, false},
	 {DVFS_FREQ_00_525000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x14313B, false},
	 {DVFS_FREQ_00_546000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x150000, false},
	 {DVFS_FREQ_00_343000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x1A6276, true},
	 {DVFS_FREQ_00_343000_F, DVFS_VOLT_00_650000_V, POSDIV_4, 0x1A6276, true},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

	// opp 5
	{{DVFS_FREQ_00_360000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x1BB13B, true},
	 {DVFS_FREQ_00_360000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x1BB13B, true},
	 {DVFS_FREQ_00_425000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x10589D, false},
	 {DVFS_FREQ_00_250000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x1A6276, true},
	 {DVFS_FREQ_00_250000_F, DVFS_VOLT_00_600000_V, POSDIV_4, 0x1A6276, true},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

	// opp 6
	{{DVFS_FREQ_00_273000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x150000, true},
	 {DVFS_FREQ_00_273000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x150000, true},
	 {DVFS_FREQ_00_360000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x1BB13B, true},
	 {DVFS_FREQ_00_208000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x100000, true},
	 {DVFS_FREQ_00_208000_F, DVFS_VOLT_00_575000_V, POSDIV_4, 0x100000, true},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_550000_V, POSDIV_NO, 0x0, false} },

#ifdef APUSYS_PARKOUT_OPP
	// opp 7
	{{DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_ACC_PARKING, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false},
	 {DVFS_FREQ_NOT_SUPPORT, DVFS_VOLT_00_575000_V, POSDIV_NO, 0x0, false} },
#endif

};

#ifdef AGING_MARGIN
/* Below array define relation between different freq <--> aging voltage */
struct apusys_aging_steps aging_tbl[APUSYS_MAX_NUM_OPPS][V_VCORE] = {
	// opp 0
	{{DVFS_FREQ_00_832000_F, MG_VOLT_18750},
	 {DVFS_FREQ_00_832000_F, MG_VOLT_18750},
	 {DVFS_FREQ_00_950000_F, MG_VOLT_18750},
	 {DVFS_FREQ_00_688000_F, MG_VOLT_18750},
	 {DVFS_FREQ_00_688000_F, MG_VOLT_18750} },
	// opp 1
	{{DVFS_FREQ_00_832000_F, MG_VOLT_18750},
	 {DVFS_FREQ_00_832000_F, MG_VOLT_18750},
	 {DVFS_FREQ_00_880000_F, MG_VOLT_18750},
	 {DVFS_FREQ_00_688000_F, MG_VOLT_18750},
	 {DVFS_FREQ_00_688000_F, MG_VOLT_18750} },
	// opp 2
	{{DVFS_FREQ_00_728000_F, MG_VOLT_18750},
	 {DVFS_FREQ_00_728000_F, MG_VOLT_18750},
	 {DVFS_FREQ_00_832000_F, MG_VOLT_18750},
	 {DVFS_FREQ_00_624000_F, MG_VOLT_18750},
	 {DVFS_FREQ_00_624000_F, MG_VOLT_18750} },
	// opp 3
	{{DVFS_FREQ_00_624000_F, MG_VOLT_12500},
	 {DVFS_FREQ_00_624000_F, MG_VOLT_12500},
	 {DVFS_FREQ_00_688000_F, MG_VOLT_12500},
	 {DVFS_FREQ_00_500000_F, MG_VOLT_12500},
	 {DVFS_FREQ_00_500000_F, MG_VOLT_12500} },
	// opp 4
	{{DVFS_FREQ_00_525000_F, MG_VOLT_12500},
	 {DVFS_FREQ_00_525000_F, MG_VOLT_12500},
	 {DVFS_FREQ_00_546000_F, MG_VOLT_12500},
	 {DVFS_FREQ_00_343000_F, MG_VOLT_12500},
	 {DVFS_FREQ_00_343000_F, MG_VOLT_12500} },
	// opp 5
	{{DVFS_FREQ_00_360000_F, MG_VOLT_06250},
	 {DVFS_FREQ_00_360000_F, MG_VOLT_06250},
	 {DVFS_FREQ_00_425000_F, MG_VOLT_06250},
	 {DVFS_FREQ_00_250000_F, MG_VOLT_06250},
	 {DVFS_FREQ_00_250000_F, MG_VOLT_06250} },
	// opp 6
	{{DVFS_FREQ_00_273000_F, MG_VOLT_06250},
	 {DVFS_FREQ_00_273000_F, MG_VOLT_06250},
	 {DVFS_FREQ_00_360000_F, MG_VOLT_06250},
	 {DVFS_FREQ_00_208000_F, MG_VOLT_06250},
	 {DVFS_FREQ_00_208000_F, MG_VOLT_06250} },
};
#endif /* AGING_MARGIN */

