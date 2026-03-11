# KittenTTS MCP Server (Windows)

This project packages the **KittenTTS nano v0.8** model as a local **Model Context Protocol (MCP)** server for Codex-compatible clients on Windows.

It is not a general training repository and it is not a hosted inference endpoint. It is a small native C++ server that runs over **stdio**, loads a fixed local ONNX model, and exposes a simple tool interface for text-to-speech:

- `speak`
- `stop_speaking`
- `list_voices`

The current build is fixed to:

- Model: `kitten_tts_nano_v0_8.onnx`
- Voice metadata: `voices_nano.json`
- Default voice: `Jasper`
- Default locale: `en-us`
- Default speed: `1.0`

## What This Repository Is For

Use this project when you want a local TTS backend that an MCP client can call as a tool.

Typical use cases:

- adding spoken responses to a coding agent
- local desktop TTS without a network service
- integrating KittenTTS into Codex through MCP
- exposing a small, predictable voice tool surface to another application

This server communicates with the client using JSON-RPC 2.0 over standard input/output and keeps logs on `stderr`, which is the expected pattern for local MCP servers.

## Model And Runtime

This MCP server wraps the **KittenTTS nano** model and initializes a local `TTS Service` runtime that:

- finds the ONNX model near the executable or in a nearby `models/` folder
- loads voice names from `voices_nano.json`
- validates voice, speed, and text input
- plays audio locally on the host machine

The server currently exposes the following behavior:

- speaking text with optional `voice`, `locale`, `speed`, and `blocking`
- stopping active playback
- listing the available predefined voices

Supported speed range in this build:

- `0.5` to `2.0`

If voice metadata cannot be loaded, the server falls back to these built-in voices:

- Bella
- Bruno
- Hugo
- Jasper
- Kiki
- Leo
- Luna
- Rosie

## Intended Platform

This repository is designed for **local Windows use** and is built as a native Visual Studio C++ executable.

It depends on local runtime files being available next to the executable or in a nearby model directory, including:

- `kitten_tts_nano_v0_8.onnx`
- `voices_nano.json`
- `onnxruntime.dll`
- `onnxruntime_providers_shared.dll`
- `libespeak-ng.dll`
- `espeak-ng-data/...`

## MCP Tool Interface

### `speak`

Speaks text aloud on the local machine.

Inputs:

- `text` (`string`, required)
- `voice` (`string`, optional)
- `locale` (`string`, optional)
- `speed` (`number`, optional)
- `blocking` (`boolean`, optional)

### `stop_speaking`

Stops current playback.

### `list_voices`

Returns the predefined voices available to this server.

## Example Codex Registration

```powershell
codex mcp add kitten-tts -- "C:\path\to\kitten_tts_mcp.exe"
```

Or in `config.toml`:

```toml
[mcp_servers.kitten-tts]
command = "C:\path\to\kitten_tts_mcp.exe"
```

## Example Usage

Once registered, an MCP client can call the server tools to:

- list installed voices
- speak short status updates
- stop playback when interrupted

Example `speak` payload:

```json
{
  "text": "This is a live MCP test from Codex.",
  "voice": "Jasper",
  "locale": "en-us",
  "speed": 1.0,
  "blocking": false
}
```

## Limitations

- This build is fixed to the **nano** model only.
- The server is intended for **local** use, not hosted inference.
- Audio playback happens on the machine running the executable.
- This repository does not include training code or evaluation benchmarks.
- Quality, pronunciation, and language coverage depend on the underlying KittenTTS model and local eSpeak-based preprocessing/runtime.

## Training Data

This repository does not train a model. It packages and serves an existing KittenTTS model for local inference.

For dataset details, original training procedure, and model-development context, refer to the upstream KittenTTS project.

## License

This project is distributed under the **Apache 2.0** license in this repository.

Third-party components and model/runtime dependencies may carry their own licenses and attribution requirements.

## Credits

- KittenTTS for the underlying text-to-speech model
- ONNX Runtime for local inference
- eSpeak NG for locale and phoneme support
- nlohmann/json for JSON handling
- FPHAM for gluing it together

# Friendly Companion Skill

The `friendly-companion` skill adds a calm, human-feeling interaction layer on top of normal assistant work.

Its purpose is not to change the substance of the response. Its purpose is to change the delivery:

- brief speech creates presence
- text carries the real content

## What It Does

This skill instructs the assistant to behave like a steady, low-key companion while staying precise and useful.

It uses short spoken lines through the `kitten-tts` MCP server when that helps the interaction feel more natural, then keeps all meaningful details in text.

Typical spoken use:

- greetings
- quick confirmations
- short status updates
- brief completion notices
- gentle transitions
- short questions

## Core Rule

Speech is the social layer. Text is the information layer.

The assistant should never put substantial content in speech. Explanations, plans, code, diffs, logs, and detailed instructions stay on screen in written form.

## Tone

The intended tone is:

- calm
- warm
- direct
- companion-like
- emotionally restrained

The skill avoids:

- hype
- forced enthusiasm
- clingy or theatrical language
- romantic framing
- assistant-speak
- therapist-speak

## Voice Behavior

Default voice guidance:

- `Rosie` as the default voice
- `Luna` as a more businesslike alternative
- `Jasper` as a male alternative

If a requested voice is unavailable, the assistant should check available voices and fall back to `Luna`.

Spoken output should generally be:

- one or two sentences
- under about 20 words
- natural to hear aloud
- short enough to act as a handoff, not a full answer

## Interaction Pattern

The default pattern is:

1. say one short spoken line when useful
2. provide the full substantive written reply
3. avoid repeated spoken follow-ups unless there is a clear conversational reason

## Best Use Cases

This skill works well for:

- companion-style agent interactions
- voice-enabled coding workflows
- spoken confirmations before a detailed text reply
- interfaces where presence matters but precision still belongs in text

## Files

- `SKILL.md`: full skill instructions
- `agents/openai.yaml`: short display metadata for agent integration

## Summary

`friendly-companion` is a delivery skill for a future-facing assistant style: present in voice, clear on screen, useful in substance, and warm without overperforming.
