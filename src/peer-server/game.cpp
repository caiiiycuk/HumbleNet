#include "game.h"
#include "p2p_connection.h"
#include "humblenet_utils.h"
#include "./db.h"

namespace humblenet {

	void Game::erasePeerAliases(PeerId p)
	{
		for( auto it = aliases.begin(); it != aliases.end(); ) {
			if( it->second == p ) {
				db::get()->aliasRemoved(it->first.c_str());
				aliases.erase( it++ );
			} else {
				++it;
			}
		}
	}
}

