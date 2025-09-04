#pragma once

#include "humblenet.h"

#include <unordered_map>
#include <string>
#include <sys/random.h>
#include <unistd.h>

namespace humblenet {

	struct P2PSignalConnection;

	struct Catalog {
		PeerId generateNewPeerId()
		{
			PeerId peerId = 0;
			while (!peerId || peers.find(peerId) != peers.end()) {
				getentropy(&peerId, sizeof(peerId));
				peerId = peerId & 0x7FFFFFFF;  // make sure it's positive
			}
			return peerId;
		}

		std::unordered_map<PeerId, P2PSignalConnection *> peers;  // not owned, must also be in signalConnections

		std::unordered_map<std::string, PeerId> aliases;

		void erasePeerAliases(PeerId p);
	};
}
