//
// Created by caiiiycuk on 25.07.25.
//
#include <cstdio>
#include <emscripten.h>

#include "humblenet.h"
#include "humblenet_p2p.h"

extern "C" bool EMSCRIPTEN_KEEPALIVE connectTo(const char* server, const char* token, const char* secret) {
    if (!humblenet_init()) {
        printf("ERR! Can't initialize humblenet\n");
        return false;
    }

    if (!humblenet_p2p_init(server, token, secret, NULL)) {
        printf("ERR! Can't initialize humblenet\n");
        return false;
    }

    if (!humblenet_p2p_is_initialized()) {
        printf("ERR! Humble net should be in initialized state\n");
        return false;
    }

    return true;
}

extern "C" void EMSCRIPTEN_KEEPALIVE wait(int ms) {
    humblenet_p2p_wait(ms);
}

extern "C" uint32_t EMSCRIPTEN_KEEPALIVE myId() {
    return humblenet_p2p_get_my_peer_id();
}

extern "C" void EMSCRIPTEN_KEEPALIVE registerAlias(const char* alias) {
    humblenet_p2p_register_alias(alias);
}

EM_JS(void, aliasQueryAdd, (const char* alias, uint32_t peer), {
    Module.aliasQueryAdd(UTF8ToString(alias), peer);
});

EM_JS(void, aliasQueryEnd, (), {
    Module.onAliasQueryEnd();
});

extern "C" void EMSCRIPTEN_KEEPALIVE queryAliases(const char* query) {
    humblenet_p2p_alias_query(query, [](std::vector<std::pair<std::string, PeerId>> matches) {
        for (const auto& match : matches) {
            aliasQueryAdd(match.first.c_str(), match.second);
        }
        aliasQueryEnd();
    });
}