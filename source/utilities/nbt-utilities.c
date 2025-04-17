// SPDX-FileCopyrightText: 2024 Infineon Technologies AG
// SPDX-License-Identifier: MIT

/**
 * \file nbt-utilities.c
 * \brief General utilities for interacting with an NBT.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "infineon/ifx-error.h"
#include "infineon/ifx-logger.h"
#include "infineon/ifx-protocol.h"
#include "infineon/ifx-apdu.h"
#include "infineon/ifx-apdu-protocol.h"
#include "infineon/nbt-apdu.h"
#include "infineon/nbt-cmd-config.h"
#include "infineon/nbt-cmd.h"

#include "nbt-utilities.h"

/**
 * \brief String used as source information for logging.
 */
#define LOG_TAG "NBT utilities"

/**
 * \brief Selects NBT (operational) application.
 *
 * \details Wraps select_application() and adds cleanup.
 *
 * \param[in] nbt NBT command abstraction.
 * \return ifx_status_t \c IFX_SUCCESS if successful, any other value in case of error.
 * \see nbt_select_application()
 */
ifx_status_t nbt_select_nbt_application(nbt_cmd_t *nbt)
{
    if (nbt == NULL)
    {
        return IFX_ERROR(LIB_NBT_APDU, NBT_SELECT_APPLICATION, IFX_ILLEGAL_ARGUMENT);
    }
    ifx_status_t status = nbt_select_application(nbt);
    ifx_apdu_destroy(nbt->apdu);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not select NBT application");
        return status;
    }
    if (nbt->response->sw != 0x9000U)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid status word for selecting NBT application: 0x%04X", nbt->response->sw);
        ifx_apdu_response_destroy(nbt->response);
        return IFX_ERROR(LIB_NBT_APDU, NBT_SELECT_APPLICATION, IFX_SW_ERROR);
    }
    ifx_apdu_response_destroy(nbt->response);
    return IFX_SUCCESS;
}

/**
 * \brief Configures NBT according to given configuration.
 *
 * \param[in] nbt NBT abstraction for communication.
 * \param[in] configuration Desired NBT configuration to be set.
 * \return ifx_status_t \c IFX_SUCCESS if successful, any other value in case of error.
 */
ifx_status_t nbt_configure(nbt_cmd_t *nbt, const struct nbt_configuration *configuration)
{
    // Validate parameters
    if ((nbt == NULL) || (configuration == NULL))
    {
        return IFX_ERROR(LIB_NBT_APDU, NBT_SET_CONFIGURATION, IFX_ILLEGAL_ARGUMENT);
    }

    // Update file access policies
    ifx_status_t status = nbt_select_nbt_application(nbt);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not select NBT application");
        return status;
    }

    // Get current file access policies
    nbt_file_access_policy_t current_faps[7];
    status = nbt_read_fap(nbt, current_faps);
    ifx_apdu_destroy(nbt->apdu);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not read current file access policies");
        return status;
    }
    if (nbt->response->sw != 0x9000U)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid status word for reading file access policy: 0x%04X", nbt->response->sw);
        ifx_apdu_response_destroy(nbt->response);
        return IFX_ERROR(LIB_NBT_APDU, NBT_UPDATE_FAP_BYTES_WITH_PASSWORD, IFX_SW_ERROR);
    }
    ifx_apdu_response_destroy(nbt->response);

    // Check file access policies to be updated
    for (size_t i = 0U; i < configuration->fap_len; i++)
    {
        bool fap_found = false;
        for (size_t j = 0U; j < (sizeof(current_faps) / sizeof(nbt_file_access_policy_t)); j++)
        {
            if (configuration->fap[i]->file_id == current_faps[j].file_id)
            {
                fap_found = true;

                // Check if file access policy needs to be updated
                if (memcmp(configuration->fap[i], &current_faps[j], sizeof(nbt_file_access_policy_t)) != 0)
                {
                    status = nbt_update_fap(nbt, configuration->fap[i]);
                    ifx_apdu_destroy(nbt->apdu);
                    if (ifx_error_check(status))
                    {
                        // clang-format off
                        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not update file access policy for file 0x%04X", configuration->fap[i]->file_id);
                        // clang-format on
                        return status;
                    }
                    if (nbt->response->sw != 0x9000U)
                    {
                        // clang-format off
                        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid status word for updating file access policy: 0x%04X", nbt->response->sw);
                        // clang-format on
                        ifx_apdu_response_destroy(nbt->response);
                        return IFX_ERROR(LIB_NBT_APDU, NBT_UPDATE_FAP_BYTES_WITH_PASSWORD, IFX_SW_ERROR);
                    }
                    ifx_apdu_response_destroy(nbt->response);
                }
                break;
            }
        }
        if (!fap_found)
        {
            ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "No file access policy found for file ID 0x%04X", configuration->fap[i]);
            return IFX_ERROR(LIB_NBT_APDU, NBT_UPDATE_FAP_BYTES_WITH_PASSWORD, IFX_PROGRAMMING_ERROR);
        }
    }

    // Set interface configuration
    status = nbt_select_configurator_application(nbt);
    ifx_apdu_destroy(nbt->apdu);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not select NBT configurator application");
        return status;
    }
    if (nbt->response->sw != 0x9000U)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid status word for selecting NBT configurator application: 0x%04X", nbt->response->sw);
        ifx_apdu_response_destroy(nbt->response);
        return IFX_ERROR(LIB_NBT_APDU, NBT_SELECT_CONFIGURATOR, IFX_SW_ERROR);
    }
    ifx_apdu_response_destroy(nbt->response);
    status = nbt_set_configuration(nbt, NBT_TAG_COMMUNICATION_INTERFACE_ENABLE, configuration->communication_interface);
    ifx_apdu_destroy(nbt->apdu);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not configure NBT interface availability");
        return status;
    }
    if (nbt->response->sw != 0x9000U)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid status word for configuring NBT interface availability: 0x%04X", nbt->response->sw);
        ifx_apdu_response_destroy(nbt->response);
        return IFX_ERROR(LIB_NBT_APDU, NBT_SET_CONFIGURATION, IFX_SW_ERROR);
    }
    ifx_apdu_response_destroy(nbt->response);
    status = nbt_set_configuration(nbt, NBT_TAG_GPIO_FUNCTION, configuration->irq_function);
    ifx_apdu_destroy(nbt->apdu);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not configure NBT GPIO/IRQ functionality");
        return status;
    }
    if (nbt->response->sw != 0x9000U)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid status word for configuring NBT GPIO/IRQ functionality: 0x%04X", nbt->response->sw);
        ifx_apdu_response_destroy(nbt->response);
        return IFX_ERROR(LIB_NBT_APDU, NBT_SET_CONFIGURATION, IFX_SW_ERROR);
    }
    ifx_apdu_response_destroy(nbt->response);

    return IFX_SUCCESS;
}

/**
 * \brief Reads data from NBT file.
 *
 * \details Combines nbt_select_file_by_id() and (potentially) multiple calls to nbt_read_binary() to get file's contents.
 *
 * \param[in] nbt NBT command abstraction.
 * \param[in] file_id NBT file to be read.
 * \param[in] offset Offset within NBT file.
 * \param[in] length Number of bytes to read.
 * \param[out] buffer Buffer to store response in (must be large enought to hold \c length number of bytes).
 * \return ifx_status_t \c IFX_SUCCESS if successful, any other value in case of error.
 * \see nbt_select_file_by_id()
 * \see nbt_read_binary()
 */
ifx_status_t nbt_read_file(nbt_cmd_t *nbt, enum nbt_fileid file_id, uint16_t offset, size_t length, uint8_t *buffer)
{
    // Validate parameters
    if ((nbt == NULL) || (buffer == NULL) || ((offset + length) > 4096U))
    {
        return IFX_ERROR(LIB_NBT_APDU, NBT_READ_BINARY, IFX_ILLEGAL_ARGUMENT);
    }

    // Select file to be read
    ifx_status_t status = nbt_select_file(nbt, file_id);
    ifx_apdu_destroy(nbt->apdu);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not select NBT file 0x%04X", file_id);
        return status;
    }
    if (nbt->response->sw != 0x9000U)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid status word for selecting NBT file 0x%04X: 0x%04X", file_id, nbt->response->sw);
        ifx_apdu_response_destroy(nbt->response);
        return IFX_ERROR(LIB_NBT_APDU, NBT_READ_BINARY, IFX_SW_ERROR);
    }
    ifx_apdu_response_destroy(nbt->response);

    // Actually read file in chunks
    for (size_t chunk_offset = 0U; chunk_offset < length; chunk_offset += 0xFFU)
    {
        uint8_t chunk_len = ((length - (offset + chunk_offset)) < 0xFFU) ? (length - (offset + chunk_offset)) : 0xFFU;
        status = nbt_read_binary(nbt, offset + chunk_offset, chunk_len);
        ifx_apdu_destroy(nbt->apdu);
        if (ifx_error_check(status))
        {
            ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not read NBT file 0x%04X", file_id);
            return status;
        }
        if (nbt->response->sw != 0x9000U)
        {
            ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid status word for reading NBT file 0x%04X: 0x%04X", file_id, nbt->response->sw);
            ifx_apdu_response_destroy(nbt->response);
            return IFX_ERROR(LIB_NBT_APDU, NBT_READ_BINARY, IFX_SW_ERROR);
        }
        if (nbt->response->len != chunk_len)
        {
            ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid data in NBT file 0x%04X", file_id);
            ifx_apdu_response_destroy(nbt->response);
            return IFX_ERROR(LIB_NBT_APDU, NBT_READ_BINARY, IFX_PROGRAMMING_ERROR);
        }
        memcpy(buffer + chunk_offset, nbt->response->data, chunk_len);
        ifx_apdu_response_destroy(nbt->response);
    }
    return IFX_SUCCESS;
}

/**
 * \brief Writes data to NBT file.
 *
 * \details Combines nbt_select_file_by_id() and (potentially) multiple calls to nbt_update_binary() to set file's contents.
 *
 * \param[in] nbt NBT command abstraction.
 * \param[in] file_id NBT file to be written.
 * \param[in] offset Offset within NBT file.
 * \param[in] data Data to be written.
 * \param[in] length Number of bytes in \c data.
 * \return ifx_status_t \c IFX_SUCCESS if successful, any other value in case of error.
 * \see nbt_select_file_by_id()
 * \see nbt_update_binary()
 */
ifx_status_t nbt_write_file(nbt_cmd_t *nbt, enum nbt_fileid file_id, uint16_t offset, const uint8_t *data, size_t length)
{
    // Validate parameters
    if ((nbt == NULL) || (data == NULL) || ((offset + length) > 4096U))
    {
        return IFX_ERROR(LIB_NBT_APDU, NBT_UPDATE_BINARY, IFX_ILLEGAL_ARGUMENT);
    }

    // Select file to be written
    ifx_status_t status = nbt_select_file(nbt, file_id);
    ifx_apdu_destroy(nbt->apdu);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not select NBT file 0x%04X", file_id);
        return status;
    }
    if (nbt->response->sw != 0x9000U)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid status word for selecting NBT file 0x%04X: 0x%04X", file_id, nbt->response->sw);
        ifx_apdu_response_destroy(nbt->response);
        return IFX_ERROR(LIB_NBT_APDU, NBT_UPDATE_BINARY, IFX_SW_ERROR);
    }
    ifx_apdu_response_destroy(nbt->response);

    // Actually write file in chunks
    for (size_t chunk_offset = 0U; chunk_offset < length; chunk_offset += 0xFFU)
    {
        uint8_t chunk_len = ((length - (offset + chunk_offset)) < 0xFFU) ? (length - (offset + chunk_offset)) : 0xFFU;
        status = nbt_update_binary(nbt, offset + chunk_offset, chunk_len, (uint8_t *) (data + chunk_offset));
        ifx_apdu_destroy(nbt->apdu);
        if (ifx_error_check(status))
        {
            ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not write NBT file 0x%04X", file_id);
            return status;
        }
        if (nbt->response->sw != 0x9000U)
        {
            ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid status word for writing NBT file 0x%04X: 0x%04X", file_id, nbt->response->sw);
            ifx_apdu_response_destroy(nbt->response);
            return IFX_ERROR(LIB_NBT_APDU, NBT_UPDATE_BINARY, IFX_SW_ERROR);
        }
        ifx_apdu_response_destroy(nbt->response);
    }
    return IFX_SUCCESS;
}

/**
 * \brief Retrieves available APDU received via pass-through mode.
 *
 * \details Wraps calls to nbt_pass_through_fetch_data() and nbt_pass_through_decode_apdu_bytes() and performs necessary cleanup.
 *
 * \param[in] nbt NBT command abstraction.
 * \param[out] apdu_buffer Buffer to store APDU in.
 * \return ifx_status_t \c IFX_SUCCESS if successful, any other value in case of error.
 * \see nbt_pass_through_fetch_data()
 * \see nbt_pass_through_decode_apdu_bytes()
 */
ifx_status_t nbt_get_passthrough_apdu(nbt_cmd_t *nbt, ifx_apdu_t *apdu_buffer)
{
    if ((nbt == NULL) || (apdu_buffer == NULL))
    {
        return IFX_ERROR(LIB_NBT_APDU, NBT_PASS_THROUGH_FETCH_DATA, IFX_ILLEGAL_ARGUMENT);
    }

    // Fetch generic data from NBT
    ifx_apdu_response_t apdu_response = {0};
    ifx_status_t status = nbt_pass_through_fetch_data(nbt, &apdu_response);
    ifx_apdu_destroy(nbt->apdu);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not fetch pass-through data from NBT");
        return status;
    }
    if (apdu_response.sw != 0x9000U)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid status word for fetching pass-through data from NBT: 0x%04X", apdu_response.sw);
        ifx_apdu_response_destroy(&apdu_response);
        return IFX_ERROR(LIB_NBT_APDU, NBT_PASS_THROUGH_FETCH_DATA, IFX_SW_ERROR);
    }

    // Parse command data
    ifx_blob_t blob = {0};
    status = nbt_pass_through_decode_apdu_bytes(&apdu_response, &blob);
    ifx_apdu_response_destroy(&apdu_response);
    if (ifx_error_check(status) || (blob.buffer == NULL) || (blob.length == 0U))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not parse APDU request received via pass-through mode");
        return status;
    }
    status = ifx_apdu_decode(apdu_buffer, blob.buffer, blob.length);
    free(blob.buffer);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Data received via pass-through modeis not in APDU format");
        return status;
    }
    return IFX_SUCCESS;
}

/**
 * \brief Sets APDU response for pass-through mode.
 *
 * \details Wraps calls to nbt_pass_through_put_response() and performs necessary cleanup.
 *
 * \param[in] nbt NBT command abstraction.
 * \param[out] response APDU response to be sent via pass-through mode.
 * \return ifx_status_t \c IFX_SUCCESS if successful, any other value in case of error.
 * \see nbt_pass_through_put_response()
 */
ifx_status_t nbt_set_passthrough_response(nbt_cmd_t *nbt, ifx_apdu_response_t *response)
{
    if ((nbt == NULL) || (response == NULL))
    {
        return IFX_ERROR(LIB_NBT_APDU, NBT_PASS_THROUGH_PUT_RESPONSE, IFX_ILLEGAL_ARGUMENT);
    }
    ifx_apdu_response_t pt_response = {0};
    ifx_status_t status = nbt_pass_through_put_response(nbt, response, &pt_response);
    ifx_apdu_destroy(nbt->apdu);
    if (ifx_error_check(status))
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Could not send pass-through response to NBT");
        return status;
    }
    if (pt_response.sw != 0x9000U)
    {
        ifx_logger_log(ifx_logger_default, LOG_TAG, IFX_LOG_ERROR, "Invalid status word for sending pass-through response to NBT: 0x%04X", pt_response.sw);
        ifx_apdu_response_destroy(&pt_response);
        return IFX_ERROR(LIB_NBT_APDU, NBT_PASS_THROUGH_PUT_RESPONSE, IFX_SW_ERROR);
    }
    ifx_apdu_response_destroy(&pt_response);
    return IFX_SUCCESS;
}
