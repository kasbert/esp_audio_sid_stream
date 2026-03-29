# esp_audio_sid_stream
ESP-ADF input stream driver for Commodore 64 SID files

You need to dowload esp-adf first. e.g.
```
git clone https://github.com/espressif/esp-adf.git
export ADF_PATH=$(pwd)/esp-adf

git clone https://github.com/kasbert/esp_audio_sid_stream
cd esp_audio_sid_stream/examples/play_mp3_control
idf.py update-dependencies
idf.py build
idf.py -p /dev/ttyUSB0 build flash monitor
```