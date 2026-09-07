#ifndef EMSCRIPTEN

#include "libwebsockets_native.h"

#include "libpoll.h"
#include <algorithm>
#include <cstring>
#include <mutex>

#include <vector>
#include <string>
#include <assert.h>

// Stupid hack - libwebsockets uses its own POLL defines on Windows
#ifdef _WIN32
#define LWS_POLLHUP (FD_CLOSE)
#define LWS_POLLIN (FD_READ | FD_ACCEPT)
#define LWS_POLLOUT (FD_WRITE)
#else
#define LWS_POLLHUP (POLLHUP|POLLERR)
#define LWS_POLLIN POLLIN
#define LWS_POLLOUT POLLOUT
#endif

#define LOG printf

static_assert(sizeof(lws_pollfd) == sizeof(pollfd), "pollfd struct size mismatch!");
static lws_callback_function* detachedDelegate;

struct LibWebSocket_Module : public poll_module_t {

	LibWebSocket_Module(lws_protocols* protocols)
	: context(NULL)
	, protocols(protocols)
	, delegate(protocols[0].callback)
	, attached(false) {
		PreSelect = (poll_pre_select)&OnPreSelect;
		PostSelect = (poll_post_select)&OnPostSelect;
		Destroy = (poll_pre_destroy)OnPreDestroy;
		ParentChain = NULL;
		ExtraMemoryPtr = NULL;

		// AAAAAIEEEEE!!!
		assert( protocols[0].callback != &InterceptCallback );
		detachedDelegate = delegate;
		
		this->protocols[0].callback = &InterceptCallback;
	}

	~LibWebSocket_Module() {
		restoreCallbacks();
	}

	void attach() {
		poll_init_with_module(this);
		attached = true;
	}

	void setContext(lws_context* value) {
		std::lock_guard<std::recursive_mutex> lock(lwsMutex);
		context = value;
	}

	bool makeInert() {
		std::lock_guard<std::recursive_mutex> lock(lwsMutex);
		context = NULL;
		restoreCallbacks();
		if (!attached)
			return false;
		attached = false;
		return true;
	}

	struct lws* connect(lws_client_connect_info* ccinfo) {
		std::lock_guard<std::recursive_mutex> lock(lwsMutex);
		return lws_client_connect_via_info(ccinfo);
	}

	int callback(struct lws_context *context
						   , struct lws *wsi
						   , enum lws_callback_reasons reason
						   , void *user, void *in, size_t len) {
		std::lock_guard<std::recursive_mutex> lock(lwsMutex);
		switch (reason) {
			case LWS_CALLBACK_PROTOCOL_INIT: {
				this->context = context;
			} break;

			case LWS_CALLBACK_PROTOCOL_DESTROY: {
				makeInert();
			} break;

			case LWS_CALLBACK_ADD_POLL_FD: {
				struct lws_pollargs *pa = (struct lws_pollargs *)in;

				struct lws_pollfd newfd;
				memset(&newfd, 0, sizeof(newfd));
				newfd.fd = pa->fd;
				newfd.events = pa->events;
				newfd.revents = 0;

				pollfds.push_back(newfd);

				//LOG("LWS_CALLBACK_ADD_POLL_FD fd: %d events: 0x%x wsi: %p\n", pa->fd, pa->events, wsi);

			} break;

			case LWS_CALLBACK_DEL_POLL_FD: {
				struct lws_pollargs *pa = (struct lws_pollargs *)in;

				// TODO: more efficient lookup
				for (unsigned int i = 0; i < pollfds.size(); i++) {
					if (pollfds[i].fd == pa->fd) {
						// move last element over deleted element
						memmove(&pollfds[i], &pollfds.back(), sizeof(pollfds[i]));
						pollfds.pop_back();
						break;
					}
				}

				//LOG("LWS_CALLBACK_DEL_POLL_FD fd: %d events: 0x%x wsi: %p\n", pa->fd, pa->events, wsi);

			} break;

			case LWS_CALLBACK_CHANGE_MODE_POLL_FD: {
				struct lws_pollargs *pa = (struct lws_pollargs *)in;
				// TODO: more efficient lookup
				for (unsigned int i = 0; i < pollfds.size(); i++) {
					if (pollfds[i].fd == pa->fd) {
						pollfds[i].events = pa->events;
						break;
					}
				}
				//LOG("LWS_CALLBACK_CHANGE_MODE_POLL_FD fd: %d events: 0x%x wsi: %p\n", pa->fd, pa->events, wsi);

			} break;

			case LWS_CALLBACK_UNLOCK_POLL: {
				// poll fds were jsut modified, need to interrupt any poll in process so we pick up the changes.
				poll_interrupt();
			} break;

			default:
				break;
		}
		return !delegate ? 0 : delegate(wsi,reason,user,in,len);
	}

private:
	lws_context* context;
	lws_protocols* protocols;
	lws_callback_function* delegate;
	bool attached;
	std::vector<struct lws_pollfd> pollfds;
	std::recursive_mutex lwsMutex;

	void restoreCallbacks() {
		for (lws_protocols* p = protocols; p && p->name; ++p) {
			if (p->callback == InterceptCallback)
				p->callback = delegate;
		}
	}

	static void OnPreSelect(LibWebSocket_Module* self, fd_set* readset, fd_set* writeset, fd_set* errorset, int* blocktime ) {
		std::lock_guard<std::recursive_mutex> lock(self->lwsMutex);
		if (self->context == NULL)
			return;

		*blocktime = std::min(*blocktime, 1000);
		if (!lws_service_adjust_timeout(self->context, *blocktime, 0))
			*blocktime = 0;

		// Add all the websocket FDs.

		for (const auto &pollfd : self->pollfds) {
			if (pollfd.events & LWS_POLLIN) {
				FD_SET(pollfd.fd, readset);
			}
			if (pollfd.events & LWS_POLLOUT) {
				FD_SET(pollfd.fd, writeset);
			}
			FD_SET(pollfd.fd, errorset);
		}
	}

	static void OnPostSelect(LibWebSocket_Module* self, int slct, fd_set* readset, fd_set* writeset, fd_set* errorset) {
		std::lock_guard<std::recursive_mutex> lock(self->lwsMutex);
		if (self->context == NULL)
			return;

		std::vector<struct lws_pollfd> ready;
		if (slct > 0) {
			for (const auto &pollfd : self->pollfds) {
				if (FD_ISSET(pollfd.fd, readset) || FD_ISSET(pollfd.fd, writeset) || FD_ISSET(pollfd.fd, errorset)) {
					struct lws_pollfd copy = pollfd;
					copy.revents = (FD_ISSET(copy.fd, readset) ? LWS_POLLIN : 0)
						| (FD_ISSET(copy.fd, writeset) ? LWS_POLLOUT : 0)
						| (FD_ISSET(copy.fd, errorset) ? LWS_POLLHUP : 0);
					ready.push_back(copy);
				}
			}
		}

		int retval;
		for (auto &pollfd : ready) {
			retval = lws_service_fd(self->context, &pollfd);
			if (retval < 0) {
				LOG("error in lws_service_fd: %d\n", retval);
			} else if (pollfd.revents != 0) {
				LOG("error: lws_service_fd thinks it's not our socket\n");
			}
		}

		if (self->context != NULL)
			lws_service_tsi(self->context, -1, 0);
	}

	static void OnPreDestroy( LibWebSocket_Module* self ) {
		self->~LibWebSocket_Module();
	}

	static int InterceptCallback(struct lws *wsi
							 , enum lws_callback_reasons reason
							 , void *user, void *in, size_t len){
		if (poll_chain() == NULL)
			return detachedDelegate ? detachedDelegate(wsi, reason, user, in, len) : 0;
		struct lws_context *context = lws_get_context(wsi);
		LibWebSocket_Module* module = (LibWebSocket_Module*)lws_context_user( context );
		if (module == NULL)
			return 0;

		return module->callback( context, wsi, reason, user, in, len );
	}
};

static int poll_extension_callback(struct lws_context *context,
										  const struct lws_extension *ext,
										  struct lws *wsi,
										  enum lws_extension_callback_reasons reason,
										  void *user, void *in, size_t len)
{
	if( reason == LWS_EXT_CB_DESTROY ) {
		if (poll_chain() == NULL)
			return 0;
		LibWebSocket_Module* module = (LibWebSocket_Module*)lws_context_user( context );
		if (module != NULL)
			module->makeInert();
	}

	return 0;
}

static lws_extension poll_extension[] = {
	{ "poll", &poll_extension_callback, 0, }
	,{ NULL, NULL, 0, }
};


struct lws_context* lws_create_context_extended( lws_context_creation_info* info ) {
	assert( info->user == NULL );

	void* storage = malloc(sizeof(LibWebSocket_Module));
	if (storage == NULL)
		return NULL;
	LibWebSocket_Module* module = new (storage) LibWebSocket_Module(const_cast<lws_protocols*>(info->protocols));
	info->user = module;
	// hook in our default protocol handler (will delegate to the user provided)
#if !defined(LWS_WITHOUT_EXTENSIONS)
	info->extensions = poll_extension;
#endif

	lws_context* context = lws_create_context(info);
	if (context == NULL) {
		info->user = NULL;
		module->~LibWebSocket_Module();
		free(module);
		return NULL;
	}

	module->setContext(context);
	module->attach();
	return context;
}

void lws_context_destroy_extended(struct lws_context* context) {
	if (context == NULL)
		return;
	LibWebSocket_Module* module = poll_chain() == NULL
		? NULL
		: static_cast<LibWebSocket_Module*>(lws_context_user(context));
	bool destroyModule = module != NULL && module->makeInert();
	lws_context_destroy(context);
	if (destroyModule)
		poll_destroy_module(module);
}


bool parseServerAddress(std::string server_string, std::string &realaddr, std::string &path, int &port, bool &use_ssl) {
	// precondition: port must contain a valid default port number
	// expecting format [<protocol>://]<server>[:<port>][/<path>]

	std::string::size_type protocolPos = server_string.find("://");
	if (protocolPos != std::string::npos) {
		std::string::size_type ptype = server_string.find("wss");
		// protocol must be first thing
		if (ptype == 0 && protocolPos == 3) {
			use_ssl = true;
		} else {
			ptype = server_string.find("ws");
			if (ptype != 0 || protocolPos != 2) {
				// invalid protocol
				return false;
			}
		}
		server_string = server_string.substr(protocolPos + 3);
	}

	std::string::size_type semicolonPos = server_string.find(':');
	std::string::size_type slashPos = server_string.find('/');
	std::string portstring;

	if (semicolonPos == 0 || slashPos == 0) {
		// only port or path supplied
		return false;
	}
	if (semicolonPos != std::string::npos) {
		// address and port
		realaddr = server_string.substr(0, semicolonPos);
	} else if (slashPos != std::string::npos) {
		// address and path
		realaddr = server_string.substr(0, slashPos);
	} else if (server_string != "") {
		// address only
		realaddr = server_string;
	}
	if (slashPos != std::string::npos && semicolonPos != std::string::npos) {
		// port and path
		portstring = server_string.substr(semicolonPos + 1, slashPos - semicolonPos - 1);
		port = atoi(portstring.c_str());
	} else if (semicolonPos != std::string::npos) {
		// port only
		portstring = server_string.substr(semicolonPos + 1);
		port = atoi(portstring.c_str());
	}
	if (port == 0) {
		port = use_ssl ? 443 : 80;
	}
	if (port <= 0 || port > 65535) {
		// invalid port number
		return false;
	}
	if (slashPos != std::string::npos) {
		// path
		path = server_string.substr(slashPos);
	}

	// postcondition: realaddr, path and port contain valid values
	return true;
}

struct lws* lws_client_connect_extended(struct lws_context* context, const char* url, const char* protocol, void* user_data ) {
	std::string realaddr;
	int port = 0;
	std::string path;
	bool use_ssl = false;
	std::string server_string(url);

	if (!parseServerAddress(server_string, realaddr, path, port, use_ssl)) {
		return NULL;
	}
	struct lws_client_connect_info ccinfo = {
		.context = context,
		.address = realaddr.c_str(),
		.port = port,
		.ssl_connection = use_ssl,
		.path = path.c_str(),
		.host = realaddr.c_str(),
		.origin = realaddr.c_str(),
		.protocol = protocol,
		.userdata = user_data,
	};

	// Establish the connection
	LibWebSocket_Module* module = (LibWebSocket_Module*)lws_context_user(context);
	return module != NULL ? module->connect(&ccinfo) : lws_client_connect_via_info(&ccinfo);
}

#endif
