#pragma once

#include "humblenet.h"
#include "humblepeer.h"
#include "catalog.h"

struct lws;

namespace humblenet {
	struct Catalog;
	struct Server;

	enum HumblePeerState {
		Opening
		, Open
		, Closing
		, Closed
	};

	struct P2PSignalConnection {
		Server* peerServer;

		std::vector<uint8_t> recvBuf;
		std::vector<char> sendBuf;
		struct lws *wsi;
		PeerId peerId;
		HumblePeerState state;

		bool webRTCsupport;
		bool trickleICE;

		Catalog *catalog;

		std::string url;

		// peers which have a P2P connection with this one
		// pointer not owned
		std::unordered_set<P2PSignalConnection *> connectedPeers;


		P2PSignalConnection(Server* s)
		: peerServer(s)
		, wsi(NULL)
		, peerId (0)
		, state(Opening)
		, webRTCsupport(false)
		, trickleICE(true)
		, catalog(NULL)
		{
		}

		ha_bool processMsg(const HumblePeer::Message* msg);

		void sendMessage(const uint8_t *buff, size_t length);
	};

}
