using AutoShiftLockConfig.Services;
using Xunit;

namespace AutoShiftLockConfig.Tests;

public class Ets2PathDetectorTests
{
    [Fact]
    public void ParseSteamLibraryFoldersVdf_ExtractsAllPaths()
    {
        const string vdf = """
            "libraryfolders"
            {
                "0"
                {
                    "path"      "C:\\Program Files (x86)\\Steam"
                }
                "1"
                {
                    "path"      "D:\\SteamLibrary"
                }
            }
            """;

        var roots = Ets2PathDetector.ParseSteamLibraryFoldersVdf(vdf);
        Assert.Contains(@"C:\Program Files (x86)\Steam", roots);
        Assert.Contains(@"D:\SteamLibrary", roots);
    }

    [Fact]
    public void InstallPluginFiles_CopiesDllAndWavAndPreservesExistingIni()
    {
        var tempRoot = Path.Combine(Path.GetTempPath(), "ets2asl_inst_test_" + Guid.NewGuid().ToString("N"));
        var srcDir = Path.Combine(tempRoot, "src_plugin");
        var dstDir = Path.Combine(tempRoot, "win_x64", "plugins");
        Directory.CreateDirectory(srcDir);
        Directory.CreateDirectory(dstDir);

        try
        {
            File.WriteAllText(Path.Combine(srcDir, "ets2_auto_shift_lock.dll"), "NEW_DLL");
            File.WriteAllText(Path.Combine(srcDir, "lock_warning.wav"), "NEW_WAV");
            File.WriteAllText(Path.Combine(dstDir, "ets2_auto_shift_lock.ini"), "[bindings]\nshift_up=kb:160\n");

            Ets2PathDetector.InstallPluginFiles(srcDir, dstDir);

            Assert.Equal("NEW_DLL", File.ReadAllText(Path.Combine(dstDir, "ets2_auto_shift_lock.dll")));
            Assert.Equal("NEW_WAV", File.ReadAllText(Path.Combine(dstDir, "lock_warning.wav")));
            Assert.Equal("[bindings]\nshift_up=kb:160\n", File.ReadAllText(Path.Combine(dstDir, "ets2_auto_shift_lock.ini")));
        }
        finally
        {
            if (Directory.Exists(tempRoot))
            {
                Directory.Delete(tempRoot, recursive: true);
            }
        }
    }
}
