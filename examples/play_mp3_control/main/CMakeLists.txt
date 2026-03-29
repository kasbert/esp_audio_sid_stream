idf_component_register(SRCS "play_mp3_control_example.c"
                    PRIV_REQUIRES esp_audio_sid_stream esp_peripherals esp_audio_dac_stream
                    EMBED_TXTFILES commando.sid M_U_L_E.sid)