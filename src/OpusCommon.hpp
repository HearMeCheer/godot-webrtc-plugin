#pragma once

namespace opus
{
constexpr static int OPUS_MAX_FRAME_SIZE = 48 * 120;
constexpr static int OPUS_MAX_NUM_CHANNELS = 2;
constexpr static int OPUS_MAX_SAMPLE_SIZE = OPUS_MAX_FRAME_SIZE * OPUS_MAX_NUM_CHANNELS;

enum class ErrorCode
{
	Ok,
	Failed
};
} //namespace opus