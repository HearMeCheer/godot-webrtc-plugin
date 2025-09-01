/**************************************************************************/
/*  WebRTCLibPeerConnection.cpp                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "WebRTCLibPeerConnection.hpp"
#include "OpusCommon.hpp"
#include "OpusDecoder.hpp"
#include "WavWriter.h"
#include "WebRTCLibDataChannel.hpp"
#include "libwebrtc.h"
#include "utils.hpp"

// rtc:
#include <cassert>
#include <optional>
#include <regex>
#include <string>
#include <variant>

using namespace godot;
using namespace godot_webrtc;

#ifdef GDNATIVE_WEBRTC
#define OK Error::OK
#define FAILED Error::FAILED
#define ERR_UNCONFIGURED Error::ERR_UNCONFIGURED
#define ERR_INVALID_PARAMETER Error::ERR_INVALID_PARAMETER
#define VERBOSE_PRINT(str) Godot::print(str)
#else
#include <godot_cpp/variant/utility_functions.hpp>
#define VERBOSE_PRINT(str) UtilityFunctions::print_verbose(str)
#endif

namespace rtc
{

enum class LogLevel
{ // Don't change, it must match plog severity
	None = 0,
	Fatal = 1,
	Error = 2,
	Warning = 3,
	Info = 4,
	Debug = 5,
	Verbose = 6
};
struct IceServer
{
	enum class Type
	{
		Stun,
		Turn
	};
	enum class RelayType
	{
		TurnUdp,
		TurnTcp,
		TurnTls
	};

	// Any type
	IceServer(const std::string& url);

	// STUN
	IceServer(std::string hostname_, uint16_t port_);
	IceServer(std::string hostname_, std::string service_);

	// TURN
	IceServer(std::string hostname_, uint16_t port, std::string username_, std::string password_,
			  RelayType relayType_ = RelayType::TurnUdp);
	IceServer(std::string hostname_, std::string service_, std::string username_,
			  std::string password_, RelayType relayType_ = RelayType::TurnUdp);

	std::string hostname;
	uint16_t port;
	Type type;
	std::string username;
	std::string password;
	RelayType relayType;
};

struct Configuration
{
	// ICE settings
	std::vector<IceServer> iceServers;
	// std::optional<ProxyServer> proxyServer; // libnice only
	std::optional<std::string> bindAddress; // libjuice only, default any

	// Options
	// CertificateType certificateType = CertificateType::Default;
	// TransportPolicy iceTransportPolicy = TransportPolicy::All;
	bool enableIceTcp = false; // libnice only
	bool enableIceUdpMux = false; // libjuice only
	bool disableAutoNegotiation = false;
	bool forceMediaTransport = false;

	// Port range
	uint16_t portRangeBegin = 1024;
	uint16_t portRangeEnd = 65535;

	// Network MTU
	std::optional<size_t> mtu;

	// Local maximum message size for Data Channels
	std::optional<size_t> maxMessageSize;
};
struct Reliability
{
	// It true, the channel does not enforce message ordering and out-of-order delivery is allowed
	bool unordered = false;

	// If both maxPacketLifeTime or maxRetransmits are unset, the channel is reliable.
	// If either maxPacketLifeTime or maxRetransmits is set, the channel is unreliable.
	// (The settings are exclusive so both maxPacketLifetime and maxRetransmits must not be set.)

	// Time window during which transmissions and retransmissions may occur
	std::optional<std::chrono::milliseconds> maxPacketLifeTime;

	// Maximum number of retransmissions that are attempted
	std::optional<unsigned int> maxRetransmits;

	// For backward compatibility, do not use
	enum class Type
	{
		Reliable = 0,
		Rexmit,
		Timed
	};
	union
	{
		Type typeDeprecated = Type::Reliable;
		[[deprecated("Use maxPacketLifeTime or maxRetransmits")]] Type type;
	};
	std::variant<int, std::chrono::milliseconds> rexmit = 0;
};
struct DataChannelInit
{
	Reliability reliability = {};
	bool negotiated = false;
	std::optional<uint16_t> id = std::nullopt;
	std::string protocol = "";
};

namespace utils
{
std::string url_decode(const std::string& str)
{
	std::string result;
	size_t i = 0;
	while (i < str.size())
	{
		char c = str[i++];
		if (c == '%')
		{
			auto value = str.substr(i, 2);
			try
			{
				if (value.size() != 2 || !std::isxdigit(value[0]) || !std::isxdigit(value[1]))
					throw std::exception();

				c = static_cast<char>(std::stoi(value, nullptr, 16));
				i += 2;
			}
			catch (...)
			{
				//PLOG_WARNING << "Invalid percent-encoded character in URL: \"%" + value + "\"";
			}
		}

		result.push_back(c);
	}

	return result;
}
} //namespace utils

bool parse_url(const std::string& url, std::vector<std::optional<std::string>>& result)
{
	// Modified regex from RFC 3986, see https://www.rfc-editor.org/rfc/rfc3986.html#appendix-B
	static const char* rs =
			R"(^(([^:.@/?#]+):)?(/{0,2}((([^:@]*)(:([^@]*))?)@)?(([^:/?#]*)(:([^/?#]*))?))?([^?#]*)(\?([^#]*))?(#(.*))?)";
	static const std::regex r(rs, std::regex::extended);

	std::smatch m;
	if (!std::regex_match(url, m, r) || m[10].length() == 0)
		return false;

	result.resize(m.size());
	std::transform(m.begin(), m.end(), result.begin(),
				   [](const auto& sm) {
					   return sm.length() > 0 ? std::make_optional(std::string(sm)) : std::nullopt;
				   });

	assert(result.size() == 18);
	return true;
}

IceServer::IceServer(const std::string& url)
{
	std::vector<std::optional<std::string>> opt;
	if (!parse_url(url, opt))
		throw std::invalid_argument("Invalid ICE server URL: " + url);

	std::string scheme = opt[2].value_or("stun");
	relayType = RelayType::TurnUdp;
	if (scheme == "stun" || scheme == "STUN")
		type = Type::Stun;
	else if (scheme == "turn" || scheme == "TURN")
		type = Type::Turn;
	else if (scheme == "turns" || scheme == "TURNS")
	{
		type = Type::Turn;
		relayType = RelayType::TurnTls;
	}
	else
		throw std::invalid_argument("Unknown ICE server protocol: " + scheme);

	if (auto& query = opt[15])
	{
		if (query->find("transport=udp") != std::string::npos)
			relayType = RelayType::TurnUdp;
		if (query->find("transport=tcp") != std::string::npos)
			relayType = RelayType::TurnTcp;
		if (query->find("transport=tls") != std::string::npos)
			relayType = RelayType::TurnTls;
	}

	username = utils::url_decode(opt[6].value_or(""));
	password = utils::url_decode(opt[8].value_or(""));

	hostname = opt[10].value();
	if (hostname.front() == '[' && hostname.back() == ']')
	{
		// IPv6 literal
		hostname.erase(hostname.begin());
		hostname.pop_back();
	}
	else
	{
		hostname = utils::url_decode(hostname);
	}

	std::string service = opt[12].value_or(relayType == RelayType::TurnTls ? "5349" : "3478");
	try
	{
		port = uint16_t(std::stoul(service));
	}
	catch (...)
	{
		throw std::invalid_argument("Invalid ICE server port in URL: " + service);
	}
}

IceServer::IceServer(std::string hostname_, uint16_t port_) :
		hostname(std::move(hostname_)), port(port_), type(Type::Stun)
{
}

IceServer::IceServer(std::string hostname_, std::string service_) :
		hostname(std::move(hostname_)), type(Type::Stun)
{
	try
	{
		port = uint16_t(std::stoul(service_));
	}
	catch (...)
	{
		throw std::invalid_argument("Invalid ICE server port: " + service_);
	}
}

IceServer::IceServer(std::string hostname_, uint16_t port_, std::string username_,
					 std::string password_, RelayType relayType_) :
		hostname(std::move(hostname_)),
		port(port_),
		type(Type::Turn),
		username(std::move(username_)),
		password(std::move(password_)),
		relayType(relayType_)
{
}

IceServer::IceServer(std::string hostname_, std::string service_, std::string username_,
					 std::string password_, RelayType relayType_) :
		hostname(std::move(hostname_)),
		type(Type::Turn),
		username(std::move(username_)),
		password(std::move(password_)),
		relayType(relayType_)
{
	try
	{
		port = uint16_t(std::stoul(service_));
	}
	catch (...)
	{
		throw std::invalid_argument("Invalid ICE server port: " + service_);
	}
}

} // namespace rtc

void LogCallback(rtc::LogLevel level, std::string message)
{
	switch (level)
	{
		case rtc::LogLevel::Fatal:
		case rtc::LogLevel::Error:
			ERR_PRINT(message.c_str());
			return;
		case rtc::LogLevel::Warning:
			WARN_PRINT(message.c_str());
			return;
		default:
			VERBOSE_PRINT(message.c_str());
			return;
	}
}

void WebRTCLibPeerConnection::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("test_method"), &WebRTCLibPeerConnection::test_method);
	ClassDB::bind_method(D_METHOD("send_audio_packet", "pcm_samples"),
						 &WebRTCLibPeerConnection::send_audio_packet);

	ADD_SIGNAL(MethodInfo("track_data_received", PropertyInfo(Variant::INT, "ssrc"),
						  PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "data"),
						  PropertyInfo(Variant::INT, "channels")));

	//ADD_SIGNAL(MethodInfo("session_description_created", PropertyInfo(Variant::STRING, "type"),
	//PropertyInfo(Variant::STRING, "sdp")));
}

void WebRTCLibPeerConnection::test_method() { VERBOSE_PRINT("test_method called!"); }

void WebRTCLibPeerConnection::send_audio_packet(const godot::PackedFloat32Array& pcm_samples)
{
	//PrintUtiity::print_verbose("send_audio_packet: pcm_samples size: %", pcm_samples.size());

	size_t input_data_size = 0;
	{
		// TODO: use ring buffer
		std::scoped_lock lock(input_data_lock);
		input_data.add(Span(pcm_samples.ptr(), pcm_samples.size()));
		input_data_size = input_data.size();
	}

	while (input_data_size >= 960)
	{
		// 48000 -> 480 * 2 = 960 (20ms)
		std::vector<float> pcm_data;

		{
			std::scoped_lock lock(input_data_lock);
			pcm_data = input_data.getSome(960);
			input_data_size = input_data.size();
		}

		if (!pcm_data.empty())
		{
			std::vector<unsigned char> encoded_data;

			if (opus::ErrorCode::Ok !=
				opus_encoder.encodeFloat(encoded_data, pcm_data.data(), pcm_data.size()))
			{
				PrintUtility::print_verbose("failed to encode buffer of size %", pcm_data.size());
			}

			if (!encoded_data.empty())
			{
				pionSendTrackDataPacket((char*)encoded_data.data(), encoded_data.size());
				//PrintUtiity::print_verbose("send_audio_packet: % samples, % encoded bytes,
				//input_data_size: %", pcm_data.size(), encoded_data.size(), input_data_size);
			}
		}
	}

	//PrintUtiity::print_verbose("send_audio_packet: % samples", pcm_samples.size());
}

void WebRTCLibPeerConnection::initialize_signaling()
{
#ifdef DEBUG_ENABLED
	//rtc::InitLogger(rtc::LogLevel::Debug, LogCallback);
#else
	//rtc::InitLogger(rtc::LogLevel::Warning, LogCallback);
#endif
}

void WebRTCLibPeerConnection::deinitialize_signaling()
{
	//rtc::Cleanup();
}

Error WebRTCLibPeerConnection::_parse_ice_server(rtc::Configuration& r_config, Dictionary p_server)
{
	ERR_FAIL_COND_V(!p_server.has("urls"), ERR_INVALID_PARAMETER);

	// Parse mandatory URL
	Array urls;
	Variant urls_var = p_server["urls"];
	if (urls_var.get_type() == Variant::STRING)
	{
		urls.push_back(urls_var);
	}
	else if (urls_var.get_type() == Variant::ARRAY)
	{
		urls = urls_var;
	}
	else
	{
		ERR_FAIL_V(ERR_INVALID_PARAMETER);
	}
	// Parse credentials (only meaningful for TURN, only support password)
	String username;
	String credential;
	if (p_server.has("username") && p_server["username"].get_type() == Variant::STRING)
	{
		username = p_server["username"];
	}
	if (p_server.has("credential") && p_server["credential"].get_type() == Variant::STRING)
	{
		credential = p_server["credential"];
	}
	for (int i = 0; i < urls.size(); i++)
	{
		rtc::IceServer srv(urls[i].operator String().utf8().get_data());
		srv.username = username.utf8().get_data();
		srv.password = credential.utf8().get_data();
		r_config.iceServers.push_back(srv);
	}
	return OK;
}

Error WebRTCLibPeerConnection::_parse_channel_config(rtc::DataChannelInit& r_config,
													 const Dictionary& p_dict)
{
	Variant nil;
	Variant v;
	if (p_dict.has("negotiated"))
	{
		r_config.negotiated = p_dict["negotiated"].operator bool();
	}
	if (p_dict.has("id"))
	{
		r_config.id = uint16_t(p_dict["id"].operator int32_t());
	}
	// If negotiated it must have an ID, and ID only makes sense when negotiated.
	ERR_FAIL_COND_V(r_config.negotiated != r_config.id.has_value(), ERR_INVALID_PARAMETER);
	// Channels cannot be both time-constrained and retry-constrained.
	ERR_FAIL_COND_V(p_dict.has("maxPacketLifeTime") && p_dict.has("maxRetransmits"),
					ERR_INVALID_PARAMETER);
	if (p_dict.has("maxPacketLifeTime"))
	{
		r_config.reliability.maxPacketLifeTime =
				std::chrono::milliseconds(p_dict["maxPacketLifeTime"].operator int32_t());
	}
	else if (p_dict.has("maxRetransmits"))
	{
		r_config.reliability.maxRetransmits = p_dict["maxRetransmits"].operator int32_t();
	}
	if (p_dict.has("ordered") && p_dict["ordered"].operator bool() == false)
	{
		r_config.reliability.unordered = true;
	}
	if (p_dict.has("protocol"))
	{
		r_config.protocol = p_dict["protocol"].operator String().utf8().get_data();
	}
	return OK;
}

WebRTCPeerConnection::ConnectionState WebRTCLibPeerConnection::_get_connection_state() const
{
	ERR_FAIL_COND_V(pionWebrtc == PionErrorCodeInvalid, STATE_CLOSED);

	PionConnectionState pionState = pionGetConnectionState();

	switch (pionState)
	{
		case PionConnectionStateNew:
			return STATE_NEW;
		case PionConnectionStateConnecting:
			return STATE_CONNECTING;
		case PionConnectionStateConnected:
			return STATE_CONNECTED;
		case PionConnectionStateDisconnected:
			return STATE_DISCONNECTED;
		case PionConnectionStateFailed:
			return STATE_FAILED;
		default:
			return STATE_DISCONNECTED;
	}
}

WebRTCLibPeerConnection::GatheringState WebRTCLibPeerConnection::_get_gathering_state() const
{
	ERR_FAIL_COND_V(pionWebrtc == PionErrorCodeInvalid, GATHERING_STATE_NEW);

	PionIceGatheringState state = pionGetIceGatheringState();
	switch (state)
	{
		case PionIceGatheringStateNew:
			return GATHERING_STATE_NEW;
		case PionIceGatheringStateGathering:
			return GATHERING_STATE_GATHERING;
		case PionIceGatheringStateComplete:
			return GATHERING_STATE_COMPLETE;
		default:
			return GATHERING_STATE_NEW;
	}
}

WebRTCLibPeerConnection::SignalingState WebRTCLibPeerConnection::_get_signaling_state() const
{
	ERR_FAIL_COND_V(pionWebrtc == PionErrorCodeInvalid, SIGNALING_STATE_CLOSED);

	PionSignalingState state = pionGetSignalingState();
	switch (state)
	{
		case PionSignalingStateStable:
			return SIGNALING_STATE_STABLE;
		case PionSignalingStateHaveLocalOffer:
			return SIGNALING_STATE_HAVE_LOCAL_OFFER;
		case PionSignalingStateHaveRemoteOffer:
			return SIGNALING_STATE_HAVE_REMOTE_OFFER;
		case PionSignalingStateHaveLocalPranswer:
			return SIGNALING_STATE_HAVE_LOCAL_PRANSWER;
		case PionSignalingStateHaveRemotePranswer:
			return SIGNALING_STATE_HAVE_REMOTE_PRANSWER;
		default:
			return SIGNALING_STATE_CLOSED;
	}
}

void onIceCandidate(const char* candidate)
{
	std::ostringstream oss;
	oss << "webrtc-native:pionc: onIceCandidate: " << candidate;

	VERBOSE_PRINT(oss.str().c_str());
}

void onDataChannelMessage(const char* msg, int len)
{
	VERBOSE_PRINT("webrtc-native: onDataChannelMessage");
}

WebRTCLibPeerConnection* g_context = nullptr;

void WebRTCLibPeerConnection::onLocalDescription(int desc_type, const char* msg)
{
	VERBOSE_PRINT("webrtc-native: onLocalDescription");

	// String type = description.type() == rtc::Description::Type::Offer ? "offer" : "answer";
	g_context->queue_signal("session_description_created", 2, "offer",
							String(std::string(msg).c_str()));
}

void WebRTCLibPeerConnection::onRemoteTrack(int kind, unsigned int ssrc, const char* mime,
											unsigned int sample_rate, unsigned short channels)
{
	PrintUtility::print_verbose(
			"webrtc-native: new track: kind % ssrc % mime type % sample rate % channels %", kind,
			ssrc, mime, sample_rate, channels);
	if (g_context->opus_decoder.init(sample_rate, 2))
	{
		VERBOSE_PRINT("webrtc-native: OPUS decoder created");
	}
	else
	{
		ERR_PRINT("webrtc-native: Failed to create OPUS decoder!");
	}
}

void WebRTCLibPeerConnection::onTrackDataCallback(unsigned int ssrc, const char* data,
												  unsigned int length)
{
	g_context->onTrackData(ssrc, data, length);
}

void WebRTCLibPeerConnection::onTrackData(unsigned int ssrc, const char* data, unsigned int length)
{
	opus::ErrorCode result =
			opus_decoder.decodeFloat(decoded_frame, (const unsigned char*)data, length);
	if (result != opus::ErrorCode::Ok)
	{
		PrintUtility::print_verbose("webrtc-native: onTrackData: Failed to decode audio data");
		return;
	}

	{
		//PrintUtiity::print_verbose("webrtc-native: onTrackData: decoded size: %",
		//decoded_frame.frameSize);
		std::scoped_lock lock(audio_buffer_lock);
		TrackAudioData& data = audio_buffer.push_back();
		data.ssrc = ssrc;
		data.pcmData.swap(decoded_frame);
	}
}

void WebRTCLibPeerConnection::emit_track_data_signal()
{
	std::optional<TrackAudioData> audio_frame;
	{
		std::scoped_lock lock(audio_buffer_lock);
		audio_frame = audio_buffer.get();
	}
	if (audio_frame)
	{
		size_t num_frames = audio_frame->getNumFrames();
		godot_data.resize(num_frames);
		for (size_t i = 0, j = 0; i < audio_frame->pcmData.data.size() && j < godot_data.size();
			 i++, j++)
		{
			auto& vec2 = godot_data[j];
			vec2.x = audio_frame->pcmData.data[i];
			if (audio_frame->pcmData.channels == 1)
			{
				vec2.y = vec2.x;
			}
			else
			{
				vec2.y = audio_frame->pcmData.data[++i];
			}
		}

		//PrintUtiity::print_verbose("webrtc-native: received data size: % num_frames: %",
		//godot_data.size(), num_frames);

		emit_signal("track_data_received", audio_frame->ssrc, godot_data,
					audio_frame->pcmData.channels);
	}
}

static void log_callback(const char* msg, int level)
{
	std::ostringstream oss;

	oss << "webrtc-native:pionc: " << msg;

	switch (level)
	{
		case 0:
			ERR_PRINT(oss.str().c_str());
			break;
		case 1:
			WARN_PRINT(oss.str().c_str());
			break;

		default:
			VERBOSE_PRINT(oss.str().c_str());
	}
}

Error WebRTCLibPeerConnection::_initialize(const Dictionary& p_config)
{
	rtc::Configuration config = {};
	if (p_config.has("iceServers") && p_config["iceServers"].get_type() == Variant::ARRAY)
	{
		Array servers = p_config["iceServers"];
		for (int i = 0; i < servers.size(); i++)
		{
			ERR_FAIL_COND_V(servers[i].get_type() != Variant::DICTIONARY, ERR_INVALID_PARAMETER);
			Dictionary server = servers[i];
			Error err = _parse_ice_server(config, server);
			ERR_FAIL_COND_V(err != OK, FAILED);
		}
	}
	g_context = this;
	return _create_pc(config, p_config);
}

#if defined(GDNATIVE_WEBRTC) || defined(GDEXTENSION_WEBRTC_40)
Object* WebRTCLibPeerConnection::_create_data_channel(const String& p_channel,
													  const Dictionary& p_channel_config)
try
{
#else
Ref<WebRTCDataChannel>
WebRTCLibPeerConnection::_create_data_channel(const String& p_channel,
											  const Dictionary& p_channel_config)
try
{
#endif
	ERR_FAIL_COND_V(pionWebrtc == PionErrorCodeInvalid, nullptr);

	// Read config from dictionary
	rtc::DataChannelInit config;

	Error err = _parse_channel_config(config, p_channel_config);
	ERR_FAIL_COND_V(err != OK, nullptr);

	String channel_name = p_channel.utf8().get_data();
	int channel_id = pionCreateDataChannel(channel_name.utf8().ptrw());
	ERR_FAIL_COND_V(channel_id == 0, nullptr);

	// std::shared_ptr<rtc::DataChannel> ch =
	// peer_connection->createDataChannel(p_channel.utf8().get_data(), config); ERR_FAIL_COND_V(ch
	// == nullptr, nullptr);

	WebRTCLibDataChannel* wrapper =
			WebRTCLibDataChannel::new_data_channel(channel_id, config.negotiated);
	ERR_FAIL_COND_V(wrapper == nullptr, nullptr);
	
	return wrapper;
}
catch (const std::exception& e)
{
	ERR_PRINT(e.what());
	ERR_FAIL_V(nullptr);
}

Error WebRTCLibPeerConnection::_create_offer()
try
{
	ERR_FAIL_COND_V(pionWebrtc == PionErrorCodeInvalid, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(_get_connection_state() != STATE_NEW, FAILED);
	//peer_connection->setLocalDescription(rtc::Description::Type::Offer);

	pionCreateOffer();
	return OK;
}
catch (const std::exception& e)
{
	ERR_PRINT(e.what());
	ERR_FAIL_V(FAILED);
}

Error WebRTCLibPeerConnection::_set_remote_description(const String& p_type, const String& p_sdp)
try
{
	ERR_FAIL_COND_V(pionWebrtc == PionErrorCodeInvalid, ERR_UNCONFIGURED);
	std::string sdp(p_sdp.utf8().get_data());
	std::string type(p_type.utf8().get_data());
	//rtc::Description desc(sdp, type);
	// peer_connection->setRemoteDescription(desc);
	// // Automatically create the answer.
	// if (p_type == String("offer")) {
	// 	peer_connection->setLocalDescription(rtc::Description::Type::Answer);
	// }
	pionSetRemoteDescription(sdp.data());
	return OK;
}
catch (const std::exception& e)
{
	ERR_PRINT(e.what());
	ERR_FAIL_V(FAILED);
}

Error WebRTCLibPeerConnection::_set_local_description(const String& p_type, const String& p_sdp)
{
	ERR_FAIL_COND_V(pionWebrtc == PionErrorCodeInvalid, ERR_UNCONFIGURED);
	// XXX Library quirk. It doesn't seem possible to create offers/answers without setting the
	// local description. Ignore this call for now to avoid crash (it's already set automatically!).
	// peer_connection->setLocalDescription(p_type == String("offer") ?
	// rtc::Description::Type::Offer : rtc::Description::Type::Answer);
	return OK;
}

#ifdef GDNATIVE_WEBRTC
Error WebRTCLibPeerConnection::_add_ice_candidate(const String& sdpMidName,
												  int64_t sdpMlineIndexName, const String& sdpName)
try
{
#else
Error WebRTCLibPeerConnection::_add_ice_candidate(const String& sdpMidName,
												  int32_t sdpMlineIndexName, const String& sdpName)
try
{
#endif
	ERR_FAIL_COND_V(pionWebrtc == PionErrorCodeInvalid, ERR_UNCONFIGURED);
	//rtc::Candidate candidate(sdpName.utf8().get_data(), sdpMidName.utf8().get_data());
	//peer_connection->addRemoteCandidate(candidate);
	return OK;
}
catch (const std::exception& e)
{
	ERR_PRINT(e.what());
	ERR_FAIL_V(FAILED);
}

Error WebRTCLibPeerConnection::_poll()
{
	ERR_FAIL_COND_V(pionWebrtc == PionErrorCodeInvalid, ERR_UNCONFIGURED);

	emit_track_data_signal();

	while (!signal_queue.empty())
	{
		mutex_signal_queue->lock();
		Signal signal = signal_queue.front();
		signal_queue.pop();
		mutex_signal_queue->unlock();
		signal.emit(this);
	}
	return OK;
}

void WebRTCLibPeerConnection::_close()
{
	// if (peer_connection != nullptr) {
	// 	try {
	// 		peer_connection->close();
	// 	} catch (const std::exception &e) {
	// 		ERR_PRINT(e.what());
	// 	}
	// }
	if (pionWebrtc != PionErrorCodeInvalid)
	{
		try
		{
			VERBOSE_PRINT("webrtc-native: pionClosePeerConnection");
			pionClosePeerConnection();
		}
		catch (const std::exception& e)
		{
			ERR_PRINT(e.what());
		}
	}

	while (!signal_queue.empty())
	{
		signal_queue.pop();
	}
}

void WebRTCLibPeerConnection::_init()
{
#ifdef GDNATIVE_WEBRTC
	register_interface(&interface);
#endif
	mutex_signal_queue = new std::mutex;

	VERBOSE_PRINT("webrtc-native: pionInit()");
	pionInit();

	_initialize(Dictionary());
}

Error WebRTCLibPeerConnection::_create_pc(rtc::Configuration& r_config, const Dictionary& p_config)
try
{
	// Prevents libdatachannel from automatically creating offers.
	r_config.disableAutoNegotiation = true;

	// peer_connection = std::make_shared<rtc::PeerConnection>(r_config);
	// ERR_FAIL_COND_V(pionWebrtc == PionErrorCodeInvalid, FAILED);

	// // Binding this should be fine as long as we call close when going out of scope.
	// peer_connection->onLocalDescription([this](rtc::Description description) {
	// 	String type = description.type() == rtc::Description::Type::Offer ? "offer" : "answer";
	// 	queue_signal("session_description_created", 2, type,
	// String(std::string(description).c_str()));
	// });
	// peer_connection->onLocalCandidate([this](rtc::Candidate candidate) {
	// 	queue_signal("ice_candidate_created", 3, String(candidate.mid().c_str()), 0,
	// String(candidate.candidate().c_str()));
	// });
	// peer_connection->onDataChannel([this](std::shared_ptr<rtc::DataChannel> channel) {
	// 	queue_signal("data_channel_received", 1, WebRTCLibDataChannel::new_data_channel(channel,
	// false));
	// });
	/*
	peer_connection->onStateChange([](rtc::PeerConnection::State state) {
		std::cout << "[State: " << state << "]" << std::endl;
	});

	peer_connection->onGatheringStateChange([](rtc::PeerConnection::GatheringState state) {
		std::cout << "[Gathering State: " << state << "]" << std::endl;
	});
	*/

	VERBOSE_PRINT("webrtc-native: _initialize");
	PionCallbacks pionCallbacks = { 0 };
	pionCallbacks.log_callback = log_callback;
	pionCallbacks.remote_track_callback = WebRTCLibPeerConnection::onRemoteTrack;
	pionCallbacks.ice_candidate_callback = onIceCandidate;
	pionCallbacks.local_description_callback = WebRTCLibPeerConnection::onLocalDescription;
	pionCallbacks.track_data_callback = WebRTCLibPeerConnection::onTrackDataCallback;
	pionSetCallbacks(pionCallbacks);

	VERBOSE_PRINT("webrtc-native: CreatePeerConnection()");
	if (r_config.iceServers.size() > 0)
	{
		std::vector<std::string> server_urls;
		PionPeerConnectionConfiguration pion_config;
		std::vector<PionIceServer> ice_servers;
		ice_servers.resize(r_config.iceServers.size());
		server_urls.resize(ice_servers.size());
		for (auto i = 0; i < r_config.iceServers.size(); i++)
		{
			rtc::IceServer& rserver = r_config.iceServers[i];
			ice_servers[i].username = rserver.username.c_str();
			ice_servers[i].credential_type = 0;
			ice_servers[i].credential = rserver.password.c_str();

			std::ostringstream url;
			switch (rserver.type)
			{
				case rtc::IceServer::Type::Stun:
					url << "stun:";
					break;
				case rtc::IceServer::Type::Turn:
					url << "turn:";
					break;
				default:
					break;
			}

			url << rserver.hostname;
			url << ":" << rserver.port;
			switch (rserver.relayType)
			{
				case rtc::IceServer::RelayType::TurnUdp:
					url << "?transport=udp";
					break;
				case rtc::IceServer::RelayType::TurnTcp:
					url << "?transport=tcp";
					break;
				case rtc::IceServer::RelayType::TurnTls:
					url << "?transport=tls";
					break;
				default:
					break;
			}
			server_urls.push_back(url.str());
			ice_servers[i].hostname = server_urls.back().c_str();
		}
		pion_config.ice_servers = ice_servers.data();
		pion_config.num_servers = (int)r_config.iceServers.size();
		pionWebrtc = pionCreatePeerConnection(&pion_config);
	}
	else
	{
		pionWebrtc = pionCreatePeerConnection(nullptr);
	}

	// TODO: init encoder when creating local track
	opus_encoder.init(48000, 1);
	opus_encoder.setComplexity(3); // 1 - 10 complexity, 10 is default and uses the most CPU
	opus_encoder.setBitrate(128000); // 48kbps bit rate is good quality. But with FEC and low
									 // complexity we need more...
	opus_encoder.setPacketLossPerc(15);

	ERR_FAIL_COND_V(pionWebrtc == PionErrorCodeInvalid, FAILED);

	{
		std::scoped_lock lock(input_data_lock);
		input_data.clear();
	}

	return OK;
}
catch (const std::exception& e)
{
	ERR_PRINT(e.what());
	ERR_FAIL_V(FAILED);
}

WebRTCLibPeerConnection::WebRTCLibPeerConnection()
{
#ifndef GDNATIVE_WEBRTC
	_init();
#endif
}

WebRTCLibPeerConnection::~WebRTCLibPeerConnection()
{
#ifdef GDNATIVE_WEBRTC
	if (_owner)
	{
		register_interface(nullptr);
	}
#endif
	_close();
	delete mutex_signal_queue;
}

void WebRTCLibPeerConnection::queue_signal(String p_name, int p_argc, const Variant& p_arg1,
										   const Variant& p_arg2, const Variant& p_arg3)
{
	mutex_signal_queue->lock();
	const Variant argv[3] = { p_arg1, p_arg2, p_arg3 };
	signal_queue.push(Signal(p_name, p_argc, argv));
	mutex_signal_queue->unlock();
}
