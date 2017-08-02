/*
 * Copyright 2016 IBM Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <common.h>
#include <netdev.h>
#include <malloc.h>
#include <image.h>

#include <asm/arch/ast_scu.h>
#include <asm/arch/ast-sdmc.h>
#include <asm/arch/vbs.h>
#include <asm/io.h>

#include "tpm-spl.h"

DECLARE_GLOBAL_DATA_PTR;

void watchdog_init(void)
{
#ifdef CONFIG_ASPEED_ENABLE_WATCHDOG
  u32 reload = AST_WDT_CLK * CONFIG_ASPEED_WATCHDOG_TIMEOUT;
  u32 reset_mask = 0x3; /* SoC | Clear after | Enable */
  /* Some boards may request the reset to trigger the EXT reset GPIO.
   * On Linux this is defined as WDT_CTRL_B_EXT.
   */
#ifdef CONFIG_ASPEED_WATCHDOG_TRIGGER_GPIO
  __raw_writel(AST_SCU_BASE + 0xA8, __raw_readl(AST_SCU_BASE + 0xA8) | 0x4);
  reset_mask |= 0x08; /* Ext */
#endif
  ast_wdt_reset(reload, reset_mask);
  printf("Watchdog: %us\n", CONFIG_ASPEED_WATCHDOG_TIMEOUT);
#endif
}

static void vboot_check_enforce(void)
{
  /* Clean the handoff marker from ROM. */
  volatile struct vbs *vbs = (volatile struct vbs*)AST_SRAM_VBS_BASE;
  if (vbs->hardware_enforce) {
    /* If we are hardware-enforcing then this U-Boot is verified. */
    setenv("verify", "yes");
  }
}

static void vboot_finish(void)
{
  /* Clean the handoff marker from ROM. */
  volatile struct vbs *vbs = (volatile struct vbs*)AST_SRAM_VBS_BASE;
  vbs->rom_handoff = 0x0;

#ifdef CONFIG_ASPEED_TPM
  ast_tpm_finish();
#endif
}

char* fit_cert_store(void)
{
  volatile struct vbs *vbs = (volatile struct vbs*)AST_SRAM_VBS_BASE;
  return (char*)(vbs->subordinate_keys);
}

void arch_preboot_os(void) {
  vboot_finish();
}

#ifdef CONFIG_FBTP
static void fan_init(void)
{
  __raw_writel(0x43004300, 0x1e786008);
}

static int get_svr_pwr(void)
{
  // GPIOB6
  return ((__raw_readl(AST_GPIO_BASE + 0x00) >> 14) & 1);
}

static void set_svr_pwr_btn(int level)
{
  u32 reg;
  // GPIOE3
  // output
  reg = __raw_readl(AST_GPIO_BASE + 0x24);
  __raw_writel(reg | (1 << 3), AST_GPIO_BASE + 0x24);

  reg = __raw_readl(AST_GPIO_BASE + 0x20);
  if (level) //high
    reg |= (1<<3);
  else // low
    reg &= ~(1<<3);
  __raw_writel(reg, AST_GPIO_BASE + 0x20);
}

static void policy_init(void)
{
  u32 reg;
  char *policy = NULL;
  char *last_state = NULL;
  char *result;
  int to_pwr_on = 0;

  // BMC's SCU3C: System Reset Control/Status Register
  reg = __raw_readl(AST_SCU_BASE + 0x3c);
  // Power on reset flag(SCU3C[0])
  // POR flag bit will be cleared at Linux init
  if (reg & 0x1) {
    // getenv return the same buffer,
    // duplicate result before call it again.
    result = getenv("por_policy");
    policy = (result) ? strdup(result) : NULL;
    // printf("%X por_policy:%s\n", policy, policy?policy:"null");

    result = getenv("por_ls");
    last_state = (result) ? strdup(result) : NULL;
    // printf("%X por_ls:%s\n", last_state, last_state?last_state:"null");

    if (policy && last_state){
      if ((!strcmp(policy, "on")) ||
          (!strcmp(policy, "lps") && !strcmp(last_state, "on"))
      ) {
        to_pwr_on = 1;
      }
    } else {
      // default power on if no por config
      to_pwr_on = 1;
    }
  }
  printf("to_pwr_on: %d, policy:%s, ls:%s, scu3c:%08X\n",
    to_pwr_on,
    policy ? policy : "null",
    last_state ? last_state : "null",
    reg);

  // Host Server should power on
  if (to_pwr_on == 1) {
    // Host Server is not on
    if (!get_svr_pwr()) {
      set_svr_pwr_btn(0);
      udelay(1000*1000);
      set_svr_pwr_btn(1);
      udelay(1000*1000);
      if (!get_svr_pwr())
        printf("!!!! Power On failed !!!!\n");
    }
  }

  // free duplicated string buffer
  if (policy)
    free(policy);
  if (last_state)
    free(last_state);
}

static void disable_bios_debug(void)
{
  u32 reg;
  // Set GPIOD0's direction as Output
  reg = __raw_readl(AST_GPIO_BASE + 0x4);
  __raw_writel(reg | (1 << 24), AST_GPIO_BASE + 0x4);

  // Set GPIOD0's value as HIGH
  reg = __raw_readl(AST_GPIO_BASE + 0x0);
  reg |= (1<<24);
  __raw_writel(reg, AST_GPIO_BASE + 0x0);
}

static int disable_snoop_dma_interrupt(void)
{
  // Disable interrupt which will not be clearred by wdt reset
  // to avoid interrupts triggered before linux kernel can handle it.
  // PCCR0: Post Code Control Register 0
#ifdef DEBUG
  printf("pccr0: %08X\n", __raw_readl(AST_LPC_BASE + 0x130));
#endif
  __raw_writel(0x0, AST_LPC_BASE + 0x130);

  return 0;
}

#endif

#ifdef CONFIG_FBY2
static int slot_12V_init(void)
{
  u32 slot_present_reg;
  u32 slot_12v_reg;
  u32 dir_reg;
  u32 toler_reg;
  uint8_t val_prim[MAX_NODES+1];
  uint8_t val_ext[MAX_NODES+1];
  uint8_t val;
  int i = 0;


  //Read GPIOZ0~Z3 and AA0~AA3
  slot_present_reg = __raw_readl(AST_GPIO_BASE + 0x1E0);
  //Set GPIOO4~O7 Watchdog reset tolerance
  toler_reg = __raw_readl(AST_GPIO_BASE + 0x0FC);
  toler_reg |= 0xF00000;
  __raw_writel(toler_reg, AST_GPIO_BASE + 0x0FC);
  //Set GPIOO4~O7 as output pin
  dir_reg = __raw_readl(AST_GPIO_BASE + 0x07C);
  dir_reg |= 0xF00000;
  __raw_writel(dir_reg, AST_GPIO_BASE + 0x07C);
  //Read GPIOO4~O7
  slot_12v_reg = __raw_readl(AST_GPIO_BASE + 0x078);

  for(i = 1 ; i < MAX_NODES + 1; i++)
  {
    val_ext[i] = (slot_present_reg >> (2*MAX_NODES+i-1)) & 0x1;
    val_prim[i] =(slot_present_reg >> (4*MAX_NODES+i-1)) & 0x1;

    val = (val_prim[i] || val_ext[i]);
    if(val == 0x00)
    {
       slot_12v_reg |= (1<<(5*MAX_NODES+i-1));
       __raw_writel(slot_12v_reg, AST_GPIO_BASE + 0x078);
    }
    else
    {
       slot_12v_reg &= ~(1<<(5*MAX_NODES+i-1));
       __raw_writel(slot_12v_reg, AST_GPIO_BASE + 0x078);
    }
  }
  return 0;
}
#endif

int board_init(void)
{
	watchdog_init();
	vboot_check_enforce();

#ifdef CONFIG_FBTP
  fan_init();
  policy_init();
  disable_bios_debug();
  disable_snoop_dma_interrupt();
#endif

#ifdef CONFIG_FBY2
  slot_12V_init();
#endif

  gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

  return 0;
}

int dram_init(void)
{
	u32 vga = ast_scu_get_vga_memsize();
	u32 dram = ast_sdmc_get_mem_size();
	gd->ram_size = dram - vga;
#ifdef CONFIG_DRAM_ECC
	gd->ram_size -= gd->ram_size >> 3; /* need 1/8 for ECC */
#endif
	return 0;
}

#ifdef CONFIG_FTGMAC100
int board_eth_init(bd_t *bd)
{
  return ftgmac100_initialize(bd);
}
#endif

#ifdef CONFIG_ASPEEDNIC
int board_eth_init(bd_t *bd)
{
  return aspeednic_initialize(bd);
}
#endif
