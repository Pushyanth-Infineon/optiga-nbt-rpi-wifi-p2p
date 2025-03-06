#include "infineon/ifx-error.h"
#include "infineon/ifx-protocol.h"
#include "infineon/ifx-utils.h"
#include "infineon/ifx-t1prime.h"
#include "infineon/nbt-cmd.h"
#include "infineon/i2c-rpi.h"
#include "infineon/logger-printf.h"

#include "utilities/nbt-utilities.h"

/* Required for POSIX threads */
#include <pthread.h>

/* Required for I2C */
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include <stdio.h>

/* NBT slave address */
#define NBT_DEFAULT_I2C_ADDRESS 0x18U
#define RPI_I2C_FILE  "/dev/i2c-1"
#define LOG_TAG "NBT example"

#define RPI_I2C_OPEN_FAIL   (-1)
#define RPI_I2C_INIT_FAIL   (-2)
#define OPTIGA_NBT_ERROR    (-3)

/**
 * \brief Skeleton for WiFi connection handover message.
 */
// clang-format off
static uint8_t WIFI_CONNECTION_HANDOVER_MESSAGE[] = 
{   // NDEF record header. MB=1b, ME=0b, CF=0b, SR=1b, IL=0b, TNF=001b
    0x91,
    // Record Type length (2 octets)
    0x02,
    // Record Type Length: 10 octets.
    0x0a,
    // Record Type: "Hs"
    0x48, 0x73,
    // Version Number: Major = 1, Minor = 3
    0x13, 
    // NDEF Record Header: MB=1b, ME=1b, CF=0b, SR=1b, IL=0b, TNF=001b
    0xd1,
    // Record Type Length: 2 octets
    0x02,
    // Payload Length: 4 octets
    0x04,
    // Record Type: "ac" (Alternate Carrier) records
    0x61, 0x63,
    // Carrier Flags: CPS=1, "active"
    0x01,
    // Carrier Data Reference Length: 1 octet
    0x01,
    // Carrier Data Reference: "0"
    0x30,
    // Auxiliary Data Reference Count: 0
    0x0,
    // NDEF Record Header: MB=0b, ME=1b, CF=0b, SR=1b, IL=1b, TNF=010b
    0x5a,
    // Record Type Length: 23 octets
    0x17,
    // Payload length: 87 octets
    0x57,
    // Id Length: 1 octet
    0x01,
    // Record Type Name: "application/vnd.wfa.wsc"
    0x61, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x76, 0x6e, 0x64, 0x2e, 0x77, 0x66, 0x61, 0x2e, 0x77, 0x73, 0x63,
    // Id: "0"
    0x30,
    // MAC address (1020)
    0x10, 0x20, 0x00,
    // Length: 6 octets
    0x06, 
    // MAC address: "123456"
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
    // Out Of Band Device Password: 2 octets
    0x10, 0x2c,
    // Payload length: 2 octets
    0x00, 0x26, 
    // Public key hash: 20 octets
    0x95, 0xc8, 0xd2, 0x7a, 0xa8, 0x94, 0x63, 0x38, 0x71, 0x3a, 0x8d, 0xe9, 0x68, 0xf8, 0xde, 0x7a, 0x72, 0x33, 0x8c, 0xef,
    // Record length: 2 octets
    0x00, 0x10,
    // Password: 16 octets
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    // Record Type: "RF Bands" - 2 octets
    0x10, 0x3c,
    // Record length: 2 octets
    0x00, 0x01, 
    // 2.4 GHz (0x01)
    0x01, 
    // Record type: "SSID"
    0x10, 0x45, 
    // Record length: 2 octets
    0x0, 0x0a, 
    // SSID: "DIRECT-Rpi" - 10 octets
    0x44, 0x49, 0x52, 0x45, 0x43, 0x54, 0x2d, 0x52, 0x70, 0x69, 
    // Record type: "Vendor extension"
    0x10, 0x49, 
    // Record length: 2 octets
    0x00, 0x6, 
    // WFA Vendor Extension ()
    0x00, 0x37, 0x2a,
    
    0x00, 0x01, 0x20
};
// clang-format on

/** GP T=1' I2C protocol - Raspberry PI */
// Protocol to handle the GP T=1' I2C protocol communication with tag
ifx_protocol_t gp_i2c_protocol;

// Protocol to handle communication with Raspberry PI I2C driver
ifx_protocol_t driver_adapter;
/* Initialize protocol driver layer here with I2C implementation.
Note: Does not work without initialized driver layer for I2C. */

/* Logger object */
ifx_logger_t logger_implementation;

/**
 * \brief NBT abstraction.
 */
static nbt_cmd_t nbt;

/* I2C file descriptor */
static int i2c_fd;


/**
 * \brief Configures NBT for Wifi connection handover usecase.
 * \details Sets file access policies, configures communication interface and writes connection handover skeleton to NDEF file.
 * \param[in] nbt NBT abstraction for communication.
 * \return ifx_status_t \c IFX_SUCCESS if successful, any other value in case of error.
 */
static ifx_status_t nbt_configure_wifi_connection_handover(nbt_cmd_t *nbt)
{
    if (nbt == NULL)
    {
        return IFX_ERROR(LIB_NBT_APDU, NBT_SET_CONFIGURATION, IFX_ILLEGAL_ARGUMENT);
    }
    const nbt_file_access_policy_t fap_cc = {.file_id = NBT_FILEID_CC,
                                             .i2c_read_access_condition = NBT_ACCESS_ALWAYS,
                                             .i2c_write_access_condition = NBT_ACCESS_NEVER,
                                             .nfc_read_access_condition = NBT_ACCESS_ALWAYS,
                                             .nfc_write_access_condition = NBT_ACCESS_NEVER};
    const nbt_file_access_policy_t fap_ndef = {.file_id = NBT_FILEID_NDEF,
                                               .i2c_read_access_condition = NBT_ACCESS_ALWAYS,
                                               .i2c_write_access_condition = NBT_ACCESS_ALWAYS,
                                               .nfc_read_access_condition = NBT_ACCESS_ALWAYS,
                                               .nfc_write_access_condition = NBT_ACCESS_NEVER};
    const nbt_file_access_policy_t fap_fap = {.file_id = NBT_FILEID_FAP,
                                              .i2c_read_access_condition = NBT_ACCESS_ALWAYS,
                                              .i2c_write_access_condition = NBT_ACCESS_ALWAYS,
                                              .nfc_read_access_condition = NBT_ACCESS_ALWAYS,
                                              .nfc_write_access_condition = NBT_ACCESS_ALWAYS};
    const nbt_file_access_policy_t fap_proprietary1 = {.file_id = NBT_FILEID_PROPRIETARY1,
                                                       .i2c_read_access_condition = NBT_ACCESS_NEVER,
                                                       .i2c_write_access_condition = NBT_ACCESS_NEVER,
                                                       .nfc_read_access_condition = NBT_ACCESS_NEVER,
                                                       .nfc_write_access_condition = NBT_ACCESS_NEVER};
    const nbt_file_access_policy_t fap_proprietary2 = {.file_id = NBT_FILEID_PROPRIETARY2,
                                                       .i2c_read_access_condition = NBT_ACCESS_NEVER,
                                                       .i2c_write_access_condition = NBT_ACCESS_NEVER,
                                                       .nfc_read_access_condition = NBT_ACCESS_NEVER,
                                                       .nfc_write_access_condition = NBT_ACCESS_NEVER};
    const nbt_file_access_policy_t fap_proprietary3 = {.file_id = NBT_FILEID_PROPRIETARY3,
                                                       .i2c_read_access_condition = NBT_ACCESS_NEVER,
                                                       .i2c_write_access_condition = NBT_ACCESS_NEVER,
                                                       .nfc_read_access_condition = NBT_ACCESS_NEVER,
                                                       .nfc_write_access_condition = NBT_ACCESS_NEVER};
    const nbt_file_access_policy_t fap_proprietary4 = {.file_id = NBT_FILEID_PROPRIETARY4,
                                                       .i2c_read_access_condition = NBT_ACCESS_NEVER,
                                                       .i2c_write_access_condition = NBT_ACCESS_NEVER,
                                                       .nfc_read_access_condition = NBT_ACCESS_NEVER,
                                                       .nfc_write_access_condition = NBT_ACCESS_NEVER};
    const nbt_file_access_policy_t *faps[] = {&fap_cc, &fap_ndef, &fap_fap, &fap_proprietary1, &fap_proprietary2, &fap_proprietary3, &fap_proprietary4};
    const struct nbt_configuration configuration = {.fap = (nbt_file_access_policy_t **) faps,
                                                    .fap_len = sizeof(faps) / sizeof(struct nbt_configuration *),
                                                    .communication_interface = NBT_COMM_INTF_NFC_ENABLED_I2C_ENABLED,
                                                    .irq_function = NBT_GPIO_FUNCTION_DISABLED};
    ifx_status_t status = nbt_configure(nbt, &configuration);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_FATAL, "Could not confgure NBT for connection handover usecase.");
        return status;
    }
}


/**
 * \brief posix thread to write the Wifi P2P connection handover Select data to NDEF file
 *
 * \details
 *   * Opens communication channel to NBT.
 *   * Configures NBT for asynchronous data transfer usecase.
 *   * Reads configuration from NBT proprietary file 1.
 *   * Sets LED state depending on configuration.
 *   * Writes LED state to NBT proprietary file 2.
 *   * Waits for button interrupt and starts loop again.
 *
 * \see nbt_configure_adt()
 */
void* nbt_write_ndef(void *arg)
{
    // Activate communication channel to NBT
    uint8_t *atpo = NULL;
    size_t atpo_len = 0U;
    ifx_status_t status = ifx_protocol_activate(&gp_i2c_protocol, &atpo, &atpo_len);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_FATAL, "Could not open communication channel to NBT");
        goto exit;
    }

    // Set NBT to Connection handover configuration
    status = nbt_configure_wifi_connection_handover(&nbt);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_FATAL, "Could not set NBT to WiFi Connection handover configuration");
        goto exit;
    }

    // Use NBT command abstraction
    status = nbt_select_nbt_application(&nbt);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not select NBT application");
        goto exit;
    }

    // Write the NDEF message
    status = nbt_write_file(&nbt, NBT_FILEID_NDEF, 0U, WIFI_CONNECTION_HANDOVER_MESSAGE, sizeof(WIFI_CONNECTION_HANDOVER_MESSAGE));
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not write NBT NDEF file");
        goto exit;
    }

exit:
    *(int *)(arg) = status;
    return arg;
}


int main()
{
    /** GP T=1' I2C protocol - PSoC&trade;6 Host MCU */
    // Protocol to handle the GP T=1' I2C protocol communication with tag
    ifx_protocol_t gp_i2c_protocol;

    // Protocol to handle communication with Raspberry PI I2C driver
    ifx_protocol_t driver_adapter;
    /* Initialize protocol driver layer here with I2C implementation.
    Note: Does not work without initialized driver layer for I2C. */

    // code placeholder
    ifx_status_t status;

    /* I2C file descriptor */
    int i2c_fd;

    /* Pthread ID */
    pthread_t ptid; 
    void *pthread_status = &status;

    /* Initialize logging */
    status = logger_printf_initialize(ifx_logger_default);
    if (ifx_error_check(status))
    {
        goto ret;
    }

    status = ifx_logger_set_level(ifx_logger_default, IFX_LOG_DEBUG);
    if (ifx_error_check(status))
    {
        goto ret;
    }

    /* Open the I2C device */
    if ((i2c_fd = open(RPI_I2C_FILE, O_RDWR)) == -1)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Failed to open I2C character device");
        status = RPI_I2C_OPEN_FAIL;
        goto ret;
    }

    /* Initialize RPI I2c driver adaptor */
    // I2C driver adapter
    status = i2c_rpi_initialize(&driver_adapter, i2c_fd, NBT_DEFAULT_I2C_ADDRESS);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not initialize I2C driver adapter");
        status = RPI_I2C_INIT_FAIL;
        goto exit;
    }

    // Use GP T=1' protocol channel as a interface to communicate with the OPTIGA&trade; Authenticate NBT
    status = ifx_t1prime_initialize(&gp_i2c_protocol, &driver_adapter);
    if (status != IFX_SUCCESS)
    {
        goto exit;
    }

    ifx_protocol_set_logger(&gp_i2c_protocol, ifx_logger_default);
    status = ifx_protocol_activate(&gp_i2c_protocol, NULL, NULL);
    if (status != IFX_SUCCESS)
    {
        goto cleanup;
    }

    // NBT command abstraction
    status = nbt_initialize(&nbt, &gp_i2c_protocol, ifx_logger_default);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not initialize NBT abstraction");
        goto cleanup;
    }

    /* Create a thread to perform nbt_write_ndef function */
    if (0 != pthread_create(&ptid, NULL, nbt_write_ndef, pthread_status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not create POSIX thread");
        goto cleanup;
    }

    /* Wait till thread completion. Should not come here */
    pthread_join(ptid, &pthread_status);
    if (*(int*)pthread_status)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "POSIX thread: nbt_write_ndef failed with: (%d)", *(int *)pthread_status);
    }

cleanup:

    // Perform cleanup of full protocol stack
    ifx_protocol_destroy(&gp_i2c_protocol);

    // Destroy NBT command abstraction
    nbt_destroy(&nbt);
exit:
    // Close the File descriptor
    close(i2c_fd);

ret:
    return status;
}
