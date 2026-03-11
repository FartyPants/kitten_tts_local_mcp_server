#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <nlohmann/json.hpp>

#include <KittenTTS>

using json = nlohmann::json;

namespace {

constexpr const char* kServerName = "kitten-tts-mcp";
constexpr const char* kServerVersion = "0.1.0";
constexpr const char* kProtocolVersion = "2025-03-26";
constexpr const char* kFixedModelName = "nano";
constexpr const char* kDefaultVoice = "Jasper";
constexpr const char* kDefaultLocale = "en-us";
constexpr float kDefaultSpeed = 1.0f;

std::string TrimAsciiWhitespace(const std::string& value) {
    const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };

    size_t begin = 0;
    while (begin < value.size() && is_space(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && is_space(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::filesystem::path GetExecutableDirectory() {
    wchar_t module_path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, module_path, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return std::filesystem::path(std::wstring(module_path, length)).parent_path();
    }
    return std::filesystem::current_path();
}

std::filesystem::path FindFileNearExecutable(const std::filesystem::path& exe_dir, const std::string& filename) {
    std::error_code ec;
    std::filesystem::path probe = exe_dir;

    for (int depth = 0; depth < 8; ++depth) {
        const std::filesystem::path candidate_same = probe / filename;
        if (std::filesystem::exists(candidate_same, ec)) {
            return candidate_same;
        }

        const std::filesystem::path candidate_models = probe / "models" / filename;
        if (std::filesystem::exists(candidate_models, ec)) {
            return candidate_models;
        }

        const std::filesystem::path parent = probe.parent_path();
        if (parent.empty() || parent == probe) {
            break;
        }
        probe = parent;
    }

    return {};
}

std::string GetEnvironmentString(const char* name, const std::string& fallback) {
    if (name == nullptr || name[0] == '\0') {
        return fallback;
    }

    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return fallback;
    }

    return value;
}

float GetEnvironmentFloat(const char* name, float fallback) {
    const std::string raw = GetEnvironmentString(name, "");
    if (raw.empty()) {
        return fallback;
    }

    try {
        return std::stof(raw);
    } catch (...) {
        return fallback;
    }
}

std::vector<std::string> DefaultVoiceList() {
    return {"Bella", "Bruno", "Hugo", "Jasper", "Kiki", "Leo", "Luna", "Rosie"};
}

std::vector<std::string> LoadVoiceNamesFromJson(const std::filesystem::path& voices_path) {
    std::ifstream input(voices_path, std::ios::binary);
    if (!input) {
        return {};
    }

    json voices_json;
    try {
        input >> voices_json;
    } catch (...) {
        return {};
    }

    if (!voices_json.is_object()) {
        return {};
    }

    std::vector<std::string> voices;
    voices.reserve(voices_json.size());
    for (auto it = voices_json.begin(); it != voices_json.end(); ++it) {
        voices.push_back(it.key());
    }

    std::sort(voices.begin(), voices.end());
    voices.erase(std::unique(voices.begin(), voices.end()), voices.end());
    return voices;
}

void WriteJsonLine(const json& message) {
    std::cout << message.dump() << "\n";
    std::cout.flush();
}

void WriteResult(const json& id, const json& result) {
    WriteJsonLine({
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    });
}

void WriteError(const json& id, int code, const std::string& message, const json& data = nullptr) {
    json error = {
        {"code", code},
        {"message", message}
    };
    if (!data.is_null()) {
        error["data"] = data;
    }

    WriteJsonLine({
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", error}
    });
}

json MakeTextContent(const std::string& text) {
    return json{
        {"type", "text"},
        {"text", text}
    };
}

class KittenRuntime {
public:
    bool Initialize() {
        const std::filesystem::path exe_dir = GetExecutableDirectory();

        default_voice_ = GetEnvironmentString("KITTEN_TTS_MCP_VOICE", kDefaultVoice);
        default_locale_ = GetEnvironmentString("KITTEN_TTS_MCP_LOCALE", kDefaultLocale);
        default_speed_ = GetEnvironmentFloat("KITTEN_TTS_MCP_SPEED", kDefaultSpeed);

        const std::string model_filename = "kitten_tts_nano_v0_8.onnx";
        const std::string voices_filename = "voices_nano.json";
        model_path_ = FindFileNearExecutable(exe_dir, model_filename);
        voices_path_ = FindFileNearExecutable(exe_dir, voices_filename);

        if (model_path_.empty() || voices_path_.empty()) {
            std::ostringstream oss;
            oss << "Could not find the fixed Kitten nano model files.";
            oss << " Expected " << model_filename << " and " << voices_filename << ".";
            last_error_ = oss.str();
            return false;
        }

        voices_ = LoadVoiceNamesFromJson(voices_path_);
        if (voices_.empty()) {
            voices_ = DefaultVoiceList();
        }

        kittentts::TTSSimpleServiceConfig config;
        config.model_name = kFixedModelName;
        config.model_path = model_path_.string();
        config.voices_path = voices_path_.string();
        config.voice = default_voice_;
        config.locale = default_locale_;
        config.speed = default_speed_;

        service_.SetEventCallback([this](const kittentts::TTSSimpleService::Event& event) {
            OnServiceEvent(event);
        });

        if (!service_.Initialize(config)) {
            last_error_ = service_.GetLastError();
            if (last_error_.empty()) {
                last_error_ = "TTSSimpleService initialization failed.";
            }
            return false;
        }

        initialized_ = true;
        return true;
    }

    void Shutdown() {
        if (!initialized_) {
            return;
        }
        service_.Stop();
        service_.Shutdown();
        initialized_ = false;
    }

    bool IsInitialized() const {
        return initialized_;
    }

    const std::string& GetLastError() const {
        return last_error_;
    }

    const std::vector<std::string>& GetVoices() const {
        return voices_;
    }

    bool Speak(
        const std::string& text,
        const std::string& voice,
        const std::string& locale,
        float speed,
        bool blocking,
        std::string& message_out) {
        message_out.clear();

        if (!initialized_) {
            message_out = "TTS runtime is not initialized.";
            return false;
        }

        const std::string trimmed_text = TrimAsciiWhitespace(text);
        if (trimmed_text.empty()) {
            message_out = "Text must not be empty.";
            return false;
        }

        if (speed < 0.5f || speed > 2.0f) {
            message_out = "Speed must be between 0.5 and 2.0.";
            return false;
        }

        const std::string effective_voice = voice.empty() ? default_voice_ : voice;
        const std::string effective_locale = locale.empty() ? default_locale_ : locale;
        if (!voices_.empty() &&
            std::find(voices_.begin(), voices_.end(), effective_voice) == voices_.end()) {
            message_out = "Unknown voice: " + effective_voice;
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            last_error_.clear();
            terminal_state_ = TerminalState::None;
            terminal_message_.clear();
        }

        service_.ClearLastError();
        service_.SetVoice(effective_voice);
        service_.SetLocale(effective_locale);
        service_.SetSpeed(speed);

        if (!service_.PlayTextUtf8(trimmed_text)) {
            message_out = service_.GetLastError();
            if (message_out.empty()) {
                message_out = "Speech request failed.";
            }
            return false;
        }

        if (!blocking) {
            message_out = "Speech started.";
            return true;
        }

        while (service_.IsPlaybackActive()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (terminal_state_ == TerminalState::Error) {
                message_out = terminal_message_.empty() ? "Speech failed." : terminal_message_;
                return false;
            }
            if (terminal_state_ == TerminalState::Stopped) {
                message_out = terminal_message_.empty() ? "Speech was stopped." : terminal_message_;
                return false;
            }
        }

        message_out = "Speech completed.";
        return true;
    }

    bool Stop(std::string& message_out) {
        message_out.clear();

        if (!initialized_) {
            message_out = "TTS runtime is not initialized.";
            return false;
        }

        service_.Stop();
        message_out = "Playback stopped.";
        return true;
    }

private:
    enum class TerminalState {
        None = 0,
        Completed,
        Stopped,
        Error
    };

    void OnServiceEvent(const kittentts::TTSSimpleService::Event& event) {
        std::lock_guard<std::mutex> lock(state_mutex_);

        switch (event.type) {
        case kittentts::TTSSimpleService::EventType::PlaybackCompleted:
            terminal_state_ = TerminalState::Completed;
            terminal_message_ = "Speech completed.";
            break;
        case kittentts::TTSSimpleService::EventType::PlaybackStopped:
            if (terminal_state_ != TerminalState::Error) {
                terminal_state_ = TerminalState::Stopped;
                terminal_message_ = event.message.empty() ? "Speech was stopped." : event.message;
            }
            break;
        case kittentts::TTSSimpleService::EventType::Error:
            terminal_state_ = TerminalState::Error;
            terminal_message_ = event.message.empty() ? "Speech failed." : event.message;
            last_error_ = terminal_message_;
            break;
        default:
            break;
        }
    }

    kittentts::TTSSimpleService service_;
    bool initialized_ = false;
    std::filesystem::path model_path_;
    std::filesystem::path voices_path_;
    std::string default_voice_;
    std::string default_locale_;
    float default_speed_ = kDefaultSpeed;
    std::vector<std::string> voices_;
    mutable std::mutex state_mutex_;
    TerminalState terminal_state_ = TerminalState::None;
    std::string terminal_message_;
    std::string last_error_;
};

KittenRuntime g_runtime;

void HandleInitialize(const json& request) {
    const json id = request.contains("id") ? request["id"] : json(nullptr);
    const json params = request.value("params", json::object());
    const std::string requested_version = params.value("protocolVersion", "");

    json result = {
        {"protocolVersion", kProtocolVersion},
        {"capabilities", {
            {"tools", {
                {"listChanged", false}
            }}
        }},
        {"serverInfo", {
            {"name", kServerName},
            {"version", kServerVersion}
        }},
        {"instructions", "KittenTTS MCP server. Tools: speak, stop_speaking, list_voices."}
    };

    if (!requested_version.empty() && requested_version != kProtocolVersion) {
        result["warnings"] = json::array({
            "Client requested a different protocolVersion; server is responding with 2025-03-26."
        });
    }

    WriteResult(id, result);
}

void HandleToolsList(const json& request) {
    const json id = request.contains("id") ? request["id"] : json(nullptr);

    json tools = json::array();
    tools.push_back({
        {"name", "speak"},
        {"title", "Speak Text"},
        {"description", "Speak text aloud on the local machine using KittenTTS."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"text", {
                    {"type", "string"},
                    {"description", "The text to speak."}
                }},
                {"voice", {
                    {"type", "string"},
                    {"description", "Optional KittenTTS voice name."}
                }},
                {"locale", {
                    {"type", "string"},
                    {"description", "Optional eSpeak locale. Default is en-us."}
                }},
                {"speed", {
                    {"type", "number"},
                    {"description", "Playback speed from 0.5 to 2.0."},
                    {"default", kDefaultSpeed}
                }},
                {"blocking", {
                    {"type", "boolean"},
                    {"description", "If true, wait until playback completes or fails."},
                    {"default", false}
                }}
            }},
            {"required", json::array({"text"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "stop_speaking"},
        {"title", "Stop Speaking"},
        {"description", "Stop current local KittenTTS playback."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "list_voices"},
        {"title", "List Voices"},
        {"description", "List the predefined KittenTTS voices available to this server."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    WriteResult(id, {{"tools", tools}});
}

void HandleToolsCall(const json& request) {
    const json id = request.contains("id") ? request["id"] : json(nullptr);
    const json params = request.value("params", json::object());
    const std::string name = params.value("name", "");
    const json args = params.value("arguments", json::object());

    if (name == "speak") {
        if (!args.contains("text") || !args["text"].is_string()) {
            WriteError(id, -32602, "Invalid arguments: 'text' must be a string");
            return;
        }

        const std::string text = args["text"].get<std::string>();
        const std::string voice = args.value("voice", "");
        const std::string locale = args.value("locale", "");
        const float speed = args.contains("speed") ? args["speed"].get<float>() : kDefaultSpeed;
        const bool blocking = args.value("blocking", false);

        std::string runtime_message;
        const bool ok = g_runtime.Speak(text, voice, locale, speed, blocking, runtime_message);

        const std::string effective_voice = voice.empty() ? kDefaultVoice : voice;
        const std::string effective_locale = locale.empty() ? kDefaultLocale : locale;
        const std::string content_text = ok ? runtime_message : ("TTS error: " + runtime_message);

        WriteResult(id, {
            {"content", json::array({MakeTextContent(content_text)})},
            {"structuredContent", {
                {"ok", ok},
                {"voice", effective_voice},
                {"locale", effective_locale},
                {"speed", speed},
                {"blocking", blocking},
                {"message", runtime_message}
            }},
            {"isError", !ok}
        });
        return;
    }

    if (name == "stop_speaking") {
        std::string runtime_message;
        const bool ok = g_runtime.Stop(runtime_message);

        WriteResult(id, {
            {"content", json::array({MakeTextContent(runtime_message)})},
            {"structuredContent", {
                {"ok", ok},
                {"message", runtime_message}
            }},
            {"isError", !ok}
        });
        return;
    }

    if (name == "list_voices") {
        const auto& voices = g_runtime.GetVoices();
        std::ostringstream oss;
        oss << "Available voices:";
        for (const auto& voice_name : voices) {
            oss << " " << voice_name;
        }

        WriteResult(id, {
            {"content", json::array({MakeTextContent(oss.str())})},
            {"structuredContent", {
                {"voices", voices}
            }},
            {"isError", false}
        });
        return;
    }

    WriteError(id, -32601, "Unknown tool: " + name);
}

} // namespace

int main() {
    std::ios::sync_with_stdio(false);

    std::cerr << "[mcp] " << kServerName << " starting\n";
    if (!g_runtime.Initialize()) {
        std::cerr << "[mcp] failed to initialize KittenTTS runtime: " << g_runtime.GetLastError() << "\n";
        return 1;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        try {
            const json request = json::parse(line);

            if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
                if (request.contains("id")) {
                    WriteError(request["id"], -32600, "Invalid Request");
                }
                continue;
            }

            const std::string method = request.value("method", "");
            const bool is_notification = !request.contains("id");

            if (method == "initialize") {
                if (!is_notification) {
                    HandleInitialize(request);
                }
            } else if (method == "notifications/initialized") {
                std::cerr << "[mcp] client initialized\n";
            } else if (method == "tools/list") {
                if (!is_notification) {
                    HandleToolsList(request);
                }
            } else if (method == "tools/call") {
                if (!is_notification) {
                    HandleToolsCall(request);
                }
            } else if (method == "ping") {
                if (!is_notification) {
                    WriteResult(request["id"], json::object());
                }
            } else {
                if (!is_notification) {
                    WriteError(request["id"], -32601, "Method not found: " + method);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[mcp] parse/dispatch error: " << e.what() << "\n";
        }
    }

    std::cerr << "[mcp] stdin closed, exiting\n";
    g_runtime.Shutdown();
    return 0;
}
