/*******************************************************************************
* \file cy_si_config.h
* \version 1.00
*
* \brief
* Definitions and function prototypes for Secure Image.
*
********************************************************************************
* \copyright
* Copyright 2023, Cypress Semiconductor Corporation. All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#ifndef CY_SI_CONFIG_H
#define CY_SI_CONFIG_H

#if defined(__cplusplus)
extern "C" {
#endif


/***************************************
*               Macros
***************************************/


/***************************************
*               TOC2
***************************************/
/** \addtogroup group_secure_image_macro
* \{
*/

/** \defgroup group_secure_image_flashboot_clock_macros Flash Boot clock selection 
* Clock selection for Flash boot execution.
* \{
*/
#define CY_SI_FLASHBOOT_CLK_8MHZ            (0UL)            /**< 8MHz clock selection for Flashboot */
#define CY_SI_FLASHBOOT_CLK_25MHZ           (1UL)            /**< 25MHz clock selection for Flashboot */
#define CY_SI_FLASHBOOT_CLK_50MHZ           (2UL)            /**< 50MHz clock selection for Flashboot */
#define CY_SI_FLASHBOOT_CLK_100MHZ          (3UL)            /**< USER Configuration */
/** \} group_secure_image_flashboot_clock_macros */

/** \defgroup group_secure_image_flashboot_wait_macros Flash Boot wait window 
* Debugger wait window selection for Flash boot execution.
* \{
*/
#define CY_SI_FLASHBOOT_WAIT_20MS           (0UL)            /**< 20ms debugger wait window for Flashboot */
#define CY_SI_FLASHBOOT_WAIT_10MS           (1UL)            /**< 10ms debugger wait window for Flashboot */
#define CY_SI_FLASHBOOT_WAIT_1MS            (2UL)            /**< 1ms debugger wait window for Flashboot */
#define CY_SI_FLASHBOOT_WAIT_0MS            (3UL)            /**< 0ms debugger wait window for Flashboot */
#define CY_SI_FLASHBOOT_WAIT_100MS          (4UL)            /**< 100ms debugger wait window for Flashboot */
/** \} group_secure_image_flashboot_wait_macros */

/** \defgroup group_secure_image_debug_pin_configuration
* Debugger pin configuration.
* \{
*/
#define CY_SI_FLASHBOOT_SWJ_DISABLE         (1UL)            /**< Do not enable SWJ pins in Flash boot. Listen window is skipped */
#define CY_SI_FLASHBOOT_SWJ_ENABLE          (2UL)            /**< Enable SWJ pins in Flash boot  */
/** \} group_secure_image_debug_pin_configuration */

/** \defgroup group_secure_image_flashboot_validate_macros Flash Boot validation 
* Flash boot validation selection in chip NORMAL mode.
* \{
*/
#define CY_SI_FLASHBOOT_VALIDATE_DISABLE    (1UL)            /**< Do not validate app1 in NORMAL mode */
#define CY_SI_FLASHBOOT_VALIDATE_ENABLE     (2UL)            /**< Validate app1 in NORMAL mode */
/** \} group_secure_image_flashboot_validate_macros */

/** \defgroup group_secure_image_FLASH_LOADER_loader_configuration
* Flash boot loader configuration.
* \{
*/
#define CY_SI_FLASHBOOT_FBLOADER_ENABLE     (1UL)            /**< Internal bootloader is launched if the other bootloader conditions are met  */
#define CY_SI_FLASHBOOT_FBLOADER_DISABLE    (2UL)            /**< Internal bootloader is disabled */
/** \} group_secure_image_FLASH_LOADER_loader_configuration */

/** \defgroup group_secure_image_app_format_macros Application format
* Application format selection for secure boot.
* \{
*/
#define CY_SI_APP_FORMAT_BASIC              (0UL)               /**< Basic application format (no header) */
#define CY_SI_APP_FORMAT_CYPRESS            (1UL)               /**< Cypress application format (Cypress header) */
#define CY_SI_APP_FORMAT_SIMPLIFIED         (2UL)               /**< Simplified application format (no header) */
/** \} group_secure_image_app_format_macros */

/** \defgroup group_security_enhancement_marker_macros Security enhancement configuration
* Security enhancement configuration for secure boot.
* \{
*/
#define CY_SECURITY_NOT_ENHANCED            (0x00000000)    /**< No Security Enhanced */
#define CY_SECURITY_ENHANCED                (0xFEDEEDDF)    /**< Security Enhanced */
/** \} group_security_enhancement_marker_macros */

/** \defgroup group_secure_image_address_macros Application Addresses 
* Secure Image and User application addresses. These define the addresses used by both the secure image and
* secure boot flow and must match those defined in the linker scripts.
* \{
*/
#define CY_SI_SECURE_FLASH_BEGIN            (0x10000000UL)  /**< Secure Image begin Flash address Configure depends on System.*/
#define CY_SI_USERAPP_FLASH_BEGIN           (0UL)           /**< Non second application image */
#define CY_SI_SECOND_APP_FLASH_BEGIN        (0x10010000UL)  /**< Second application image begin Flash address Configure depends on System.*/
/** \} group_secure_image_address_macros */

/** \} group_secure_image_macro */


/***************************************
*         Access Restriction
***************************************/
/** \addtogroup group_normal_access_restriction_macro
* \{
*/

/** \defgroup group_CM0+_acc_restriction_macros Normal Access restriction
* Normal Access restriction for CM0+ access port.
* \{
*/
#define CY_SI_CM0_ENABLE                (0UL)  /**< CM0 ACCESS PORT ENABLE */
#define CY_SI_CM0_DISABLE_TMP           (1UL)  /**< CM0 ACCESS PORT TEMPORARY DISABLE */
#define CY_SI_CM0_DISABLE               (2UL)  /**< CM0 ACCESS PORT PERMANENTLY_DISABLE */
/** \} group_CM0+_acc_restriction_macros */

/** \defgroup group_CM7_acc_restriction_macros Normal Access restriction
* Normal Access restriction for CM7 access port.
* \{
*/
#define CY_SI_CM7_ENABLE                (0UL)  /**< CM7 ACCESS PORT ENABLE */
#define CY_SI_CM7_DISABLE_TMP           (1UL)  /**< CM7 ACCESS PORT TEMPORARY DISABLE */
#define CY_SI_CM7_DISABLE               (2UL)  /**< CM7 ACCESS PORT PERMANENTLY_DISABLE */
/** \} group_CM7_acc_restriction_macros */

/** \defgroup group_SYS_acc_restriction_macros Normal Access restriction
* Normal Access restriction for System access port.
* \{
*/
#define CY_SI_SYS_ENABLE                (0UL)  /**< SYS ACCESS PORT ENABLE */
#define CY_SI_SYS_DISABLE_TMP           (1UL)  /**< SYS ACCESS PORT TEMPORARY DISABLE */
#define CY_SI_SYS_DISABLE               (2UL)  /**< SYS ACCESS PORT PERMANENTLY_DISABLE */
/** \} group_SYS_acc_restriction_macros */

/** \defgroup group_MPU_enable_macros System access port MPU enable
* System access port MPU enable.
* \{
*/
#define CY_SI_MPU_DISABLE               (0UL)  /**< MPU disable. Not configure MPU by boot process */
#define CY_SI_MPU_ENABLE                (1UL)  /**< MPU disable. Configure MPU by boot process */
/** \} group_MPU_enable_macros */

/** \defgroup group_MPU_direct_exe_macros System access port MPU direct execute configuration
* System access port MPU direct execute configuration.
* \{
*/
#define CY_SI_DIRECT_EXE_DISABLE        (1UL)  /**< Disable Direct Execute system call functionality */
#define CY_SI_DIRECT_EXE_ENABLE         (0UL)  /**< Enable Direct Execute system call functionality */
/** \} group_MPU_direct_exe_macros */

/** \defgroup group_MPU_Flash_acc_macros SYS access port MPU flash access configuration
* SYS access port MPU flash access configuration.
* \{
*/
#define CY_SI_FLASH_ENABLE              (0UL)  /**< FLASH accessible :Entire region */
#define CY_SI_FLASH_ENABLE_7_8          (1UL)  /**< FLASH accessible :7/8 */
#define CY_SI_FLASH_ENABLE_3_4          (2UL)  /**< FLASH accessible :3/4 */
#define CY_SI_FLASH_ENABLE_HALF         (3UL)  /**< FLASH accessible :HALF */
#define CY_SI_FLASH_ENABLE_1_4          (4UL)  /**< FLASH accessible :1/4 */
#define CY_SI_FLASH_ENABLE_1_8          (5UL)  /**< FLASH accessible :1/8 */
#define CY_SI_FLASH_ENABLE_1_16         (6UL)  /**< FLASH accessible :1/16 */
#define CY_SI_FLASH_DISABLE             (7UL)  /**< FLASH accessible :NOTHING */
/** \} group_MPU_Flash_acc_macros */

/** \defgroup group_MPU_RAM0_acc_macros SYS access port MPU RAM0 access configuration
* SYS access port MPU RAM0 access configuration.
* \{
*/
#define CY_SI_RAM0_ENABLE               (0UL)  /**< RAM0 accessible :Entire region */
#define CY_SI_RAM0_ENABLE_7_8           (1UL)  /**< RAM0 accessible :7/8 */
#define CY_SI_RAM0_ENABLE_3_4           (2UL)  /**< RAM0 accessible :3/4 */
#define CY_SI_RAM0_ENABLE_HAFL          (3UL)  /**< RAM0 accessible :HALF */
#define CY_SI_RAM0_ENABLE_1_4           (4UL)  /**< RAM0 accessible :1/4 */
#define CY_SI_RAM0_ENABLE_1_8           (5UL)  /**< RAM0 accessible :1/8 */
#define CY_SI_RAM0_ENABLE_1_16          (6UL)  /**< RAM0 accessible :1/16 */
#define CY_SI_RAM0_DISABLE              (7UL)  /**< RAM0 accessible :DISABLE */
/** \} group_MPU_RAM0_acc_macros */

/** \defgroup group_MPU_WFlash_acc_macros SYS access port MPU work flash access configuration
* SYS access port MPU work flash access configuration.
* \{
*/
#define CY_SI_WORK_FLASH_ENABLE         (0UL)  /**< WORK FLASH accessible :Entire region */
#define CY_SI_WORK_FLASH_ENABLE_HALF    (1UL)  /**< WORK FLASH accessible :HALF */
#define CY_SI_WORK_FLASH_ENABLE_1_4     (2UL)  /**< WORK FLASH accessible :1/4 */
#define CY_SI_WORK_FLASH_DISABLE        (3UL)  /**< WORK FLASH accessible :NOTHING */
/** \} group_MPU_WFlash_acc_macros */

/** \defgroup group_MPU_SFlash_acc_macros SYS access port MPU Sflash access configuration
* SYS access port MPU Sflash access configuration.
* \{
*/
#define CY_SI_SFLASH_ENABLE             (0UL)  /**< SFLASH accessible :Entire region */
#define CY_SI_SFLASH_ENABLE_HAFL        (1UL)  /**< SFLASH accessible :HALF */
#define CY_SI_SFLASH_ENABLE_1_4         (2UL)  /**< SFLASH accessible :1/4 */
#define CY_SI_SFLASH_DISABLE            (3UL)  /**< SFLASH accessible :NOTHING */
/** \} group_MPU_SFlash_acc_macros */

/** \defgroup group_MPU_MMIO_acc_macros SYS access port MPU MMIO access configuration
* SYS access port MPU MMIO access configuration.
* \{
*/
#define CY_SI_MMIO_ENABLE               (0UL)  /**< MMIO accessible :All MMIO register */
#define CY_SI_MMIO_ENABLE_IPC           (1UL)  /**< Only IPC MMIO registers accessible(system calls) */
#define CY_SI_MMIO_DISABLE              (2UL)  /**< No MMIO access */
/** \} group_MPU_MMIO_acc_macros */

/** \defgroup group_MPU_SMIF_acc_macros SYS access port MPU SMIF access configuration
* SYS access port MPU SMIF access configuration.
* \{
*/
#define CY_SI_SMIF_XIP_ENABLE           (0UL)  /**< SMIF XIP Enable */
#define CY_SI_SMIF_XIP_DISABLE          (1UL)  /**< SMIF XIP Disable */
/** \} group_MPU_SMIF_acc_macros */

/** \} group_normal_access_restriction_macro */


/***************************************
*    Application Protection
***************************************/
/** \addtogroup group_application_protection_macro
* \{
*/

/** \defgroup group_SWPU_number_macros Number of SWPU configuration
* Number of SWPU configuration.
* \{
*/
#define N_FWPU                (0UL)  /**< Number of flash write protection Max 16 Configure depends on System.*/
#define N_ERPU                (1UL)  /**< Number of efuse read  protection Max  4 Configure depends on System.*/
#define N_EWPU                (1UL)  /**< Number of efuse write protection Max  4 Configure depends on System.*/
/** \} group_SWPU_number_macros */

/** \defgroup group_SWPU_enable_attribute_macros SWPU enable and attribute configuration
* SWPU enable and attribute configuration.
* \{
*/
#define APP_PROT_ENABLE       (1UL)  /**< Application Protection Enable */
#define APP_PROT_DISABLE      (0UL)  /**< Application Protection Disable */
#define APP_PROT_ALLOW        (1UL)  /**< Access Allow */
#define APP_PROT_PROHIBIT     (0UL)  /**< Access Prohibit */
/** \} group_SWPU_enable_attribute_macros */

/** \} group_application_protection_macro */


/***************************************
*    Application Header
***************************************/
/** \addtogroup group_application_header_macro
* \{
*/

/** \defgroup group_Secure_Image_virsion_macros Secure Image version configuration
* Secure Image version configuration.
* \{
*/
#define CY_SI_VERSION_MAJOR             (0UL)       /**< Major version Configure depends on System.*/
#define CY_SI_VERSION_MINOR             (1UL)       /**< Minor version Configure depends on System.*/
/** \} group_Secure_Image_virsion_macros */

/** \defgroup group_secure_image_app_type_macros Application type
* Application type selection for secure boot.
* \{
*/
#define CY_SI_APP_ID_FLASHBOOT          (0x8001UL)  /**< Flash boot ID Type */
#define CY_SI_APP_ID_SECUREIMG          (0x8002UL)  //(0x0UL)     /**< Secure image ID Type */
#define CY_SI_APP_ID_BOOTLOADER         (0x8003UL)  /**< Bootloader ID Type */
/** \} group_secure_image_app_type_macros */

/** \} group_application_header_macro */


/***************************************
*    Application Authentication
***************************************/
/** \addtogroup group_application_authentication_macro
* \{
*/

/** \defgroup group_applicagtion_image_macros Application image address configuration
* Application image address configuration for authentication.
* \{
*/
#define CY_SI_SECURE_FLASH_BEGIN_CM7    (0x10020000UL)  /**< Secure Image begin Flash address. Configure depends on System. */
#define CY_M7_SI_SIZE                   (0x0000FE00UL)  /**< Authentication Size Configure depends on System. */
#define CY_SI_SIGNATURE_ADDR            (CY_SI_SECURE_FLASH_BEGIN_CM7 + CY_M7_SI_SIZE)  /**< Signature address Configure depends on System. */
/** \} group_applicagtion_image_macros */

/** \} group_application_authentication_macro */


/***************************************
*            Constants
***************************************/
/** \cond INTERNAL */

/* TOC2 */
#define CY_SI_TOC_FLAGS_CLOCKS_POS          (0UL)           /**< Bit position of Flashboot clock selection */
#define CY_SI_TOC_FLAGS_DELAY_POS           (2UL)           /**< Bit position of Flashboot wait window selection */
#define CY_SI_TOC_FLAGS_SWJEN_POS           (5UL)           /**< Bit position of SWJ pin configuration */
#define CY_SI_TOC_FLAGS_APP_VERIFY_POS      (7UL)           /**< Bit position of Flashboot NORMAL mode app1 validation */
#define CY_SI_TOC_FLAGS_FBLOADER_ENABLE_POS (9UL)           /**< Bit position of Flashboot Loader Enable */

#define CY_SI_TOC2_OBJECTSIZE               (0x000001FCUL)  /**< Number of TOC2 object */
#define CY_SI_TOC2_MAGICNUMBER              (0x01211220UL)  /**< TOC2 identifier */
#define CY_SI_CM71_1stAPP_FLASH_BEGIN       (0UL)           /**< Address of CM7_1 First User Application Object */
#define CY_SI_CM71_2ndAPP_FLASH_BEGIN       (0UL)           /**< Address of CM7_1 Second User Application Object */
#define CY_SI_CM72_1stAPP_FLASH_BEGIN       (0UL)           /**< Address of CM7_2 First User Application Object */
#define CY_SI_CM72_2ndAPP_FLASH_BEGIN       (0UL)           /**< Address of CM7_2 Second User Application Object */
#define CY_SI_PUBLIC_KEY                    (0x17006400UL)  /**< PUBLIC KEY address in SFlash */
#define CY_SI_SWPU_BEGIN                    (0x17007600UL)  /**< Address of SWPU configuration */
#define CY_SI_SECURE_DIGSIG_SIZE            (512u)          /**< Size (in Bytes) of the digital signature for RSA-4K*/

/* Access Restriction */
#define CY_SI_CM0_AP_POS                    (0UL)  /**< Bit position of CM0 ACCESS PORT*/
#define CY_SI_CM7_AP_POS                    (2UL)  /**< Bit position of CM7 ACCESS PORT*/
#define CY_SI_SYS_AP_POS                    (4UL)  /**< Bit position of SYS ACCESS PORT*/
#define CY_SI_AP_MPU_POS                    (6UL)  /**< Bit position of MPU ACCESS PORT */
#define CY_SI_DIRECT_EXECUTE_POS            (7UL)  /**< Bit position of DIRECT EXE */
#define CY_SI_FLASH_POS                     (8UL)  /**< Bit position of FLASH ACCESS */
#define CY_SI_RAM0_POS                      (11UL) /**< Bit position of RAM0 ACCESS */
#define CY_SI_WORK_FLASH_POS                (14UL) /**< Bit position of WORK FLASH ACCESS */
#define CY_SI_SFLASH_POS                    (16UL) /**< Bit position of SFLASH ACCESS */
#define CY_SI_MMIO_POS                      (18UL) /**< Bit position of MMIO ACCESS */
#define CY_SI_SMIF_XIP_POS                  (20UL) /**< Bit position of SMIF XIP ACCESS */

/* Application Header */
#define CY_SI_APP_VERSION     ((CY_SI_VERSION_MAJOR << 24u) | (CY_SI_VERSION_MINOR << 16u)) /**< App Version */

/* Application Protection */
#define OBJECT_SIZE           (4UL * 4u + N_FWPU *16u + N_ERPU *16u + N_EWPU *16u)  /**< Number of Object Size (bytes) */

/* Application Authentication */
#define CY_FB_PBKEY_STRUCT_OFFSET  (8u)  /**< Public key offset */

/* Flash boot functions */
#define CY_SI_IMGVAL_VERIFYAPP_ADDR     ((volatile uint32_t *)0x17002040UL)         /**< Flash boot verify app function address */
#define CY_SI_IMGVAL_VERIFYAPP_REG      (*(uint32_t *)CY_SI_IMGVAL_VERIFYAPP_ADDR)  /**< Flash boot verify app function register */
#define CY_SI_IMGVAL_VALIDKEY_ADDR      ((volatile uint32_t *)0x17002044UL)         /**< Flash boot validate key function address */
#define CY_SI_IMGVAL_VALIDKEY_REG       (*(uint32_t *)CY_SI_IMGVAL_VALIDKEY_ADDR)   /**< Flash boot validate key function register */


/***************************************
*     SFlash function typedefs
***************************************/
typedef uint32_t (*sflash_verifyapp_func_t)(uint32_t param0, uint32_t param1, uint32_t param2,
                                 cy_stc_crypto_rsa_pub_key_t *param3);
typedef uint32_t (*sflash_validkey_func_t)(uint32_t param0, cy_stc_crypto_rsa_pub_key_t *param1);


/***************************************
*               Structs
***************************************/

/**
  * \brief TOC2 Structure
  */
typedef struct{
    volatile uint32_t objSize;          /**< Object size (Bytes) */
    volatile uint32_t magicNum;         /**< TOC ID (magic number) */
    volatile uint32_t smifCfgAddr;      /**< SMIF configuration structure */
    volatile uint32_t cm0pappAddr1;     /**< First user application object address */
    volatile uint32_t cm0pappFormat1;   /**< First user application format */
    volatile uint32_t cm0pappAddr2;     /**< Second user application object address */
    volatile uint32_t cm0pappFormat2;   /**< Second user application format */
    volatile uint32_t cm71appAddr1;     /**< Second user application format */
    volatile uint32_t cm71appAddr2;     /**< Second user application format */
    volatile uint32_t cm72appAddr1;     /**< Second user application format */
    volatile uint32_t cm72appAddr2;     /**< Second user application format */
    volatile uint32_t reserved1[52];    /**< Second user application format */
    volatile uint32_t securityMarker;   /**< Security Update Marker */
    volatile uint32_t shashObj;         /**< Number of additional objects to be verified (S-HASH) */
    volatile uint32_t sigKeyAddr;       /**< Signature verification key address */
    volatile uint32_t swpuAddr;         /**< Address of SWPU object */
    volatile uint32_t toc2Addr;         /**< Address of TOC2 */
    volatile uint32_t addObj[58];       /**< Additional objects to include in S-HASH */
    volatile uint32_t tocFlags;         /**< Flags in TOC to control Flash boot options */
    volatile uint32_t crc;              /**< Reserved */
}cy_stc_si_toc_t;

/**
  * \brief JTAG Restriction Structure
  */
typedef struct{
    volatile uint32_t nar;             /**< Normal Access Restrictions */
    volatile uint32_t ndar;            /**< Normal Dead Access Restrictions */
}cy_stc_si_nar_t;

/**
  * \brief Application Protection structure
  */
typedef struct {
    uint32_t reserved0   : 2;                      /**< Reserved */
    uint32_t addr30      : 30;                     /**< Base address for FWPU */
} appprot_flash_write_prot_addr_t;

typedef struct {
    uint32_t region_size : 30;                     /**< Region size for FWPU */
    uint32_t reserved0   : 1;                      /**< Reserved */
    uint32_t enable      : 1;                      /**< Enable for FWPU */
} appprot_flash_write_prot_size_t;

typedef struct {
    uint32_t offset      : 16;                     /**< Offset address for ERPU/EWPU */
    uint32_t reserved0   : 16;                     /**< Reserved */
} appprot_efuse_prot_offset_t;

typedef struct {
    uint32_t region_size : 16;                     /**< Region size for ERPU/EWPU */
    uint32_t reserved0   : 15;                     /**< Reserved */
    uint32_t enable      : 1;                      /**< Enable for ERPU/EWPU */
} appprot_efuse_prot_size_t;

typedef struct {
    uint32_t urw         : 1;                      /**< User accsee attribute */
    uint32_t prw         : 1;                      /**< Privileged accsee attribute */
    uint32_t ns          : 1;                      /**< Secure accsee attribute */
    uint32_t reserved0   : 13;                     /**< Reserved */
    uint32_t pc_mask     : 16;                     /**< PC mask setting */
} appprot_prot_att_t;

typedef struct{
    volatile uint32_t               objSize;       /**< Number of configured elements */
    volatile uint32_t               n_fwpu;        /**< Number of FWPU objects */
#if N_FWPU >= 1
    appprot_flash_write_prot_addr_t fwpu0_adr;     /**< FWPU0 base address */
    appprot_flash_write_prot_size_t fwpu0_size;    /**< FWPU0 region size and enable */
    appprot_prot_att_t              fwpu0_sl_att;  /**< FWPU0 slave attribute */
    appprot_prot_att_t              fwpu0_ms_att;  /**< FWPU0 master attribute */
#endif
#if N_FWPU >= 2
    appprot_flash_write_prot_addr_t fwpu1_adr;     /**< FWPU1 base address */
    appprot_flash_write_prot_size_t fwpu1_size;    /**< FWPU1 region size and enable */
    appprot_prot_att_t              fwpu1_sl_att;  /**< FWPU1 slave attribute */
    appprot_prot_att_t              fwpu1_ms_att;  /**< FWPU1 master attribute */
#endif
#if N_FWPU >= 3
    appprot_flash_write_prot_addr_t fwpu2_adr;     /**< FWPU2 base address */
    appprot_flash_write_prot_size_t fwpu2_size;    /**< FWPU2 region size and enable */
    appprot_prot_att_t              fwpu2_sl_att;  /**< FWPU2 slave attribute */
    appprot_prot_att_t              fwpu2_ms_att;  /**< FWPU2 master attribute */
#endif
#if N_FWPU >= 4
    appprot_flash_write_prot_addr_t fwpu3_adr;     /**< FWPU3 base address */
    appprot_flash_write_prot_size_t fwpu3_size;    /**< FWPU3 region size and enable */
    appprot_prot_att_t              fwpu3_sl_att;  /**< FWPU3 slave attribute */
    appprot_prot_att_t              fwpu3_ms_att;  /**< FWPU3 master attribute */
#endif
#if N_FWPU >= 5
    appprot_flash_write_prot_addr_t fwpu4_adr;     /**< FWPU4 base address */
    appprot_flash_write_prot_size_t fwpu4_size;    /**< FWPU4 region size and enable */
    appprot_prot_att_t              fwpu4_sl_att;  /**< FWPU4 slave attribute */
    appprot_prot_att_t              fwpu4_ms_att;  /**< FWPU4 master attribute */
#endif
#if N_FWPU >= 6
    appprot_flash_write_prot_addr_t fwpu5_adr;     /**< FWPU5 base address */
    appprot_flash_write_prot_size_t fwpu5_size;    /**< FWPU5 region size and enable */
    appprot_prot_att_t              fwpu5_sl_att;  /**< FWPU5 slave attribute */
    appprot_prot_att_t              fwpu5_ms_att;  /**< FWPU5 master attribute */
#endif
#if N_FWPU >= 7
    appprot_flash_write_prot_addr_t fwpu6_adr;     /**< FWPU6 base address */
    appprot_flash_write_prot_size_t fwpu6_size;    /**< FWPU6 region size and enable */
    appprot_prot_att_t              fwpu6_sl_att;  /**< FWPU6 slave attribute */
    appprot_prot_att_t              fwpu6_ms_att;  /**< FWPU6 master attribute */
#endif
#if N_FWPU >= 8
    appprot_flash_write_prot_addr_t fwpu7_adr;     /**< FWPU7 base address */
    appprot_flash_write_prot_size_t fwpu7_size;    /**< FWPU7 region size and enable */
    appprot_prot_att_t              fwpu7_sl_att;  /**< FWPU7 slave attribute */
    appprot_prot_att_t              fwpu7_ms_att;  /**< FWPU7 master attribute */
#endif
#if N_FWPU >= 9
    appprot_flash_write_prot_addr_t fwpu8_adr;     /**< FWPU8 base address */
    appprot_flash_write_prot_size_t fwpu8_size;    /**< FWPU8 region size and enable */
    appprot_prot_att_t              fwpu8_sl_att;  /**< FWPU8 slave attribute */
    appprot_prot_att_t              fwpu8_ms_att;  /**< FWPU8 master attribute */
#endif
#if N_FWPU >= 10
    appprot_flash_write_prot_addr_t fwpu9_adr;     /**< FWPU9 base address */
    appprot_flash_write_prot_size_t fwpu9_size;    /**< FWPU9 region size and enable */
    appprot_prot_att_t              fwpu9_sl_att;  /**< FWPU9 slave attribute */
    appprot_prot_att_t              fwpu9_ms_att;  /**< FWPU9 master attribute */
#endif
#if N_FWPU >= 11
    appprot_flash_write_prot_addr_t fwpu10_adr;    /**< FWPU10 base address */
    appprot_flash_write_prot_size_t fwpu10_size;   /**< FWPU10 region size and enable */
    appprot_prot_att_t              fwpu10_sl_att  /**< FWPU10 slave attribute */;
    appprot_prot_att_t              fwpu10_ms_att  /**< FWPU10 master attribute */;
#endif
#if N_FWPU >= 12
    appprot_flash_write_prot_addr_t fwpu11_adr;    /**< FWPU11 base address */
    appprot_flash_write_prot_size_t fwpu11_size;   /**< FWPU11 region size and enable */
    appprot_prot_att_t              fwpu11_sl_att  /**< FWPU11 slave attribute */;
    appprot_prot_att_t              fwpu11_ms_att  /**< FWPU11 master attribute */;
#endif
#if N_FWPU >= 13
    appprot_flash_write_prot_addr_t fwpu12_adr;    /**< FWPU12 base address */
    appprot_flash_write_prot_size_t fwpu12_size;   /**< FWPU12 region size and enable */
    appprot_prot_att_t              fwpu12_sl_att  /**< FWPU12 slave attribute */;
    appprot_prot_att_t              fwpu12_ms_att  /**< FWPU12 master attribute */;
#endif
#if N_FWPU >= 14
    appprot_flash_write_prot_addr_t fwpu13_adr;    /**< FWPU13 base address */
    appprot_flash_write_prot_size_t fwpu13_size;   /**< FWPU13 region size and enable */
    appprot_prot_att_t              fwpu13_sl_att  /**< FWPU13 slave attribute */;
    appprot_prot_att_t              fwpu13_ms_att  /**< FWPU13 master attribute */;
#endif
#if N_FWPU >= 15
    appprot_flash_write_prot_addr_t fwpu14_adr;    /**< FWPU14 base address */
    appprot_flash_write_prot_size_t fwpu14_size;   /**< FWPU14 region size and enable */
    appprot_prot_att_t              fwpu14_sl_att  /**< FWPU14 slave attribute */;
    appprot_prot_att_t              fwpu14_ms_att  /**< FWPU14 master attribute */;
#endif
#if N_FWPU >= 16
    appprot_flash_write_prot_addr_t fwpu15_adr;    /**< FWPU15 base address */
    appprot_flash_write_prot_size_t fwpu15_size;   /**< FWPU15 region size and enable */
    appprot_prot_att_t              fwpu15_sl_att  /**< FWPU15 slave attribute */;
    appprot_prot_att_t              fwpu15_ms_att  /**< FWPU15 master attribute */;
#endif

    volatile uint32_t               n_erpu;        /**< Number of ERPU objects */
#if N_ERPU >= 1
    appprot_efuse_prot_offset_t     erpu0_offset;  /**< ERPU0 base address offset */
    appprot_efuse_prot_size_t       erpu0_size;    /**< ERPU0 region size and enable */
    appprot_prot_att_t              erpu0_sl_att;  /**< ERPU0 slave attribute */
    appprot_prot_att_t              erpu0_ms_att;  /**< ERPU0 master attribute */
#endif
#if N_ERPU >= 2
    appprot_efuse_prot_offset_t     erpu1_offset;  /**< ERPU1 base address offset */
    appprot_efuse_prot_size_t       erpu1_size;    /**< ERPU1 region size and enable */
    appprot_prot_att_t              erpu1_sl_att;  /**< ERPU1 slave attribute */
    appprot_prot_att_t              erpu1_ms_att;  /**< ERPU1 master attribute */
#endif
#if N_ERPU >= 3
    appprot_efuse_prot_offset_t     erpu2_offset;  /**< ERPU2 base address offset */
    appprot_efuse_prot_size_t       erpu2_size;    /**< ERPU2 region size and enable */
    appprot_prot_att_t              erpu2_sl_att;  /**< ERPU2 slave attribute */
    appprot_prot_att_t              erpu2_ms_att;  /**< ERPU2 master attribute */
#endif
#if N_ERPU >= 4
    appprot_efuse_prot_offset_t     erpu3_offset;  /**< ERPU0 base address offset */
    appprot_efuse_prot_size_t       erpu3_size;    /**< ERPU0 region size and enable */
    appprot_prot_att_t              erpu3_sl_att;  /**< ERPU0 slave attribute */
    appprot_prot_att_t              erpu3_ms_att;  /**< ERPU0 master attribute */
#endif
    volatile uint32_t               n_ewpu;        /**< Number of ERPU objects */
#if N_EWPU >= 1
    appprot_efuse_prot_offset_t     ewpu0_offset;  /**< EWPU0 base address offset */
    appprot_efuse_prot_size_t       ewpu0_size;    /**< EWPU0 region size and enable */
    appprot_prot_att_t              ewpu0_sl_att;  /**< EWPU0 slave attribute */
    appprot_prot_att_t              ewpu0_ms_att;  /**< EWPU0 master attribute */
#endif
#if N_EWPU >= 2
    appprot_efuse_prot_offset_t     ewpu1_offset;  /**< EWPU1 base address offset */
    appprot_efuse_prot_size_t       ewpu1_size;    /**< EWPU1 region size and enable */
    appprot_prot_att_t              ewpu1_sl_att;  /**< EWPU1 slave attribute */
    appprot_prot_att_t              ewpu1_ms_att;  /**< EWPU1 master attribute */
#endif
#if N_EWPU >= 3
    appprot_efuse_prot_offset_t     ewpu2_offset;  /**< EWPU2 base address offset */
    appprot_efuse_prot_size_t       ewpu2_size;    /**< EWPU2 region size and enable */
    appprot_prot_att_t              ewpu2_sl_att;  /**< EWPU2 slave attribute */
    appprot_prot_att_t              ewpu2_ms_att;  /**< EWPU2 master attribute */
#endif
#if N_EWPU >= 4
    appprot_efuse_prot_offset_t     ewpu3_offset;  /**< EWPU3 base address offset */
    appprot_efuse_prot_size_t       ewpu3_size;    /**< EWPU3 region size and enable */
    appprot_prot_att_t              ewpu3_sl_att;  /**< EWPU3 slave attribute */
    appprot_prot_att_t              ewpu3_ms_att;  /**< EWPU3 master attribute */
#endif
}cy_stc_si_app_prot_t;

/**
  * \brief Application header Structure
  */
typedef struct{
    volatile uint32_t objSize;                   /**< Object size (Bytes) */
    volatile uint32_t appId;                     /**< Application ID/version */
    volatile uint32_t appAttributes;             /**< Attributes (reserved for future use) */
    volatile uint32_t numCores;                  /**< Number of cores */
    volatile uint32_t core0Vt;                   /**< (CM0+)VT offset - offset to the vector table from that entry */
    volatile uint32_t core0Id;                   /**< CM0+ core ID */
}cy_stc_si_appheader_t;

/**
  * \brief ReadUniqueID API Structure
  */
typedef struct
{
    uint32_t        : 24;                        /* Reserved */
    uint32_t Opcode : 8;                         /**< Opecode for ReqdUniqueID API */
} rd_unique_id_arg0_t;

typedef struct
{
    rd_unique_id_arg0_t arg0;                    /** < \ref rd_unique_id_arg0_t */
    uint32_t            resv[7ul];               /* Reserved */
} rd_unique_id_args_t;

/**
  * \brief TransitiontoSecure API Structure
  */
typedef struct
{
    uint32_t        : 8;                         /* Reserved */
    uint32_t Debug  : 8;                         /**< Debug flag 1: SECURE_WITH_DEBUG, others: SECURE*/
    uint32_t        : 8;                         /* Reserved */
    uint32_t Opcode : 8;                         /**< Opecode for TransitiontoSecure API */
} trans_to_secure_arg0_t;

typedef struct
{
    uint32_t Acc_restrict  : 32;                 /**< Access restriction*/
} trans_to_secure_arg1_t;

typedef struct
{
    uint32_t Dead_Acc_restrict  : 32;            /**< Dead Access restriction*/
} trans_to_secure_arg2_t;

typedef struct
{
    trans_to_secure_arg0_t arg0;                 /** < \ref trans_to_secure_arg0_t */
    trans_to_secure_arg1_t arg1;                 /** < \ref trans_to_secure_arg1_t */
    trans_to_secure_arg2_t arg2;                 /** < \ref trans_to_secure_arg2_t */
    uint32_t               resv[5ul];            /* Reserved */
} trans_to_secure_args_t;

/**
  * \brief TransitiontoRMA API Structure
  */
typedef struct
{
    uint32_t        : 24;                        /* Reserved */
    uint32_t Opcode : 8;                         /**< Opecode for TransitiontoRMA API */
} trans_to_rma_arg0_t;

typedef struct
{
    uint32_t Objsize : 32;                       /**< Object Size */
} trans_to_rma_arg1_t;

typedef struct
{
    uint32_t CommandId : 32;                     /**< Command ID */
} trans_to_rma_arg2_t;

typedef struct
{
    uint32_t UniqueID_0 : 32;                    /**< Unique ID word 0*/
} trans_to_rma_arg3_t;

typedef struct
{
    uint32_t UniqueID_1 : 32;                    /**< Unique ID word 1*/
} trans_to_rma_arg4_t;

typedef struct
{
    uint32_t UniqueID_2 : 32;                    /**< Unique ID word 2 (3bytes) */
} trans_to_rma_arg5_t;

typedef struct
{
    uint32_t dataAddr : 32;                      /**< Signature address (4bytes) */
} trans_to_rma_arg6_t;

typedef struct
{
    trans_to_rma_arg0_t arg0;                    /** < \ref trans_to_rma_arg0_t */
    trans_to_rma_arg1_t arg1;                    /** < \ref trans_to_rma_arg1_t */
    trans_to_rma_arg2_t arg2;                    /** < \ref trans_to_rma_arg2_t */
    trans_to_rma_arg3_t arg3;                    /** < \ref trans_to_rma_arg3_t */
    trans_to_rma_arg4_t arg4;                    /** < \ref trans_to_rma_arg4_t */
    trans_to_rma_arg5_t arg5;                    /** < \ref trans_to_rma_arg5_t */
    trans_to_rma_arg6_t arg6;                    /** < \ref trans_to_rma_arg6_t */
    uint32_t            resv[1ul];               /* Reserved */
} trans_to_rma_args_t;

/**
  * \brief SROM API Structure
  */
typedef union
{
    uint32_t               arg[8ul];             /* Reserved */
    rd_unique_id_args_t    RdUnId;               /**< ReadUniqueID API */
    trans_to_secure_args_t TransitionToSecure;   /**< TransitiontoSecure API */   
    trans_to_rma_args_t    TransitionToRMA;      /**< TransitiontoRMA API */
} srom_api_args_t;


/***************************************
*        Function Prototypes
***************************************/

/**
* \addtogroup group_secure_image_functions
* \{
*/

/**
* \addtogroup group_secure_image_functions_direct
* \{
*/
__STATIC_INLINE uint32_t Cy_FB_VerifyApplication(uint32_t address, uint32_t length, uint32_t signature,
                                 cy_stc_crypto_rsa_pub_key_t *publicKey);
__STATIC_INLINE uint32_t Cy_FB_IsValidKey(uint32_t tocAddr, cy_stc_crypto_rsa_pub_key_t *publicKey);
/** \} group_secure_image_functions_direct */

/**
* \addtogroup group_secure_image_functions_direct
* \{
*/

/*******************************************************************************
* Function Name: Cy_FB_VerifyApplication
****************************************************************************//**
*
* \brief Verifies the secure application digital signature.
*
* This function relies on the assumption that the application digital signature
* was calculated in the following manner:
* 1. SHA-256 hash of the binary application image is calculated.
* 2. The hash (digital digest) is signed using a RSA-1024/2056 <b>private</b> 
*    key to generate the digital signature.
* 3. The digital signature is placed in the application object in Cypress format.
*
* The application verification is performed by performing the following
* operations:
* 1. SHA-256 hash of the binary application image is calculated.
* 2. The application digital signature is decrypted using the RSA-1024/2056
*    <b>public</b> key.
* 3. The hash and the decrypted digital signature are compared. If they are
*    equivalent, the image is valid.
*
* \note This is a direct branch to a function residing in SFlash.
*
* \param address      
* Staring address of the application area to be verified with secure signature.
*
* \param length
* The length of the area to be verified.
* 
* \param signature
* Starting address of the signature inside the application residing in Flash.
* 
* \param publicKey
* Pointer to the public key structure.
*
* \return
* - 1 if the digital secure signature verification succeeds.
* - 0 if the digital secure signature verification of the application fails.
*
*******************************************************************************/
__STATIC_INLINE uint32_t Cy_FB_VerifyApplication(uint32_t address, uint32_t length, 
                                        uint32_t signature, cy_stc_crypto_rsa_pub_key_t *publicKey)
{
    sflash_verifyapp_func_t fp = (sflash_verifyapp_func_t)CY_SI_IMGVAL_VERIFYAPP_REG;
    return ( fp(address, length, signature, publicKey) );
}


/*******************************************************************************
* Function Name: Cy_FB_IsValidKey
****************************************************************************//**
*
* \brief Checks whether the Public Key structure is valid.
*
* The public key structure must be as specified as in cy_si_stc_public_key_t.
* Supported signature schemes are:
*   0x00: RSASSA-PKCS1-v1_5-2048
*   0x01: RSASSA-PKCS1-v1_5-1024
*
* \note This is a direct branch to a function residing in SFlash.
*
* \return
* - 1 if Public Key has a valid format
* - 0 if Public Key has an invalid format
*
*******************************************************************************/
__STATIC_INLINE uint32_t Cy_FB_IsValidKey(uint32_t tocAddr, cy_stc_crypto_rsa_pub_key_t *publicKey)
{
    sflash_validkey_func_t fp = (sflash_validkey_func_t)CY_SI_IMGVAL_VALIDKEY_REG;
    return ( fp(tocAddr, publicKey) );
}

/** \} group_secure_image_functions_direct */

/** \} group_secure_image_functions */

#if defined(__cplusplus)
}
#endif

#endif /* CY_SI_CONFIG_H */

/* [] END OF FILE */
