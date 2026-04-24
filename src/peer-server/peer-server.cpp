#include <signal.h>

#include <cstdlib>
#include <cstring>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#	include <windows.h>
#else  // posix
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>

#	include <sys/types.h>
#	include <sys/stat.h>
#	include <unistd.h>
#endif

#include <libwebsockets.h>

#include "humblenet.h"
#include "humblepeer.h"

#include "catalog.h"
#include "p2p_connection.h"
#include "server.h"
#include "logging.h"

using namespace humblenet;

namespace humblenet {
	namespace {
		uint64_t nowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
		}
	}

	ha_bool sendP2PMessage(P2PSignalConnection *conn, const uint8_t *buff, size_t length) {
		conn->sendMessage(buff, length);
		return true;
	}

}  // namespace humblenet

static std::unique_ptr<Server> peerServer;


static ha_bool p2pSignalProcess(const humblenet::HumblePeer::Message *msg, void *user_data) {
	return reinterpret_cast<P2PSignalConnection *>(user_data)->processMsg(msg);
}

static bool lookup_peer_impl(const std::string& hostname) {
	auto& aliases = peerServer->catalog->aliases;
	return aliases.find(hostname) != aliases.end();
}

static bool lookup_peer(const std::string& hostname) {
	auto found = lookup_peer_impl(hostname);
	if (!found) {
		std::string aliases = "";
		for (auto &game : peerServer->catalog->aliases) {
			aliases += game.first + ", ";
		}
	}

	return found;
}

static const char* get_http_body(void *user) {
	if (*(char*)user) return "{\"found\":true}";
	else return "{\"found\":false}";
}

int callback_humblepeer(struct lws *wsi
				  , enum lws_callback_reasons reason
				  , void *user, void *in, size_t len) {

	if (peerServer && peerServer->catalog) {
		peerServer->catalog->expireSessions(nowMs());
	}

	switch (reason) {
	case LWS_CALLBACK_HTTP:
		{
			const char *uri = (const char *)in;
			if (strncmp(uri, "/lookup/", 8) == 0) {
				const char *hostname = uri + 8;
				(*(char*)user) = lookup_peer(hostname) ? 1 : 0;
				unsigned char buffer[8192];
				memset(buffer, 0, sizeof(buffer));
				unsigned char *p = buffer;
				unsigned char *end = buffer + sizeof(buffer);
				if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end))
					return 1;
				if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CACHE_CONTROL, (unsigned char *)"no-cache", 8, &p, end))
					return 1;
				if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE, (unsigned char *)"application/json", 16, &p, end))
					return 1;
				if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_ACCESS_CONTROL_ALLOW_ORIGIN, (unsigned char *)"*", 1, &p, end))
					return 1;
				if (lws_add_http_header_content_length(wsi, strlen(get_http_body(user)), &p, end))
					return 1;
				if (lws_finalize_write_http_header(wsi, buffer, &p, end))
					return 1;
				// lws_write(wsi, buffer, p - buffer, LWS_WRITE_HTTP_HEADERS);
				lws_callback_on_writable(wsi);
				return 0;
			} else {
				lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, NULL);
			}
		}
		break;
	case LWS_CALLBACK_HTTP_WRITEABLE:
		{
			const char* body = NULL;
			body = get_http_body(user);
			size_t len = strlen(body);
			unsigned char buffer[8192];
			memset(buffer, 0, sizeof(buffer));
			strncpy((char*)buffer, body, len);

			if (lws_write(wsi, buffer, len, LWS_WRITE_HTTP_FINAL) != len) {
				return 1;
			}
			if (lws_http_transaction_completed(wsi)) {
				return -1;
			}
			return 0;
		}
		break;

	case LWS_CALLBACK_ESTABLISHED:
		{
			std::unique_ptr<P2PSignalConnection> conn(new P2PSignalConnection(peerServer.get()));
			conn->wsi = wsi;

			struct sockaddr_storage addr;
			socklen_t len = sizeof(addr);
			size_t bufsize = std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 1;
			std::vector<char> ipstr(bufsize, 0);
			int port;

			int socket = lws_get_socket_fd(wsi);
			getpeername(socket, (struct sockaddr*)&addr, &len);

			if (addr.ss_family == AF_INET) {
				struct sockaddr_in *s = (struct sockaddr_in*)&addr;
				port = ntohs(s->sin_port);
				inet_ntop(AF_INET, &s->sin_addr, &ipstr[0], INET_ADDRSTRLEN);
			}
			else { // AF_INET6
				struct sockaddr_in6 *s = (struct sockaddr_in6*)&addr;
				port = ntohs(s->sin6_port);
				inet_ntop(AF_INET6, &s->sin6_addr, &ipstr[0], INET6_ADDRSTRLEN);
			}

			conn->url = std::string(&ipstr[0]);
			conn->url += std::string(":");
			conn->url += std::to_string(port);

			LOG_INFO("New connection from \"%s\"\n", conn->url.c_str());

			peerServer->signalConnections.emplace(wsi, std::move(conn));
		}

		break;


	case LWS_CALLBACK_CLOSED:
		{
			auto it = peerServer->signalConnections.find(wsi);
			if (it == peerServer->signalConnections.end()) {
				// Tried to close nonexistent signal connection
				LOG_ERROR("Tried to close a signaling connection which doesn't appear to exist\n");
				return 0;
			}

			P2PSignalConnection *conn = it->second.get();
			assert(conn != NULL);

			conn->state = Closed;
			LOG_INFO("Closing connection to peer %u (%s)\n", conn->peerId, conn->url.c_str());

			if (conn->session != NULL && conn->catalog != NULL) {
				PeerId peerId = conn->session->peerId;
				if (conn->peerInitiatedClose) {
					conn->catalog->destroySession(peerId);
					LOG_INFO("Destroyed signaling session for peer %u after clean websocket close\n", peerId);
				} else {
					conn->catalog->detachSession(conn->session, nowMs() + Catalog::kReconnectGracePeriodMs);
					LOG_INFO("Detached signaling session for peer %u; resume token remains valid for %llu ms\n",
						peerId, static_cast<unsigned long long>(Catalog::kReconnectGracePeriodMs));
				}
				conn->session = NULL;
			}

			// and finally remove from list of signal connections
			// this is unique_ptr so it also destroys the object
			peerServer->signalConnections.erase(it);
		}

		break;

	case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
		{
			auto it = peerServer->signalConnections.find(wsi);
			if (it != peerServer->signalConnections.end()) {
				it->second->peerInitiatedClose = true;
			}
		}
		break;

	case LWS_CALLBACK_RECEIVE:
		{
			auto it = peerServer->signalConnections.find(wsi);
			if (it == peerServer->signalConnections.end()) {
				// Receive on nonexistent signal connection
				return 0;
			}

			char *inBuf = reinterpret_cast<char *>(in);
			it->second->recvBuf.insert(it->second->recvBuf.end(), inBuf, inBuf + len);

			// If we finished receiving a whole message
			if (!lws_remaining_packet_payload(wsi) && lws_is_final_fragment(wsi)) {
				// function which will parse recvBuf
				ha_bool retval = parseMessage(it->second->recvBuf, p2pSignalProcess, it->second.get());
				if (!retval) {
					// error in parsing, close connection
					LOG_ERROR("Error in parsing message from \"%s\"\n", it->second->url.c_str());
					return -1;
				}
			}
		}

		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		{
			assert(wsi != NULL);
			auto it = peerServer->signalConnections.find(wsi);
			if (it == peerServer->signalConnections.end()) {
				// nonexistent signal connection, close it
				return -1;
			}

			P2PSignalConnection *conn = it->second.get();
			if (conn->wsi != wsi) {
				// this connection is not the one currently active in humbleNetState.
				// this one must be obsolete, close it
				return -1;
			}
			if (conn->state == Closing || conn->state == Closed) {
				return -1;
			}

			if (conn->sendBuf.empty()) {
				// no data in sendBuf
				return 0;
			}

			size_t bufsize = conn->sendBuf.size();
			std::vector<unsigned char> sendbuf(LWS_SEND_BUFFER_PRE_PADDING + bufsize + LWS_SEND_BUFFER_POST_PADDING, 0);
			memcpy(&sendbuf[LWS_SEND_BUFFER_PRE_PADDING], &conn->sendBuf[0], bufsize);
			int retval = lws_write(conn->wsi, &sendbuf[LWS_SEND_BUFFER_PRE_PADDING], bufsize, LWS_WRITE_BINARY);
			if (retval < 0) {
				// error while sending, close the connection
				return -1;
			}
			if (retval < bufsize) {
				// This should not happen. lws_write returns the number of bytes written but it includes the headers it adds to pre padding which we don't know about.
				// So if it actually does a partial write there is no way for us to know how much of our data was sent and how much was headers, the API would be broken.
				// The docs say it buffers data internally and sends it all, so this shouldn't happen.
				LOG_ERROR("Partial write to peer %u (%s)\n", conn->peerId, conn->url.c_str());
				return -1;
			}

			// successful write
			conn->sendBuf.clear();
		}
		break;

	case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
		break;

	case LWS_CALLBACK_PROTOCOL_INIT:
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		// we don't care
		break;

	default:
		// LOG_WARNING("callback_humblepeer %p %u %p %p %u\n", wsi, reason, user, in, len);
		break;
	}

	return 0;
}

struct lws_protocols protocols[] = {
	  { "humblepeer", callback_humblepeer, 1 }
	, { NULL, NULL, 0 }
};
void help(const std::string& prog, const std::string& error = "")
{
	if (!error.empty()) {
		std::cerr << "Error: " << error << "\n\n";
	}
	std::cerr
		<< "WebRTC-NET peer match-making server\n"
		<< " " << prog << " [-h|--help] [--port <port>] [--tls --tls-cert <path> --tls-key <path>] [-v <level>]\n"
		<< "   Starts the signaling server on the provided port, or port 8080 by default.\n"
		<< "   Without --tls, it serves plain HTTP/WebSocket.\n"
		<< "   With --tls, it serves HTTPS/WSS on the same --port value.\n"
		<< "   --port <port>      Signaling server port\n"
		<< "   --tls              Enable the TLS vhost\n"
		<< "   --tls-cert <path>  TLS certificate chain file\n"
		<< "   --tls-key <path>   TLS private key file\n"
		<< "   -v <level>         Application log verbosity: error|warn|notice|info|debug\n"
		<< "   -h, --help Displays this help\n"
		<< std::endl;
}

static bool keepGoing = true;

void sighandler(int sig)
{
	keepGoing = false;
}

static bool parsePort(const std::string& value, int& port)
{
	char* end = NULL;
	long parsed = strtol(value.c_str(), &end, 10);
	if (end == value.c_str() || *end != '\0' || parsed < 1 || parsed > 65535) {
		return false;
	}
	port = static_cast<int>(parsed);
	return true;
}

int main(int argc, char *argv[]) {
	bool enable_tls = false;
	int port = 8080;
	int logLevelMask = LLL_ERR | LLL_WARN | LLL_NOTICE;
	std::string tls_cert_filepath;
	std::string tls_private_key_filepath;
	// Parse command line arguments
	for (int i = 1; i < argc; ++i) {
		std::string arg  = argv[i];
		if (arg == "-h" || arg == "--help") {
			help(argv[0]);
			exit(1);
		} else if (arg == "--port") {
			if (++i >= argc) {
				help(argv[0], "--port requires a port number");
				exit(2);
			}
			if (!parsePort(argv[i], port)) {
				help(argv[0], "--port must be a number from 1 to 65535");
				exit(2);
			}
		} else if (arg == "--tls") {
			enable_tls = true;
		} else if (arg == "--tls-cert") {
			if (++i >= argc) {
				help(argv[0], "--tls-cert requires a path");
				exit(2);
			}
			tls_cert_filepath = argv[i];
		} else if (arg == "--tls-key") {
			if (++i >= argc) {
				help(argv[0], "--tls-key requires a path");
				exit(2);
			}
			tls_private_key_filepath = argv[i];
		} else if (arg == "-v" || arg == "--verbosity") {
			if (++i >= argc) {
				help(argv[0], "-v requires a log level (error|warn|notice|info|debug)");
				exit(2);
			}
			logLevelMask = logLevelMaskFromName(argv[i]);
			if (logLevelMask == 0) {
				help(argv[0], "Unknown log level for -v: " + std::string(argv[i]));
				exit(2);
			}
		} else {
			help(argv[0], "Unknown option: " + arg);
			exit(2);
		}
	}
	if (enable_tls && tls_cert_filepath.empty()) {
		help(argv[0], "--tls requires --tls-cert <path>");
		exit(2);
	}
	if (enable_tls && tls_private_key_filepath.empty()) {
		help(argv[0], "--tls requires --tls-key <path>");
		exit(2);
	}
	if (!enable_tls && (!tls_cert_filepath.empty() || !tls_private_key_filepath.empty())) {
		help(argv[0], "--tls-cert and --tls-key require --tls");
		exit(2);
	}

	logFileOpen("", logLevelMask);
	// lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG | LLL_EXT, NULL);

	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	peerServer.reset(new Server());

	struct lws_context_creation_info info;
	memset(&info, 0, sizeof(info));
	info.gid = -1;
	info.uid = -1;
	info.options = LWS_SERVER_OPTION_IGNORE_MISSING_CERT | LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
	info.ka_time = 300;      // сек до первого KA
	info.ka_interval = 30;   // интервал между KA
	info.ka_probes = 5;
	const lws_retry_bo_t retry = {
		.secs_since_valid_ping = 30,
		.secs_since_valid_hangup = 100,
	};
	info.retry_and_idle_policy = &retry;
	
	peerServer->context = lws_create_context(&info);
	if (peerServer->context == NULL) {
		// TODO: error message
		exit(1);
	}

	info.port = port;
	info.protocols = protocols;
	if (!enable_tls) {
		LOG_WARNING("--tls not specified, not starting TLS server\n");
		info.vhost_name = "HTTP_vhost";
		if (lws_create_vhost(peerServer->context, &info) == NULL) {
			LOG_ERROR("Failed to create vhost for port %d\n", port);
			exit(1);
		}
	} else {
		info.vhost_name = "SSL_vhost";
		info.ssl_cert_filepath = tls_cert_filepath.c_str();
		info.ssl_private_key_filepath = tls_private_key_filepath.c_str();
		info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
		if (lws_create_vhost(peerServer->context, &info) == NULL) {
			LOG_ERROR("Failed to create vhost (SSL) for port %d\n", port);
			exit(1);
		}
	}

	while (keepGoing) {
		// use timeout so we will eventually process signals
		// but relatively long to reduce CPU load
		// TODO: configurable timeout
		lws_service(peerServer->context, 1000);
	}

	peerServer.reset();

	return 0;
}
