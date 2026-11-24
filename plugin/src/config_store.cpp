#include "config_store.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace ets2asl {
namespace {

std::string trim(std::string_view sv) {
    size_t start = 0;
    while (start < sv.size() && std::isspace(static_cast<unsigned char>(sv[start]))) {
        ++start;
    }
    size_t end = sv.size();
    while (end > start && std::isspace(static_cast<unsigned char>(sv[end - 1]))) {
        --end;
    }
    return std::string(sv.substr(start, end - start));
}

std::string to_lower(std::string_view sv) {
    std::string out(sv);
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

std::string to_upper(std::string_view sv) {
    std::string out(sv);
    for (char& ch : out) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return out;
}

std::vector<std::string> split(std::string_view sv, char delim) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= sv.size()) {
        const size_t pos = sv.find(delim, start);
        if (pos == std::string_view::npos) {
            parts.emplace_back(sv.substr(start));
            break;
        }
        parts.emplace_back(sv.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

bool parse_u32(std::string_view sv, uint32_t& out) {
    if (sv.empty()) {
        return false;
    }
    uint64_t val = 0;
    for (char ch : sv) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
        val = val * 10 + static_cast<uint64_t>(ch - '0');
        if (val > 0xFFFFFFFFull) {
            return false;
        }
    }
    out = static_cast<uint32_t>(val);
    return true;
}

bool parse_float(std::string_view sv, float& out) {
    try {
        std::string s(sv);
        size_t idx = 0;
        const float val = std::stof(s, &idx);
        if (idx != s.size() || !std::isfinite(val)) {
            return false;
        }
        out = val;
        return true;
    } catch (...) {
        return false;
    }
}

bool is_valid_xinput_button(std::string_view btn) {
    static const char* const kButtons[] = {
        "A", "B", "X", "Y", "LB", "RB", "BACK", "START", "LS", "RS",
        "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT", "LT", "RT",
    };
    for (const char* valid : kButtons) {
        if (btn == valid) {
            return true;
        }
    }
    return false;
}

bool is_valid_guid(std::string_view guid) {
    if (guid.size() != 36) {
        return false;
    }
    for (size_t i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (guid[i] != '-') {
                return false;
            }
        } else if (!std::isxdigit(static_cast<unsigned char>(guid[i]))) {
            return false;
        }
    }
    return true;
}

bool parse_pov_dir(std::string_view sv, PovDirection& out) {
    const std::string up = to_upper(sv);
    if (up == "UP") {
        out = PovDirection::Up;
        return true;
    }
    if (up == "RIGHT") {
        out = PovDirection::Right;
        return true;
    }
    if (up == "DOWN") {
        out = PovDirection::Down;
        return true;
    }
    if (up == "LEFT") {
        out = PovDirection::Left;
        return true;
    }
    return false;
}

const char* pov_dir_name(PovDirection dir) {
    switch (dir) {
        case PovDirection::Up:
            return "UP";
        case PovDirection::Right:
            return "RIGHT";
        case PovDirection::Down:
            return "DOWN";
        case PovDirection::Left:
            return "LEFT";
    }
    return "UP";
}

bool parse_bool(std::string_view sv, bool& out) {
    const std::string low = to_lower(sv);
    if (low == "true" || low == "1" || low == "yes" || low == "on") {
        out = true;
        return true;
    }
    if (low == "false" || low == "0" || low == "no" || low == "off") {
        out = false;
        return true;
    }
    return false;
}

}  // namespace

BindingSpec parse_binding_token(std::string_view token) {
    BindingSpec spec;
    const std::string trimmed = trim(token);
    if (trimmed.empty()) {
        return spec;
    }

    const auto parts = split(trimmed, ':');
    if (parts.empty()) {
        return spec;
    }

    const std::string kind = to_lower(parts[0]);
    if (kind == "kb" && parts.size() == 2) {
        uint32_t vk = 0;
        if (parse_u32(parts[1], vk) && vk >= 1 && vk <= 254) {
            spec.source = BindingSource::Keyboard;
            spec.vk_code = vk;
        }
        return spec;
    }

    if (kind == "xinput" && parts.size() == 3) {
        uint32_t slot = 0;
        const std::string btn = to_upper(parts[2]);
        if (parse_u32(parts[1], slot) && slot <= 3 && is_valid_xinput_button(btn)) {
            spec.source = BindingSource::XInput;
            spec.xinput_slot = static_cast<uint8_t>(slot);
            spec.xinput_button = btn;
        }
        return spec;
    }

    if (kind == "dinput" && parts.size() >= 4) {
        const std::string guid = to_upper(parts[1]);
        const std::string sub = to_lower(parts[2]);
        if (!is_valid_guid(guid)) {
            return spec;
        }
        if (sub == "btn" && parts.size() == 4) {
            uint32_t idx = 0;
            if (parse_u32(parts[3], idx) && idx < 128) {
                spec.source = BindingSource::DirectInputButton;
                spec.device_guid = guid;
                spec.index = idx;
            }
            return spec;
        }
        if (sub == "pov" && parts.size() == 5) {
            uint32_t idx = 0;
            PovDirection dir = PovDirection::Up;
            if (parse_u32(parts[3], idx) && idx < 4 && parse_pov_dir(parts[4], dir)) {
                spec.source = BindingSource::DirectInputPov;
                spec.device_guid = guid;
                spec.index = idx;
                spec.pov_dir = dir;
            }
            return spec;
        }
    }

    return spec;
}

std::string format_binding_token(const BindingSpec& spec) {
    switch (spec.source) {
        case BindingSource::None:
            return "";
        case BindingSource::Keyboard:
            return "kb:" + std::to_string(spec.vk_code);
        case BindingSource::XInput:
            return "xinput:" + std::to_string(spec.xinput_slot) + ":" + to_upper(spec.xinput_button);
        case BindingSource::DirectInputButton:
            return "dinput:" + to_upper(spec.device_guid) + ":btn:" + std::to_string(spec.index);
        case BindingSource::DirectInputPov:
            return "dinput:" + to_upper(spec.device_guid) + ":pov:" + std::to_string(spec.index) + ":" +
                   pov_dir_name(spec.pov_dir);
    }
    return "";
}

PluginConfig parse_ini_text(std::string_view text) {
    PluginConfig cfg;

    // Strip UTF-8 BOM if present
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }

    std::string section;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string_view line_sv = (nl == std::string_view::npos)
                                       ? text.substr(pos)
                                       : text.substr(pos, nl - pos);
        if (!line_sv.empty() && line_sv.back() == '\r') {
            line_sv.remove_suffix(1);
        }

        const std::string line = trim(line_sv);
        if (!line.empty() && line[0] != ';' && line[0] != '#') {
            if (line.front() == '[' && line.back() == ']') {
                section = to_lower(trim(line.substr(1, line.size() - 2)));
            } else {
                const size_t eq = line.find('=');
                if (eq != std::string::npos) {
                    const std::string key = to_lower(trim(line.substr(0, eq)));
                    const std::string val = trim(line.substr(eq + 1));

                    if (section == "thresholds") {
                        if (key == "brake_threshold") {
                            float f = 0.0f;
                            if (parse_float(val, f)) {
                                cfg.brake_threshold = std::clamp(f, 0.01f, 1.0f);
                            }
                        } else if (key == "stop_speed_kmh") {
                            float f = 0.0f;
                            if (parse_float(val, f)) {
                                cfg.stop_speed_kmh = std::clamp(f, 0.0f, 10.0f);
                            }
                        } else if (key == "brake_source") {
                            const std::string low = to_lower(val);
                            if (low == "effective") {
                                cfg.brake_source = BrakeSource::Effective;
                            } else if (low == "input") {
                                cfg.brake_source = BrakeSource::Input;
                            }
                        }
                    } else if (section == "audio") {
                        if (key == "enabled") {
                            bool b = true;
                            if (parse_bool(val, b)) {
                                cfg.audio_enabled = b;
                            }
                        } else if (key == "sound_file") {
                            if (!val.empty()) {
                                cfg.audio_sound_file = val;
                            }
                        } else if (key == "cooldown_ms") {
                            uint32_t u = 0;
                            if (parse_u32(val, u)) {
                                cfg.audio_cooldown_ms = std::clamp(u, 0u, 5000u);
                            }
                        }
                    } else if (section == "timing") {
                        if (key == "pulse_ms") {
                            uint32_t u = 0;
                            if (parse_u32(val, u)) {
                                cfg.pulse_ms = std::clamp(u, 20u, 500u);
                            }
                        } else if (key == "pulse_gap_ms") {
                            uint32_t u = 0;
                            if (parse_u32(val, u)) {
                                cfg.pulse_gap_ms = std::clamp(u, 20u, 500u);
                            }
                        }
                    } else if (section == "bindings") {
                        if (key == "shift_up") {
                            cfg.bindings[static_cast<size_t>(ShiftAction::ShiftUp)] = parse_binding_token(val);
                        } else if (key == "shift_down") {
                            cfg.bindings[static_cast<size_t>(ShiftAction::ShiftDown)] = parse_binding_token(val);
                        } else if (key == "gear_d") {
                            cfg.bindings[static_cast<size_t>(ShiftAction::GearD)] = parse_binding_token(val);
                        } else if (key == "gear_n") {
                            cfg.bindings[static_cast<size_t>(ShiftAction::GearN)] = parse_binding_token(val);
                        } else if (key == "gear_r") {
                            cfg.bindings[static_cast<size_t>(ShiftAction::GearR)] = parse_binding_token(val);
                        }
                    }
                }
            }
        }

        if (nl == std::string_view::npos) {
            break;
        }
        pos = nl + 1;
    }

    return cfg;
}

std::string serialize_ini_text(const PluginConfig& cfg) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(2);
    oss << "[thresholds]\n";
    oss << "brake_threshold=" << std::clamp(cfg.brake_threshold, 0.01f, 1.0f) << "\n";
    oss << "stop_speed_kmh=" << std::clamp(cfg.stop_speed_kmh, 0.0f, 10.0f) << "\n";
    oss << "brake_source=" << (cfg.brake_source == BrakeSource::Effective ? "effective" : "input") << "\n\n";

    oss << "[audio]\n";
    oss << "enabled=" << (cfg.audio_enabled ? "true" : "false") << "\n";
    oss << "sound_file=" << cfg.audio_sound_file << "\n";
    oss << "cooldown_ms=" << std::clamp(cfg.audio_cooldown_ms, 0u, 5000u) << "\n\n";

    oss << "[timing]\n";
    oss << "pulse_ms=" << std::clamp(cfg.pulse_ms, 20u, 500u) << "\n";
    oss << "pulse_gap_ms=" << std::clamp(cfg.pulse_gap_ms, 20u, 500u) << "\n\n";

    oss << "[bindings]\n";
    oss << "shift_up=" << format_binding_token(cfg.bindings[static_cast<size_t>(ShiftAction::ShiftUp)]) << "\n";
    oss << "shift_down=" << format_binding_token(cfg.bindings[static_cast<size_t>(ShiftAction::ShiftDown)]) << "\n";
    oss << "gear_d=" << format_binding_token(cfg.bindings[static_cast<size_t>(ShiftAction::GearD)]) << "\n";
    oss << "gear_n=" << format_binding_token(cfg.bindings[static_cast<size_t>(ShiftAction::GearN)]) << "\n";
    oss << "gear_r=" << format_binding_token(cfg.bindings[static_cast<size_t>(ShiftAction::GearR)]) << "\n";
    return oss.str();
}

ConfigWatcher::ConfigWatcher(std::filesystem::path ini_path) : ini_path_(std::move(ini_path)) {}

bool ConfigWatcher::reload_if_changed(PluginConfig& out_cfg) {
    std::error_code ec;
    if (!std::filesystem::exists(ini_path_, ec) || ec) {
        return false;
    }

    const auto mtime = std::filesystem::last_write_time(ini_path_, ec);
    if (ec) {
        return false;
    }

    if (has_loaded_ && mtime == last_mtime_) {
        return false;
    }

    std::ifstream ifs(ini_path_, std::ios::binary);
    if (!ifs.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << ifs.rdbuf();
    out_cfg = parse_ini_text(buffer.str());
    last_mtime_ = mtime;
    has_loaded_ = true;
    return true;
}

}  // namespace ets2asl
