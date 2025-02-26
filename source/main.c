#include "infineon/ifx-error.h"
#include "infineon/ifx-protocol.h"
#include "infineon/ifx-utils.h"
#include "infineon/ifx-t1prime.h"
#include "infineon/nbt-cmd.h"
#include "infineon/i2c-rpi.h"
#include "infineon/logger-printf.h"

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

int main()
{
    /** GP T=1' I2C protocol - PSoC&trade;6 Host MCU */
    // Protocol to handle the GP T=1' I2C protocol communication with tag
    ifx_protocol_t gp_i2c_protocol;

    // Protocol to handle communication with Raspberry PI I2C driver
    ifx_protocol_t driver_adapter;
    /* Initialize protocol driver layer here with I2C implementation.
    Note: Does not work without initialized driver layer for I2C. */

    /* Logger object */
    ifx_logger_t logger_implementation;

    // code placeholder
    ifx_status_t status;

    /* I2C file descriptor */
    int i2c_fd;

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

    // Select Type 4 Tag application
    uint8_t select_app[] = {0x00, 0xa4, 0x04, 0x00, 0x07, 0xd2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00};

    // Select application with File ID: E1A1 and length = 1:
    uint8_t select_aid[] = {0x00, 0xa4, 0x00, 0x0c, 0x02, 0xe1, 0xa1, 0x00};

    // Write one byte (0xEE)
    uint8_t write_file[] = {0x00, 0xd6, 0x00, 0x00, 0x01, 0xEE};

    // Read back: 
    uint8_t read_file[] = {0x00, 0xb0, 0x00, 0x00, 0x01};

    // Exchange data with the secure element
    uint8_t *response = malloc(30);
    size_t response_len = 0u;
    status = ifx_protocol_transceive(&gp_i2c_protocol, select_app, sizeof(select_app), &response, &response_len);
    if (response != NULL) free(response);
    if (status != IFX_SUCCESS)
    {
        goto cleanup;
    }

    status = ifx_protocol_transceive(&gp_i2c_protocol, select_aid, sizeof(select_aid), &response, &response_len);
    if (response != NULL) free(response);
    if (status != IFX_SUCCESS)
    {
        goto cleanup;
    }

    status = ifx_protocol_transceive(&gp_i2c_protocol, write_file, sizeof(write_file), &response, &response_len);
    if (response != NULL) free(response);
    if (status != IFX_SUCCESS)
    {
        goto cleanup;
    }

    status = ifx_protocol_transceive(&gp_i2c_protocol, read_file, sizeof(read_file), &response, &response_len);
    if (response != NULL) free(response);
    if (status != IFX_SUCCESS)
    {
        goto cleanup;
    }

cleanup:

    // Perform cleanup of full protocol stack
    ifx_protocol_destroy(&gp_i2c_protocol);

exit:
    // Close the File 
    close(i2c_fd);

ret:
    return status;
}
