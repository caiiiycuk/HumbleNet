#pragma once

#include "humblenet.h"

#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <sys/random.h>
#include <unistd.h>

namespace humblenet {

	struct P2PSignalConnection;
	struct PeerSession {
		PeerId peerId;
		std::string reconnectToken;
		std::unordered_set<std::string> aliases;
		P2PSignalConnection *connection;
		uint64_t expiresAtMs;

		PeerSession(PeerId peerId_, const std::string& reconnectToken_)
		: peerId(peerId_)
		, reconnectToken(reconnectToken_)
		, connection(NULL)
		, expiresAtMs(0)
		{
		}
	};

	struct Catalog {
		static constexpr uint64_t kReconnectGracePeriodMs = 5ULL * 60ULL * 1000ULL;

		PeerId generateNewPeerId()
		{
			PeerId peerId = 0;
			while (!peerId || sessions.find(peerId) != sessions.end()) {
				getentropy(&peerId, sizeof(peerId));
				peerId = peerId & 0x7FFFFFFF;  // make sure it's positive
			}
			return peerId;
		}

		std::string generateReconnectToken() const;
		PeerSession *createSession();
		PeerSession *findSessionByReconnectToken(const std::string& reconnectToken);
		void attachSession(PeerSession *session, P2PSignalConnection *conn);
		void detachSession(PeerSession *session, uint64_t expiresAtMs);
		void destroySession(PeerId peerId);
		void expireSessions(uint64_t nowMs);
		void registerAlias(PeerSession *session, const std::string& alias);
		bool unregisterAlias(PeerSession *session, const std::string& alias);
		void unregisterAllAliases(PeerSession *session);

		std::unordered_map<PeerId, P2PSignalConnection *> peers;  // active signaling connections only
		std::unordered_map<PeerId, std::unique_ptr<PeerSession> > sessions;
		std::unordered_map<std::string, PeerSession *> reconnectTokens;

		std::unordered_map<std::string, PeerId> aliases;
	};
}
