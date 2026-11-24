using System.Globalization;
using System.Text;

namespace AutoShiftLockConfig.Models;

public sealed class PluginConfig
{
    public float BrakeThreshold { get; set; } = 0.25f;
    public float StopSpeedKmh { get; set; } = 0.5f;
    public BrakeSource BrakeSource { get; set; } = BrakeSource.Input;

    public bool AudioEnabled { get; set; } = true;
    public string AudioSoundFile { get; set; } = "lock_warning.wav";
    public int AudioCooldownMs { get; set; } = 350;

    public int PulseMs { get; set; } = 60;
    public int PulseGapMs { get; set; } = 80;

    public BindingSpec ShiftUp { get; set; } = BindingSpec.None;
    public BindingSpec ShiftDown { get; set; } = BindingSpec.None;
    public BindingSpec GearD { get; set; } = BindingSpec.None;
    public BindingSpec GearN { get; set; } = BindingSpec.None;
    public BindingSpec GearR { get; set; } = BindingSpec.None;

    public static PluginConfig Parse(string text)
    {
        var cfg = new PluginConfig();
        if (string.IsNullOrEmpty(text))
        {
            return cfg;
        }

        if (text[0] == '\uFEFF')
        {
            text = text[1..];
        }

        var section = string.Empty;
        using var reader = new StringReader(text);
        string? rawLine;
        while ((rawLine = reader.ReadLine()) is not null)
        {
            var line = rawLine.Trim();
            if (line.Length == 0 || line[0] == ';' || line[0] == '#')
            {
                continue;
            }

            if (line[0] == '[' && line[^1] == ']')
            {
                section = line[1..^1].Trim().ToLowerInvariant();
                continue;
            }

            var eq = line.IndexOf('=');
            if (eq < 0)
            {
                continue;
            }

            var key = line[..eq].Trim().ToLowerInvariant();
            var val = line[(eq + 1)..].Trim();

            switch (section)
            {
                case "thresholds":
                    if (key == "brake_threshold" &&
                        float.TryParse(val, NumberStyles.Float, CultureInfo.InvariantCulture, out var brake) &&
                        float.IsFinite(brake))
                    {
                        cfg.BrakeThreshold = Math.Clamp(brake, 0.01f, 1.0f);
                    }
                    else if (key == "stop_speed_kmh" &&
                             float.TryParse(val, NumberStyles.Float, CultureInfo.InvariantCulture, out var speed) &&
                             float.IsFinite(speed))
                    {
                        cfg.StopSpeedKmh = Math.Clamp(speed, 0.0f, 10.0f);
                    }
                    else if (key == "brake_source")
                    {
                        var low = val.ToLowerInvariant();
                        if (low == "effective") cfg.BrakeSource = BrakeSource.Effective;
                        else if (low == "input") cfg.BrakeSource = BrakeSource.Input;
                    }
                    break;

                case "audio":
                    if (key == "enabled" && TryParseBool(val, out var enabled))
                    {
                        cfg.AudioEnabled = enabled;
                    }
                    else if (key == "sound_file" && val.Length > 0)
                    {
                        cfg.AudioSoundFile = val;
                    }
                    else if (key == "cooldown_ms" &&
                             int.TryParse(val, NumberStyles.None, CultureInfo.InvariantCulture, out var cooldown))
                    {
                        cfg.AudioCooldownMs = Math.Clamp(cooldown, 0, 5000);
                    }
                    break;

                case "timing":
                    if (key == "pulse_ms" &&
                        int.TryParse(val, NumberStyles.None, CultureInfo.InvariantCulture, out var pulse))
                    {
                        cfg.PulseMs = Math.Clamp(pulse, 20, 500);
                    }
                    else if (key == "pulse_gap_ms" &&
                             int.TryParse(val, NumberStyles.None, CultureInfo.InvariantCulture, out var gap))
                    {
                        cfg.PulseGapMs = Math.Clamp(gap, 20, 500);
                    }
                    break;

                case "bindings":
                    if (key == "shift_up") cfg.ShiftUp = BindingSpec.Parse(val);
                    else if (key == "shift_down") cfg.ShiftDown = BindingSpec.Parse(val);
                    else if (key == "gear_d") cfg.GearD = BindingSpec.Parse(val);
                    else if (key == "gear_n") cfg.GearN = BindingSpec.Parse(val);
                    else if (key == "gear_r") cfg.GearR = BindingSpec.Parse(val);
                    break;
            }
        }

        return cfg;
    }

    public string Serialize()
    {
        var sb = new StringBuilder();
        var inv = CultureInfo.InvariantCulture;
        sb.AppendLine("[thresholds]");
        sb.AppendLine($"brake_threshold={Math.Clamp(BrakeThreshold, 0.01f, 1.0f).ToString("0.00", inv)}");
        sb.AppendLine($"stop_speed_kmh={Math.Clamp(StopSpeedKmh, 0.0f, 10.0f).ToString("0.00", inv)}");
        sb.AppendLine($"brake_source={(BrakeSource == BrakeSource.Effective ? "effective" : "input")}");
        sb.AppendLine();

        sb.AppendLine("[audio]");
        sb.AppendLine($"enabled={(AudioEnabled ? "true" : "false")}");
        sb.AppendLine($"sound_file={AudioSoundFile}");
        sb.AppendLine($"cooldown_ms={Math.Clamp(AudioCooldownMs, 0, 5000)}");
        sb.AppendLine();

        sb.AppendLine("[timing]");
        sb.AppendLine($"pulse_ms={Math.Clamp(PulseMs, 20, 500)}");
        sb.AppendLine($"pulse_gap_ms={Math.Clamp(PulseGapMs, 20, 500)}");
        sb.AppendLine();

        sb.AppendLine("[bindings]");
        sb.AppendLine($"shift_up={ShiftUp.ToToken()}");
        sb.AppendLine($"shift_down={ShiftDown.ToToken()}");
        sb.AppendLine($"gear_d={GearD.ToToken()}");
        sb.AppendLine($"gear_n={GearN.ToToken()}");
        sb.AppendLine($"gear_r={GearR.ToToken()}");
        return sb.ToString();
    }

    public static PluginConfig Load(string iniPath)
    {
        if (!File.Exists(iniPath))
        {
            return new PluginConfig();
        }
        return Parse(File.ReadAllText(iniPath, Encoding.UTF8));
    }

    public void Save(string iniPath)
    {
        var dir = Path.GetDirectoryName(iniPath);
        if (!string.IsNullOrEmpty(dir))
        {
            Directory.CreateDirectory(dir);
        }
        File.WriteAllText(iniPath, Serialize(), new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
    }

    private static bool TryParseBool(string val, out bool result)
    {
        switch (val.ToLowerInvariant())
        {
            case "true":
            case "1":
            case "yes":
            case "on":
                result = true;
                return true;
            case "false":
            case "0":
            case "no":
            case "off":
                result = false;
                return true;
            default:
                result = false;
                return false;
        }
    }
}
