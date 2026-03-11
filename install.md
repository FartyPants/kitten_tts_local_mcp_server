# KittenTTS MCP Server Install Guide

This guide shows how to register the local KittenTTS MCP server with Codex.

## 1. Build the server

kitten_tts_mcp.exe`

## 2. Confirm the runtime files exist

The MCP server needs these files in the output folder or a nearby `models` folder:

- `kitten_tts_nano_v0_8.onnx`
- `voices_nano.json`
- `onnxruntime.dll`
- `onnxruntime_providers_shared.dll`
- `libespeak-ng.dll`
- `espeak-ng-data\...`

The project post-build step already copies these into:

## 3. Add the server to Codex

Run this command in a terminal:

```powershell
codex mcp add kitten-tts -- "C:\<an actual path to>\kitten_tts_mcp.exe"
```

That registers the server as `kitten-tts`.

## 4. Alternative: add it manually in config

If you prefer editing config directly, add this to your Codex config file:

```toml
[mcp_servers.kitten-tts]
command = "C:\<an actual path to>\kitten_tts_mcp.exe"
```

Typical config location on Windows:

`C:\Users\<YourUser>\.codex\config.toml`

## 5. Restart Codex

After adding the server, restart Codex so it reloads MCP configuration.

## 6. Verify it works

Once Codex reconnects, the server should expose these tools:

- `speak`
- `stop_speaking`
- `list_voices`

The server is fixed to the Kitten `nano` model:

- model: `kitten_tts_nano_v0_8.onnx`
- voices: `voices_nano.json`

## 7. Optional environment variables

These are optional. The model itself is not configurable in this build.

```powershell
$env:KITTEN_TTS_MCP_VOICE="Jasper"
$env:KITTEN_TTS_MCP_LOCALE="en-us"
$env:KITTEN_TTS_MCP_SPEED="1.0"
```

If you want Codex to launch the server with those values, wrap the server in a small `.cmd` or PowerShell launcher and register that launcher instead.

## Notes

- This server uses stdio MCP transport, so nothing else should write protocol data to stdout.
- Engine debug logs go to stderr, which is safe for MCP.
- This build uses only the Kitten `nano` v0.8 model.

## To try:

Use the kitten-tts MCP server to list available voices, then speak "This is a live MCP test from Codex." using Jasper.


## commands 
 When communicating with me, decide whether to use voice. If you use voice, keep it under about 12 words and use it only for quick
  updates like "Running tests now", "Build failed", or "Patch applied". Put everything substantial in text, including reasoning, diffs,
  commands, stack traces, and code.


 Always begin each substantive response with a very short spoken preface through the kitten-tts MCP server. Keep the spoken part to one
  sentence, ideally 4 to 10 words, summarizing the immediate action or outcome, such as "Checking the codebase now" or "Patch applied
  successfully." After that, put the full response in text. Never speak long explanations, source code, diffs, stack traces, logs, or
  detailed instructions aloud. Spoken output is for quick orientation only; the screen is the source of full detail.


 Act like a friendly AI agent and always begin substantive replies with a short spoken preface using the kitten-tts MCP server. The
  spoken preface should feel warm, natural, and concise, usually one sentence and under 10 words, such as "I’m checking that now" or "I’ve
  finished the update." After the spoken preface, provide the full details in text. Keep voice output limited to brief status updates,
  confirmations, blockers, or handoffs. Never speak long explanations, code, diffs, logs, stack traces, or detailed instructions aloud.
  The voice is for presence and quick orientation; the screen is for the full answer.


 Be a calm, companion-like AI agent: warm, present, and easy to work with, without sounding theatrical, clingy, or overly cute. Always
  begin substantive replies with a short spoken preface using the kitten-tts MCP server. Keep the spoken part brief, natural, and
  understated, usually one sentence and under 10 words, such as "I’m on it," "I checked that," or "Here’s what I found." After that, put
  the full response in text. Use speech to create presence and smooth handoffs, not to deliver detail. Never read long explanations, code,
  diffs, logs, stack traces, or detailed instructions aloud. Keep the tone friendly, steady, and human, with the screen remaining the
  place for anything substantial.


 Put it near the top of AGENTS.md, in the behavior/instructions section, not in project-specific build notes.

  Best placement:

  1. Right after the main role/personality section.
  2. Before coding rules, build steps, or repo conventions.
  3. Under a short heading like ## Voice Interaction or ## Spoken Responses.


 ## Voice Interaction
  Be a calm, companion-like AI agent: warm, present, and easy to work with, without sounding theatrical, clingy, or overly cute. Always
  begin substantive replies with a short spoken preface using the kitten-tts MCP server. Keep the spoken part brief, natural, and
  understated, usually one sentence and under 10 words, such as "I'm on it," "I checked that," or "Here's what I found." After that, put
  the full response in text. Use speech to create presence and smooth handoffs, not to deliver detail. Never read long explanations, code,
  diffs, logs, stack traces, or detailed instructions aloud. Keep the tone friendly, steady, and human, with the screen remaining the
  place for anything substantial.
