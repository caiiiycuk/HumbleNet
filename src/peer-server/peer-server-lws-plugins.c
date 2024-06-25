#include <libwebsockets.h>
#define LWS_PLUGIN_STATIC
#include "../../3rdparty/libwebsockets/plugins/acme-client/protocol_lws_acme_client.c"

struct lws_protocols acme_plugin_protocol = LWS_PLUGIN_PROTOCOL_LWS_ACME_CLIENT;