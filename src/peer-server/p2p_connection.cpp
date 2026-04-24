#include "p2p_connection.h"

#include <chrono>

#include "logging.h"
#include "catalog.h"
#include "server.h"

#include "humblenet_utils.h"


namespace humblenet {
	namespace {
		uint64_t nowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
		}
	}

	ha_bool P2PSignalConnection::processMsg(const HumblePeer::Message* msg)
	{
		auto msgType = msg->message_type();

		// we don't accept anything but HelloServer from a peer which hasn't sent one yet
		if (!this->peerId) {
			if (msgType != HumblePeer::MessageType::HelloServer) {
				LOG_WARNING("Got non-HelloServer message (%s) from non-authenticated peer \"%s\"\n", HumblePeer::EnumNameMessageType(msgType), this->url.c_str());
				// TODO: send error message
				return false;
			}
		}

		switch (msgType) {
			case HumblePeer::MessageType::P2POffer:
			{
				auto p2p = reinterpret_cast<const HumblePeer::P2POffer*>(msg->message());
				auto peer = p2p->peerId();

				// look up target peer, send message on
				// TODO: should ensure there's not already a negotiation going

				bool emulated = (p2p->flags() & 0x1);

				LOG_INFO("P2POffer from peer %u (%s) to peer %u", this->peerId, url.c_str(), peer);
				if (emulated) {
					LOG_INFO(", emulated connections not allowed\n");
					// TODO: better error message
					sendNoSuchPeer(this, peer);
					return true;
				}

				auto it = catalog->peers.find(peer);
				if (it == catalog->peers.end()) {
					LOG_WARNING(", no such peer\n");
					sendNoSuchPeer(this, peer);
				} else {
					P2PSignalConnection *otherPeer = it->second;
					assert(otherPeer != NULL);

					// if not emulated check if otherPeer supports WebRTC
					// to avoid unnecessary round trip
					if (!emulated && !otherPeer->webRTCsupport) {
						// webrtc connections not supported
						LOG_INFO("(%s), refusing because target doesn't support WebRTC\n", otherPeer->url.c_str());
						sendPeerRefused(this, peer);
						return true;
					}

					LOG_INFO("(%s)\n", otherPeer->url.c_str());

					// TODO: should check that it doesn't exist already
					this->connectedPeers.insert(otherPeer->peerId);

					// set peer id to originator so target knows who wants to connect
					sendP2PConnect(otherPeer, this->peerId, p2p->flags(), p2p->offer()->c_str());
				}

			}
				break;

			case HumblePeer::MessageType::P2PAnswer:
			{
				auto p2p = reinterpret_cast<const HumblePeer::P2PAnswer*>(msg->message());
				auto peer = p2p->peerId();

				// look up target peer, send message on
				// TODO: should only send if peers are currently negotiating connection

				auto it = catalog->peers.find(peer);

				if (it == catalog->peers.end()) {
					LOG_WARNING("P2PResponse from peer %u (%s) to nonexistent peer %u\n", this->peerId, this->url.c_str(), peer);
					sendNoSuchPeer(this, peer);
					return true;
				} else {
					P2PSignalConnection *otherPeer = it->second;
					assert(otherPeer != NULL);
					assert(otherPeer->peerId == peer);

					bool otherPeerConnected = otherPeer->connectedPeers.find(this->peerId) != otherPeer->connectedPeers.end();
					if (!otherPeerConnected) {
						// we got P2PResponse but there's been no P2PConnect from
						// the peer we're supposed to respond to
						// client is either confused or malicious
						LOG_WARNING("P2PResponse from peer %u (%s) to peer %u (%s) who has not requested a P2P connection\n", this->peerId, this->url.c_str(), otherPeer->peerId, otherPeer->url.c_str());
						sendNoSuchPeer(this, peer);
						return true;
					}

					// TODO: should assert it's not there yet
					this->connectedPeers.insert(otherPeer->peerId);

					// set peer id to originator so target knows who wants to connect
					sendP2PResponse(otherPeer, this->peerId, p2p->offer()->c_str());
				}
			}
				break;

			case HumblePeer::MessageType::ICECandidate:
			{
				auto p2p = reinterpret_cast<const HumblePeer::ICECandidate*>(msg->message());
				auto peer = p2p->peerId();

				// look up target peer, send message on
				// TODO: should only send if peers are currently negotiating connection

				auto it = catalog->peers.find(peer);

				if (it == catalog->peers.end()) {
					LOG_WARNING(", no such peer\n");
					sendNoSuchPeer(this, peer);
				} else {
					P2PSignalConnection *otherPeer = it->second;
					assert(otherPeer != NULL);
					sendICECandidate(otherPeer, this->peerId, p2p->offer()->c_str());
				}

			}
				break;

			case HumblePeer::MessageType::P2PReject:
			{
				auto p2p = reinterpret_cast<const HumblePeer::P2PReject*>(msg->message());
				auto peer = p2p->peerId();

				auto it = catalog->peers.find(peer);
				// TODO: check there's such a connection attempt ongoing
				if (it == catalog->peers.end()) {
					switch (p2p->reason()) {
						case HumblePeer::P2PRejectReason::PeerRefused:
							LOG_WARNING("Peer %u (%s) tried to refuse connection from nonexistent peer %u\n", peerId, url.c_str(), peer);
							break;
						case HumblePeer::P2PRejectReason::NotFound:
							LOG_WARNING("Peer %u (%s) sent unexpected NotFound to non existent peer %u", peerId, url.c_str(), peer);
							break;
					}
					break;
				} else {
					P2PSignalConnection *otherPeer = it->second;
					assert(otherPeer != NULL);
					switch (p2p->reason()) {
						case HumblePeer::P2PRejectReason::PeerRefused:
							LOG_INFO("Peer %u (%s) refused connection from peer %u (%s)\n", this->peerId, this->url.c_str(), otherPeer->peerId, otherPeer->url.c_str());
							sendPeerRefused(otherPeer, this->peerId);
							break;
						case HumblePeer::P2PRejectReason::NotFound:
							LOG_WARNING("Peer %u (%s) setnt unexpected NotFound from peer %u (%s)\n", this->peerId, this->url.c_str(), otherPeer->peerId, otherPeer->url.c_str());
							sendPeerRefused(otherPeer, this->peerId);
							break;
					}
				}
			}
				break;

			case HumblePeer::MessageType::HelloServer:
			{
				auto hello = reinterpret_cast<const HumblePeer::HelloServer*>(msg->message());

				if (peerId != 0) {
					LOG_ERROR("Got hello HelloServer from client which already has a peer ID (%u)\n", peerId);
					return true;
				}

				if ((hello->flags() & 0x01) == 0) {
					LOG_ERROR("Client %s does not support WebRTC\n", url.c_str());
					return true;
				}

#pragma message ("TODO handle user authentication")

				this->catalog = peerServer->catalog.get();
				if (this->catalog == NULL) {
					// invalid game
					return false;
				}

				auto attributes = hello->attributes();
				const flatbuffers::String *platform = nullptr;
				if (attributes) {
					auto platform_lookup = attributes->LookupByKey("platform");
					if (platform_lookup) {
						platform = platform_lookup->value();
					}
				}

				bool resumed = false;
				std::string requestedReconnectToken = hello->reconnectToken() ? hello->reconnectToken()->str() : "";
				PeerSession *peerSession = NULL;
				if (!requestedReconnectToken.empty()) {
					peerSession = this->catalog->findSessionByReconnectToken(requestedReconnectToken);
					if (peerSession == NULL) {
						LOG_INFO("Resume token rejected for \"%s\": unknown token\n", url.c_str());
					} else if (peerSession->expiresAtMs != 0 && peerSession->expiresAtMs <= nowMs()) {
						LOG_WARNING("Resume token expired for \"%s\": peer %u exceeded the reconnect grace period\n", url.c_str(), peerSession->peerId);
						this->catalog->destroySession(peerSession->peerId);
						peerSession = NULL;
					} else {
						if (peerSession->connection != NULL) {
							LOG_INFO("Resume token accepted for \"%s\": replacing lingering connection for peer %u\n", url.c_str(), peerSession->peerId);
						}
						resumed = true;
					}
				}

				if (peerSession == NULL) {
					peerSession = this->catalog->createSession();
				}

				this->catalog->attachSession(peerSession, this);
				this->webRTCsupport = true;
				this->trickleICE = !(hello->flags() & 0x2);
				LOG_INFO("%s signaling session from \"%s\" (peer %u, platform: %s)\n",
					resumed ? "Resumed" : "Created",
					url.c_str(), peerId, platform ? platform->c_str(): "");

				// send hello to client
				sendHelloClient(this, peerId, peerSession->reconnectToken);
			}
				break;

			case HumblePeer::MessageType::HelloClient:
				// TODO: log address
				LOG_ERROR("Got hello HelloClient, not supposed to happen\n");
				break;

			case HumblePeer::MessageType::P2PConnected:
				// TODO: log address
				LOG_INFO("P2PConnect from peer %u\n", peerId);
				// TODO: clean up ongoing negotiations lit
				break;
			case HumblePeer::MessageType::P2PDisconnect:
				LOG_INFO("P2PDisconnect from peer %u\n", peerId);
				break;

				// Alias processing
			case HumblePeer::MessageType::AliasRegister:
			{
				auto reg = reinterpret_cast<const HumblePeer::AliasRegister*>(msg->message());
				auto alias = reg->alias();

				auto existing = catalog->aliases.find( alias->c_str() );

				if( existing != catalog->aliases.end() && existing->second != peerId ) {
					LOG_INFO("Rejecting peer %u's request to register alias '%s' which is already registered to peer %u\n", peerId, alias->c_str(), existing->second );
	#pragma message ("TODO implement registration failure")
				} else {
					catalog->registerAlias(session, alias->c_str());
					LOG_INFO("Registering alias '%s' to peer %u\n", alias->c_str(), peerId );
	#pragma message ("TODO implement registration success")
				}
			}
				break;

			case HumblePeer::MessageType::AliasUnregister:
			{
				auto unreg = reinterpret_cast<const HumblePeer::AliasUnregister*>(msg->message());
				auto alias = unreg->alias();

				if (alias) {
					if(catalog->unregisterAlias(session, alias->c_str())) {
						LOG_INFO("Unregistring alias '%s' for peer %u\n", alias->c_str(), peerId );
	#pragma message ("TODO implement unregister sucess")
					} else {
						LOG_INFO("Rejecting unregister of alias '%s' for peer %u\n", alias->c_str(), peerId );
	#pragma message ("TODO implement unregister failure")
					}
				} else {
					catalog->unregisterAllAliases(session);
					LOG_INFO("Unregistring all aliases for peer for peer %u\n", peerId );
	#pragma message ("TODO implement unregister sucess")
				}
			}
				break;

			case HumblePeer::MessageType::AliasLookup:
			{
				auto lookup = reinterpret_cast<const HumblePeer::AliasLookup*>(msg->message());
				auto alias = lookup->alias();

				auto existing = catalog->aliases.find( alias->c_str() );

				if( existing != catalog->aliases.end() ) {
					LOG_INFO("Lookup of alias '%s' for peer %u resolved to peer %u\n", alias->c_str(), peerId, existing->second );
					sendAliasResolved(this, alias->c_str(), existing->second);
				} else {
					LOG_INFO("Lookup of alias '%s' for peer %u failed. No alias registered\n", alias->c_str(), peerId );
					sendAliasResolved(this, alias->c_str(), 0);
				}
			}
				break;

			case HumblePeer::MessageType::AliasQuery: {
				static std::vector<std::pair<std::string, PeerId>> matched;
				static std::string cachedQuery = "";
				static long updatedAt = 0;

				auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()
				).count();

				auto aliasQuery = reinterpret_cast<const HumblePeer::AliasQuery*>(msg->message());
				auto query = std::string(aliasQuery->query()->c_str());

				if (!query.empty() && query[0] == '=') {
					auto cmp = query.substr(1, query.length()-1);
					std::vector<std::pair<std::string, PeerId>> matched;
					const auto& it = catalog->aliases.find(cmp);
					if (it != catalog->aliases.end()) {
						matched.emplace_back(it->first, it->second);
					}

					LOG_INFO("Query for alias '%s' for peer %u resolved to %d peers\n", query.c_str(), peerId,
						matched.size());

					sendAliasQueryResolved(this, query, matched);
					return true;
				}

				if (now - updatedAt > 1000 || cachedQuery != query) {
					matched.clear();
					for (const auto& it: catalog->aliases) {
						if (query.length() == 0 || it.first.find(query) == 0) {
							matched.emplace_back(it.first, it.second);
						}
					}

					LOG_INFO("Query for aliases '%s' for peer %u resolved to %d/%d peers\n", query.c_str(), peerId,
						matched.size(), catalog->aliases.size() );

					cachedQuery = query;
					updatedAt = now;
				} else {
					LOG_INFO("[CACHE] Query for aliases '%s' for peer %u resolved to %d/%d peers\n", query.c_str(), peerId,
						matched.size(), catalog->aliases.size() );
				}

				sendAliasQueryResolved(this, query, matched);
			}
				break;

			default:
				LOG_WARNING("Unhandled P2P Message: %s\n", HumblePeer::EnumNameMessageType(msgType));
				break;
		}

		return true;
	}


	void P2PSignalConnection::sendMessage(const uint8_t *buff, size_t length) {
		bool wasEmpty = this->sendBuf.empty();
		this->sendBuf.insert(this->sendBuf.end(), buff, buff + length);
		if (wasEmpty) {
			peerServer->triggerWrite(this->wsi);
		}
	}

}
