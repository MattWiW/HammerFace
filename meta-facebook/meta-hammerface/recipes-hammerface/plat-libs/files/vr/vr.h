/*
 *
 * Copyright 2016-present Facebook. All Rights Reserved.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __VR_H__
#define __VR_H__

#ifdef __cplusplus
extern "C" {
#endif

enum {
  VR_CPU0_VCCIN = 0x90,
  VR_CPU0_VSA = 0x90,
  VR_CPU0_VCCIO = 0x94,
  VR_CPU0_VDDQ_ABC = 0xE0,
  VR_CPU0_VDDQ_DEF = 0xE4,
  VR_CPU0_VDDQ_ABC_EVT = 0x1C,
  VR_CPU0_VDDQ_DEF_EVT= 0x14,
  VR_CPU1_VCCIN = 0xB0,
  VR_CPU1_VSA = 0xB0,
  VR_CPU1_VCCIO = 0xB4,
  VR_CPU1_VDDQ_GHJ = 0xA0,
  VR_CPU1_VDDQ_KLM = 0xA4,
  VR_CPU1_VDDQ_GHJ_EVT = 0x0C,
  VR_CPU1_VDDQ_KLM_EVT = 0x04,
  VR_PCH_PVNN = 0xD0,
  VR_PCH_P1V05 = 0xD0,
};

enum {
  CMD_READ = 0xDC,
  CMD_WRITE = 0xDA,
  CMD_SAVE = 0xDB,
};

enum
{
  //temperature
  TEMP_LOW_MULT1 = 0x10,
  TEMP_HIGH_MULT1 = 0x11,
  TEMP_LOW_OFFSET1 = 0x12,
  TEMP_HIGH_OFFSET1 = 0x13,
  TEMP_LOW_MULT2 = 0x14,
  TEMP_HIGH_MULT2 = 0x15,
  TEMP_REG = 0x16,
  TEMP_ZONE = 0x17,

  //Current
  OUT_CURRENT_LOW_MULT1 = 0x34,
  OUT_CURRENT_HIGH_MULT1 = 0x35,
  OUT_CURRENT_LOW_OFFSET1 = 0x36,
  OUT_CURRENT_HIGH_OFFSET1 = 0x37,
  OUT_CURRENT_LOW_MULT2 = 0x38,
  OUT_CURRENT_HIGH_MULT2 = 0x39,
  OUT_CURRENT_REG = 0x3A,
  OUT_CURRENT_SVID = 0x3C,

  //Power
  IN_PWR_LOW_MULT1 = 0x20,
  IN_PWR_HIGH_MULT1 = 0x21,
  IN_PWR_LOW_OFFSET1 = 0x22,
  IN_PWR_HIGH_OFFSET1 = 0x23,
  IN_PWR_LOW_MULT2 = 0x24,
  IN_PWR_HIGH_MULT2 = 0x25,
  IN_PWR_REG = 0x27,
  IN_PWR_SVID = 0x28,
  IN_VOL_RAW = 0x2B,
  IN_CURRENT_RAW = 0x2C,
};

enum {
  VR_LOOP_PAGE_0 = 0x60,
  VR_LOOP_PAGE_1 = 0x61,
  VR_LOOP_PAGE_2 = 0x62
};

enum {
  VR_STATUS_SUCCESS       = 0,
  VR_STATUS_FAILURE       = -1,
  VR_STATUS_NOT_AVAILABLE = -2,
};

int vr_read_PI3020_volt(uint8_t vr, float *value);
int vr_read_PI3020_current(uint8_t vr, float *value);
int vr_read_PI3020_power(uint8_t vr, float *value);
int vr_read_PI3020_temp(uint8_t vr, float *value);

int vr_fw_version(uint8_t vr, char *outvers_str);
int vr_fw_update(uint8_t fru, uint8_t board_info, const char *file);


int vr_read_volt(uint8_t vr, uint8_t loop, float *value);
int vr_read_curr(uint8_t vr, uint8_t loop, float *value);
int vr_read_power(uint8_t vr, uint8_t loop, float *value);
int vr_read_temp(uint8_t vr, uint8_t loop, float *value);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
