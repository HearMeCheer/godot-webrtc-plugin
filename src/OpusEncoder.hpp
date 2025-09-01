#pragma once

#include "OpusCommon.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace opus
{

class Encoder
{
public:
	// Constructor
	Encoder();

	// Destructor
	~Encoder();

	bool init(int32_t sample_rate, int16_t channels);

	// Delete copy constructor and assignment operator
	Encoder(const Encoder&) = delete;
	Encoder& operator=(const Encoder&) = delete;

	// Encode PCM data (int16)
	ErrorCode encode(std::vector<unsigned char>& output, const std::vector<int16_t>& pcm_data,
					 int frame_size);

	// Encode PCM data (float)
	ErrorCode encodeFloat(std::vector<unsigned char>& output, const float* pcm_data,
						  int frame_size);

	// Set encoder bitrate (bits per second)
	void setBitrate(int32_t bitrate);

	// Set encoder complexity (0 to 10)
	void setComplexity(int complexity);

	// Set packet loss percentage (0 to 100)
	void setPacketLossPerc(int pl_perc);

private:
	class Impl;
	std::unique_ptr<Impl> impl;
};

} //namespace opus