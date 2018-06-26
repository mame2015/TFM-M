/*
 * Copyright (c) 2017-2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include "region_defs.h"
#include "tfm_core.h"
#include "tfm_internal.h"
#include "target_cfg.h"
#include "uart_stdout.h"
#include "secure_utilities.h"
#include "secure_fw/spm/spm_api.h"
#ifdef TFM_PSA_API
#include "psa_client.h"
#include "psa_service.h"
#include "tfm_thread.h"
#include "tfm_queue.h"
#endif

/*
 * Avoids the semihosting issue
 * FixMe: describe 'semihosting issue'
 */
#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
__asm("  .global __ARM_use_no_argv\n");
#endif

#ifndef TFM_LVL
#error TFM_LVL is not defined!
#endif
#if (TFM_LVL != 1) && (TFM_LVL != 3)
#error Only TFM_LVL 1 and 3 are supported!
#endif

/* Macros to pick linker symbols and allow to form the partition data base */
#define REGION(a, b, c) a##b##c
#define REGION_NAME(a, b, c) REGION(a, b, c)
#define REGION_DECLARE(a, b, c) extern uint32_t REGION_NAME(a, b, c)

REGION_DECLARE(Image$$, TFM_UNPRIV_SCRATCH, $$ZI$$Base);
REGION_DECLARE(Image$$, TFM_UNPRIV_SCRATCH, $$ZI$$Limit);

#define SPNIDEN_SEL_STATUS (0x01u << 7)
#define SPNIDEN_STATUS     (0x01u << 6)
#define SPIDEN_SEL_STATUS  (0x01u << 5)
#define SPIDEN_STATUS      (0x01u << 4)
#define NIDEN_SEL_STATUS   (0x01u << 3)
#define NIDEN_STATUS       (0x01u << 2)
#define DBGEN_SEL_STATUS   (0x01u << 1)
#define DBGEN_STATUS       (0x01u << 0)

#define All_SEL_STATUS (SPNIDEN_SEL_STATUS | SPIDEN_SEL_STATUS | \
                        NIDEN_SEL_STATUS | DBGEN_SEL_STATUS)

void configure_debug_registers(void)
{
    volatile struct sysctrl_t *sys_ctrl =
        (struct sysctrl_t *)CMSDK_SYSCTRL_BASE_S;

#if defined(DAUTH_NONE)
    /* Set all the debug enable selector bits to 1 */
    sys_ctrl->secdbgset = All_SEL_STATUS;
    /* Set all the debug enable bits to 0 */
    sys_ctrl->secdbgclr =
        DBGEN_STATUS | NIDEN_STATUS | SPIDEN_STATUS | SPNIDEN_STATUS;
#elif defined(DAUTH_NS_ONLY)
    /* Set all the debug enable selector bits to 1 */
    sys_ctrl->secdbgset = All_SEL_STATUS;
    /* Set the debug enable bits to 1 for NS, and 0 for S mode */
    sys_ctrl->secdbgset = DBGEN_STATUS | NIDEN_STATUS;
    sys_ctrl->secdbgclr = SPIDEN_STATUS | SPNIDEN_STATUS;
#elif defined(DAUTH_FULL)
    /* Set all the debug enable selector bits to 1 */
    sys_ctrl->secdbgset = All_SEL_STATUS;
    /* Set all the debug enable bits to 1 */
    sys_ctrl->secdbgset =
        DBGEN_STATUS | NIDEN_STATUS | SPIDEN_STATUS | SPNIDEN_STATUS;
#else
#if !defined(DAUTH_CHIP_DEFAULT)
#error "No debug authentication setting is provided."
#endif
    /* Set all the debug enable selector bits to 0 */
    sys_ctrl->secdbgclr = All_SEL_STATUS;
    /* No need to set any enable bits because the value depends on
     *input signals
     */
#endif
}

void configure_ns_code(void)
{
    /* SCB_NS.VTOR points to the Non-secure vector table base address */
    SCB_NS->VTOR = (NS_CODE_START);

    /* Setups Main stack pointer of the non-secure code */
    uint32_t ns_msp = *((uint32_t *)(NS_CODE_START));

    __TZ_set_MSP_NS(ns_msp);

    /* The entry contains address of the Reset_handler (CMSIS-CORE) function */
    uint32_t entry_ptr = *((uint32_t *)(NS_CODE_START + 4));

    /* Clears LSB of the function address to indicate the function-call
     * will perform the switch from secure to non-secure
     */
    ns_entry = (nsfptr_t) (entry_ptr & (~0x1));
}

int32_t tfm_core_init(void)
{
    /* Enables fault handlers */
    enable_fault_handlers();

    /* Configure the debug configuration registers */
    configure_debug_registers();

    __enable_irq();

    uart_init(UART0_CHANNEL);
    LOG_MSG("Secure image initializing!");

#ifdef TFM_CORE_DEBUG
    printf("TFM level is: %d\r\n", TFM_LVL);
#endif

    /* Configures non-secure memory spaces in the target */
    sau_and_idau_cfg();
    mpc_init_cfg();
    ppc_init_cfg();
    configure_ns_code();

    /* Configures all interrupts to retarget NS state, except for
     * secure peripherals
     */
    nvic_interrupt_target_state_cfg();
    /* Enable secure peripherals interrupts */
    nvic_interrupt_enable();

    tfm_scratch_area =
        (uint8_t *)&REGION_NAME(Image$$, TFM_UNPRIV_SCRATCH, $$ZI$$Base);
    tfm_scratch_area_size =
        (uint32_t)&REGION_NAME(Image$$, TFM_UNPRIV_SCRATCH, $$ZI$$Limit) -
        (uint32_t)&REGION_NAME(Image$$, TFM_UNPRIV_SCRATCH, $$ZI$$Base);
    return 0;
}

int main(void)
{
    tfm_core_init();

    tfm_spm_db_init();
#if TFM_LVL != 1
    if (tfm_spm_mpu_init() != SPM_ERR_OK) {
        ERROR_MSG("Failed to set up initial MPU configuration! Halting.");
        while (1) {
            ;
        }
    }
#endif
    tfm_spm_partition_set_state(TFM_SP_CORE_ID, SPM_PARTITION_STATE_RUNNING);

    extern uint32_t Stack_Mem[];

    __set_PSPLIM((uint32_t)Stack_Mem);

    if (tfm_spm_partition_init() != SPM_ERR_OK) {
        /* Certain systems might refuse to boot altogether if partitions fail
         * to initialize. This is a placeholder for such an error handler
         */
    }

#ifdef TFM_CORE_DEBUG
    /* Jumps to non-secure code */
    LOG_MSG("Jumping to non-secure code...");
#endif

#ifdef TFM_PSA_API
    tfm_queue_init();
    tfm_thread_init();
#endif

    /* We close the TFM_SP_CORE_ID partition, because its only purpose is
     * to be able to pass the state checks for the tests started from secure.
     */
    tfm_spm_partition_set_state(TFM_SP_CORE_ID, SPM_PARTITION_STATE_CLOSED);
    tfm_spm_partition_set_state(TFM_SP_NON_SECURE_ID,
                              SPM_PARTITION_STATE_RUNNING);

    jump_to_ns_code();
}
