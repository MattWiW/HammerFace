/*
 * fw-util
 *
 * Copyright 2015-present Facebook. All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <stdint.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <facebook/bic.h>
#include <openbmc/pal.h>
#include <openbmc/ipmi.h>

#define PAGE_SIZE  0x1000
#define AST_WDT_BASE 0x1e785000
#define WDT2_TIMEOUT_OFFSET 0x30

static void
print_usage_help(void) {
  uint8_t sku = 0;
  sku = pal_get_iom_type();

  printf("Usage: fw-util <all|server|iom|scc> <--version>\n");
  printf("       fw-util <server> <--update> <--cpld|--bios|--bic|--bicbl> <path>\n");
  printf("       fw-util <iom> <--update> <--rom|--bmc> <path>\n");
}

static void
print_fw_scc_ver(void) {

  uint8_t ver[32] = {0};
  int ret = 0;
  uint8_t status;

  // Read Firwmare Versions of Expander via IPMB
  // ID: exp is 0, ioc is 1
  ret = exp_get_fw_ver(ver);
  if( !ret )
    printf("Expander Version: 0x%02x%02x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);
  else
    printf("Get Expander FW Verion Fail...\n");

  //Have to check HOST power, if is off, shows NA
  pal_get_server_power(FRU_SLOT1, &status);
  if (status != SERVER_POWER_ON) {
    printf("SCC IOC Version: NA (Server Power-off)\n");
  }
  else {
    ret = exp_get_ioc_fw_ver(ver);
    if( !ret )
      printf("SCC IOC Version: %x.%x.%x.%x\n", ver[3], ver[2], ver[1], ver[0]);
      else
      printf("Get SCC IOC FW Version Fail...\n");
  }

  return;
}

static void
print_fw_ioc_ver(void) {

  uint8_t ver[32] = {0};
  int ret = 0;
  uint8_t status;

  //Have to check HOST power, if is off, shows NA
  pal_get_server_power(FRU_SLOT1, &status);
  if (status != SERVER_POWER_ON) {
    printf("IOM IOC Version: NA (Server Power-off)\n");
  }
  else {
    // Read Firwmare Versions of IOM IOC viacd MCTP
    ret = pal_get_iom_ioc_ver(ver);
    if(!ret)
      printf("IOM IOC Version: %x.%x.%x.%x\n", ver[3], ver[2], ver[1], ver[0]);
    else
      printf("Get IOM IOC FW Version Fail...\n");
  }

  return;
}

static void
print_fw_bmc_ver(void) {
  FILE *fp = NULL;
  char str[32];

  fp = fopen("/etc/issue", "r");
  if (fp != NULL) {
    fseek(fp, 16, SEEK_SET);
    memset(str, 0, sizeof(char) * 32);
    if (!fscanf(fp, "%s", &str)) {
      strcpy(str, "NA");
    }
    fclose(fp);
    printf("BMC Version: %s\n", str);
  }

  return;
}

// TODO: Need to confirm the interpretation of firmware version for print
// Right now using decimal to print the versions
static void
print_fw_ver(uint8_t slot_id) {
  int i;
  uint8_t ver[32] = {0};
  uint8_t status = 0;

  //Check Fru Present
  if (pal_is_fru_prsnt(slot_id, &status)) {
    printf("pal_is_fru_prsnt failed for fru: %d\n", slot_id);
    return;
  }
  if (status == 0) {
    printf("Can't get Server FW version due to that Server is not present!\n");
    return;
  } 

  //Check Power Status
  if (pal_get_server_power(slot_id, &status)) {
    printf("Fail to get Server power status\n");
    return;
  }
  if(status == SERVER_12V_OFF) {
    printf("Can't get Server FW version due to that Server is 12V-off!\n");
    return;
  }

  // Print CPLD Version
  if (bic_get_fw_ver(slot_id, FW_CPLD, ver)) {
    return;
  }

  printf("CPLD Version: 0x%02x%02x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);

  // Print Bridge-IC Version
  if (bic_get_fw_ver(slot_id, FW_BIC, ver)) {
    return;
  }

  printf("Bridge-IC Version: v%x.%02x\n", ver[0], ver[1]);

  // Print Bridge-IC Bootloader Version
  if (bic_get_fw_ver(slot_id, FW_BIC_BOOTLOADER, ver)) {
    return;
  }

  printf("Bridge-IC Bootloader Version: v%x.%02x\n", ver[0], ver[1]);

  // Print ME Version
  if (bic_get_fw_ver(slot_id, FW_ME, ver)){
    return;
  }

  printf("ME Version: %x.%x.%x.%x%x\n", ver[0], ver[1], ver[2], ver[3], ver[4]);

  // Print PVCCIN VR Version
  if (bic_get_fw_ver(slot_id, FW_PVCCIN_VR, ver)){
    return;
  }

  printf("PVCCIN VR Version: 0x%02x%02x, 0x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);

  // Print DDRAB VR Version
  if (bic_get_fw_ver(slot_id, FW_DDRAB_VR, ver)){
    return;
  }

  printf("DDRAB VR Version: 0x%02x%02x, 0x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);

  // Print P1V05 VR Version
  if (bic_get_fw_ver(slot_id, FW_P1V05_VR, ver)){
    return;
  }

  printf("P1V05 VR Version: 0x%02x%02x, 0x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);

  // Print PVCCGBE VR Version
  if (bic_get_fw_ver(slot_id, FW_PVCCGBE_VR, ver)){
    return;
  }

  printf("PVCCGBE VR Version: 0x%02x%02x, 0x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);

  // Print PVCCSCSUS VR Version
  if (bic_get_fw_ver(slot_id, FW_PVCCSCSUS_VR, ver)){
    return;
  }

  printf("PVCCSCSUS VR Version: 0x%02x%02x, 0x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);

  // Print BIOS version
  if (pal_get_sysfw_ver(slot_id, ver)) {
    return;
  }

  // BIOS version response contains the length at offset 2 followed by ascii string
  printf("BIOS Version: ");
  for (i = 3; i < 3+ver[2]; i++) {
    printf("%c", ver[i]);
  }
    printf("\n");
}

int
fw_update_slot(char **argv, uint8_t slot_id) {

  uint8_t status;
  int ret;
  char cmd[80];

  ret = pal_is_fru_prsnt(slot_id, &status);
  if (ret < 0) {
     printf("pal_is_fru_prsnt failed for fru: %d\n", slot_id);
     goto err_exit;
  }
  if (status == 0) {
    printf("Server board is not present!\n");
    goto err_exit;
  }
  if (!strcmp(argv[3], "--cpld")) {
     return bic_update_fw(slot_id, UPDATE_CPLD, argv[4]);
  }
  if (!strcmp(argv[3], "--bios")) {
    pal_set_server_power(slot_id, SERVER_POWER_OFF);
    ret = bic_update_fw(slot_id, UPDATE_BIOS, argv[4]);
    pal_set_server_power(slot_id, SERVER_12V_CYCLE);
    sleep(3);
    pal_set_server_power(slot_id, SERVER_POWER_ON);
    return ret;
  }
  if (!strcmp(argv[3], "--bic")) {
    return bic_update_fw(slot_id, UPDATE_BIC, argv[4]);
  }
  if (!strcmp(argv[3], "--bicbl")) {
    return bic_update_fw(slot_id, UPDATE_BIC_BOOTLOADER, argv[4]);
  }

err_exit:
  print_usage_help();
  return -1;
}

int
get_mtd_name(const char* name, char* dev)
{
  FILE* partitions = fopen("/proc/mtd", "r");
  char line[256], mnt_name[32];
  unsigned int mtdno;
  int found = 0;

  dev[0] = '\0';
  while (fgets(line, sizeof(line), partitions)) {
    if(sscanf(line, "mtd%d: %*0x %*0x %s",
                 &mtdno, mnt_name) == 2) {
      if(!strcmp(name, mnt_name)) {
        sprintf(dev, "/dev/mtd%d", mtdno);
        found = 1;
        break;
      }
    }
  }
  fclose(partitions);
  return found;
}

void sig_handler(int signo) { 
  //Catch SIGINT, SIGTERM. If recived signal remove FW Update Flag File.
  //Kill flashcp process if there is any.
  char singal[10]={0};
  int retry = 3;
  bool is_flashcp_running = false;

  if (signo == SIGINT)
    sprintf(singal, "SIGINT");
  else if (signo == SIGTERM)
    sprintf(singal, "SIGTERM");
  else
    sprintf(singal, "UNKNOWN");


  if (signo == SIGINT || signo == SIGTERM) {

    printf("Got signal %d(%s). Preparinging to stop fw update.\n", signo, singal);
    syslog(LOG_WARNING, "Got signal %d(%s). Preparinging to stop fw update.\n", signo, singal);

    //If there is flashcp process running, then kill it
    do {
      if (!run_command("pidof flashcp  &> /dev/null")){ // flashcp is running
        is_flashcp_running = true;
        run_command("killall flashcp");
      }else // flashcp is not running
        break;

      retry--;
      sleep(1);
    } while (is_flashcp_running && (retry >= 0));

    if ((retry == 0) && is_flashcp_running){
      printf("Fail to kill flashcp...\n");
      syslog(LOG_WARNING, "Fail to kill flashcp...\n");
    }

    pal_set_fw_update_ongoing(FRU_SLOT1, 0);
    exit(0);
  }
}

int
main(int argc, char **argv) {
  uint8_t fru;
  int ret = -2;
  char cmd[80];
  char dev[12];
  uint8_t sku = 0;
  uint32_t wdt_fd;
  uint32_t wdt30;
  void *wdt_reg;
  void *wdt30_reg;
  char dual_flash_mtd_name[32];
  char single_flash_mtd_name[32];
  bool is_bmc_update_flag = false;

  //Catch signals
  signal(SIGINT, sig_handler);
  signal(SIGKILL, sig_handler);
  signal(SIGTERM, sig_handler);

  sku = pal_get_iom_type();

  // Check for border conditions
  if ((argc != 3) && (argc != 5)) {
    goto exit;
  }

  // Derive fru from first parameter
  if (!strcmp(argv[1], "server")) {
    fru = FRU_SLOT1;
  } else if (!strcmp(argv[1] , "iom")) {
    fru = FRU_IOM;
  }else if (!strcmp(argv[1] , "scc")) {
    fru = FRU_SCC;
  }else if (!strcmp(argv[1] , "all")) {
    fru = FRU_ALL;
  } else {
      goto exit;
  }

  // check operation to perform
  if (!strcmp(argv[2], "--version")) {
    switch(fru) {
      case FRU_SLOT1:
        print_fw_ver(FRU_SLOT1);
        break;

      case FRU_IOM:
        print_fw_bmc_ver();
        if (sku == IOM_IOC) {
          print_fw_ioc_ver();
        }
        break;

      case FRU_SCC:
        print_fw_scc_ver();
        break;

      case FRU_ALL:
        print_fw_ver(FRU_SLOT1);
        print_fw_bmc_ver();
        if (sku == IOM_IOC) {
          print_fw_ioc_ver();
        }
        print_fw_scc_ver();
        break;
    }
    return 0;
  }


  if (!strcmp(argv[2], "--update")) {
    if (argc != 5) {
      goto exit;
    }

    pal_set_fw_update_ongoing(FRU_SLOT1, 2000);

    if (fru == FRU_SLOT1) {
      ret =  fw_update_slot(argv, fru);
      goto exit;
    }

    // TODO: This is a workaround to detect the correct flash (flash0/flash1)
    // We need to remove this once the mtd partition is fixed in the kernel code
    else if (fru == FRU_IOM) {
      wdt_fd = open("/dev/mem", O_RDWR | O_SYNC );
      if (wdt_fd < 0) {
        ret = wdt_fd;
        goto exit;
      }
      wdt_reg = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, wdt_fd,
             AST_WDT_BASE);
      wdt30_reg = (char*)wdt_reg + WDT2_TIMEOUT_OFFSET;

      wdt30 = *(volatile uint32_t*) wdt30_reg;

      munmap(wdt_reg, PAGE_SIZE);
      close(wdt_fd);

      if (!strcmp(argv[3], "--rom")) {
        if (((wdt30 >> 1) & 0x1) == 0) {  // default boot: flash 0
          sprintf(dual_flash_mtd_name, "\"flash0\"");
        } else {                          // second boot: flash 1
          sprintf(dual_flash_mtd_name, "\"flash1\"");
        }
        if (!get_mtd_name(dual_flash_mtd_name, dev)) {
          printf("Error: Cannot find %s MTD partition in /proc/mtd\n", dual_flash_mtd_name);
          ret = -1;
          goto exit;
        }

        printf("Flashing to device: %s\n", dev);
        snprintf(cmd, sizeof(cmd), "flashcp -v %s %s", argv[4], dev);
        ret = run_command(cmd);
        if (ret == 0) {
          syslog(LOG_CRIT, "RO BMC firmware update successfully");
          printf("Updated RO BMC successfully, reboot BMC now.\n");
          is_bmc_update_flag = true;
        }
        goto exit;
      }
      else if (!strcmp(argv[3], "--bmc")) {
        if (((wdt30 >> 1) & 0x1) == 0) { 
          sprintf(dual_flash_mtd_name, "\"flash1\"");
          sprintf(single_flash_mtd_name, "\"flash0\"");
        } else {
          sprintf(dual_flash_mtd_name, "\"flash0\"");
          sprintf(single_flash_mtd_name, "\"flash1\"");
       }

        if (!get_mtd_name(dual_flash_mtd_name, dev)) {
          printf("Note: Cannot find %s MTD partition in /proc/mtd\n", dual_flash_mtd_name);
          if (!get_mtd_name(single_flash_mtd_name, dev)) {
            printf("Error: Cannot find %s MTD partition in /proc/mtd\n", single_flash_mtd_name);
            ret = -1;
            goto exit;
          }
        }

        printf("Flashing to device: %s\n", dev);
        snprintf(cmd, sizeof(cmd), "flashcp -v %s %s", argv[4], dev);
        ret = run_command(cmd);
        if (ret == 0) {
          syslog(LOG_CRIT, "RW BMC firmware update successfully");
          printf("Updated RW BMC successfully, reboot BMC now.\n");          
          is_bmc_update_flag = true;          
        }
        goto exit;
      }
    }
  }

exit:
  if (ret == -2)
    print_usage_help();
  else {
    //clear fw update timer
    pal_set_fw_update_ongoing(FRU_SLOT1, 0);

    if (is_bmc_update_flag) {
      //clear the data partition after doing firmware update
      run_command("rm -rf /mnt/data/*");
      sleep(5);
      run_command("reboot");
    }
  }
  return ret;
}
