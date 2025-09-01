#include "WavWriter.h"

static void writeWavHeader(std::ofstream& ofs, uint32_t data_size, int sample_rate, int bits_per_sample, int channels) {
    uint32_t byte_rate = sample_rate * channels * bits_per_sample / 8;
    uint16_t block_align = channels * bits_per_sample / 8;

    // RIFF chunk descriptor
    ofs.write("RIFF", 4);
    uint32_t chunk_size = 36 + data_size; // 4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
    ofs.write(reinterpret_cast<const char*>(&chunk_size), 4);
    ofs.write("WAVE", 4);

    // fmt sub-chunk
    ofs.write("fmt ", 4);
    uint32_t sub_chunk1_size = 16; // For PCM
    ofs.write(reinterpret_cast<const char*>(&sub_chunk1_size), 4);
    uint16_t audio_format = (bits_per_sample == 16) ? 1 : 3; // 1 for PCM, 3 for IEEE float
    ofs.write(reinterpret_cast<const char*>(&audio_format), 2);
    ofs.write(reinterpret_cast<const char*>(&channels), 2);
    ofs.write(reinterpret_cast<const char*>(&sample_rate), 4);
    ofs.write(reinterpret_cast<const char*>(&byte_rate), 4);
    ofs.write(reinterpret_cast<const char*>(&block_align), 2);
    ofs.write(reinterpret_cast<const char*>(&bits_per_sample), 2);

    // data sub-chunk
    ofs.write("data", 4);
    ofs.write(reinterpret_cast<const char*>(&data_size), 4);
}

void WavWriter::write(const std::string& filename, const std::vector<int16_t>& pcm_data,
                      int sample_rate, int channels) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    uint32_t data_size = static_cast<uint32_t>(pcm_data.size() * sizeof(int16_t));
    int bits_per_sample = 16;

    writeWavHeader(ofs, data_size, sample_rate, bits_per_sample, channels);

    // Write PCM data
    ofs.write(reinterpret_cast<const char*>(pcm_data.data()), data_size);
}

void WavWriter::write(const std::string& filename, const std::vector<float>& pcm_data,
                      int sample_rate, int channels) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    uint32_t data_size = static_cast<uint32_t>(pcm_data.size() * sizeof(float));
    int bits_per_sample = 32;

    writeWavHeader(ofs, data_size, sample_rate, bits_per_sample, channels);

    // Write PCM data
    ofs.write(reinterpret_cast<const char*>(pcm_data.data()), data_size);
}