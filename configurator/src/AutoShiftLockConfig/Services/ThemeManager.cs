using System.Runtime.InteropServices;

namespace AutoShiftLockConfig.Services;

public enum AppTheme
{
    Light = 0,
    Dark = 1,
}

public sealed record ThemePalette(
    Color WindowBack,
    Color SurfaceBack,
    Color InputBack,
    Color TextPrimary,
    Color TextSecondary,
    Color TextAccent,
    Color TextWarning,
    Color TextSuccess,
    Color TextDanger,
    Color ButtonBack,
    Color ButtonBorder,
    Color PrimaryButtonBack,
    Color PrimaryButtonFore,
    Color GroupFore);

public static class ThemeManager
{
    private const int DwmwaUseImmersiveDarkMode = 20;
    private const int DwmwaUseImmersiveDarkModeLegacy = 19;

    public static readonly ThemePalette Light = new(
        WindowBack: Color.FromArgb(245, 246, 248),
        SurfaceBack: Color.White,
        InputBack: Color.White,
        TextPrimary: Color.FromArgb(28, 31, 36),
        TextSecondary: Color.FromArgb(95, 102, 114),
        TextAccent: Color.FromArgb(28, 78, 165),
        TextWarning: Color.FromArgb(180, 100, 0),
        TextSuccess: Color.FromArgb(21, 122, 59),
        TextDanger: Color.FromArgb(185, 28, 28),
        ButtonBack: Color.FromArgb(236, 239, 244),
        ButtonBorder: Color.FromArgb(202, 208, 218),
        PrimaryButtonBack: Color.FromArgb(24, 118, 210),
        PrimaryButtonFore: Color.White,
        GroupFore: Color.FromArgb(40, 48, 60));

    public static readonly ThemePalette Dark = new(
        WindowBack: Color.FromArgb(22, 24, 29),
        SurfaceBack: Color.FromArgb(30, 33, 40),
        InputBack: Color.FromArgb(39, 43, 52),
        TextPrimary: Color.FromArgb(236, 239, 244),
        TextSecondary: Color.FromArgb(154, 162, 178),
        TextAccent: Color.FromArgb(110, 175, 255),
        TextWarning: Color.FromArgb(245, 180, 75),
        TextSuccess: Color.FromArgb(85, 205, 130),
        TextDanger: Color.FromArgb(255, 115, 115),
        ButtonBack: Color.FromArgb(46, 51, 62),
        ButtonBorder: Color.FromArgb(70, 77, 92),
        PrimaryButtonBack: Color.FromArgb(37, 132, 230),
        PrimaryButtonFore: Color.White,
        GroupFore: Color.FromArgb(198, 206, 220));

    [DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int attrValue, int attrSize);

    public static ThemePalette GetPalette(AppTheme theme) => theme == AppTheme.Dark ? Dark : Light;

    public static string GetPreferencePath(string? customRootDir = null)
    {
        var baseDir = customRootDir ?? Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "ETS2AutoShiftLock");
        return Path.Combine(baseDir, "theme.txt");
    }

    public static AppTheme LoadPreference(string? customRootDir = null)
    {
        try
        {
            var path = GetPreferencePath(customRootDir);
            if (File.Exists(path))
            {
                var content = File.ReadAllText(path).Trim().ToLowerInvariant();
                if (content == "dark") return AppTheme.Dark;
                if (content == "light") return AppTheme.Light;
            }
        }
        catch
        {
            // Ignore preference read errors
        }
        return AppTheme.Dark;
    }

    public static void SavePreference(AppTheme theme, string? customRootDir = null)
    {
        try
        {
            var path = GetPreferencePath(customRootDir);
            var dir = Path.GetDirectoryName(path);
            if (!string.IsNullOrEmpty(dir))
            {
                Directory.CreateDirectory(dir);
            }
            File.WriteAllText(path, theme == AppTheme.Dark ? "dark" : "light");
        }
        catch
        {
            // Ignore preference write errors
        }
    }

    public static void ApplyWindowTitleBarTheme(IntPtr hwnd, AppTheme theme)
    {
        if (hwnd == IntPtr.Zero) return;
        int dark = theme == AppTheme.Dark ? 1 : 0;
        try
        {
            if (DwmSetWindowAttribute(hwnd, DwmwaUseImmersiveDarkMode, ref dark, sizeof(int)) != 0)
            {
                DwmSetWindowAttribute(hwnd, DwmwaUseImmersiveDarkModeLegacy, ref dark, sizeof(int));
            }
        }
        catch
        {
            // Ignore on older Windows builds lacking DWM dark titlebar support
        }
    }

    public static void ApplyToControlTree(Control root, AppTheme theme, Button? primaryButton = null)
    {
        var p = GetPalette(theme);
        root.BackColor = p.WindowBack;
        root.ForeColor = p.TextPrimary;
        ApplyRecursive(root, p, primaryButton);
    }

    private static void ApplyRecursive(Control parent, ThemePalette p, Button? primaryButton)
    {
        foreach (Control c in parent.Controls)
        {
            switch (c)
            {
                case GroupBox grp:
                    grp.BackColor = p.SurfaceBack;
                    grp.ForeColor = p.GroupFore;
                    break;
                case Button btn:
                    btn.FlatStyle = FlatStyle.Flat;
                    if (btn == primaryButton)
                    {
                        btn.BackColor = p.PrimaryButtonBack;
                        btn.ForeColor = p.PrimaryButtonFore;
                        btn.FlatAppearance.BorderColor = p.PrimaryButtonBack;
                        btn.FlatAppearance.BorderSize = 0;
                    }
                    else
                    {
                        btn.BackColor = p.ButtonBack;
                        btn.ForeColor = p.TextPrimary;
                        btn.FlatAppearance.BorderColor = p.ButtonBorder;
                        btn.FlatAppearance.BorderSize = 1;
                    }
                    break;
                case TextBox tb:
                    tb.BackColor = p.InputBack;
                    tb.ForeColor = p.TextPrimary;
                    tb.BorderStyle = BorderStyle.FixedSingle;
                    break;
                case NumericUpDown num:
                    num.BackColor = p.InputBack;
                    num.ForeColor = p.TextPrimary;
                    break;
                case ComboBox cb:
                    cb.BackColor = p.InputBack;
                    cb.ForeColor = p.TextPrimary;
                    cb.FlatStyle = FlatStyle.Flat;
                    break;
                case CheckBox chk:
                    chk.BackColor = Color.Transparent;
                    chk.ForeColor = p.TextPrimary;
                    break;
                case Label lbl:
                    lbl.BackColor = Color.Transparent;
                    lbl.ForeColor = p.TextPrimary;
                    break;
                case Panel pnl:
                    pnl.BackColor = parent is GroupBox ? p.SurfaceBack : p.WindowBack;
                    pnl.ForeColor = p.TextPrimary;
                    break;
            }

            if (c.HasChildren)
            {
                ApplyRecursive(c, p, primaryButton);
            }
        }
    }
}
