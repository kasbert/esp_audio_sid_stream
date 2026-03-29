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
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "audio_mem.h"
#include "audio_sys.h"
#include "audio_error.h"
#include "audio_element.h"
#include "esp_log.h"
#include "esp_err.h"

#include "audio_idf_version.h"
#include "sid_stream.h"
#include "libcsid.h"

static const char *TAG = "SID_STREAM";

typedef struct sid_stream {
    sid_stream_cfg_t    config;
    bool                is_open;
    int                 framecount;
 } sid_stream_t;

//

static int _sid_read(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    sid_stream_t *sid_stream = (sid_stream_t *)audio_element_getdata(self);
    //ESP_LOGI(TAG, "Read SID stream, len %d %p, ticks_to_wait %d", len, buffer, ticks_to_wait);

    //esp_err_t res = ESP_OK;
    AUDIO_NULL_CHECK(TAG, buffer, return ESP_FAIL);
    if (sid_stream->config.sid_config.bit_depth == 16) {
        libcsid_render16((unsigned short *)buffer, len / 2);
    } else {
        libcsid_render8((unsigned char *)buffer, len);
    }

    return len;
}

static int _sid_process(audio_element_handle_t self, char *buffer, int buffer_len)
{
    //ESP_LOGI(TAG, "Process SID stream, in_len %d %p", buffer_len, buffer);
    sid_stream_t *sid_stream = (sid_stream_t *)audio_element_getdata(self);
    int r_size = audio_element_input(self, buffer, buffer_len);
    ESP_LOGD(TAG, "SID process size %d data sample: %02x %02x %02x %02x", 
        r_size, (int)buffer[0], (int)buffer[1], (int)buffer[2], (int)buffer[3]);
    if (!sid_stream->framecount) {
        // startup delay to wait for the next element to be ready
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    sid_stream->framecount++;
    int w_size = 0;
    if (r_size > 0) {
        w_size = audio_element_output(self, buffer, r_size);
        //ESP_LOGI(TAG, "Write data to next element, w_size %d", w_size);
    } else {
        w_size = r_size;
    }
    return w_size;
}

static esp_err_t _sid_open(audio_element_handle_t self)
{
    ESP_LOGI(TAG, "Open SID stream");
    esp_err_t res = ESP_OK;
    sid_stream_t *sid_stream = (sid_stream_t *)audio_element_getdata(self);
    sid_stream->framecount = 0;
    if (sid_stream->is_open) {
        return ESP_OK;
    }
    res = audio_element_set_input_timeout(self, 2000 / portTICK_RATE_MS);
    sid_stream->is_open = true;
    return res;
}

static esp_err_t _sid_close(audio_element_handle_t self)
{
    ESP_LOGI(TAG, "Close SID stream");
    sid_stream_t *sid_stream = (sid_stream_t *)audio_element_getdata(self);
    esp_err_t res = ESP_OK;
    sid_stream->is_open = false;
    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
        audio_element_report_pos(self);
        audio_element_set_byte_pos(self, 0);
    }
    return res;
}

esp_err_t sid_stream_load(audio_element_handle_t self, char *data, int data_size)
{
    //sid_stream_t *sid_stream = (sid_stream_t *)audio_element_getdata(self);
    esp_err_t res = libcsid_load((unsigned char *)data, data_size, 0);
    if (res) {
        ESP_LOGE(TAG, "libcsid_load %d", res);
    } else {
        ESP_LOGI(TAG, "SID Title: %s", libcsid_gettitle());
        ESP_LOGI(TAG, "SID Author: %s", libcsid_getauthor());
        ESP_LOGI(TAG, "SID Info: %s", libcsid_getinfo());
    }
    return res;
}


static esp_err_t _sid_destroy(audio_element_handle_t self)
{
    sid_stream_t *sid_stream = (sid_stream_t *)audio_element_getdata(self);
    ESP_LOGI(TAG, "Destroy SID stream");
    esp_err_t res = ESP_OK;
    if (sid_stream->is_open) {
        res = _sid_close(self);
    }
    audio_free(sid_stream);
    libcsid_deinit();
    return res;
}

audio_element_handle_t sid_stream_init(sid_stream_cfg_t *config)
{
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();

    cfg.destroy = _sid_destroy;
    cfg.open = _sid_open;
    cfg.close = _sid_close;
    cfg.process = _sid_process;
    cfg.read = _sid_read;
    cfg.tag = "sid";
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.buffer_len = config->buffer_len;
    cfg.stack_in_ext = config->ext_stack;

    sid_stream_t *sid_stream = audio_calloc(1, sizeof(sid_stream_t));
    AUDIO_NULL_CHECK(TAG, sid_stream, return NULL);
    memcpy(&sid_stream->config, config, sizeof(sid_stream_cfg_t));

    audio_element_handle_t el = audio_element_init(&cfg);
    audio_element_setdata(el, sid_stream);

    // allocates 64k
    libcsid_init(config->sid_config.framerate, SIDMODEL_6581);

    ESP_LOGD(TAG, "sid_stream_init init,el:%p", el);
    return el;
}

