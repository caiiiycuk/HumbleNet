#include <libwebsockets.h>

#include "server.h"
#include "logging.h"

namespace humblenet {
	Server::Server()
	: context(NULL),
	catalog(new Catalog())
	{
	}

	void Server::triggerWrite(struct lws *wsi)
	{
		lws_callback_on_writable(wsi);
	}

	void Server::closeConnection(P2PSignalConnection *conn)
	{
		if (conn == NULL || conn->wsi == NULL || conn->state == Closed || conn->state == Closing) {
			return;
		}

		conn->state = Closing;
		conn->sendBuf.clear();
		lws_close_reason(conn->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
		lws_callback_on_writable(conn->wsi);
	}
}
