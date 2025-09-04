#include "server.h"
#include "logging.h"
#include "hmac.h"

namespace humblenet {
	Server::Server()
	: context(NULL),
	catalog(new Catalog())
	{
	}

	Catalog *Server::getVerifiedGame(const HumblePeer::HelloServer* hello)
	{
		auto gameToken = hello->gameToken();
		auto gameSignature = hello->gameSignature();

		if (!gameToken || !gameSignature) {
			LOG_ERROR("No game token provided!\n");
			return nullptr;
		}

		return catalog.get();
	}

	void Server::triggerWrite(struct lws *wsi)
	{
		lws_callback_on_writable(wsi);
	}

	void Server::populateStunServers(std::vector<ICEServer> &servers)
	{
		if (turnServer == "auto" && !turnUsername.empty() && !turnPassword.empty()) {
			LOG_INFO("using js-dos turn/stun '%s' '%s'\n", turnUsername.c_str(), turnPassword.c_str());
			servers.emplace_back("cloud.js-dos.com:3478");
			servers.emplace_back("cloud.js-dos.com:3478", turnUsername, turnPassword);
			servers.emplace_back("cloud.js-dos.com:5349", turnUsername, turnPassword);
		} else {
			LOG_ERROR("Not using js-dos turn/stun\n");
			if( ! stunServerAddress.empty() ) {
				servers.emplace_back(stunServerAddress);
			}
			if (!turnServer.empty() && !turnUsername.empty() && !turnPassword.empty()) {
				servers.emplace_back(turnServer, turnUsername, turnPassword);
			}
		}
	}
}