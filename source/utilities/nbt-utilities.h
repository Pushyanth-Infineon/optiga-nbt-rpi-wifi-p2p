// SPDX-FileCopyrightText: 2024 Infineon Technologies AG
// SPDX-License-Identifier: MIT

/**
 * \file nbt-utilities.h
 * \brief General utilities for interacting with an NBT.
 */
#ifndef NBT_UTILITIES_H
#define NBT_UTILITIES_H

#include <stddef.h>
#include <stdint.h>

#include "infineon/nbt-apdu.h"
#include "infineon/nbt-cmd-config.h"
#include "infineon/nbt-cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Default I2C address of NBT device.
 */
#define NBT_DEFAULT_I2C_ADDRESS 0x18U

/** \struct nbt_configuration
 * \brief Simple configuration struct to set NBT to desired state.
 *
 * \see nbt_configure()
 */
struct nbt_configuration
{
    /**
     * \brief File access policies to be set.
     */
    nbt_file_access_policy_t **fap;

    /**
     * \brief Number of file access policies in nbt_configuration.fap.
     */
    size_t fap_len;

    /**
     * \brief NBT interface configuration (availability over interface).
     */
    nbt_communication_interface_tags communication_interface;

    /**
     * \brief NBT interrupt pin configuration.
     */
    nbt_gpio_function_tags irq_function;
};

/** \enum nbt_fileid
 * \brief File IDs for different NBT files.
 */
enum nbt_fileid
{
    /**
     * \brief NFC Capability Container (CC).
     */
    NBT_FILEID_CC = 0xE103U,

    /**
     * \brief NFC NDEF file.
     */
    NBT_FILEID_NDEF = 0xE104U,

    /**
     * \brief File Access Policy file (FAP).
     */
    NBT_FILEID_FAP = 0xE1AFU,

    /**
     * \brief NBT Proprietary file 1.
     */
    NBT_FILEID_PROPRIETARY1 = 0xE1A1U,

    /**
     * \brief NBT Proprietary file 2.
     */
    NBT_FILEID_PROPRIETARY2 = 0xE1A2U,

    /**
     * \brief NBT Proprietary file 3.
     */
    NBT_FILEID_PROPRIETARY3 = 0xE1A3U,

    /**
     * \brief NBT Proprietary file 4.
     */
    NBT_FILEID_PROPRIETARY4 = 0xE1A4U
};

/**
 * \brief Selects NBT (operational) application.
 *
 * \details Wraps select_application() and adds cleanup.
 *
 * \param[in] nbt NBT command abstraction.
 * \return ifx_status_t \c IFX_SUCCESS if successful, any other value in case of error.
 * \see nbt_select_application()
 */
ifx_status_t nbt_select_nbt_application(nbt_cmd_t *nbt);

/**
 * \brief Configures NBT according to given configuration.
 *
 * \param[in] nbt NBT abstraction for communication.
 * \param[in] configuration Desired NBT configuration to be set.
 * \return ifx_status_t \c IFX_SUCCESS if successful, any other value in case of error.
 */
ifx_status_t nbt_configure(nbt_cmd_t *nbt, const struct nbt_configuration *configuration);

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
ifx_status_t nbt_read_file(nbt_cmd_t *nbt, enum nbt_fileid file_id, uint16_t offset, size_t length, uint8_t *buffer);

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
ifx_status_t nbt_write_file(nbt_cmd_t *nbt, enum nbt_fileid file_id, uint16_t offset, const uint8_t *data, size_t length);

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
ifx_status_t nbt_get_passthrough_apdu(nbt_cmd_t *nbt, ifx_apdu_t *apdu_buffer);

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
ifx_status_t nbt_set_passthrough_response(nbt_cmd_t *nbt, ifx_apdu_response_t *response);

#ifdef __cplusplus
}
#endif

#endif // NBT_UTILITIES_H
