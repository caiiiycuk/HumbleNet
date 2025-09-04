#include "server.h"
#include "logging.h"

namespace humblenet {
	Server::Server()
	: context(NULL),
	catalog(new Catalog())
	{
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