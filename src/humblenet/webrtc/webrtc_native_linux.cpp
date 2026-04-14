#include "../src/libwebrtc.h"

#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/create_peerconnection_factory.h"
#include "api/data_channel_interface.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/set_local_description_observer_interface.h"
#include "api/set_remote_description_observer_interface.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"

struct libwebrtc_connection;
struct libwebrtc_data_channel;

class CreateDescriptionObserver;

void LogRtcError(const char* prefix, const webrtc::RTCError& error) {
	std::fprintf(stderr, "webrtc_native_linux: %s: %s\n", prefix, error.message());
}

struct libwebrtc_context {
	lwrtc_callback_function callback = nullptr;
	std::vector<std::string> stun_servers;
	std::string turn_server;
	std::string turn_username;
	std::string turn_password;

	std::unique_ptr<rtc::Thread> network_thread;
	std::unique_ptr<rtc::Thread> worker_thread;
	std::unique_ptr<rtc::Thread> signaling_thread;

	webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
};

struct libwebrtc_data_channel : public webrtc::DataChannelObserver {
	libwebrtc_connection* connection = nullptr;
	void* user_data = nullptr;
	webrtc::scoped_refptr<webrtc::DataChannelInterface> channel;
	bool inbound = false;
	bool open_notified = false;
	bool close_notified = false;

	libwebrtc_data_channel(libwebrtc_connection* parent,
	                       webrtc::scoped_refptr<webrtc::DataChannelInterface> data_channel,
	                       bool is_inbound);
	~libwebrtc_data_channel() override;

	void OnStateChange() override;
	void OnMessage(const webrtc::DataBuffer& buffer) override;
	void OnBufferedAmountChange(uint64_t) override {}
};

struct libwebrtc_connection : public webrtc::PeerConnectionObserver {
	libwebrtc_context* ctx = nullptr;
	void* user_data = nullptr;
	webrtc::scoped_refptr<webrtc::PeerConnectionInterface> connection;
	webrtc::scoped_refptr<webrtc::DataChannelInterface> bootstrap_channel;
	bool disconnected_notified = false;
	bool destroy_notified = false;
	bool established_notified = false;

	~libwebrtc_connection() override = default;

	void NotifyError(const char* message) {
		if(ctx && ctx->callback) {
			ctx->callback(ctx, this, nullptr, LWRTC_CALLBACK_ERROR, user_data, (void*)message, message ? (int)std::char_traits<char>::length(message) : 0);
		}
	}

	void NotifyDestroyed() {
		if(destroy_notified) {
			return;
		}
		destroy_notified = true;
		if(ctx && ctx->callback) {
			ctx->callback(ctx, this, nullptr, LWRTC_CALLBACK_DESTROY, user_data, nullptr, 0);
		}
	}

	void NotifyDisconnected() {
		if(disconnected_notified) {
			return;
		}
		disconnected_notified = true;
		if(ctx && ctx->callback) {
			ctx->callback(ctx, this, nullptr, LWRTC_CALLBACK_DISCONNECTED, user_data, nullptr, 0);
		}
	}

	void CloseFromError(const char* message) {
		NotifyError(message);
		if(connection) {
			connection->Close();
		} else {
			NotifyDisconnected();
			NotifyDestroyed();
		}
	}

	void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override {
		if(new_state == webrtc::PeerConnectionInterface::SignalingState::kClosed) {
			NotifyDisconnected();
			NotifyDestroyed();
		}
	}

	void OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {
		auto* wrapper = new libwebrtc_data_channel(this, channel, true);
		if(channel && channel->state() == webrtc::DataChannelInterface::kOpen) {
			wrapper->OnStateChange();
		}
	}

	void OnRenegotiationNeeded() override {}

	void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
		switch(new_state) {
			case webrtc::PeerConnectionInterface::kIceConnectionConnected:
			case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
				if(!established_notified && ctx && ctx->callback) {
					established_notified = true;
					ctx->callback(ctx, this, nullptr, LWRTC_CALLBACK_ESTABLISHED, user_data, nullptr, 0);
				}
				break;
			case webrtc::PeerConnectionInterface::kIceConnectionFailed:
			case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
			case webrtc::PeerConnectionInterface::kIceConnectionClosed:
				NotifyDisconnected();
				if(connection && new_state != webrtc::PeerConnectionInterface::kIceConnectionClosed) {
					connection->Close();
				}
				break;
			default:
				break;
		}
	}

	void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState) override {}

	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
		if(!ctx || !ctx->callback || !candidate) {
			return;
		}

		std::string sdp;
		if(!candidate->ToString(&sdp) || sdp.empty()) {
			NotifyError("ice-candidate-to-string");
			return;
		}
		ctx->callback(ctx, this, nullptr, LWRTC_CALLBACK_ICE_CANDIDATE, user_data, (void*)sdp.c_str(), (int)sdp.size());
	}

	void OnIceConnectionReceivingChange(bool) override {}
	void OnIceCandidatesRemoved(const std::vector<cricket::Candidate>&) override {}
	void OnAddTrack(webrtc::scoped_refptr<webrtc::RtpReceiverInterface>,
	                const std::vector<webrtc::scoped_refptr<webrtc::MediaStreamInterface>>&) override {}
	void OnRemoveTrack(webrtc::scoped_refptr<webrtc::RtpReceiverInterface>) override {}
};

libwebrtc_data_channel::libwebrtc_data_channel(libwebrtc_connection* parent,
                                               webrtc::scoped_refptr<webrtc::DataChannelInterface> data_channel,
                                               bool is_inbound)
	: connection(parent)
	, user_data(parent ? parent->user_data : nullptr)
	, channel(std::move(data_channel))
	, inbound(is_inbound) {
	if(channel) {
		channel->RegisterObserver(this);
	}
}

libwebrtc_data_channel::~libwebrtc_data_channel() {
	if(channel) {
		channel->UnregisterObserver();
	}
}

void libwebrtc_data_channel::OnStateChange() {
	if(!connection || !connection->ctx || !connection->ctx->callback || !channel) {
		return;
	}

	switch(channel->state()) {
		case webrtc::DataChannelInterface::kOpen: {
			if(open_notified) {
				return;
			}
			open_notified = true;
			const std::string label = channel->label();
			connection->ctx->callback(
				connection->ctx,
				connection,
				this,
				inbound ? LWRTC_CALLBACK_CHANNEL_ACCEPTED : LWRTC_CALLBACK_CHANNEL_CONNECTED,
				user_data,
				label.empty() ? nullptr : (void*)label.c_str(),
				(int)label.size());
			break;
		}
		case webrtc::DataChannelInterface::kClosed: {
			if(close_notified) {
				return;
			}
			close_notified = true;
			const std::string label = channel->label();
			connection->ctx->callback(
				connection->ctx,
				connection,
				this,
				LWRTC_CALLBACK_CHANNEL_CLOSED,
				user_data,
				label.empty() ? nullptr : (void*)label.c_str(),
				(int)label.size());
			delete this;
			break;
		}
		default:
			break;
	}
}

void libwebrtc_data_channel::OnMessage(const webrtc::DataBuffer& buffer) {
	if(!connection || !connection->ctx || !connection->ctx->callback) {
		return;
	}

	if(connection->ctx->callback(
		connection->ctx,
		connection,
		this,
		LWRTC_CALLBACK_CHANNEL_RECEIVE,
		user_data,
		(void*)buffer.data.data(),
		(int)buffer.size())) {
		if(channel) {
			channel->Close();
		}
	}
}

class SetLocalDescriptionObserver : public webrtc::SetLocalDescriptionObserverInterface {
public:
	explicit SetLocalDescriptionObserver(libwebrtc_connection* connection)
		: connection_(connection) {}

	void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
		if(!error.ok()) {
			LogRtcError("set-local-description", error);
			if(connection_) {
				connection_->CloseFromError("set-local-description");
			}
		}
	}

private:
	libwebrtc_connection* connection_;
};

class CreateDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
public:
	CreateDescriptionObserver(libwebrtc_connection* connection, webrtc::SdpType expected_type)
		: connection_(connection)
		, expected_type_(expected_type) {}

	void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
		if(!connection_ || !connection_->connection || !connection_->ctx || !connection_->ctx->callback || !desc) {
			delete desc;
			return;
		}

		if(desc->GetType() != expected_type_) {
			delete desc;
			connection_->CloseFromError("unexpected-local-description-type");
			return;
		}

		std::string sdp;
		if(!desc->ToString(&sdp) || sdp.empty()) {
			delete desc;
			connection_->CloseFromError("local-description-to-string");
			return;
		}

		std::unique_ptr<webrtc::SessionDescriptionInterface> owned_desc(desc);
		connection_->connection->SetLocalDescription(
			std::move(owned_desc),
			rtc::make_ref_counted<SetLocalDescriptionObserver>(connection_));

		connection_->ctx->callback(
			connection_->ctx,
			connection_,
			nullptr,
			LWRTC_CALLBACK_LOCAL_DESCRIPTION,
			connection_->user_data,
			(void*)sdp.c_str(),
			(int)sdp.size());
	}

	void OnFailure(webrtc::RTCError error) override {
		LogRtcError("create-description", error);
		if(connection_) {
			connection_->CloseFromError("create-description");
		}
	}

private:
	libwebrtc_connection* connection_;
	webrtc::SdpType expected_type_;
};

class SetRemoteDescriptionObserver : public webrtc::SetRemoteDescriptionObserverInterface {
public:
	enum class Action {
		kSetOffer,
		kSetAnswer
	};

	SetRemoteDescriptionObserver(libwebrtc_connection* connection, Action action)
		: connection_(connection)
		, action_(action) {}

	void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
		if(!error.ok()) {
			LogRtcError("set-remote-description", error);
			if(connection_) {
				connection_->CloseFromError("set-remote-description");
			}
			return;
		}

		if(!connection_ || !connection_->connection) {
			return;
		}

		if(action_ == Action::kSetOffer) {
			connection_->connection->CreateAnswer(
				rtc::make_ref_counted<CreateDescriptionObserver>(connection_, webrtc::SdpType::kAnswer).get(),
				webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
		}
	}

private:
	libwebrtc_connection* connection_;
	Action action_;
};

webrtc::PeerConnectionInterface::RTCConfiguration BuildRtcConfig(const libwebrtc_context* ctx) {
	webrtc::PeerConnectionInterface::RTCConfiguration config;
	config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

	for(const std::string& server : ctx->stun_servers) {
		webrtc::PeerConnectionInterface::IceServer ice_server;
		ice_server.uri = "stun:" + server;
		ice_server.urls.push_back(ice_server.uri);
		config.servers.push_back(ice_server);
	}

	if(!ctx->turn_server.empty() && !ctx->turn_username.empty() && !ctx->turn_password.empty()) {
		webrtc::PeerConnectionInterface::IceServer ice_server;
		ice_server.uri = "turn:" + ctx->turn_server;
		ice_server.urls.push_back(ice_server.uri);
		ice_server.username = ctx->turn_username;
		ice_server.password = ctx->turn_password;
		config.servers.push_back(ice_server);
	}

	return config;
}

extern "C" {

struct libwebrtc_context* libwebrtc_create_context(lwrtc_callback_function callback) {
	if(!callback) {
		return nullptr;
	}

	rtc::InitializeSSL();

	std::unique_ptr<libwebrtc_context> ctx(new libwebrtc_context());
	ctx->callback = callback;
	ctx->network_thread = rtc::Thread::CreateWithSocketServer();
	ctx->worker_thread = rtc::Thread::Create();
	ctx->signaling_thread = rtc::Thread::Create();

	if(!ctx->network_thread || !ctx->worker_thread || !ctx->signaling_thread) {
		return nullptr;
	}

	ctx->network_thread->Start();
	ctx->worker_thread->Start();
	ctx->signaling_thread->Start();

	ctx->factory = webrtc::CreatePeerConnectionFactory(
		ctx->network_thread.get(),
		ctx->worker_thread.get(),
		ctx->signaling_thread.get(),
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr);

	if(!ctx->factory) {
		std::fprintf(stderr, "webrtc_native_linux: failed to create PeerConnectionFactory\n");
		return nullptr;
	}

	return ctx.release();
}

void libwebrtc_destroy_context(struct libwebrtc_context* ctx) {
	if(!ctx) {
		return;
	}

	ctx->factory = nullptr;
	if(ctx->signaling_thread) {
		ctx->signaling_thread->Stop();
	}
	if(ctx->worker_thread) {
		ctx->worker_thread->Stop();
	}
	if(ctx->network_thread) {
		ctx->network_thread->Stop();
	}
	delete ctx;
}

void libwebrtc_set_stun_servers(struct libwebrtc_context* ctx, const char** servers, int count) {
	if(!ctx) {
		return;
	}

	ctx->stun_servers.clear();
	for(int i = 0; i < count; ++i) {
		if(servers[i]) {
			ctx->stun_servers.emplace_back(servers[i]);
		}
	}
}

void libwebrtc_add_turn_server(struct libwebrtc_context* ctx, const char* server, const char* username, const char* password) {
	if(!ctx) {
		return;
	}

	ctx->turn_server = server ? server : "";
	ctx->turn_username = username ? username : "";
	ctx->turn_password = password ? password : "";
}

struct libwebrtc_connection* libwebrtc_create_connection_extended(struct libwebrtc_context* ctx, void* user_data) {
	if(!ctx || !ctx->factory) {
		return nullptr;
	}

	std::unique_ptr<libwebrtc_connection> connection(new libwebrtc_connection());
	connection->ctx = ctx;
	connection->user_data = user_data;

	webrtc::PeerConnectionDependencies dependencies(connection.get());
	auto error_or_peer_connection = ctx->factory->CreatePeerConnectionOrError(BuildRtcConfig(ctx), std::move(dependencies));
	if(!error_or_peer_connection.ok()) {
		LogRtcError("create-peer-connection", error_or_peer_connection.error());
		return nullptr;
	}

	connection->connection = std::move(error_or_peer_connection.value());
	return connection.release();
}

struct libwebrtc_data_channel* libwebrtc_create_channel(struct libwebrtc_connection* connection, const char* name) {
	if(!connection || !connection->connection) {
		return nullptr;
	}

	webrtc::scoped_refptr<webrtc::DataChannelInterface> channel;
	if(connection->bootstrap_channel) {
		channel = connection->bootstrap_channel;
		connection->bootstrap_channel = nullptr;
	} else {
		webrtc::DataChannelInit config;
		config.ordered = false;
		config.reliable = false;
		config.maxRetransmits = 0;
		auto error_or_channel = connection->connection->CreateDataChannelOrError(name ? name : "default", &config);
		if(!error_or_channel.ok()) {
			LogRtcError("create-data-channel", error_or_channel.error());
			connection->CloseFromError("create-data-channel");
			return nullptr;
		}
		channel = std::move(error_or_channel.value());
	}

	if(!channel) {
		connection->CloseFromError("create-data-channel");
		return nullptr;
	}

	auto* wrapper = new libwebrtc_data_channel(connection, channel, false);
	if(channel->state() == webrtc::DataChannelInterface::kOpen) {
		wrapper->OnStateChange();
	}
	return wrapper;
}

int libwebrtc_create_offer(struct libwebrtc_connection* connection) {
	if(!connection || !connection->connection) {
		return 0;
	}

	webrtc::DataChannelInit config;
	config.ordered = false;
	config.reliable = false;
	config.maxRetransmits = 0;
	auto error_or_channel = connection->connection->CreateDataChannelOrError("default", &config);
	if(!error_or_channel.ok()) {
		LogRtcError("create-bootstrap-data-channel", error_or_channel.error());
		connection->CloseFromError("create-bootstrap-data-channel");
		return 0;
	}
	connection->bootstrap_channel = std::move(error_or_channel.value());
	if(!connection->bootstrap_channel) {
		connection->CloseFromError("create-bootstrap-data-channel");
		return 0;
	}

	connection->connection->CreateOffer(
		rtc::make_ref_counted<CreateDescriptionObserver>(connection, webrtc::SdpType::kOffer).get(),
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
	return 1;
}

int libwebrtc_set_offer(struct libwebrtc_connection* connection, const char* sdp) {
	if(!connection || !connection->connection || !sdp) {
		return 0;
	}

	webrtc::SdpParseError error;
	std::unique_ptr<webrtc::SessionDescriptionInterface> description =
		webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, sdp, &error);
	if(!description) {
		std::fprintf(stderr, "webrtc_native_linux: set-offer parse error at '%s': %s\n", error.line.c_str(), error.description.c_str());
		return 0;
	}

	connection->connection->SetRemoteDescription(
		std::move(description),
		rtc::make_ref_counted<SetRemoteDescriptionObserver>(connection, SetRemoteDescriptionObserver::Action::kSetOffer));
	return 1;
}

int libwebrtc_set_answer(struct libwebrtc_connection* connection, const char* sdp) {
	if(!connection || !connection->connection || !sdp) {
		return 0;
	}

	webrtc::SdpParseError error;
	std::unique_ptr<webrtc::SessionDescriptionInterface> description =
		webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp, &error);
	if(!description) {
		std::fprintf(stderr, "webrtc_native_linux: set-answer parse error at '%s': %s\n", error.line.c_str(), error.description.c_str());
		return 0;
	}

	connection->connection->SetRemoteDescription(
		std::move(description),
		rtc::make_ref_counted<SetRemoteDescriptionObserver>(connection, SetRemoteDescriptionObserver::Action::kSetAnswer));
	return 1;
}

int libwebrtc_add_ice_candidate(struct libwebrtc_connection* connection, const char* candidate) {
	if(!connection || !connection->connection || !candidate) {
		return 0;
	}

	webrtc::SdpParseError error;
	std::unique_ptr<webrtc::IceCandidateInterface> ice(
		webrtc::CreateIceCandidate("data", 0, candidate, &error));
	if(!ice) {
		std::fprintf(stderr, "webrtc_native_linux: add-ice-candidate parse error at '%s': %s\n", error.line.c_str(), error.description.c_str());
		return 0;
	}

	return connection->connection->AddIceCandidate(ice.get()) ? 1 : 0;
}

int libwebrtc_write(struct libwebrtc_data_channel* channel, const void* data, int len) {
	if(!channel || !channel->channel || channel->channel->state() != webrtc::DataChannelInterface::kOpen || !data || len < 0) {
		return -1;
	}

	rtc::CopyOnWriteBuffer buffer(static_cast<const uint8_t*>(data), (size_t)len);
	return channel->channel->Send(webrtc::DataBuffer(buffer, true)) ? len : -1;
}

void libwebrtc_close_channel(struct libwebrtc_data_channel* channel) {
	if(!channel || !channel->channel) {
		return;
	}

	channel->channel->Close();
}

void libwebrtc_close_connection(struct libwebrtc_connection* connection) {
	if(!connection || !connection->connection) {
		return;
	}

	connection->connection->Close();
}

}
