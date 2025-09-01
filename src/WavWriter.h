#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstdint>

class WavWriter {
public:
    // Write 16-bit integer PCM data to a WAV file
    static void write(const std::string& filename, const std::vector<int16_t>& pcm_data,
                      int sample_rate, int channels);

    // Write floating-point PCM data to a WAV file
    static void write(const std::string& filename, const std::vector<float>& pcm_data,
                      int sample_rate, int channels);
};
