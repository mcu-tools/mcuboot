#include "rollback_counter.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdalign.h>

/* Platform defined macro
    #define CY_RBC_ROLLBACK_COUNTERS_NUM - platform defined counters amount
    #define CY_RBC_CHECKSUM_LENGTH - platform defined CRC size
    #define CY_RBC_ALIGN - platform defined memory alignment
*/

#if (defined(CY_RBC_ROLLBACK_COUNTERS_NUM)) && (defined(CY_RBC_CHECKSUM_LENGTH)) && (defined(CY_RBC_ALIGN))


/* Should be aligned to memory row and be one row in size */
typedef struct
{
    uint32_t counters[CY_RBC_ROLLBACK_COUNTERS_NUM];
    uint32_t checkSum[CY_RBC_CHECKSUM_LENGTH / 4U];
} cy_rbc_t;

typedef union
{
	cy_rbc_t data;
	volatile uint8_t page[CY_RBC_ALIGN];
} cy_rbc_page_t;

#define CY_RBC_START_ADDR (0x3203FC00UL)

#define CY_RBC_MAIN     ((cy_rbc_t*)(CY_RBC_START_ADDR))
#define CY_RBC_BACKUP   ((cy_rbc_t*)(CY_RBC_START_ADDR + 0x200UL))

static bool cy_rbc_is_checksum_valid(const cy_rbc_t *counters);
static cy_rbc_result_t cy_rbc_erase_backup(void);
static cy_rbc_result_t cy_rbc_compare_backup(const cy_rbc_t *cntrs1, const cy_rbc_t *cntrs2);
static cy_rbc_result_t cy_rbc_apply_backup(void);

uint32_t cy_rbc_read_rollback_counter(uint8_t counter)
{
    uint32_t value = 0;

    if(counter < CY_RBC_ROLLBACK_COUNTERS_NUM)
    {
        value = CY_RBC_MAIN->counters[counter];
    }

    return value;
}

cy_rbc_result_t cy_rbc_update_rollback_counter(uint8_t counter, uint32_t value)
{
    uint32_t actValue;
    cy_rbc_result_t rc = CY_RBC_INVALID;
    cy_rbc_t tempCntrs = {0};

    if(counter < CY_RBC_ROLLBACK_COUNTERS_NUM)
    {
        actValue = CY_RBC_MAIN->counters[counter];
        if(value > actValue)
        {
            /* Get current counters values and update the requested counter */
            tempCntrs = *CY_RBC_MAIN;
            tempCntrs.counters[counter] = value;

            /* Calculate checksum for updated counters */
            rc = cy_rbc_calc_check_sum(tempCntrs.counters, sizeof(tempCntrs.counters), (uint8_t*)tempCntrs.checkSum);

            /* Program updated counters to the backup row */
            if(CY_RBC_SUCCESS == rc)
            {
                rc = CY_RBC_INVALID;
                rc = cy_rbc_program((uint8_t *)&tempCntrs, (uintptr_t)CY_RBC_BACKUP);
            }

            /* Copy the backup row to the main row and erase the backup */
            if(CY_RBC_SUCCESS == rc)
            {
                rc = cy_rbc_apply_backup();
            }
        }
        else if(value == actValue)
        {
            rc = CY_RBC_SUCCESS;
        }
        else
        {
            /* continue */
        }
    }

    return rc;
}

cy_rbc_result_t cy_rbc_clear(void)
{
    cy_rbc_result_t rc = CY_RBC_INVALID;

    rc = cy_rbc_erase((uintptr_t)CY_RBC_MAIN);

    if(rc == CY_RBC_SUCCESS)
    {
        rc = CY_RBC_INVALID;
        rc = cy_rbc_erase((uintptr_t)CY_RBC_BACKUP);
    }

    return rc;
}

cy_rbc_result_t cy_rbc_recover_rollback_counter(void)
{
    cy_rbc_result_t rc = CY_RBC_INVALID;

    if(cy_rbc_is_erased((const uint8_t*)CY_RBC_BACKUP))
    {
        if(cy_rbc_is_checksum_valid(CY_RBC_MAIN))
        {
            rc = CY_RBC_SUCCESS;
        }
    }
    else
    {
        if(cy_rbc_is_checksum_valid(CY_RBC_BACKUP))
        {
            if(cy_rbc_is_checksum_valid(CY_RBC_MAIN))
            {
                if(cy_rbc_compare_backup(CY_RBC_BACKUP, CY_RBC_MAIN) == CY_RBC_SUCCESS)
                {
                    rc = cy_rbc_apply_backup();
                }
                else
                {
                    if(cy_rbc_erase_backup() == CY_RBC_SUCCESS)
                    {
                        if(cy_rbc_is_checksum_valid(CY_RBC_MAIN))
                        {
                            rc = CY_RBC_SUCCESS;
                        }
                    }
                }
            }
            else
            {
                rc = cy_rbc_apply_backup();
            }
        }
        else
        {
            if(cy_rbc_erase_backup() == CY_RBC_SUCCESS)
            {
                if(cy_rbc_is_checksum_valid(CY_RBC_MAIN))
                {
                    rc = CY_RBC_SUCCESS;
                }
            }
        }
    }

    return rc;
}

/*******************************************************************************
* Function Name: cy_rbc_is_checksum_valid
********************************************************************************
* Checks if checksum for the rollback protection counters storage matches.
*
* \param counters   Pointer to the rollback protection counters storage.
*
* \return           true if the checksum matches, false otherwise.
*******************************************************************************/
static bool cy_rbc_is_checksum_valid(const cy_rbc_t *counters)
{
    bool valid = false;

    uint8_t checkSum[CY_RBC_CHECKSUM_LENGTH] = {0U};

    if(cy_rbc_calc_check_sum(counters->counters, sizeof(counters->counters), checkSum) == CY_RBC_SUCCESS)
    {
        volatile int32_t cmpResult = -1;

        cmpResult = memcmp(checkSum, counters->checkSum, CY_RBC_CHECKSUM_LENGTH);

        if(cmpResult == 0)
        {
            valid = true;
        }
    }

    return valid;
}

/*******************************************************************************
* Function Name: cy_rbc_erase_backup
********************************************************************************
* Erases backup storage of rollback protection counters.
*
* \return     CY_RBC_SUCCESS if successful.
*******************************************************************************/
static cy_rbc_result_t cy_rbc_erase_backup(void)
{
    cy_rbc_result_t rc = CY_RBC_INVALID;

    rc = cy_rbc_erase((uint32_t)CY_RBC_BACKUP);

    return rc;
}

/*******************************************************************************
* Function Name: cy_rbc_compare_backup
********************************************************************************
* Compares two rollback protection counters storages.
*
* \param cntrs1   Pointer to the first rollback protection counters storage.
* \param cntrs1   Pointer to the second rollback protection counters storage.
*
* \return         CY_RBC_SUCCESS if at least one counter in the first storage is greater,
*                 CY_RBC_INVALID otherwise.
*******************************************************************************/
static cy_rbc_result_t cy_rbc_compare_backup(const cy_rbc_t *cntrs1, const cy_rbc_t *cntrs2)
{
    cy_rbc_result_t rc = CY_RBC_INVALID;
    uint32_t i;

    for(i = 0UL; i < CY_RBC_ROLLBACK_COUNTERS_NUM; i++)
    {
        if(cntrs1->counters[i] > cntrs2->counters[i])
        {
            rc = CY_RBC_SUCCESS;
            break;
        }
    }

    return rc;
}

/*******************************************************************************
* Function Name: cy_rbc_apply_backup
********************************************************************************
* Copies backup rollback protection counters storage to the main one.
*
* \return     CY_RBC_SUCCESS if successful.
*******************************************************************************/
static cy_rbc_result_t cy_rbc_apply_backup(void)
{
    cy_rbc_result_t rc = CY_RBC_INVALID;
    const cy_rbc_t *tempCntrs = CY_RBC_BACKUP;

    if(cy_rbc_program((const uint8_t *)tempCntrs, (uintptr_t)CY_RBC_MAIN) == CY_RBC_SUCCESS)
    {
        rc = cy_rbc_erase_backup();
    }

    return rc;
}

#endif