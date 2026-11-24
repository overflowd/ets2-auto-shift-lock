using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace AutoShiftLockConfig.Services;

public static class Ets2PathDetector
{
    public const string PluginDllName = "ets2_auto_shift_lock.dll";
    public const string DefaultWavName = "lock_warning.wav";
    public const string ConfigIniName = "ets2_auto_shift_lock.ini";

    private static readonly Regex VdfPathRegex = new(
        "\"path\"\\s+\"([^\"]+)\"",
        RegexOptions.IgnoreCase | RegexOptions.Compiled);

    public static IReadOnlyList<string> ParseSteamLibraryFoldersVdf(string vdfContent)
    {
        var results = new List<string>();
        foreach (Match m in VdfPathRegex.Matches(vdfContent))
        {
            var raw = m.Groups[1].Value.Replace(@"\\", @"\").Trim();
            if (raw.Length > 0 && !results.Contains(raw, StringComparer.OrdinalIgnoreCase))
            {
                results.Add(raw);
            }
        }
        return results;
    }

    public static IReadOnlyList<string> FindEts2PluginsDirectories(IEnumerable<string>? candidateSteamRoots = null)
    {
        var steamRoots = new List<string>();
        if (candidateSteamRoots is not null)
        {
            steamRoots.AddRange(candidateSteamRoots);
        }
        else
        {
            var regSteam = TryGetSteamInstallPathFromRegistry();
            if (!string.IsNullOrEmpty(regSteam))
            {
                steamRoots.Add(regSteam);
                var vdfPath = Path.Combine(regSteam, "steamapps", "libraryfolders.vdf");
                if (File.Exists(vdfPath))
                {
                    steamRoots.AddRange(ParseSteamLibraryFoldersVdf(File.ReadAllText(vdfPath)));
                }
            }

            foreach (var drive in DriveInfo.GetDrives())
            {
                if (!drive.IsReady || drive.DriveType != DriveType.Fixed)
                {
                    continue;
                }
                steamRoots.Add(Path.Combine(drive.RootDirectory.FullName, "Program Files (x86)", "Steam"));
                steamRoots.Add(Path.Combine(drive.RootDirectory.FullName, "SteamLibrary"));
                steamRoots.Add(Path.Combine(drive.RootDirectory.FullName, "Games", "SteamLibrary"));
            }
        }

        var found = new List<string>();
        foreach (var root in steamRoots.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            var gameDir = Path.Combine(root, "steamapps", "common", "Euro Truck Simulator 2");
            var binX64 = Path.Combine(gameDir, "bin", "win_x64");
            if (Directory.Exists(binX64))
            {
                var pluginsDir = Path.Combine(binX64, "plugins");
                if (!found.Contains(pluginsDir, StringComparer.OrdinalIgnoreCase))
                {
                    found.Add(pluginsDir);
                }
            }
        }
        return found;
    }

    public static void InstallPluginFiles(string sourcePluginDir, string targetPluginsDir)
    {
        var srcDll = Path.Combine(sourcePluginDir, PluginDllName);
        if (!File.Exists(srcDll))
        {
            throw new FileNotFoundException($"Eklenti DLL dosyası bulunamadı: {srcDll}", srcDll);
        }

        Directory.CreateDirectory(targetPluginsDir);
        File.Copy(srcDll, Path.Combine(targetPluginsDir, PluginDllName), overwrite: true);

        var srcWav = Path.Combine(sourcePluginDir, DefaultWavName);
        if (File.Exists(srcWav))
        {
            File.Copy(srcWav, Path.Combine(targetPluginsDir, DefaultWavName), overwrite: true);
        }
    }

    private static string? TryGetSteamInstallPathFromRegistry()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam");
            var val = key?.GetValue("SteamPath") as string;
            if (!string.IsNullOrWhiteSpace(val))
            {
                return val.Replace('/', '\\');
            }
        }
        catch
        {
            // Ignore registry access failures
        }
        return null;
    }
}
