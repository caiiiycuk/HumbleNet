#include "catalog.h"
#include "p2p_connection.h"
#include "humblenet_utils.h"
#include "server.h"

#include <array>
#include <cassert>
#include <vector>

namespace humblenet {

	std::string Catalog::generateReconnectToken() const
	{
		static const char hex[] = "0123456789abcdef";
		std::array<uint8_t, 16> randomBytes = {};
		int result = getentropy(randomBytes.data(), randomBytes.size());
		assert(result == 0);

		std::string reconnectToken;
		reconnectToken.resize(randomBytes.size() * 2);
		for (size_t i = 0; i < randomBytes.size(); ++i) {
			reconnectToken[2 * i] = hex[randomBytes[i] >> 4];
			reconnectToken[2 * i + 1] = hex[randomBytes[i] & 0x0F];
		}
		return reconnectToken;
	}

	PeerSession *Catalog::createSession()
	{
		PeerId peerId = generateNewPeerId();
		std::string reconnectToken;
		do {
			reconnectToken = generateReconnectToken();
		} while (reconnectTokens.find(reconnectToken) != reconnectTokens.end());

		auto inserted = sessions.emplace(peerId, std::unique_ptr<PeerSession>(new PeerSession(peerId, reconnectToken)));
		PeerSession *session = inserted.first->second.get();
		reconnectTokens.emplace(reconnectToken, session);
		return session;
	}

	PeerSession *Catalog::findSessionByReconnectToken(const std::string& reconnectToken)
	{
		auto it = reconnectTokens.find(reconnectToken);
		if (it == reconnectTokens.end()) {
			return NULL;
		}
		return it->second;
	}

	void Catalog::attachSession(PeerSession *session, P2PSignalConnection *conn)
	{
		assert(session != NULL);
		assert(conn != NULL);

		if (session->connection != NULL) {
			P2PSignalConnection *oldConnection = session->connection;
			peers.erase(session->peerId);
			oldConnection->connectedPeers.clear();
			oldConnection->session = NULL;
			oldConnection->catalog = NULL;
			oldConnection->peerId = 0;
			oldConnection->peerServer->closeConnection(oldConnection);
		}

		session->connection = conn;
		session->expiresAtMs = 0;
		peers[session->peerId] = conn;

		conn->catalog = this;
		conn->session = session;
		conn->peerId = session->peerId;
	}

	void Catalog::detachSession(PeerSession *session, uint64_t expiresAtMs)
	{
		if (session == NULL) {
			return;
		}

		peers.erase(session->peerId);
		session->connection = NULL;
		session->expiresAtMs = expiresAtMs;
	}

	void Catalog::registerAlias(PeerSession *session, const std::string& alias)
	{
		assert(session != NULL);
		aliases[alias] = session->peerId;
		session->aliases.insert(alias);
	}

	bool Catalog::unregisterAlias(PeerSession *session, const std::string& alias)
	{
		if (session == NULL) {
			return false;
		}

		auto existing = aliases.find(alias);
		if (existing == aliases.end() || existing->second != session->peerId) {
			return false;
		}

		aliases.erase(existing);
		session->aliases.erase(alias);
		return true;
	}

	void Catalog::unregisterAllAliases(PeerSession *session)
	{
		if (session == NULL) {
			return;
		}

		for (const auto& alias : session->aliases) {
			auto existing = aliases.find(alias);
			if (existing != aliases.end() && existing->second == session->peerId) {
				aliases.erase(existing);
			}
		}
		session->aliases.clear();
	}

	void Catalog::destroySession(PeerId peerId)
	{
		auto sessionIt = sessions.find(peerId);
		if (sessionIt == sessions.end()) {
			return;
		}

		PeerSession *session = sessionIt->second.get();
		if (session->connection != NULL) {
			peers.erase(peerId);
			session->connection->session = NULL;
			session->connection->catalog = NULL;
			session->connection->peerId = 0;
			session->connection = NULL;
		}

		unregisterAllAliases(session);
		reconnectTokens.erase(session->reconnectToken);

		for (auto& it : sessions) {
			if (it.second->connection != NULL) {
				it.second->connection->connectedPeers.erase(peerId);
			}
		}

		sessions.erase(sessionIt);
	}

	void Catalog::expireSessions(uint64_t nowMs)
	{
		std::vector<PeerId> expiredPeerIds;
		for (const auto& it : sessions) {
			const PeerSession *session = it.second.get();
			if (session->connection == NULL && session->expiresAtMs != 0 && session->expiresAtMs <= nowMs) {
				expiredPeerIds.push_back(session->peerId);
			}
		}

		for (PeerId peerId : expiredPeerIds) {
			destroySession(peerId);
		}
	}
}
