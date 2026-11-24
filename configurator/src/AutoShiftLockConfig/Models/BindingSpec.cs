using System.Globalization;
using System.Windows.Forms;

namespace AutoShiftLockConfig.Models;

public enum BindingSource
{
    None = 0,
    Keyboard,
    XInput,
    DirectInputButton,
    DirectInputPov,
}

public enum PovDirection
{
    Up = 0,
    Right,
    Down,
    Left,
}

public enum BrakeSource
{
    Input = 0,
    Effective,
}

public sealed record BindingSpec
{
    private static readonly HashSet<string> ValidXInputButtons = new(StringComparer.Ordinal)
    {
        "A", "B", "X", "Y", "LB", "RB", "BACK", "START", "LS", "RS",
        "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT", "LT", "RT",
    };

    public static readonly BindingSpec None = new();

    public BindingSource Source { get; init; } = BindingSource.None;
    public uint VkCode { get; init; }
    public byte XInputSlot { get; init; }
    public string XInputButton { get; init; } = string.Empty;
    public string DeviceGuid { get; init; } = string.Empty;
    public uint Index { get; init; }
    public PovDirection PovDir { get; init; } = PovDirection.Up;

    public static BindingSpec Keyboard(uint vkCode) => new()
    {
        Source = BindingSource.Keyboard,
        VkCode = vkCode,
    };

    public static BindingSpec XInput(byte slot, string button) => new()
    {
        Source = BindingSource.XInput,
        XInputSlot = slot,
        XInputButton = button.ToUpperInvariant(),
    };

    public static BindingSpec DirectInputButton(string productGuid, uint buttonIndex) => new()
    {
        Source = BindingSource.DirectInputButton,
        DeviceGuid = productGuid.ToUpperInvariant(),
        Index = buttonIndex,
    };

    public static BindingSpec DirectInputPov(string productGuid, uint povIndex, PovDirection dir) => new()
    {
        Source = BindingSource.DirectInputPov,
        DeviceGuid = productGuid.ToUpperInvariant(),
        Index = povIndex,
        PovDir = dir,
    };

    public static BindingSpec Parse(string? token)
    {
        if (string.IsNullOrWhiteSpace(token))
        {
            return None;
        }

        var parts = token.Trim().Split(':');
        if (parts.Length == 0)
        {
            return None;
        }

        var kind = parts[0].ToLowerInvariant();
        if (kind == "kb" && parts.Length == 2)
        {
            if (uint.TryParse(parts[1], NumberStyles.None, CultureInfo.InvariantCulture, out var vk) && vk is >= 1 and <= 254)
            {
                return Keyboard(vk);
            }
            return None;
        }

        if (kind == "xinput" && parts.Length == 3)
        {
            var btn = parts[2].ToUpperInvariant();
            if (byte.TryParse(parts[1], NumberStyles.None, CultureInfo.InvariantCulture, out var slot) &&
                slot <= 3 &&
                ValidXInputButtons.Contains(btn))
            {
                return XInput(slot, btn);
            }
            return None;
        }

        if (kind == "dinput" && parts.Length >= 4)
        {
            if (!Guid.TryParseExact(parts[1], "D", out var parsedGuid))
            {
                return None;
            }
            var guidStr = parsedGuid.ToString("D").ToUpperInvariant();
            var sub = parts[2].ToLowerInvariant();
            if (sub == "btn" && parts.Length == 4)
            {
                if (uint.TryParse(parts[3], NumberStyles.None, CultureInfo.InvariantCulture, out var idx) && idx < 128)
                {
                    return DirectInputButton(guidStr, idx);
                }
                return None;
            }

            if (sub == "pov" && parts.Length == 5)
            {
                if (uint.TryParse(parts[3], NumberStyles.None, CultureInfo.InvariantCulture, out var idx) &&
                    idx < 4 &&
                    TryParsePovDirection(parts[4], out var dir))
                {
                    return DirectInputPov(guidStr, idx, dir);
                }
                return None;
            }
        }

        return None;
    }

    public string ToToken() => Source switch
    {
        BindingSource.None => string.Empty,
        BindingSource.Keyboard => $"kb:{VkCode}",
        BindingSource.XInput => $"xinput:{XInputSlot}:{XInputButton.ToUpperInvariant()}",
        BindingSource.DirectInputButton => $"dinput:{DeviceGuid.ToUpperInvariant()}:btn:{Index}",
        BindingSource.DirectInputPov => $"dinput:{DeviceGuid.ToUpperInvariant()}:pov:{Index}:{PovDirectionToken(PovDir)}",
        _ => string.Empty,
    };

    public string ToDisplayText() => Source switch
    {
        BindingSource.None => "Atanmadı",
        BindingSource.Keyboard => $"Klavye: {FormatVkName(VkCode)} (VK {VkCode})",
        BindingSource.XInput => $"Gamepad #{XInputSlot + 1}: {XInputButton}",
        BindingSource.DirectInputButton => $"Direksiyon/Cihaz: Buton {Index + 1} [{ShortGuid(DeviceGuid)}]",
        BindingSource.DirectInputPov => $"Direksiyon/Cihaz: POV {Index + 1} {PovDirectionToken(PovDir)} [{ShortGuid(DeviceGuid)}]",
        _ => "Atanmadı",
    };

    private static string ShortGuid(string guid) => guid.Length >= 8 ? guid[..8] : guid;

    private static string FormatVkName(uint vk) => vk switch
    {
        160 => "Sol Shift",
        161 => "Sağ Shift",
        162 => "Sol Ctrl",
        163 => "Sağ Ctrl",
        164 => "Sol Alt",
        165 => "Sağ Alt",
        _ => ((Keys)vk).ToString(),
    };

    private static bool TryParsePovDirection(string value, out PovDirection dir)
    {
        switch (value.ToUpperInvariant())
        {
            case "UP":
                dir = PovDirection.Up;
                return true;
            case "RIGHT":
                dir = PovDirection.Right;
                return true;
            case "DOWN":
                dir = PovDirection.Down;
                return true;
            case "LEFT":
                dir = PovDirection.Left;
                return true;
            default:
                dir = PovDirection.Up;
                return false;
        }
    }

    private static string PovDirectionToken(PovDirection dir) => dir switch
    {
        PovDirection.Up => "UP",
        PovDirection.Right => "RIGHT",
        PovDirection.Down => "DOWN",
        PovDirection.Left => "LEFT",
        _ => "UP",
    };
}
