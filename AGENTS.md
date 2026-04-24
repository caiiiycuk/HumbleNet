# Agent Workflow

All reasoning, design decisions, and implementation choices for this project must assume the project targets only Linux and WebAssembly/JavaScript. Do not optimize for, validate against, or preserve compatibility with other native platforms unless the user explicitly asks for it.

After any code changes, always verify that all project targets build successfully.

Also always build `webrtcnet` with:

1. `cd emscripten`
2. `ninja -j32 webrtcnet`

If any build step fails, fix the reported errors before considering the task complete.
