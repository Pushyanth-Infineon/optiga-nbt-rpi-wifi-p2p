// SPDX-FileCopyrightText: Copyright (c) 2025 Infineon Technologies AG
// SPDX-License-Identifier: MIT

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
#define NDEF_LEN_TAG_LENGTH     0x02U
#define RPI_I2C_FILE  "/dev/i2c-1"
#define LOG_TAG "NBT example"

#define RPI_I2C_OPEN_FAIL   (-1)
#define RPI_I2C_INIT_FAIL   (-2)
#define OPTIGA_NBT_ERROR    (-3)
#define WRITE_FILE_FAIL     (-4)

typedef struct ndef_read {
    size_t length;
    uint8_t *ndef_bytes;
    ifx_status_t status;
} ndef_read;


/**
 * \brief Skeleton for WiFi connection handover message.
 */
// clang-format off
static uint8_t WIFI_CONNECTION_HANDOVER_MESSAGE[] = 
{
	0x00, 0x77, 0x91, 0x02, 0x0a, 0x48, 0x73, 0x13, 
	0xd1, 0x02, 0x04, 0x61, 0x63, 0x01, 0x01, 0x30, 
	0x00, 0x5a, 0x17, 0x4c, 0x01, 0x61, 0x70, 0x70, 
	0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 
	0x2f, 0x76, 0x6e, 0x64, 0x2e, 0x77, 0x66, 0x61, 
	0x2e, 0x77, 0x73, 0x63, 0x30, 0x00, 0x4a, 0x10, 
	0x01, 0x00, 0x02, 0x00, 0x06, 0x10, 0x20, 0x00, 
	0x06, 0xde, 0xa6, 0x32, 0xaa, 0x45, 0xba, 0x10, 
	0x2c, 0x00, 0x16, 0xce, 0xec, 0x12, 0x76, 0x2e, 
	0x66, 0x39, 0x7b, 0x56, 0xda, 0xd6, 0x4f, 0xd2, 
	0x70, 0xbb, 0x3d, 0x69, 0x4c, 0x78, 0xfb, 0x00, 
	0x07, 0x10, 0x3c, 0x00, 0x01, 0x01, 0x10, 0x45, 
	0x00, 0x0d, 0x44, 0x49, 0x52, 0x45, 0x43, 0x54, 
	0x2d, 0x52, 0x61, 0x73, 0x50, 0x69, 0x31, 0x10, 
	0x49, 0x00, 0x06, 0x00, 0x37, 0x2a, 0x00, 0x01, 
	0x20
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
                                               .nfc_write_access_condition = NBT_ACCESS_ALWAYS};
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
    (void)arg;
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
    return (void *)((long)status);
}

/**
 * \brief posix thread to read the Wifi P2P connection handover Select data NDEF message
 *
 * \details
 *   * Opens communication channel to NBT.
 *   * Reads the NDEF messagea
 */
void* nbt_read_ndef(void *arg)
{
    // Activate communication channel to NBT
    uint8_t *atpo = NULL;
    size_t atpo_len = 0U;
    size_t ndef_length = NDEF_LEN_TAG_LENGTH;   // Initial NDEF file length.
    uint32_t offset = 0;
    uint8_t* ndef_bytes = malloc(ndef_length);

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
    ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_INFO, "Reading (0x%x) bytes from offset (0x%x)", ndef_length, offset);
    status = nbt_read_file(&nbt, NBT_FILEID_NDEF, offset, ndef_length, ndef_bytes);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not write NBT NDEF file");
        goto exit;
    }

    offset += NDEF_LEN_TAG_LENGTH;
    ndef_length += (ndef_bytes[0] << 8) + (ndef_bytes[1]);
    ndef_bytes = realloc(ndef_bytes, ndef_length + NDEF_LEN_TAG_LENGTH);

    // Read the whole NDEF message
    ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_INFO, "Reading (0x%x) bytes from offset (0x%x)", ndef_length, offset);
    status = nbt_read_file(&nbt, NBT_FILEID_NDEF, offset, (ndef_length - offset), (ndef_bytes + offset));
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not write NBT NDEF file");
        goto exit;
    }

    for (int i = 0; i < ndef_length; i++) {
        printf("0x%x, ", ndef_bytes[i]);
    }
    printf("\n");

    /* Update the passed structure */
    ((ndef_read *)arg)->length = ndef_length;
    ((ndef_read *)arg)->ndef_bytes = ndef_bytes;

exit:
    return (void *)((long)status);
}

void usage(char *file)
{
    printf("Usage:\n");
    printf("%s [OPTION] [FILE]\n", file);
    printf("OPTIONS:\n");
    printf("\t-write [FILE]\t- Write NDEF file. Save the written bytes into the file.\n");
    printf("\t-read  [FILE]\t- Read NDEF file. Save the read bytes into the file. \n");
}

/**
 * -write - Write NDEF in OPTIGA NBT
 * -read - Read NDEF message from OPTIGA NBT
 * -help - Show this help
*/
int main(int argc, char *argv[])
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

    bool is_write;
    char *file_name = NULL;

    /* Pthread ID */
    pthread_t ptid;
    ifx_status_t pthread_status;
    ndef_read read_data;

    /* Parse command line options */
    if (argc < 2)
    {
        printf("ERROR: Atleast one argument is expected.\n");
        usage(argv[0]);
        return 1;
    }
    else
    {
        if (strncmp("-write", argv[1], 6) == 0)
            is_write = true;
        else if (strncmp("-read", argv[1], 5) == 0)
            is_write = false;
        else if (strncmp("-help", argv[1], 5) == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else
        {
            printf("ERROR: Invalid argument: %s\n", argv[1]);
            usage(argv[0]);
            return 2;
        }

        if (argc == 3)
        {
            file_name = argv[2];
        }
        else
        {
            printf("ERROR: Invalid number of arguments for %s.\n", (is_write) ? "-write" : "-read");
            usage(argv[0]);
            return 3;
        }
    }

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

    if (is_write)
    {
        /* Create a thread to perform nbt_write_ndef function */
        if (0 != pthread_create(&ptid, NULL, nbt_write_ndef, (void *)&pthread_status))
        {
            ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not create POSIX thread");
            goto cleanup;
        }
        pthread_join(ptid, (void *)&pthread_status);
        if (pthread_status)
        {
            ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "POSIX thread: nbt_write_ndef failed with: (%d)", pthread_status);
            goto cleanup;
        }
        else
        {
            ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_INFO, "Successfully written (%d) bytes to OPTIGA NBT", sizeof(WIFI_CONNECTION_HANDOVER_MESSAGE));
        }
    }
    else
    {
        /* Create a thread to perform nbt_read_ndef function */
        if (0 != pthread_create(&ptid, NULL, nbt_read_ndef, (void *)&read_data))
        {
            ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not create POSIX thread");
            goto cleanup;
        }
        pthread_join(ptid, (void *)&read_data.status);
        if (read_data.status)
        {
            ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "POSIX thread: nbt_read_ndef failed with: (%d)", read_data.status);
            goto cleanup;
        }
    }

    /* Write Data into file */
    FILE *fd = fopen(argv[2], "wb");
    size_t actual_length, bytes_written;
    if (fd == NULL)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Error opening file: %s", argv[2]);
        goto cleanup;
    }

    if (!is_write)
    {
        actual_length = read_data.length;
        bytes_written = fwrite(read_data.ndef_bytes, 1, actual_length, fd);
    }
    else
    {
        actual_length = sizeof WIFI_CONNECTION_HANDOVER_MESSAGE;
        bytes_written = fwrite(WIFI_CONNECTION_HANDOVER_MESSAGE, 1, actual_length, fd);
    }

    if (bytes_written != actual_length)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Failed to write (%d) bytes of data into: %s", actual_length, argv[2]);
        status = WRITE_FILE_FAIL;
    }
    else
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_INFO, "Successfullly written (%d) bytes of data into: %s", actual_length, argv[2]);
    }

    fclose(fd);

    if (!is_write)
    {
        /* Free the dynamic memory */
        free(read_data.ndef_bytes);
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
