#include <libwebsockets.h>

#include "server.h"
#include "logging.h"

namespace humblenet {
	Server::Server()
	: context(NULL),
	catalog(new Catalog())
	{
	}

	void Server::closeConnection(P2PSignalConnection *conn)
	{
		if (conn == NULL || conn->wsi == NULL || conn->state == Closed || conn->state == Closing) {
			return;
		}

		conn->state = Closing;
		conn->sendQueue.clear();
		conn->queuedBytes = 0;
		lws_close_reason(conn->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
		lws_callback_on_writable(conn->wsi);
	}
}
