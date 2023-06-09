#pragma once

#include <map>

#include "MovesetConverters.hpp"

#include "GameTypes.h"

const auto cg_odd_hitbox_aliases = std::map<Byte, Byte>{
	{ 0x26, 0x2f }, // Leg hitbox
	{ 0x25, 0x2e }, // Leg 
	{ 0x24, 0x2c }, // Leg 
	{ 0x42, 0x52 }, // Alisa DES2
	{ 0x41, 0x51 }, // Alisa DES F1
	//{ 0x44, 0x09 }, // Kunimitsu b+2 right arm, tofix properly
	//{ 0x45, 0x09 }, // Kunimitsu doton 2 right arm, tofix properly
	{ 0x40, 0x50 }, // DJ laser
	{ 0x1E, 0x50 }, // Kunimitsu fire breath
};

const auto cg_even_hitbox_aliases = std::map<Byte, Byte>{
	{ 0x1E, 0x16 }, // Kunimitsu fire breath
};

// Aliases for extraprops andrequirements
// Will be built from cg_propertyAliases, attempts to guess unknown values in between known values
 std::map<unsigned int, s_propAlias> MovesetConverter::T6ToT7::InitAliasDictionary()
{
	return std::map<unsigned int, s_propAlias> {
	{ 3, {.target_id = 3 }}, // 1536: Enemy standing, throw (checking vuln?)
	{ 27, {.target_id = 27 }}, // 2536: Enemy airborne, throw (checking vuln?)
	{ 46, {.target_id = 44 }}, // Counterhit
	{ 49, {.target_id = 47 }}, // MAPPING
	{ 51, {.target_id = 48 }}, // MAPPING
	{ 53, {.target_id = 52 }}, // (ASUKA) aiki_y -> Standing
	{ 57, {.target_id = 60 }}, // (KING) Kg_rope_n -> Kg_rope4_n
	{ 58, {.target_id = 61 }}, // MAPPING
	{ 59, {.target_id = 62 }}, // (HEIHACHI) sOKI_00 -> sOKI_C00
	{ 63, {.target_id = 66 }}, // (HEIHACHI) Co_fumu_01 -> Standing
	{ 64, {.target_id = 67 }}, // MAPPING
	{ 65, {.target_id = 68 }}, // Incoming high
	{ 68, {.target_id = 71 }}, // Incoming high attack
	{ 69, {.target_id = 72 }}, // Incoming mid attack
	{ 70, {.target_id = 73 }}, // (ALISA) Aa_chain_kam -> DA_Aa_ChainDash
	{ 74, {.target_id = 77 }}, // (ASUKA) Jn_apasyou -> Jn_ganmen_n
	{ 77, {.target_id = 80 }}, // (HEIHACHI) sJUMP_00B -> sTRN_L00B
	{ 78, {.target_id = 81 }}, // (HEIHACHI) sJUMP_00B -> sTRN_R00B
	{ 81, {.target_id = 84 }}, // (HEIHACHI) Co_AOzen -> Co_Fcross
	{ 82, {.target_id = 85 }}, // (HEIHACHI) sDw_AIR00_ -> Co_UkemR0D
	{ 83, {.target_id = 86 }}, // (HEIHACHI) sDw_AIR00_ -> Co_UkemL0D
	{ 84, {.target_id = 87 }}, // (HEIHACHI) sDw_AIR00_ -> UNKNOWN-MOVE
	{ 93, {.target_id = 96 }}, // (ASUKA) Jn_apasyou -> Jn_ganmen_n
	{ 98, {.target_id = 100 }}, // (HEIHACHI) sOKI_00 -> cOKI_00
	{ 99, {.target_id = 101 }}, // lower than X frames before attack is active
	{ 100, {.target_id = 102 }}, // X or greater frames before attack is active
	{ 101, {.target_id = 103 }}, // lower than X frames before attack stops being active
	{ 102, {.target_id = 104 }}, // X frames or greater before attack stops being active
	{ 103, {.target_id = 105 }}, // X frames before attack stops being active
	{ 105, {.target_id = 107 }}, // (HEIHACHI) sOKI_00 -> sOKI_C00
	{ 111, {.target_id = 113 }}, // (ZAFINA) Qt_fKAM01 -> Qt_fKAM_st
	{ 112, {.target_id = 114 }}, // MAPPING
	{ 115, {.target_id = 117 }}, // MAPPING
	{ 120, {.target_id = 122 }}, // (HEIHACHI) dwDWN_00U -> Downed
	{ 122, {.target_id = 124 }}, // (HEIHACHI) He_sDASHF -> He_sDASHF
	{ 124, {.target_id = 126 }}, // Opponent downed
	{ 126, {.target_id = 128 }}, // (HEIHACHI) sRUN__01F -> Co_fumuA00
	{ 128, {.target_id = 130 }}, // MAPPING
	{ 129, {.target_id = 131 }}, // MAPPING
	{ 130, {.target_id = 132 }}, // (HEIHACHI) He_Rnage_y -> SideEscR_n
	{ 131, {.target_id = 133 }}, // MAPPING
	{ 132, {.target_id = 134 }}, // (ASUKA) As_tacF_y -> UNKNOWN-MOVE
	{ 133, {.target_id = 135 }}, // Death
	{ 144, {.target_id = 146 }}, // (HEIHACHI) sSTEP_01L -> sWALK_00L
	{ 145, {.target_id = 147 }}, // (HEIHACHI) sSTEP_01L -> sWALK_21L
	{ 146, {.target_id = 148 }}, // (HWOARANG) Hw_sr2cr00_ -> sSIDEL00Rv
	{ 147, {.target_id = 149 }}, // (HWOARANG) Hw_sr2cr00_ -> sSIDER00Rv
	{ 148, {.target_id = 150 }}, // (HEIHACHI) sWALK_01L -> He_st2cr00_
	{ 149, {.target_id = 151 }}, // (HEIHACHI) sWALK_01L -> sJUMP_00_
	{ 152, {.target_id = 154 }}, // (KUMA) Km_spgoro3 -> UNKNOWN-MOVE
	{ 153, {.target_id = 155 }}, // (DRAGUNOV) Cn_lplkrp -> Cn_lplkrpuraNSP
	{ 159, {.target_id = 163, .param_alias = {
		{ 8233, 8295 },
		{ 9237, 11755 },
	} }}, // Is character wallsplat
	{ 160, {.target_id = 164 }}, // MAPPING
	{ 161, {.target_id = 165 }}, // MAPPING
	{ 163, {.target_id = 167 }}, // (KUMA) Km_4KAM02_ -> KUMA_MK
	{ 165, {.target_id = 182 }}, // (HEIHACHI) sDm_IHAR00M -> wDm_KaoB_00B
	{ 172, {.target_id = 189 }}, // (HEIHACHI) He_sDASHB -> wDASH_lkB
	{ 173, {.target_id = 190 }}, // (HEIHACHI) wDm_AirF_WB -> wDm_GndF_00
	{ 199, {.target_id = 216 }}, // (HEIHACHI) sOKI_00 -> sOKI_C00
	{ 200, {.target_id = 217 }}, // Player character id
	{ 201, {.target_id = 218 }}, // Player NOT character id
	{ 202, {.target_id = 0x99999 }}, // Opponent character id
	{ 203, {.target_id = 0x99999 }}, // Opponent NOT character id
	{ 204, {.target_id = 0x99999 }}, // Partner character id
	{ 205, {.target_id = 0x99999 }}, // Partner NOT character id
	{ 206, {.target_id = 0x99999 }}, // Unknown U/F+4, 3
	{ 208, {.target_id = 0x99999 }}, // Opponent partner character id
	{ 209, {.target_id = 0x99999 }}, // Opponent partner NOT character id
	{ 214, {.target_id = 225 }}, // (HEIHACHI) sJUMP_00_ -> Co_sJP_RP
	{ 228, {.target_id = 234 }}, // (NINA) sDm_KAOBntR -> R_bintrp
	{ 232, {.target_id = 251 }}, // (HEIHACHI) He_sKAM00_ -> He_sSTEPB
	{ 234, {.target_id = 253 }}, // (BRYAN) Bx_sDASHF -> Bx_super_RPsp
	{ 246, {.target_id = 265 }}, // (HEIHACHI) He_Ihp2_n -> Standing
	{ 260, {.target_id = 283 }}, // (HEIHACHI) wDm_AirF_00E -> Standing
	{ 262, {.target_id = 289 }}, // MAPPING
	{ 288, {.target_id = 319 }}, // MAPPING
	{ 294, {.target_id = 329 }}, // MAPPING
	{ 302, {.target_id = 337 }}, // (PAUL) st4_y -> Standing
	{ 306, {.target_id = 341 }}, // MAPPING
	{ 307, {.target_id = 342 }}, // MAPPING
	{ 308, {.target_id = 343 }}, // (MARDUK) Mt_LP00 -> Mt_01F_L_ESC_n
	{ 309, {.target_id = 344 }}, // (KAZUYA) sJUMP_00_ -> Kz_Beam01
	{ 313, {.target_id = 352 }}, // MAPPING
	{ 317, {.target_id = 357 }}, // MAPPING
	{ 375, {.target_id = 418 }}, // (KAZUYA) Kz_hp2_y -> Esc_hp2_n
	{ 379, {.target_id = 422 }}, // (DRAGUNOV) Cn_stf_y -> Esc_Cn_alttac_n
	{ 384, {.target_id = 424 }}, // MAPPING
	{ 388, {.target_id = 532 }}, // (BRYAN) Bx_midco_RK -> Standing
	{ 389, {.target_id = 533 }}, // (MIGUEL) Mt_P_wup -> Standing
	{ 400, {.target_id = 544 }}, // MAPPING
	{ 401, {.target_id = 545 }}, // MAPPING
	{ 404, {.target_id = 548 }}, // MAPPING
	{ 408, {.target_id = 552 }}, // (PAUL) Pl_hz_01 -> tekki_cl
	{ 410, {.target_id = 554 }}, // (JACK) Km_t_srpF -> Km_rulu00
	{ 414, {.target_id = 564 }}, // (EDDY) Cp_Direct -> Cp_KAM
	{ 426, {.target_id = 576 }}, // (HEIHACHI) He_sDASHB -> wDASH_lkB
	{ 440, {.target_id = 590 }}, // (KAZUYA) Kz_Transform -> Kz_Transform2
	{ 441, {.target_id = 591 }}, // MAPPING
	{ 454, {.target_id = 604 }}, // (HEIHACHI) wDm_He_wasinage0y -> Standing
	{ 457, {.target_id = 44 }}, // (JIN) Jz_round_RK -> Jz_round_RKbackE // Really?
	{ 458, {.target_id = 608 }}, // (KING) Kg_p01E -> Kg_back_n
	{ 459, {.target_id = 609 }}, // (HEIHACHI) sJUMP_00_ -> Co_IGrenade
	{ 462, {.target_id = 610 }}, // (YOSHIMITSU) Ym_sKAM00_ -> Ym_Noutou
	{ 463, {.target_id = 611 }}, // (YOSHIMITSU) sRUN__00F -> Ym_Xguard_0
	{ 466, {.target_id = 614 }}, // (JIN) Co_fumuA00 R
	{ 467, {.target_id = 615 }}, // Screw/Bound
	{ 468, {.target_id = 614 }}, // Juggle
	{ 483, {.target_id = 741 }}, // (GANRYU) Sm_hip00 -> Sm_mata00
	{ 484, {.target_id = 742 }}, // (BRYAN) Bx_sFUN00_ -> Bx_sFUN00E
	{ 489, {.target_id = 747 }}, // (EDDY) Cp_gKAM00_ -> Cp_sit2st00
	{ 643, {.target_id = 813 }}, // MAPPING
	{ 644, {.target_id = 814 }}, // MAPPING
	{ 645, {.target_id = 815 }}, // MAPPING
	{ 646, {.target_id = 816 }}, // MAPPING
	{ 665, {.target_id = 846, .param_alias = {
		{ 6799, 7043 },
		{ 6661, 6887 },
		{ 6737, 6941 },
		{ 6773, 7019 },
		{ 6777, 7023 },
		{ 6879, 7115 },
		{ 7133, 7351 },
		{ 2663, 2859 },
		{ 2799, 2999 },
		{ 495, 451 },
		{ 209, 207 },
		{ 505, 459 },
		{ 523, 475 },
		{ 525, 477 },
		{ 557, 499 },
		{ 581, 525 },
		{ 591, 533 },
		{ 601, 543 },
		{ 9147, 12087 },
		{ 9151, 12109 },
		{ 1615, 1577 },
		{ 9159, 12131 },
		{ 9167, 12149 },
		{ 9195, 12181 },
		{ 1685, 1709 },
		{ 1961, 2029 },
		{ 2189, 2301 },
		{ 7991, 8091 },
	} }}, // Jin parry
	{ 678, {.target_id = 859, .param_alias = {
		{ 13, 13 },
		{ 9, 9 },
		{ 19, 12 },
		{ 16, 8 },
		{ 12, 18 },
		{ 8, 15 },
		{ 18, 17 },
		{ 15, 14 },
		{ 17, 11 },
		{ 14, 7 },
		{ 11, 21 },
		{ 7, 20 },
		{ 21, 26 },
		{ 20, 25 },
		{ 26, 24 },
		{ 25, 23 },
		{ 24, 44 },
		{ 23, 46 },
		{ 36, 47 },
		{ 37, 50 },
		{ 38, 51 },
		{ 39, 4 },
		{ 40, 22 },
		{ 4, 64 },
		{ 22, 2 },
		{ 1, 31 },
		{ 2, 3 },
		{ 31, 4 },
		{ 3, 0 },
		{ 0, 61 },
		{ 48, 62 },
		{ 45, 65 },
		{ 46, 13 },
		{ 49, 9 },
	} }}, // Jin parry
	{ 679, {.target_id = 860, .param_alias = {
		{ 13, 19 },
		{ 9, 16 },
		{ 19, 12 },
		{ 16, 8 },
		{ 12, 18 },
		{ 8, 15 },
		{ 18, 17 },
		{ 15, 14 },
		{ 17, 11 },
		{ 14, 7 },
		{ 11, 21 },
		{ 7, 20 },
		{ 21, 26 },
		{ 20, 25 },
		{ 26, 24 },
		{ 25, 23 },
		{ 24, 44 },
		{ 23, 46 },
		{ 36, 47 },
		{ 37, 50 },
		{ 38, 51 },
		{ 39, 4 },
		{ 40, 22 },
		{ 4, 64 },
		{ 22, 2 },
		{ 1, 31 },
		{ 2, 3 },
		{ 31, 4 },
		{ 3, 0 },
		{ 0, 61 },
		{ 48, 62 },
		{ 45, 65 },
		{ 46, 13 },
		{ 49, 9 },
	} }}, // Jin parry
	{ 682, {.target_id = 863, .param_alias = {
		{ 13, 19 },
		{ 9, 16 },
		{ 19, 12 },
		{ 16, 8 },
		{ 12, 18 },
		{ 8, 15 },
		{ 18, 17 },
		{ 15, 14 },
		{ 17, 11 },
		{ 14, 7 },
		{ 11, 21 },
		{ 7, 20 },
		{ 21, 26 },
		{ 20, 25 },
		{ 26, 24 },
		{ 25, 23 },
		{ 24, 44 },
		{ 23, 46 },
		{ 36, 47 },
		{ 37, 50 },
		{ 38, 51 },
		{ 39, 4 },
		{ 40, 22 },
		{ 4, 64 },
		{ 22, 2 },
		{ 1, 31 },
		{ 2, 3 },
		{ 31, 4 },
		{ 3, 0 },
		{ 0, 61 },
		{ 45, 65 },
		{ 46, 13 },
		{ 49, 9 },
	} }}, // Jin parry
	{ 683, {.target_id = 864, .param_alias = {
		{ 13, 19 },
		{ 9, 16 },
		{ 19, 12 },
		{ 16, 8 },
		{ 12, 18 },
		{ 8, 15 },
		{ 18, 17 },
		{ 15, 14 },
		{ 17, 11 },
		{ 14, 7 },
		{ 11, 21 },
		{ 7, 20 },
		{ 21, 26 },
		{ 20, 25 },
		{ 26, 24 },
		{ 25, 23 },
		{ 24, 44 },
		{ 23, 46 },
		{ 36, 47 },
		{ 37, 50 },
		{ 38, 51 },
		{ 39, 4 },
		{ 40, 22 },
		{ 4, 64 },
		{ 22, 2 },
		{ 1, 31 },
		{ 2, 3 },
		{ 31, 4 },
		{ 3, 0 },
		{ 0, 61 },
		{ 48, 62 },
		{ 45, 65 },
	} }}, // Jin parry
	{ 690, {.target_id = 881 }}, // Requirements end
	{ 0x8001, {.target_id = 0x8001 }}, // MAPPING
	{ 0x8002, {.target_id = 0x8002 }}, // MAPPING
	{ 0x8003, {.target_id = 0x8003 }}, // MAPPING
	{ 0x8004, {.target_id = 0x8004 }}, // (BOB_SATSUMA) Ft_Hadoh -> Standing
	{ 0x8006, {.target_id = 0x8006 }}, // (STEVE_FOX) Uk_rp_body2 -> Standing
	{ 0x8007, {.target_id = 0x8007 }}, // (ARMOR_KING) Ak_t_srp00B -> Standing
	{ 0x8008, {.target_id = 0x8008 }}, // MAPPING
	{ 0x800b, {.target_id = 0x800b }}, // MAPPING
	{ 0x800e, {.target_id = 0x824d }}, // MAPPING
	{ 0x800f, {.target_id = 0x8003 }}, // (ALISA) Fall_dwKAM
	{ 0x8011, {.target_id = 0x81a7 }}, // (ARMOR_KING) sDm_LmixerFire
	{ 0x8013, {.target_id = 0x8011 }}, // MAPPING
	{ 0x8014, {.target_id = 0x8011 }}, // (ALISA) sDm_PNG00_B5
	{ 0x801b, {.target_id = 0x801e }}, // (ALISA) Aa_sKAM00_
	{ 0x801c, {.target_id = 0x801f }}, // (ALISA) sRUN__00F
	{ 0x801d, {.target_id = 0x802e }}, // (HEIHACHI) He_rp1 -> Standing
	{ 0x801f, {.target_id = 0x8036 }}, // (ALISA) Aa_s_trp2 -> Standing
	{ 0x8022, {.target_id = 0x8039 }}, // (ALISA) sDw_JACKup2
	{ 0x8023, {.target_id = 0x803a }}, // (ALISA) Aa_missileP_y
	{ 0x8024, {.target_id = 0x803b }}, // (ALISA) sDm_2K3_Fy
	{ 0x8026, {.target_id = 0x803d }}, // (JIN) Jz_Ieye_n
	{ 0x8028, {.target_id = 0x803f }}, // (DRAGUNOV) 5hit_n
	{ 0x802b, {.target_id = 0x8042 }}, // (ALISA) sDm_DwKAOF0
	{ 0x802c, {.target_id = 0x8043 }}, // (ALISA) Co_ChgInJmpBalS
	{ 0x802d, {.target_id = 0x8044 }}, // (EDDY) Esc_nagexx19_n
	{ 0x8031, {.target_id = 0x8046 }}, // (HEIHACHI) He_sjk0S -> Standing
	{ 0x8034, {.target_id = 0x8048 }}, // (ALISA) sDm_zutukiF
	{ 0x803d, {.target_id = 0x804c }}, // (JACK) Jc_IMissile2 -> Standing
	{ 0x803e, {.target_id = 0x8056 }}, // (HWOARANG) Hw_agerkFAST
	{ 0x8040, {.target_id = 0x8058 }}, // (ALISA) Aa_chain_Lste
	{ 0x8042, {.target_id = 0x805a }}, // (BOB_SATSUMA) Ft_SUPER_4
	{ 0x8043, {.target_id = 0x805b }}, // (DRAGUNOV) 5hit_n
	{ 0x804f, {.target_id = 0x8061 }}, // (BOB_SATSUMA) Ft_SUPER_4
	{ 0x8051, {.target_id = 0x8063 }}, // Alisa DES stuff
	{ 0x8058, {.target_id = 0x806a }}, // MAPPING
	{ 0x8059, {.target_id = 0x806b }}, // (YOSHIMITSU) Ym_rocket -> wDm_Ym_rocketB
	{ 0x805a, {.target_id = 0x806c }}, // MAPPING
	{ 0x805f, {.target_id = 0x821b }}, // (ASUKA) aiki_y
	{ 0x8062, {.target_id = 0x8074 }}, // (ASUKA) aiki_n
	{ 0x8064, {.target_id = 0x8076 }}, // (ALISA) Aa_sSTEPB
	{ 0x8065, {.target_id = 0x8077 }}, // (ALISA) Grd_STEP_L
	{ 0x8066, {.target_id = 0x8078 }}, // (BRYAN) Bx_sFUN00_
	{ 0x806c, {.target_id = 0x807c }}, // (BRYAN) Bx_slash_LK -> Standing
	{ 0x8087, {.target_id = 0x8097 }}, // (DEVIL_JIN) Esc_hp2_y
	{ 0x808b, {.target_id = 0x809b }}, // (HEIHACHI) He_Ihp2_n -> Standing
	{ 0x808e, {.target_id = 0x809e }}, // (HEIHACHI) Mo_SUPER_0 -> Standing
	{ 0x8091, {.target_id = 0x80a1 }}, // (HEIHACHI) Co_IConductorU -> Standing
	{ 0x8092, {.target_id = 0x80a2 }}, // (HEIHACHI) Co_IConductorU -> Standing
	{ 0x8093, {.target_id = 0x80a3 }}, // MAPPING
	{ 0x8096, {.target_id = 0x80a6 }}, // (HEIHACHI) sDm_IHAR00M -> wDm_KaoB_00B
	{ 0x80a5, {.target_id = 0x80b8 }}, // (DRAGUNOV) Cn_UltacF_n
	{ 0x80aa, {.target_id = 0x80bd }}, // Kazuya devil mode
	{ 0x80af, {.target_id = 0x80d4 }}, // (EDDY) sJUMP_00F -> Cp_st2cr_Rv
	{ 0x80b6, {.target_id = 0x80db }}, // (HEIHACHI) wDm_AirF_00E -> Downed
	{ 0x80b7, {.target_id = 0x80dc }}, // (HEIHACHI) wDm_AirF_00 -> Standing
	{ 0x80cb, {.target_id = 0x80f0 }}, // (KAZUYA) Kz_hp2_y -> Esc_hp2_n
	{ 0x80d7, {.target_id = 0x80fc }}, // (DRAGUNOV) Cn_UltacF_y
	{ 0x80ea, {.target_id = 0x817c }}, // (ALISA) sGrd_YC00G
	{ 0x80eb, {.target_id = 0x817d }}, // (ALISA) Mo_SUPER_0
	{ 0x80ec, {.target_id = 0x817e }}, // (ALISA) dwMOV_L0R
	{ 0x80ed, {.target_id = 0x817f }}, // (ALISA) Male_CST2
	{ 0x80ee, {.target_id = 0x8180 }}, // (ALISA) wDm_AirF_00
	{ 0x80ef, {.target_id = 0x8181 }}, // (ALISA) nuke36_n
	{ 0x80f0, {.target_id = 0x8182 }}, // (MARDUK) Mt_KamF_01
	{ 0x80f1, {.target_id = 0x8183 }}, // (KING) Kg_IAirRRbom_SB
	{ 0x80f3, {.target_id = 0x8185 }}, // (ALISA) nuke36_y
	{ 0x80f6, {.target_id = 0x8188 }}, // MAPPING
	{ 0x80f7, {.target_id = 0x8189 }}, // (DEVIL_JIN) Dj_kazup00E
	{ 0x80f8, {.target_id = 0x818a }}, // (ALISA) Co_Aokilk00
	{ 0x80fa, {.target_id = 0x818c }}, // (HWOARANG) sDm_KAO17LS -> Standing
	{ 0x80fb, {.target_id = 0x818d }}, // (ALISA) Co_ChgInStdBalSolo
	{ 0x80fc, {.target_id = 0x818e }}, // (ALISA) Aa_lrp00
	{ 0x80fd, {.target_id = 0x818f }}, // (ALISA) Wi_IFire
	{ 0x80fe, {.target_id = 0x8190 }}, // (JACK) Km_wpressE
	{ 0x8100, {.target_id = 0x8193 }}, // (ALISA) sRUN__00F
	{ 0x8102, {.target_id = 0x8195 }}, // (KING) Kg_FRnage00_y
	{ 0x8103, {.target_id = 0x8196 }}, // MAPPING
	{ 0x8104, {.target_id = 0x8197 }}, // (ALISA) Aa_spiral
	{ 0x8105, {.target_id = 0x8198 }}, // (ALISA) Aa_sDASHB
	{ 0x8106, {.target_id = 0x8199 }}, // (ALISA) dwup_hei_xe
	{ 0x8108, {.target_id = 0x8188 }}, // (ARMOR_KING) Ak_DOKU
	{ 0x810a, {.target_id = 0x81a3 }}, // (ALISA) Tco_dwupR50
	{ 0x810b, {.target_id = 0x81a4 }}, // (ALISA) dwup_hei_xU
	{ 0x810c, {.target_id = 0x81a5 }}, // (DRAGUNOV) Cn_suranage_y
	{ 0x810d, {.target_id = 0x81a6 }}, // (PAUL) rakuyo2JST
	{ 0x810e, {.target_id = 0x81a7 }}, // (ALISA) sDm_Lmixer
	{ 0x810f, {.target_id = 0x81a8 }}, // (ALISA) Co_ChgInStdBal01
	{ 0x8110, {.target_id = 0x81a9 }}, // (ALISA) sDw_dwhiji
	{ 0x8111, {.target_id = 0x81aa }}, // (KUMA) Km_spgoroE
	{ 0x8113, {.target_id = 0x81ac }}, // (ALISA) wDm_AirF_WB
	{ 0x8114, {.target_id = 0x81ad }}, // (ALISA) FallBalconyFKAM
	{ 0x811c, {.target_id = 0x81c8 }}, // (HEIHACHI) sSTEP_21L -> He_st2cr00B
	{ 0x811d, {.target_id = 0x81c9 }}, // (EDDY) Cp_cWLK00F -> Cp_cr2st00_
	{ 0x811f, {.target_id = 0x81cb }}, // MAPPING
	{ 0x8120, {.target_id = 0x81cc }}, // MAPPING
	{ 0x8121, {.target_id = 0x81cd }}, // (ZAFINA) sSTEP_21L -> st2cr_00B
	{ 0x812a, {.target_id = 0x81d6 }}, // (HEIHACHI) wDm_AirF_00E -> Standing
	{ 0x812b, {.target_id = 0x81d7 }}, // (BOB_SATSUMA) Ft_Hadoh -> Standing
	{ 0x812f, {.target_id = 0x81db }}, // (BRYAN) Bx_midco_RK
	{ 0x8131, {.target_id = 0x81dd }}, // (EDDY) Co_Uokilk01
	{ 0x8132, {.target_id = 0x81de }}, // (KING) sGrd_Kgfrank00
	{ 0x8133, {.target_id = 0x81df }}, // (ALISA) wDm_GndF_Up
	{ 0x8134, {.target_id = 0x81e0 }}, // (ALISA) Co_IRaygun
	{ 0x8135, {.target_id = 0x81e1 }}, // (ALISA) Co_ChgInStdBal01
	{ 0x8137, {.target_id = 0x81e3 }}, // (ALISA) sDw_JACKup2
	{ 0x8138, {.target_id = 0x81e4 }}, // MAPPING
	{ 0x8139, {.target_id = 0x81e5 }}, // (ASUKA) JINjun_n
	{ 0x813a, {.target_id = 0x81e6 }}, // (KAZUYA) Kz_Lnage_y
	{ 0x813b, {.target_id = 0x81e8 }}, // MAPPING
	{ 0x813c, {.target_id = 0x81e9 }}, // Wallsplat (wDm_AirF_Up)
	{ 0x813d, {.target_id = 0x81ea }}, // (ALISA) fatal00
	{ 0x813e, {.target_id = 0x81eb }}, // (FENG) Ta_combo4
	{ 0x813f, {.target_id = 0x81ec }}, // (ALISA) wDm_AirF_00
	{ 0x8142, {.target_id = 0x81ef }}, // MAPPING
	{ 0x8143, {.target_id = 0x81f0 }}, // MAPPING
	{ 0x8145, {.target_id = 0x81f2 }}, // (DEVIL_JIN) Dj_beam_03
	{ 0x8146, {.target_id = 0x81f3 }}, // Dj_beam_01
	{ 0x8147, {.target_id = 0x81f4 }}, // (DEVIL_JIN) Dj_beam_03
	{ 0x814c, {.target_id = 0x81f9 }}, // (DEVIL_JIN) sJUMP_00F_Grd
	{ 0x8152, {.target_id = 0x81ff }}, // (ALISA) Aa_66WPchain2 -> Standing
	{ 0x8153, {.target_id = 0x8200 }}, // (ALISA) Aa_66WPchain2 -> Standing
	{ 0x8155, {.target_id = 0x8202 }}, // (KING) akiresu_yxy -> akiresu_yxyE
	{ 0x815a, {.target_id = 0x8207 }}, // (ALISA) Aa_lrp00
	{ 0x815c, {.target_id = 0x8209 }}, // (ALISA) wDm_AirF_00
	{ 0x815d, {.target_id = 0x820a }}, // (HEIHACHI) wDm_He_wasinage0y -> Standing
	{ 0x8183, {.target_id = 0x8245 }}, // (FENG) Ta_atemi_lp_y
	{ 0x8184, {.target_id = 0x8246 }}, // (ASUKA) aiki_y -> Standing
	{ 0x8188, {.target_id = 0x824a }}, // (KAZUYA) Kz_sKAM00_ -> Standing
	{ 0x8189, {.target_id = 0x824b }}, // MAPPING
	{ 0x818a, {.target_id = 0x824c }}, // (ALISA) sDw_AIR00_
	{ 0x818b, {.target_id = 0x824d }}, // MAPPING
	{ 0x818c, {.target_id = 0x824e }}, // (ALISA) wDm_GndF_00E
	{ 0x818f, {.target_id = 0x8251 }}, // (ALISA) Co_Change_Kam
	{ 0x8192, {.target_id = 0x8254 }}, // (HEIHACHI) He_shoryu_S
	{ 0x8193, {.target_id = 0x8255 }}, // Uses up screw/bound
	{ 0x8194, {.target_id = 0x8256 }}, // (KING) Kg_AirRRbom_y
	{ 0x8196, {.target_id = 0x8257 }}, // (BOB_SATSUMA) Ft_tsshoryu
	{ 0x8197, {.target_id = 0x8258 }}, // (YOSHIMITSU) Ym_sideL00Rv
	{ 0x819d, {.target_id = 0x818f }}, // MAPPING
	{ 0x819e, {.target_id = 0x825f }}, // (KING) Kg_IAirRRbom_n2
	{ 0x81a0, {.target_id = 0x8261 }}, // (HEIHACHI) Co_INunchaku -> Co_INunchaku_2
	{ 0x81a1, {.target_id = 0x8262 }}, // (KAZUYA) Kz_mach2F_n
	{ 0x81a3, {.target_id = 0x8264 }}, // (YOSHIMITSU) Co_INaginata
	{ 0x81a4, {.target_id = 0x8265 }}, // (YOSHIMITSU) Ym_jprp00F
	{ 0x81a7, {.target_id = 0x8268 }}, // (YOSHIMITSU) Ym_lp01
	{ 0x81a8, {.target_id = 0x8269 }}, // (YOSHIMITSU) Ym_jprp00F
	{ 0x81a9, {.target_id = 0x826a }}, // (JACK) Jc_smtlp00
	{ 0x81aa, {.target_id = 0x826b }}, // (ALISA) Aa_chainWchain
	{ 0x81ab, {.target_id = 0x826c }}, // (ALISA) Aa_lk00B
	{ 0x81ac, {.target_id = 0x826d }}, // (ALISA) Aa_rk00F
	{ 0x81b1, {.target_id = 0x8273 }}, // (HEIHACHI) He_sSIDEL10
	{ 0x81b2, {.target_id = 0x8274 }}, // (PANDA) Pn_IChuchu
	{ 0x81b3, {.target_id = 0x8275 }}, // (ALISA) Fm_Ihip_n
	{ 0x81b4, {.target_id = 0x8276 }}, // (GANRYU) Sm_tasuki_y -> Uccha_nuke_n
	{ 0x81b5, {.target_id = 0x856d }}, // (KAZUYA) Kz_hp2_y -> Esc_hp2_n
	{ 0x81b6, {.target_id = 0x8278 }}, // (ALISA) Co_UkemL0D
	{ 0x81b9, {.target_id = 0x827b }}, // MAPPING
	{ 0x81bc, {.target_id = 0x827e }}, // (YOSHIMITSU) Ym_jirai_ho_00 -> Standing
	{ 0x81bd, {.target_id = 0x827f }}, // MAPPING
	{ 0x81c3, {.target_id = 0x8285 }}, // (ASUKA) aiki_y
	{ 0x81c4, {.target_id = 0x8286 }}, // (ASUKA) aiki_y
	{ 0x81ca, {.target_id = 0x832c }}, // (ALISA) Wi_IFire
	{ 0x81cb, {.target_id = 0x832d }}, // (ALISA) Wi_IFire
	{ 0x81cc, {.target_id = 0x832e }}, // (ALISA) Wi_IFire
	{ 0x81ce, {.target_id = 0x8330 }}, // (ALISA) Wi_IFire
	{ 0x81cf, {.target_id = 0x8331 }}, // MAPPING
	{ 0x81d0, {.target_id = 0x8332 }}, // MAPPING
	{ 0x81d1, {.target_id = 0x8333 }}, // (ALISA) Wi_IFire
	{ 0x81d3, {.target_id = 0x8335 }}, // (ALISA) Wi_IFire
	{ 0x81d6, {.target_id = 0x81f3 }}, // MAPPING
	{ 0x81d7, {.target_id = 0x8337 }}, // (ALISA) Wi_IFire
	{ 0x81da, {.target_id = 0x8338 }}, // (ALISA) Wi_IFire
	{ 0x81dd, {.target_id = 0x8339 }}, // (ALISA) Wi_IFire
	{ 0x81e3, {.target_id = 0x833b }}, // (ALISA) Wi_IFire
	{ 0x81e5, {.target_id = 0x833c }}, // (ALISA) Co_IAssaultRifle
	{ 0x81e6, {.target_id = 0x833d }}, // (ALISA) Co_IHandGun_Fire
	{ 0x81e7, {.target_id = 0x833e }}, // (ALISA) Co_IShotgun
	{ 0x81ef, {.target_id = 0x820c }}, // (ALISA) Co_IBouquet
	{ 0x8205, {.target_id = 0x835c }}, // (ALISA) Co_IFLnage00_n
	{ 0x820b, {.target_id = 0x8362 }}, // (ALISA) Co_IHude_P
	{ 0x820c, {.target_id = 0x8363 }}, // MAPPING
	{ 0x820d, {.target_id = 0x8364 }}, // (ALISA) Co_IHude_P
	{ 0x820f, {.target_id = 0x8366 }}, // (ALISA) Co_ISoftCream
	{ 0x8210, {.target_id = 0x8367 }}, // (ALISA) Co_IHude_P
	{ 0x8211, {.target_id = 0x8368 }}, // (ALISA) Co_IHude_P
	{ 0x8214, {.target_id = 0x836b }}, // (ANNA) An_IKinoko_y
	{ 0x8217, {.target_id = 0x836e }}, // (ANNA) An_IKinoko_y
	{ 0x8218, {.target_id = 0x836f }}, // (ANNA) An_IKinoko_y
	{ 0x8219, {.target_id = 0x8370 }}, // (ANNA) An_IKinoko_y
	{ 0x821b, {.target_id = 0x8372 }}, // (ANNA) An_IKinoko_y
	{ 0x821f, {.target_id = 0x8376 }}, // (JULIA) Jl_IGomi_y
	{ 0x8227, {.target_id = 0x837e }}, // (ALISA) Co_IGlove
	{ 0x8228, {.target_id = 0x837f }}, // (ALISA) Co_IGlove
	{ 0x8230, {.target_id = 0x8387 }}, // (ALISA) sRUN__00F
	{ 0x8231, {.target_id = 0x8388 }}, // (ALISA) Aa_wDASH_air01
	{ 0x8232, {.target_id = 0x8389 }}, // (ALISA) Aa_isu -> Standing
	{ 0x8233, {.target_id = 0x838a }}, // (ALISA) Aa_isu -> Standing
	{ 0x8234, {.target_id = 0x838b }}, // (ALISA) Co_Change_Kam
	{ 0x8235, {.target_id = 0x838c }}, // (ALISA) Aa_runchop
	{ 0x8236, {.target_id = 0x838d }}, // (ALISA) SideEscL_y_kubi
	{ 0x8237, {.target_id = 0x838e }}, // (ALISA) Aa_missileP_n
	{ 0x8238, {.target_id = 0x838f }}, // (ALISA) Aa_missileP_n
	{ 0x8239, {.target_id = 0x8390 }}, // (ALISA) Aa_wpbreak
	{ 0x823a, {.target_id = 0x8391 }}, // (ALISA) Aa_kubimissileP_n
	{ 0x823b, {.target_id = 0x8392 }}, // (ALISA) Co_Change_Kam
	{ 0x823c, {.target_id = 0x8393 }}, // (ALISA) Aa_spiral
	{ 0x8249, {.target_id = 0x83c2 }}, // Change other player floor level
	{ 0x824d, {.target_id = 0x83a2 }}, // MAPPING
	{ 0x8257, {.target_id = 0x83ac }}, // (NINA) Ni_dwkiA
	{ 0x8258, {.target_id = 0x83ad }}, // (ASUKA) Jn_blk00 -> Jn_up03n
	{ 0x8259, {.target_id = 0x83ae }}, // (HEIHACHI) He_kawaraE -> Standing
	{ 0x825b, {.target_id = 0x83b0 }}, // (ALISA) Co_tacF_n
	{ 0x825d, {.target_id = 0x83b2 }}, // (HWOARANG) Hw_swrbk00 -> Standing
	{ 0x825f, {.target_id = 0x83b4 }}, // (ALISA) Aa_ateatama -> Standing
	{ 0x8262, {.target_id = 0x83b7 }}, // (DEVIL_JIN) Dj_IHorn
	{ 0x8263, {.target_id = 0x83b8 }}, // (DEVIL_JIN) Dj_IHorn
	{ 0x8264, {.target_id = 0x83b9 }}, // (DEVIL_JIN) Dj_IHorn
	{ 0x8265, {.target_id = 0x83ba }}, // (DEVIL_JIN) Dj_IHorn
	{ 0x8266, {.target_id = 0x83bb }}, // (DEVIL_JIN) Dj_IHorn
	{ 0x8269, {.target_id = 0x83be }}, // (LIN_XIAOYU) Fu_xiao01yE
	{ 0x826e, {.target_id = 0x83c3, .nofill = true }}, // Balconybreak victim: set opponent move ID
	{ 0x8271, {.target_id = 0x8408 }}, // (ALISA) sJUMP_00_
	{ 0x82cd, {.target_id = 0x82d8 }}, // Set Raven clone move id
	{ 0x82d5, {.target_id = 0x8428 }}, // (ALISA) sRUN__00F
	{ 0x82d6, {.target_id = 0x8429 }}, // (ALISA) Co_sJP_RP
	{ 0x82d7, {.target_id = 0x842a }}, // (ALISA) Co_sJP_RP
	{ 0x82d9, {.target_id = 0x842c }}, // (ALISA) Aa_runchop
	{ 0x82db, {.target_id = 0x842e }}, // MAPPING
	{ 0x82dc, {.target_id = 0x842f }}, // MAPPING
	{ 0x82e9, {.target_id = 0x843c }}, // Throw camera?
	{ 0x82ea, {.target_id = 0x843d }}, // Throw camera?2
	{ 0x82ec, {.target_id = 0x843f }}, // (LARS) La_rk00B_n
	{ 0x82ed, {.target_id = 0x84d1 }}, // MAPPING
	{ 0x82ee, {.target_id = 0x8441 }}, // (ALISA) Male_Cnt_M1
	{ 0x82f6, {.target_id = 0x84c1 }}, // (ALISA) Fall_dwKAM
	{ 0x82f8, {.target_id = 0x84c3 }}, // (ASUKA) Popup_Ate_n
	{ 0x82f9, {.target_id = 0x84c4 }}, // (HEIHACHI) Co_wDASH_lk00 -> Standing
	{ 0x82fb, {.target_id = 0x84c6 }}, // MAPPING
	{ 0x8301, {.target_id = 0x84cc }}, // (KUMA) Km_zenring -> Standing
	{ 0x8303, {.target_id = 0x84ce }}, // MAPPING
	{ 0x830a, {.target_id = 0x853c }}, // (HEIHACHI) sDm_2K3_Fy -> wDm_GndF_00
	{ 0x830b, {.target_id = 0x853d }}, // (ALISA) sDw_AIR00_
	};
}

namespace T6_T7_Aliases
{
	Byte OddHitboxByte(Byte value)
	{
		auto item = cg_odd_hitbox_aliases.find(value);
		if (item != cg_odd_hitbox_aliases.end()) {
			return item->second;
		}
		return value;
	}

	Byte EvenHitboxByte(Byte value)
	{
		auto item = cg_even_hitbox_aliases.find(value);
		if (item != cg_even_hitbox_aliases.end()) {
			return item->second;
		}
		return value;
	}

	unsigned char GetCharacterIdAlias(unsigned char value)
	{
		if (value <= 18) return value;
		if (19 <= value && value <= 31) return value - 1;
		if (value <= 42) return value;
		if (43 <= value && value <= 49) return value - 1;
		return value - 2;
	}

	int ConvertMove0x98(int value)
	{
		// What the fuck?
		int tmp = 0;
		for (unsigned int i = 0; i < 7; ++i)
		{
			int bitVal = (value & (1 << i)) != 0;
			tmp |= bitVal << (7 - i);
		}
		return (value >> 7) | (tmp << 24);
	}

	void ApplyHitboxAlias(unsigned int& hitbox)
	{
		Byte* orig_hitbox_bytes = (Byte*)&hitbox;

		for (unsigned int i = 0; i < 4; ++i)
		{
			Byte b = orig_hitbox_bytes[i];
			orig_hitbox_bytes[i] = (b & 1) ? OddHitboxByte(b) : EvenHitboxByte(b);
		}
	}
};
