#include "daisy_seed.h"
#include "generic_usb_port.h"
extern "C" {
#include "global.h"
#include "generic_usb_audio.h"
#include "generic_usb_cdc.h"
#include "generic_usb_sd.h"
}

using namespace daisy;

DaisySeed hw;

static void AudioCallback(AudioHandle::InputBuffer in,
                          AudioHandle::OutputBuffer out,
                          size_t size)
{
    int16_t mic[GENERIC_USB_AUDIO_FRAMES_PER_MS * GENERIC_USB_AUDIO_MIC_CHANNELS];
    int16_t spk[GENERIC_USB_AUDIO_FRAMES_PER_MS * GENERIC_USB_AUDIO_SPK_CHANNELS];

    const size_t frames = size > GENERIC_USB_AUDIO_FRAMES_PER_MS
                            ? GENERIC_USB_AUDIO_FRAMES_PER_MS
                            : size;

    for(size_t i = 0; i < frames; i++)
    {
        mic[2 * i + 0] = static_cast<int16_t>(in[0][i] * 32767.0f);
        mic[2 * i + 1] = static_cast<int16_t>(in[1][i] * 32767.0f);
    }

    AudioIF_PushSamples(mic, frames);
    const uint32_t got = AudioIF_PopSamples(spk, frames);

    for(size_t i = 0; i < frames; i++)
    {
        if(i < got)
        {
            out[0][i] = static_cast<float>(spk[2 * i + 0]) / 32768.0f;
            out[1][i] = static_cast<float>(spk[2 * i + 1]) / 32768.0f;
        }
        else
        {
            out[0][i] = 0.0f;
            out[1][i] = 0.0f;
        }
    }
}

int main(void)
{
    hw.Init(true);

    hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.SetAudioBlockSize(48);
    hw.StartAudio(AudioCallback);

    GenericUSB_Init();
    GenericUSB_SDInit();

    uint32_t last = 0;
    bool led = false;

    while(1)
    {
        uint32_t now = System::GetNow();
        GenericUSB_SetNowMs(now);
        GenericUSB_Task();
        GenericUSB_SDTask();

        if(now - last >= 250)
        {
            last = now;
            led = !led;
            hw.SetLed(led);
#if DEBUG_TEST_CDC
            GenericUSB_CDC_WriteString(led ? "LED: ON\r\n" : "LED: OFF\r\n");
            GenericUSB_CDC_Flush();
#endif
        }
    }    
}
