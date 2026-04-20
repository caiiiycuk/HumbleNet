# Agent Workflow

After any code changes, always verify that all project targets build successfully.

Also always build `jsapi` with:

1. `cd emscripten`
2. `ninja -j32 jsapi`

If any build step fails, fix the reported errors before considering the task complete.
