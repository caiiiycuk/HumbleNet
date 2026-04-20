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
}
