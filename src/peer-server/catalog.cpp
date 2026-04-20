#include "catalog.h"
#include "p2p_connection.h"
#include "humblenet_utils.h"

namespace humblenet {

	void Catalog::erasePeerAliases(PeerId p)
	{
		for( auto it = aliases.begin(); it != aliases.end(); ) {
			if( it->second == p ) {
				aliases.erase( it++ );
			} else {
				++it;
			}
		}
	}
}
