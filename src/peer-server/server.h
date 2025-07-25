#pragma once

#include "game.h"
#include "p2p_connection.h"

#include <unordered_map>
#include <string>
#include <memory>

struct lws_context;
struct lws;

namespace humblenet {
	class GameDB;

	struct Server {
		struct lws_context *context;

		// we need to be able to lookup connections on both
		// websocket (for the receive callback) and peer id (when sending stuff)
		std::unordered_map<struct lws *, std::unique_ptr<P2PSignalConnection> > signalConnections;

		std::unordered_map<GameId, std::unique_ptr<Game> > games;

		std::string stunServerAddress;
		std::string turnServer;
		std::string turnUsername;
		std::string turnPassword;


		Server(std::shared_ptr<GameDB> _gameDB);

		Game *getVerifiedGame(const HumblePeer::HelloServer* hello);
		void populateStunServers(std::vector<ICEServer>& servers);
		void triggerWrite(struct lws* wsi);

	private:
		std::shared_ptr<GameDB> m_gameDB;
	};

}