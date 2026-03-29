/*
 *  * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _SID_STREAM_H_
#define _SID_STREAM_H_

#include <stdint.h>
#include "audio_element.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      SID audio configurations
 *           
 */
typedef struct
{
    int framerate;            /*!< Audio framerate (Hz) */
    int bit_depth;            /*!< Audio bit depth (8, 16, 32) */
} sid_config_t;

/**
 * @brief      SID Stream configurations
 *             Default value will be used if any entry is zero
 */
typedef struct {
    sid_config_t            sid_config;         /*!<  driver configurations */
    int                     task_stack;         /*!< Task stack size */
    int                     task_core;          /*!< Task running in core (0 or 1) */
    int                     task_prio;          /*!< Task priority (based on freeRTOS priority) */
    int                     buffer_len;         /*!< dac_stream buffer length */
    bool                    ext_stack;          /*!< Allocate stack on extern ram */
} sid_stream_cfg_t;

#define SID_STREAM_TASK_STACK           (3072+512)
#define SID_STREAM_BUF_SIZE             (2048)
#define SID_STREAM_TASK_PRIO            (23)
#define SID_STREAM_TASK_CORE            (0)

#define SID_STREAM_CFG_DEFAULT() {                    \
    .sid_config = {                                   \
        .framerate = 22050,                           \
        .bit_depth = 8,                               \
    },                                                \
    .task_stack = SID_STREAM_TASK_STACK,              \
    .task_core = SID_STREAM_TASK_CORE,                \
    .task_prio = SID_STREAM_TASK_PRIO,                \
    .buffer_len =  SID_STREAM_BUF_SIZE,               \
    .ext_stack = false,                               \
}

/**
 * @brief      Initialize SID stream
 *             Only support AUDIO_STREAM_READER type
 *
 * @param      config   The SID Stream configuration
 *
 * @return     The audio element handle
 */
audio_element_handle_t sid_stream_init(sid_stream_cfg_t *config);

esp_err_t sid_stream_load(audio_element_handle_t sid_stream, char *data, int data_size);

/**
 * @brief      Setup clock for SID Stream, this function is only used with handle created by `sid_stream_init`
 *
 * @param[in]  sid_stream   The sid element handle
 * @param[in]  rate  Clock rate (in Hz)
 * @param[in]  bits  Audio bit width (16, 32)
 * @param[in]  ch    Number of Audio channels (1: Mono, 2: Stereo)
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t sid_stream_set_data(audio_element_handle_t sid_stream, int rate, int bits, int ch);

#ifdef __cplusplus
}
#endif

#endif
