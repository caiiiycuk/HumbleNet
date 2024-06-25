#include <signal.h>

#include <cstdlib>
#include <cstring>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <algorithm>

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

#include "game.h"
#include "p2p_connection.h"
#include "server.h"
#include "logging.h"
#include "game_db.h"

using namespace humblenet;

namespace humblenet {

	ha_bool sendP2PMessage(P2PSignalConnection *conn, const uint8_t *buff, size_t length) {
		conn->sendMessage(buff, length);
		return true;
	}

}  // namespace humblenet

static std::unique_ptr<Server> peerServer;


static ha_bool p2pSignalProcess(const humblenet::HumblePeer::Message *msg, void *user_data) {
	return reinterpret_cast<P2PSignalConnection *>(user_data)->processMsg(msg);
}

int callback_humblepeer(struct lws *wsi
				  , enum lws_callback_reasons reason
				  , void *user, void *in, size_t len) {


	switch (reason) {

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

			LOG_INFO("Closing connection to peer %u (%s)\n", conn->peerId, conn->url.c_str());

			if (conn->peerId != 0) {
				// remove it from list of peers
				assert(conn->game != NULL);
				auto it2 = conn->game->peers.find(conn->peerId);
				// if peerId is valid (nonzero)
				// this MUST exist
				assert(it2 != conn->game->peers.end());
				conn->game->peers.erase(it2);

				// remove any aliases to this peer
				conn->game->erasePeerAliases(conn->peerId);
			}

			// and finally remove from list of signal connections
			// this is unique_ptr so it also destroys the object
			peerServer->signalConnections.erase(it);
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

			// function which will parse recvBuf
			ha_bool retval = parseMessage(it->second->recvBuf, p2pSignalProcess, it->second.get());
			if (!retval) {
				// error in parsing, close connection
				LOG_ERROR("Error in parsing message from \"%s\"\n", it->second->url.c_str());
				return -1;
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

			// successful write
			conn->sendBuf.erase(conn->sendBuf.begin(), conn->sendBuf.begin() + retval);
			// check if it was only partial
			if (!conn->sendBuf.empty()) {
				lws_callback_on_writable(conn->wsi);
			}
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

extern "C" struct lws_protocols acme_plugin_protocol;

struct lws_protocols protocols_8080[] = {
	  { "humblepeer", callback_humblepeer, 1 }
	, { NULL, NULL, 0 }
};

struct lws_protocols protocols_443[] = {
	  { "humblepeer", callback_humblepeer, 1 }
	, acme_plugin_protocol

	, { NULL, NULL, 0 }
};

void help(const std::string& prog, const std::string& error = "")
{
	if (!error.empty()) {
		std::cerr << "Error: " << error << "\n\n";
	}
	std::cerr
		<< "Humblenet peer match-making server\n"
		<< " " << prog << "[-h] --email em@example.com --common-name test.example.com\n"
		<< "   --email Used to request SSL certificate from Let's Encrypt\n"
		<< "   --common-name Domain name Let's Encrypt will ping during ACME and issue certs for\n"
		<< "   -h     Displays this help\n"
		<< std::endl;
}

static bool keepGoing = true;

void sighandler(int sig)
{
	keepGoing = false;
}

int main(int argc, char *argv[]) {
	char* email = nullptr;
	char* common_name = nullptr;
	// Parse command line arguments
	for (int i = 1; i < argc; ++i) {
		std::string arg  = argv[i];
		if (arg == "-h") {
			help(argv[0]);
			exit(1);
		} else if (arg == "--email") {
			++i;
			if (i < argc) {
				email = argv[i];
			} else {
				help(argv[0], "--email option requires an argument");
				exit(2);
			}
		} else if (arg == "--common-name") {
			++i;
			if (i < argc) {
				common_name = argv[i];
			} else {
				help(argv[0], "--common_name option requires an argument");
				exit(2);
			}
		}
	}

	if (email == nullptr || common_name == nullptr) {
		help(argv[0], "--email and --common-name are required if you want to run with TLS\n");
	}

	// logFileOpen("peer-server.log");
	logFileOpen("");

	std::ofstream ofs("peer-server.pidfile");
#ifndef _WIN32
	ofs << (int)getpid() << std::endl;
#endif // _WIN32

	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	std::shared_ptr<GameDB> gameDB;

	gameDB.reset(new GameDBAnonymous());
	// gameDB.reset(new GameDBFlatFile(config.gameDB.substr(5)));

	peerServer.reset(new Server(gameDB));
	peerServer->stunServerAddress = "stun.cloudflare.com:3478";

	struct lws_context_creation_info info;
	memset(&info, 0, sizeof(info));
	info.gid = -1;
	info.uid = -1;
	info.options = LWS_SERVER_OPTION_IGNORE_MISSING_CERT | LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
	
	peerServer->context = lws_create_context(&info);
	if (peerServer->context == NULL) {
		// TODO: error message
		exit(1);
	}

	info.port = 8080;
	info.vhost_name = "HTTP_8080_vhost";
	struct lws_protocol_vhost_options pvo6 = {
		NULL, NULL, "email", email
	}, pvo5 = {
		&pvo6, NULL, "common-name", common_name
	}, pvo4 = {
		&pvo5, NULL, "directory-url", "https://acme-v02.api.letsencrypt.org/directory" //"https://acme-staging-v02.api.letsencrypt.org/directory"
	}, pvo3 = {
		&pvo4, NULL, "auth-path", "./auth.jwk"
	}, pvo2 = {
		&pvo3, NULL, "cert-path", "./peer-server.key.crt"
	}, pvo1 = {
		&pvo2, NULL, "key-path", "./peer-server.key.pem" /* would be an absolute path */
	}, pvo = {
		NULL,                  /* "next" pvo linked-list */
		&pvo1,                 /* "child" pvo linked-list */
		"lws-acme-client",        /* protocol name we belong to on this vhost */
		""                     /* ignored */
	};
	info.pvo = &pvo;

	struct lws_vhost *host_8080 = lws_create_vhost(peerServer->context, &info);
	if (host_8080 == NULL) {
		LOG_ERROR("Failed to create vhost for port 8080\n");
		exit(1);
	}

	if (email == nullptr || common_name == nullptr) {
		LOG_WARNING("--email or --common-name not specified, not starting TLS server\n");
	} else {
		info.protocols = protocols_443;
		info.port = 443;
		info.vhost_name = "SSL_vhost";
		info.ssl_cert_filepath = "./peer-server.key.crt";
		info.ssl_private_key_filepath = "./peer-server.key.pem";
		info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
		struct lws_vhost *host_443 = lws_create_vhost(peerServer->context, &info);
		if (host_443 == NULL) {
			LOG_ERROR("Failed to create vhost for port 443\n");
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

#ifndef _WIN32
	unlink("peer-server.pidfile");
#endif // _WIN32

	return 0;
}
