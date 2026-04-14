#include "../src/libwebrtc.h"

#include <cstdio>
#include <string>
#include <vector>

struct libwebrtc_context {
	lwrtc_callback_function callback;
	std::vector<std::string> stun_servers;
	std::string turn_server;
	std::string turn_username;
	std::string turn_password;
};

struct libwebrtc_connection {
	libwebrtc_context* ctx;
	void* user_data;
	bool closed;
};

struct libwebrtc_data_channel {
	libwebrtc_connection* connection;
	std::string name;
	bool closed;
};

static int report_unimplemented(libwebrtc_connection* connection, const char* operation) {
	if(connection && connection->ctx && connection->ctx->callback) {
		connection->ctx->callback(
			connection->ctx,
			connection,
			nullptr,
			LWRTC_CALLBACK_ERROR,
			connection->user_data,
			(void*)operation,
			operation ? (int)std::char_traits<char>::length(operation) : 0
		);
	}
	std::fprintf(stderr, "webrtc_native_linux: unimplemented operation: %s\n", operation ? operation : "(unknown)");
	return 0;
}

extern "C" {

struct libwebrtc_context* libwebrtc_create_context(lwrtc_callback_function callback) {
	if(!callback) {
		return nullptr;
	}

	libwebrtc_context* ctx = new libwebrtc_context();
	ctx->callback = callback;
	return ctx;
}

void libwebrtc_destroy_context(struct libwebrtc_context* ctx) {
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
	if(!ctx) {
		return nullptr;
	}

	libwebrtc_connection* connection = new libwebrtc_connection();
	connection->ctx = ctx;
	connection->user_data = user_data;
	connection->closed = false;
	return connection;
}

struct libwebrtc_data_channel* libwebrtc_create_channel(struct libwebrtc_connection* connection, const char* name) {
	if(!connection || connection->closed) {
		return nullptr;
	}

	libwebrtc_data_channel* channel = new libwebrtc_data_channel();
	channel->connection = connection;
	channel->name = name ? name : "";
	channel->closed = false;
	return channel;
}

int libwebrtc_create_offer(struct libwebrtc_connection* connection) {
	return report_unimplemented(connection, "libwebrtc_create_offer");
}

int libwebrtc_set_offer(struct libwebrtc_connection* connection, const char* sdp) {
	(void)connection;
	(void)sdp;
	return report_unimplemented(connection, "libwebrtc_set_offer");
}

int libwebrtc_set_answer(struct libwebrtc_connection* connection, const char* sdp) {
	(void)connection;
	(void)sdp;
	return report_unimplemented(connection, "libwebrtc_set_answer");
}

int libwebrtc_add_ice_candidate(struct libwebrtc_connection* connection, const char* candidate) {
	(void)connection;
	(void)candidate;
	return report_unimplemented(connection, "libwebrtc_add_ice_candidate");
}

int libwebrtc_write(struct libwebrtc_data_channel* channel, const void* data, int len) {
	(void)data;
	if(!channel || channel->closed) {
		return -1;
	}
	return len;
}

void libwebrtc_close_channel(struct libwebrtc_data_channel* channel) {
	if(!channel || channel->closed) {
		return;
	}

	channel->closed = true;
	if(channel->connection && channel->connection->ctx && channel->connection->ctx->callback) {
		channel->connection->ctx->callback(
			channel->connection->ctx,
			channel->connection,
			channel,
			LWRTC_CALLBACK_CHANNEL_CLOSED,
			channel->connection->user_data,
			channel->name.empty() ? nullptr : (void*)channel->name.c_str(),
			(int)channel->name.size()
		);
	}
	delete channel;
}

void libwebrtc_close_connection(struct libwebrtc_connection* connection) {
	if(!connection || connection->closed) {
		return;
	}

	connection->closed = true;
	if(connection->ctx && connection->ctx->callback) {
		connection->ctx->callback(connection->ctx, connection, nullptr, LWRTC_CALLBACK_DISCONNECTED, connection->user_data, nullptr, 0);
		connection->ctx->callback(connection->ctx, connection, nullptr, LWRTC_CALLBACK_DESTROY, connection->user_data, nullptr, 0);
	}
	delete connection;
}

}
