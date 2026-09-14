#ifndef NACL_AUDIO_WAVEFORM_COMMON_H

#define NACL_AUDIO_WAVEFORM_COMMON_H



#include <stdint.h>

#include <math.h>



#define NACL_AUDIO_WINDOW_SIZE 512  // Block size for amplitude calculations



#pragma pack(push, 1)



// Packet structure dispatched over our unified event stream

typedef struct {

    uint32_t window_size;          // Total PCM samples evaluated

    float    rms_amplitude;        // Root-Mean-Square value (0.0 to 1.0)

    float    peak_amplitude;       // Peak absolute value (0.0 to 1.0)

    float    decibels;             // Normalized amplitude in dB (-120.0f to 0.0f)

    float    waveform_samples[32]; // Downsampled envelope snapshot for visualization

} AudioWaveformFrame;



#pragma pack(pop)



/**

 * @brief Utility function to compute normalized RMS amplitude from raw 16-bit PCM

 */

static inline AudioWaveformFrame ncl_process_pcm_frame(const int16_t* pcm_samples, size_t sample_count) {

    AudioWaveformFrame frame = {0};

    frame.window_size = (uint32_t)sample_count;



    double sum_squares = 0.0;

    int16_t peak_raw = 0;



    // Process samples to find peak and sum of squares

    for (size_t i = 0; i < sample_count; ++i) {

        int16_t sample = pcm_samples[i];

        double norm_sample = (double)sample / 32768.0;

        sum_squares += norm_sample * norm_sample;



        int16_t abs_sample = (sample < 0) ? -sample : sample;

        if (abs_sample > peak_raw) {

            peak_raw = abs_sample;

        }

    }



    // Compute RMS and peak values

    double mean_square = (sample_count > 0) ? (sum_squares / sample_count) : 0.0;

    frame.rms_amplitude = (float)sqrt(mean_square);

    frame.peak_amplitude = (float)peak_raw / 32768.0f;



    // Convert to decibels with a -120dB floor

    if (frame.rms_amplitude > 0.000001f) {

        frame.decibels = 20.0f * log10f(frame.rms_amplitude);

    } else {

        frame.decibels = -120.0f;

    }



    // Generate a downsampled visual representation (32 structural bands)

    if (sample_count >= 32) {

        size_t stride = sample_count / 32;

        for (size_t i = 0; i < 32; ++i) {

            int16_t peak_stride = 0;

            for (size_t j = 0; j < stride; ++j) {

                int16_t sample = pcm_samples[i * stride + j];

                int16_t abs_sample = (sample < 0) ? -sample : sample;

                if (abs_sample > peak_stride) {

                    peak_stride = abs_sample;

                }

            }

            frame.waveform_samples[i] = (float)peak_stride / 32768.0f;

        }

    }



    return frame;

}



#endif // NACL_AUDIO_WAVEFORM_COMMON_H