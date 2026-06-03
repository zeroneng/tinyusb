#include "daisy_seed.h"
#include "usb_port.h"
extern "C" {
#include "global.h"
#if USB_ENABLE_AUDIO
#include "usb_audio.h"
#endif
#include "usb_cdc.h"
#include "usb_sd.h"
}

using namespace daisy;

DaisySeed hw;

#if USB_ENABLE_AUDIO
static float ClampAudio(float x)
{
    if(x > 1.0f)
        return 1.0f;
    if(x < -1.0f)
        return -1.0f;
    return x;
}

static void AudioCallback(AudioHandle::InputBuffer in,
                          AudioHandle::OutputBuffer out,
                          size_t size)
{
    int16_t line[GENERIC_USB_AUDIO_FRAMES_PER_MS * GENERIC_USB_AUDIO_MIC_CHANNELS];
    int16_t usb_spk[GENERIC_USB_AUDIO_FRAMES_PER_MS * GENERIC_USB_AUDIO_SPK_CHANNELS];

    const size_t frames = size > GENERIC_USB_AUDIO_FRAMES_PER_MS
                            ? GENERIC_USB_AUDIO_FRAMES_PER_MS
                            : size;

    for(size_t i = 0; i < frames; i++)
    {
        line[2 * i + 0] = static_cast<int16_t>(in[0][i] * 32767.0f);
        line[2 * i + 1] = static_cast<int16_t>(in[1][i] * 32767.0f);

        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }

    AudioIF_PushSamples(line, frames);

    // Local monitoring is line-through. If the host is also sending USB
    // speaker samples, mix them into the same physical outputs.
    const uint32_t got = AudioIF_PopSamples(usb_spk, frames);
    for(size_t i = 0; i < got; i++)
    {
        const float usb_l = static_cast<float>(usb_spk[2 * i + 0]) / 32768.0f;
        const float usb_r = static_cast<float>(usb_spk[2 * i + 1]) / 32768.0f;
        out[0][i] = ClampAudio(out[0][i] + usb_l);
        out[1][i] = ClampAudio(out[1][i] + usb_r);
    }
}
#endif

int main(void)
{
    hw.Init(true);

#if USB_ENABLE_AUDIO
    hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.SetAudioBlockSize(48);
    hw.StartAudio(AudioCallback);
#endif

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
#if USB_ENABLE_CDC && DEBUG_TEST_CDC
            GenericUSB_CDC_WriteString(led ? "LED: ON\r\n" : "LED: OFF\r\n");
            GenericUSB_CDC_Flush();
#endif
        }
    }    
}
