#ifndef LIBWEBSOCKETS_NATIVE_H
#define LIBWEBSOCKETS_NATIVE_H

#include <libwebsockets.h>

struct lws_context* lws_create_context_extended( struct lws_context_creation_info* info );
struct lws* lws_client_connect_extended(struct lws_context* context, const char* url, const char* protocol, void* user_data ); 

#endif // LIBWEBSOCKETS_NATIVE_H
