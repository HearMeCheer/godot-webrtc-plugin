#pragma once

#include "OpusCommon.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace opus
{

struct PcmDataFloat
{
	std::vector<float> data;
	int channels = 0;
	int frameSize = 0;

	PcmDataFloat() { data.reserve(OPUS_MAX_SAMPLE_SIZE); }

	void swap(PcmDataFloat& other)
	{
		std::swap(data, other.data);
		std::swap(channels, other.channels);
		std::swap(frameSize, other.frameSize);
	}

    size_t getNumSamples() const { return data.size(); }
    size_t getNumFrames() const { return frameSize; }
};

class Decoder
{
public:
	// Constructor
	Decoder();

	// Destructor
	~Decoder();

	bool init(int32_t sample_rate, int16_t channels);

	// Delete copy constructor and assignment operator
	Decoder(const Decoder&) = delete;
	Decoder& operator=(const Decoder&) = delete;

	// Decode Opus-encoded data
	std::vector<int16_t> decode(const unsigned char* data, int32_t len, bool decode_fec = false);
	ErrorCode decodeFloat(PcmDataFloat& outputPcm, const unsigned char* data, int32_t len,
						  bool decode_fec = false);

	int getDataChannels(const unsigned char* data) const;

private:
	class Impl;
	std::unique_ptr<Impl> impl;
};

} //namespace opus