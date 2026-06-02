#include "daisy_seed.h"
#include "jammate_tinyusb_port.h"
extern "C" {
#include "jammate_tinyusb_audio.h"
#include "jammate_tinyusb_cdc.h"
}

using namespace daisy;

DaisySeed hw;

static void AudioCallback(AudioHandle::InputBuffer in,
                          AudioHandle::OutputBuffer out,
                          size_t size)
{
    int16_t mic[JAMMATE_AUDIO_FRAMES_PER_MS * JAMMATE_AUDIO_MIC_CHANNELS];
    int16_t spk[JAMMATE_AUDIO_FRAMES_PER_MS * JAMMATE_AUDIO_SPK_CHANNELS];

    const size_t frames = size > JAMMATE_AUDIO_FRAMES_PER_MS
                            ? JAMMATE_AUDIO_FRAMES_PER_MS
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

    JamMate_TinyUSB_Init();

    uint32_t last = 0;
    bool led = false;

    while(1)
    {
        uint32_t now = System::GetNow();
        JamMate_TinyUSB_Task();

        if(now - last >= 250)
        {
            last = now;
            led = !led;
            hw.SetLed(led);
            JamMate_CDC_WriteString(led ? "LED: ON\r\n" : "LED: OFF\r\n");
            JamMate_CDC_Flush();
        }
    }    
}
