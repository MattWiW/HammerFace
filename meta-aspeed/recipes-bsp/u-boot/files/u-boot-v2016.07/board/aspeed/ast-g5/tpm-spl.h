/*
 * (C) Copyright 2016-Present, Facebook, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>

#include <tpm.h>

#include <asm/arch/vbs.h>

/* Define a TPM NVram index probe for provision tests. */
#define AST_TPM_PROBE_INDEX    0x101

/* Years the next upgrade must occur, protect against timestamp DoS */
#define AST_TPM_MAX_YEARS 1

enum ast_tpm_rollback_type {
  AST_TPM_ROLLBACK_SUBORDINATE = 0,
  AST_TPM_ROLLBACK_UBOOT = 1,
  AST_TPM_ROLLBACK_KERNEL = 2,
};

struct tpm_rollback_t {
  uint32_t subordinate;
  uint32_t uboot;
  uint32_t kernel;
  uint32_t fallback_subordinate;
  uint32_t fallback_uboot;
  uint32_t fallback_kernel;
};

enum ast_tpm_pcrs {
  AST_TPM_PCR_SPL = 0,
  AST_TPM_PCR_FIT = 1,
  AST_TPM_PCR_UBOOT = 2,
  AST_TPM_PCR_ENV = 3,
};


/**
 * int ast_tpm_provision() - Perform a 1-time provision of the TPM.
 *
 * @return a status for vboot_status
 */
int ast_tpm_provision(struct vbs *vbs);

/**
 * int ast_tpm_nv_provision() - Preform a 1-time NV provision.
 *
 * @return a status for vboot_status
 */
int ast_tpm_nv_provision(struct vbs *vbs);

/**
 * int ast_tpm_owner_provision() - Preform a 1-time owner provision.
 *
 * @return a status for vboot_status
 */
int ast_tpm_owner_provision(struct vbs *vbs);

/**
 * int ast_tpm_extend() - Hash and extend into a PCR
 *
 * @return a status for tpm_status
 */
int ast_tpm_extend(uint32_t index, unsigned char* data, uint32_t data_len);

/**
 * int ast_tpm_try_version() - Try to verify a version is not rolling back.
 *
 * @param vbs the vboot structure.
 * @param image set this to either ROLLBACK_UBOOT or ROLLBACK_KERNEL (see above)
 * @param version this is the version you are attempting to boot.
 * @param no_fallback set to any value greater than 0 will not allow a fallback.
 * @return a status for vboot_status
 */
int ast_tpm_try_version(struct vbs *vbs, uint8_t image, uint32_t version,
                        bool no_fallback);

/**
 * int ast_tpm_finish() - Lock the physical presence.
 *
 * @return a status for vboot_status
 */
int ast_tpm_finish(void);
