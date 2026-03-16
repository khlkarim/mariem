#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <stdio.h>

#define DEVICE_FORMAT ma_format_f32
#define DEVICE_CHANNELS 2
#define DEVICE_SAMPLE_RATE 48000

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
  ma_waveform_read_pcm_frames((ma_waveform *)pDevice->pUserData, pOutput, frameCount, NULL);

  (void)pInput; /* Unused. */
}

int main(int argc, char **argv) {
  ma_waveform sineWave;
  ma_device_config deviceConfig;
  ma_device device;
  ma_waveform_config sineWaveConfig;

  deviceConfig = ma_device_config_init(ma_device_type_playback);
  deviceConfig.playback.format = DEVICE_FORMAT;
  deviceConfig.playback.channels = DEVICE_CHANNELS;
  deviceConfig.sampleRate = DEVICE_SAMPLE_RATE;
  deviceConfig.dataCallback = data_callback;
  deviceConfig.pUserData = &sineWave;

  if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
    printf("Failed to open playback device.\n");
    return -4;
  }

  printf("Device Name: %s\n", device.playback.name);

  sineWaveConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, 0.2, 220);
  ma_waveform_init(&sineWaveConfig, &sineWave);

  if (ma_device_start(&device) != MA_SUCCESS) {
    printf("Failed to start playback device.\n");
    ma_device_uninit(&device);
    return -5;
  }

  printf("Press Enter to quit...\n");
  getchar();

  ma_device_uninit(&device);
  ma_waveform_uninit(&sineWave); /* Uninitialize the waveform after the device so we don't pull it from under the device while it's being reference in the data callback. */

  (void)argc;
  (void)argv;
  return 0;
}
