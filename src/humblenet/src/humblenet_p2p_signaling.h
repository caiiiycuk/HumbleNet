#ifndef HUMBLENET_SIGNALING
#define HUMBLENET_SIGNALING

#include "humblepeer.h"
#include "libsocket.h"
#include <cstddef>
#include <deque>
#include <vector>

namespace humblenet {
	struct P2PSignalConnection {
		internal_socket_t *wsi;
		std::vector<uint8_t> recvBuf;
		std::deque<std::vector<uint8_t>> sendQueue;
		size_t queuedBytes;
		bool writableWakePending;

		P2PSignalConnection()
		: wsi(NULL)
		, queuedBytes(0)
		, writableWakePending(false)
		{
		}

        void disconnect() {
            if( wsi )
                internal_close_socket(wsi);
            wsi = NULL;
        }

        void drop() {
            if( wsi )
                internal_abort_socket(wsi);
            wsi = NULL;
        }
    };
    
	bool register_protocol( internal_context_t* context );
}

ha_bool humblenet_signaling_connect();
void humblenet_signaling_force_reconnect(const char* reason);

#endif // HUMBLENET_SIGNALING
