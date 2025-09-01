#include "OpusDecoder.hpp"

#include <opus.h>

#include "utils.hpp"

namespace opus
{

class Decoder::Impl
{
public:
	// Constructor
	bool init(opus_int32 sample_rate, int channels);
	void close();

	// Destructor
	~Impl();

	// Decode Opus-encoded data
	std::vector<opus_int16> decode(const unsigned char* data, opus_int32 len,
								   bool decode_fec = false);
	ErrorCode decodeFloat(PcmDataFloat& outputPcm, const unsigned char* data, opus_int32 len,
						  bool decode_fec = false);

	int getDataChannels(const unsigned char* data) const;

private:
	OpusDecoder* m_decoder = nullptr;
	int m_channels = 0;
	opus_int32 m_sample_rate = 0;
};

bool Decoder::Impl::init(opus_int32 sample_rate, int channels)
{
	close();

	int err = OPUS_OK;
	PrintUtility::print_verbose("webrtc-native: opus_decoder_create args: sample_rate % channels %",
							   sample_rate, channels);
	m_sample_rate = sample_rate;
	m_channels = channels;
	m_decoder = opus_decoder_create(sample_rate, channels, &err);
	if (err != OPUS_OK || m_decoder == nullptr)
	{
		const char* err_message = opus_strerror(err);
		PrintUtility::print_verbose("webrtc-native: opus_decoder_create failed with error: % (%)",
								   err, err_message);

		return false;
	}

	return true;
}

void Decoder::Impl::close()
{
	if (m_decoder)
	{
		opus_decoder_destroy(m_decoder);
		m_decoder = nullptr;
	}
}

Decoder::Impl::~Impl() { close(); }

int Decoder::Impl::getDataChannels(const unsigned char* data) const
{
	return opus_packet_get_nb_channels(data);
}

std::vector<opus_int16> Decoder::Impl::decode(const unsigned char* data, opus_int32 len,
											  bool decode_fec)
{
	// Maximum packet size is 1500
	// Maximum number of samples per channel per frame is 5760 for 48kHz (120ms frame)
	const int MAX_FRAME_SIZE = 5760;
	std::vector<opus_int16> pcm_output(MAX_FRAME_SIZE * m_channels);

	int frame_size = opus_decode(m_decoder, data, len, pcm_output.data(), MAX_FRAME_SIZE,
								 decode_fec ? 1 : 0);

	if (frame_size < 0)
	{
		PrintUtility::print_verbose("webrtc-native: opus_decode failed with error: %",
								   opus_strerror(frame_size));
	}

	// Resize the vector to the actual number of samples decoded
	pcm_output.resize(frame_size * m_channels);
	return std::move(pcm_output);
}

ErrorCode Decoder::Impl::decodeFloat(PcmDataFloat& outputPcm, const unsigned char* data,
									 opus_int32 len, bool decode_fec)
{
	// Maximum packet size is 1500
	// Maximum number of samples per channel per frame is 5760 for 48kHz (120ms frame)
	const int MAX_FRAME_SIZE = 5760;
	outputPcm.data.resize(MAX_FRAME_SIZE * 2);

	int chan = getDataChannels(data);
	if (chan > m_channels)
	{
		PrintUtility::print_verbose(
				"webrtc-native: opus_decode_float number of channels in packet: % initialized: %",
				chan, m_channels);
		return ErrorCode::Failed;
	}

	// float avg = 0.0f;
	// for (int i=0; i<len; i++)
	// {
	//     avg += (float)data[i];
	// }
	// if (len > 0)
	//     avg /= len;
	// PrintUtiity::print_verbose("webrtc-native: opus_decode_float avg input value: %", avg);

	int frame_size = opus_decode_float(m_decoder, data, len, outputPcm.data.data(), MAX_FRAME_SIZE,
									   decode_fec ? 1 : 0);

	if (frame_size < 0)
	{
		PrintUtility::print_verbose("webrtc-native: opus_decode failed with error: %",
								   opus_strerror(frame_size));
		return ErrorCode::Failed;
	}

	// float avg = 0.0f;
	// for (float& s : pcm_output)
	// {
	//     avg += s;
	// }
	// if (!pcm_output.empty())
	//     avg /= pcm_output.size();

	// PrintUtiity::print_verbose("webrtc-native: opus_decode_float avg output value: % frame_size:
	// %", avg, frame_size);

	// Resize the vector to the actual number of samples decoded
	outputPcm.data.resize(frame_size * m_channels);
	outputPcm.frameSize = frame_size;
	outputPcm.channels = chan;

	return ErrorCode::Ok;
}

// ============================================================================

Decoder::Decoder() : impl(std::make_unique<Impl>()) {}

Decoder::~Decoder() {}

bool Decoder::init(int32_t sample_rate, int16_t channels)
{
	return impl->init(sample_rate, channels);
}

std::vector<int16_t> Decoder::decode(const unsigned char* data, int32_t len, bool decode_fec)
{
	return impl->decode(data, len, decode_fec);
}

ErrorCode Decoder::decodeFloat(PcmDataFloat& outputPcm, const unsigned char* data, int32_t len,
							   bool decode_fec)
{
	return impl->decodeFloat(outputPcm, data, len, decode_fec);
}

int Decoder::getDataChannels(const unsigned char* data) const
{
	return impl->getDataChannels(data);
}

} //namespace opus
