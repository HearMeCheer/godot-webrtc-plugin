#include "OpusEncoder.hpp"

#include <opus.h>

#include "utils.hpp"

namespace opus
{

class Encoder::Impl
{
public:
	// Constructor
	bool init(opus_int32 sample_rate, int channels);
	void close();

	// Destructor
	~Impl();

	void setBitrate(int32_t bitrate);

	// Set encoder complexity (0 to 10)
	void setComplexity(int complexity);

	// Set packet loss percentage (0 to 100)
	void setPacketLossPerc(int pl_perc);

	// Encode PCM data (int16)
	ErrorCode encode(std::vector<unsigned char>& output, const std::vector<int16_t>& pcm_data,
					 int frame_size);

	// Encode PCM data (float)
	ErrorCode encodeFloat(std::vector<unsigned char>& output, const float* pcm_data,
						  int frame_size);

private:
	OpusDecoder* m_decoder = nullptr;
	OpusEncoder* m_encoder = nullptr;
	int m_channels = 0;
	opus_int32 m_sample_rate = 0;
};

bool Encoder::Impl::init(opus_int32 sample_rate, int channels)
{
	close();

	int err = OPUS_OK;
	PrintUtility::print_verbose("webrtc-native: opus_decoder_create args: sample_rate % channels %",
							   sample_rate, channels);
	m_sample_rate = sample_rate;
	m_channels = channels;
	int application = OPUS_APPLICATION_AUDIO;
	m_encoder = opus_encoder_create(sample_rate, channels, application, &err);
	if (err != OPUS_OK || m_encoder == nullptr)
	{
		const char* err_message = opus_strerror(err);
		PrintUtility::print_verbose("webrtc-native: opus_encoder_create failed with error: % (%)",
								   err, err_message);

		return false;
	}

	return true;
}

void Encoder::Impl::close()
{
	if (m_encoder)
	{
		opus_encoder_destroy(m_encoder);
		m_encoder = nullptr;
	}
}

Encoder::Impl::~Impl() { close(); }

void Encoder::Impl::setBitrate(int32_t bitrate)
{
	int err = opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(bitrate));
	if (err != OPUS_OK)
	{
		const char* err_message = opus_strerror(err);
		PrintUtility::print_verbose(
				"webrtc-native: opus_encoder: setBitrate failed with error: % (%)", err,
				err_message);
	}
}

// Set encoder complexity (0 to 10)
void Encoder::Impl::setComplexity(int complexity)
{
	int err = opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(complexity));
	if (err != OPUS_OK)
	{
		const char* err_message = opus_strerror(err);
		PrintUtility::print_verbose(
				"webrtc-native: opus_encoder: setComplexity failed with error: % (%)", err,
				err_message);
	}
}

// Set packet loss percentage (0 to 100)
void Encoder::Impl::setPacketLossPerc(int pl_perc)
{
	int err = opus_encoder_ctl(m_encoder, OPUS_SET_PACKET_LOSS_PERC(pl_perc));
	if (err != OPUS_OK)
	{
		const char* err_message = opus_strerror(err);
		PrintUtility::print_verbose(
				"webrtc-native: opus_encoder: setPacketLoss failed with error: % (%)", err,
				err_message);
	}
}

// Encode PCM data (int16)
ErrorCode Encoder::Impl::encode(std::vector<unsigned char>& output,
								const std::vector<int16_t>& pcm_data, int frame_size)
{
	if (pcm_data.empty())
	{
		PrintUtility::print_verbose("PCM data is empty.");
		return ErrorCode::Failed;
	}

	const int max_packet_size = 4000; // Maximum packet size for Opus
	output.resize(max_packet_size);

	int num_samples = frame_size * m_channels;
	if (pcm_data.size() < static_cast<size_t>(num_samples))
	{
		PrintUtility::print_verbose("PCM data size is less than expected frame size.");
		return ErrorCode::Failed;
	}

	int bytes_encoded =
			opus_encode(m_encoder, pcm_data.data(), frame_size, output.data(), max_packet_size);

	if (bytes_encoded < 0)
	{
		PrintUtility::print_verbose("Opus encoding failed: %", opus_strerror(bytes_encoded));
		return ErrorCode::Failed;
	}

	output.resize(bytes_encoded);
	return ErrorCode::Ok;
}

// Encode PCM data (float)
ErrorCode Encoder::Impl::encodeFloat(std::vector<unsigned char>& output, const float* pcm_data,
									 int frame_size)
{
	// if (pcm_data.empty()) {
	// 	PrintUtiity::print_verbose("PCM data is empty.");
	// 	return ErrorCode::Failed;
	// }

	const int max_packet_size = 4000; // Maximum packet size for Opus
	output.resize(max_packet_size);

	int num_samples = frame_size * m_channels;
	// if (pcm_data.size() < static_cast<size_t>(num_samples)) {
	// 	PrintUtiity::print_verbose("PCM data size is less than expected frame size.");
	// 	return ErrorCode::Failed;
	// }

	int bytes_encoded =
			opus_encode_float(m_encoder, pcm_data, frame_size, output.data(), max_packet_size);

	if (bytes_encoded < 0)
	{
		PrintUtility::print_verbose("Opus encoding failed: %", opus_strerror(bytes_encoded));
		return ErrorCode::Failed;
	}

	output.resize(bytes_encoded);
	return ErrorCode::Ok;
}

// ============================================================================

Encoder::Encoder() : impl(std::make_unique<Impl>()) {}

Encoder::~Encoder() {}

bool Encoder::init(int32_t sample_rate, int16_t channels)
{
	return impl->init(sample_rate, channels);
}

// Set encoder bitrate (bits per second)
void Encoder::setBitrate(int32_t bitrate) { impl->setBitrate(bitrate); }

// Set encoder complexity (0 to 10)
void Encoder::setComplexity(int complexity) { impl->setComplexity(complexity); }

// Set packet loss percentage (0 to 100)
void Encoder::setPacketLossPerc(int pl_perc) { impl->setPacketLossPerc(pl_perc); }

// Encode PCM data (int16)
ErrorCode Encoder::encode(std::vector<unsigned char>& output, const std::vector<int16_t>& pcm_data,
						  int frame_size)
{
	return impl->encode(output, pcm_data, frame_size);
}

// Encode PCM data (float)
ErrorCode Encoder::encodeFloat(std::vector<unsigned char>& output, const float* pcm_data,
							   int frame_size)
{
	return impl->encodeFloat(output, pcm_data, frame_size);
}

} //namespace opus
