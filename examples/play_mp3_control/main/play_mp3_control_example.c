/* Play mp3 file by audio pipeline
   with possibility to start, stop, pause and resume playback
   as well as adjust volume

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "dac_stream.h"
#include "sid_stream.h"
#include "esp_peripherals.h"
#include "periph_touch.h"
#include "periph_adc_button.h"
#include "periph_button.h"
#include "periph_console.h"
//#include "board.h"

static const char *TAG = "PLAY_FLASH_MP3_CONTROL";

audio_pipeline_handle_t pipeline;
audio_element_handle_t dac_stream_writer, sid_stream_reader;

static struct marker {
    int pos;
    const uint8_t *start;
    const uint8_t *end;
} file_marker;

extern const uint8_t commando_sid_start[] asm("_binary_commando_sid_start");
extern const uint8_t commando_sid_end[]   asm("_binary_commando_sid_end");

extern const uint8_t M_U_L_E_start[] asm("_binary_M_U_L_E_sid_start");
extern const uint8_t M_U_L_E_end[]   asm("_binary_M_U_L_E_sid_end");


static void set_next_file_marker()
{
    static int idx = 0;

    switch (idx) {
        case 0:
            file_marker.start = commando_sid_start;
            file_marker.end   = commando_sid_end;
            break;
        case 1:
            file_marker.start = M_U_L_E_start;
            file_marker.end   = M_U_L_E_end;
            break;
        default:
            ESP_LOGE(TAG, "[ * ] Not supported index = %d", idx);
    }
    if (++idx > 1) {
        idx = 0;
    }
    file_marker.pos = 0;
}

int mp3_music_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    int read_size = file_marker.end - file_marker.start - file_marker.pos;
    if (read_size == 0) {
        return AEL_IO_DONE;
    } else if (len < read_size) {
        read_size = len;
    }
    memcpy(buf, file_marker.start + file_marker.pos, read_size);
    file_marker.pos += read_size;
    return read_size;
}

static esp_err_t cli_play(esp_periph_handle_t periph, int argc, char *argv[])
{
    ESP_LOGI(TAG, "app_audio play");
    /*
    int index = 0;
    if (argc == 1) {
        index = atoi(argv[0]);
    } else {
        ESP_LOGE(TAG, "Invalid play parameter");
        return ESP_ERR_INVALID_ARG;
    }
    */

    ESP_LOGI(TAG, "[ * ] [mode] tap event");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    audio_pipeline_reset_ringbuffer(pipeline);
    audio_pipeline_reset_elements(pipeline);
    set_next_file_marker();
    sid_stream_load(sid_stream_reader, (char *)file_marker.start, file_marker.end - file_marker.start);
    audio_pipeline_run(pipeline);
    return ESP_OK;
}

static esp_err_t cli_pause(esp_periph_handle_t periph, int argc, char *argv[])
{
    audio_pipeline_pause(pipeline);
    ESP_LOGI(TAG, "app_audio paused");
    return ESP_OK;
}

static esp_err_t cli_resume(esp_periph_handle_t periph, int argc, char *argv[])
{
    audio_pipeline_resume(pipeline);
    ESP_LOGI(TAG, "app_audio resume");
    return ESP_OK;
}

static esp_err_t cli_stop(esp_periph_handle_t periph, int argc, char *argv[])
{
    audio_pipeline_stop(pipeline);
    ESP_LOGI(TAG, "app_audio stop");
    return ESP_OK;
}

static esp_err_t get_pos(esp_periph_handle_t periph, int argc, char *argv[])
{
    int pos = 0;
    //esp_audio_time_get(player, &pos);
    ESP_LOGI(TAG, "Current time position is %d second", pos / 1000);
    return ESP_OK;
}

static esp_err_t sys_reset(esp_periph_handle_t periph, int argc, char *argv[])
{
    esp_restart();
    return ESP_OK;
}

static esp_err_t show_free_mem(esp_periph_handle_t periph, int argc, char *argv[])
{
    AUDIO_MEM_SHOW(TAG);
    return ESP_OK;
}

const periph_console_cmd_t cli_cmd[] = {
    /* ======================== Esp_audio ======================== */
    { .cmd = "play",        .id = 1, .help = "Play music",               .func = cli_play },
    { .cmd = "pause",       .id = 2, .help = "Pause",                    .func = cli_pause },
    { .cmd = "resume",      .id = 3, .help = "Resume",                   .func = cli_resume },
    { .cmd = "stop",        .id = 3, .help = "Stop player",              .func = cli_stop },
    //{ .cmd = "setvol",      .id = 4, .help = "Set volume",               .func = cli_set_vol },
    //{ .cmd = "getvol",      .id = 5, .help = "Get volume",               .func = cli_get_vol },
    { .cmd = "getpos",      .id = 6, .help = "Get position by seconds",  .func = get_pos },

    /* ======================== System ======================== */
    { .cmd = "reboot",      .id = 30, .help = "Reboot system",            .func = sys_reset },
    { .cmd = "free",        .id = 31, .help = "Get system free memory",   .func = show_free_mem },
#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    { .cmd = "stat",        .id = 32, .help = "Show processor time of all FreeRTOS tasks",      .func = run_time_stats },
#endif
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    { .cmd = "tasks",       .id = 33, .help = "Get information about running tasks",            .func = task_list },
#endif
};

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set("*", ESP_LOG_DEBUG);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[ 2 ] Create audio pipeline, add all elements to pipeline, and subscribe pipeline event");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    const int framerate = 22050;
    const int bit_depth = 8;
    const int channels = 1;

    ESP_LOGI(TAG, "[2.1] Create sid stream to decode sid file and set custom read callback");
    sid_stream_cfg_t sid_cfg = SID_STREAM_CFG_DEFAULT();
    sid_cfg.sid_config.framerate = framerate;
    sid_cfg.sid_config.bit_depth = bit_depth;
    sid_stream_reader = sid_stream_init(&sid_cfg);
    //audio_element_set_read_cb(sid_stream, sid_music_read_cb, NULL);

    ESP_LOGI(TAG, "[2.2] Create dac stream to write data to dac");
    dac_stream_cfg_t dac_cfg = DAC_STREAM_CFG_DEFAULT();
    dac_cfg.dac_config.enable_left = true;
    dac_cfg.dac_config.enable_right = false;
    dac_cfg.dac_config.output_type = DAC_OUTPUT_TYPE_MONO_MIX;
    dac_stream_writer = dac_stream_init(&dac_cfg);
    dac_stream_set_clk(dac_stream_writer, framerate, bit_depth, channels);

    ESP_LOGI(TAG, "[2.3] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, sid_stream_reader, "sid");
    audio_pipeline_register(pipeline, dac_stream_writer, "dac");

    ESP_LOGI(TAG, "[2.4] Link it together [sid_music_read_cb]-->sid_stream-->dac_stream-->[codec_chip]");
    const char *link_tag[2] = {"sid", "dac"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

    ESP_LOGI(TAG, "[ 3 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[3.1] Initialize keys on board");
    //audio_board_key_init(set);

    ESP_LOGI(TAG, "[3.2] Register console to peripherals");
    periph_console_cfg_t console_cfg = {
        .command_num = sizeof(cli_cmd) / sizeof(periph_console_cmd_t),
        .commands = cli_cmd,
    };
    esp_periph_handle_t console_handle = periph_console_init(&console_cfg);

    ESP_LOGI(TAG, "[3.3] Start console, please input ...");
    esp_periph_start(set, console_handle);
    
    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGW(TAG, "[ 5 ] Tap touch buttons to control music player:");
    ESP_LOGW(TAG, "      [Play] to start, pause and resume, [Set] to stop.");
    ESP_LOGW(TAG, "      [Vol-] or [Vol+] to adjust volume.");

    ESP_LOGI(TAG, "[ 5.1 ] Start audio_pipeline");

    set_next_file_marker();
    sid_stream_load(sid_stream_reader, (char *)file_marker.start, file_marker.end - file_marker.start);

    audio_pipeline_run(pipeline);

    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            continue;
        }
        ESP_LOGI(TAG, "[ * ] Event received from %s, cmd = %d, data = %p",
                audio_element_get_tag(msg.source), msg.cmd, msg.data);
        // TODO Handle event
    }

    ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    audio_pipeline_unregister(pipeline, sid_stream_reader);
    audio_pipeline_unregister(pipeline, dac_stream_writer);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Make sure audio_pipeline_remove_listener is called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(dac_stream_writer);
    audio_element_deinit(sid_stream_reader);
}
