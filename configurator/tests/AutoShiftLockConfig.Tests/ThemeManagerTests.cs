using AutoShiftLockConfig.Services;
using Xunit;

namespace AutoShiftLockConfig.Tests;

public class ThemeManagerTests
{
    [Fact]
    public void SaveAndLoadPreference_RoundTripsDarkAndLight()
    {
        var tempRoot = Path.Combine(Path.GetTempPath(), "ets2asl_theme_test_" + Guid.NewGuid().ToString("N"));
        try
        {
            // Defaults to Dark when preference file does not exist
            Assert.Equal(AppTheme.Dark, ThemeManager.LoadPreference(tempRoot));

            ThemeManager.SavePreference(AppTheme.Light, tempRoot);
            Assert.Equal(AppTheme.Light, ThemeManager.LoadPreference(tempRoot));

            ThemeManager.SavePreference(AppTheme.Dark, tempRoot);
            Assert.Equal(AppTheme.Dark, ThemeManager.LoadPreference(tempRoot));
        }
        finally
        {
            if (Directory.Exists(tempRoot))
            {
                Directory.Delete(tempRoot, recursive: true);
            }
        }
    }

    [Fact]
    public void Palettes_HaveDistinctDarkAndLightBackgrounds()
    {
        var dark = ThemeManager.GetPalette(AppTheme.Dark);
        var light = ThemeManager.GetPalette(AppTheme.Light);

        Assert.True(dark.WindowBack.GetBrightness() < 0.2f);
        Assert.True(light.WindowBack.GetBrightness() > 0.8f);
        Assert.True(dark.TextPrimary.GetBrightness() > 0.8f);
        Assert.True(light.TextPrimary.GetBrightness() < 0.2f);
    }
}
