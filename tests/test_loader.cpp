#include "humblenet.h"

#include <iostream>

#define debug_out(x) { std::cout << x << std::endl; }

int main(int argc, char* argv[])
{
	debug_out("Initializing WebRTC-NET");
	if (!humblenet_init()) {
		debug_out("WebRTC-NET init failed : " << humblenet_get_error());
		return 1;
	}
	const char* err = humblenet_get_error();
	debug_out("WebRTC-NET Init Success");
	humblenet_shutdown();
	debug_out("WebRTC-NET shutdown");

	debug_out("Initializing WebRTC-NET (Round 2)");
	if (!humblenet_init()) {
		debug_out("WebRTC-NET init failed : " << humblenet_get_error());
		return 1;
	}
	err = humblenet_get_error();
	debug_out("WebRTC-NET Init Success");
	humblenet_shutdown();
	debug_out("WebRTC-NET shutdown");

	return 0;
}
