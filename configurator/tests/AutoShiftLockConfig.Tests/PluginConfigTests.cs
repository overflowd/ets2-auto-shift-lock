using AutoShiftLockConfig.Models;
using AutoShiftLockConfig.Services;
using Xunit;

namespace AutoShiftLockConfig.Tests;

public class PluginConfigTests
{
    [Fact]
    public void Defaults_MatchSpecValues()
    {
        var cfg = PluginConfig.Parse("");
        Assert.Equal(0.25f, cfg.BrakeThreshold, 3);
        Assert.Equal(0.5f, cfg.StopSpeedKmh, 3);
        Assert.Equal(BrakeSource.Input, cfg.BrakeSource);
        Assert.True(cfg.AudioEnabled);
        Assert.Equal("lock_warning.wav", cfg.AudioSoundFile);
        Assert.Equal(350, cfg.AudioCooldownMs);
        Assert.Equal(60, cfg.PulseMs);
        Assert.Equal(80, cfg.PulseGapMs);
        Assert.Equal(BindingSource.None, cfg.ShiftUp.Source);
        Assert.Equal(BindingSource.None, cfg.ShiftDown.Source);
        Assert.Equal(BindingSource.None, cfg.GearD.Source);
        Assert.Equal(BindingSource.None, cfg.GearN.Source);
        Assert.Equal(BindingSource.None, cfg.GearR.Source);
    }

    [Theory]
    [InlineData("kb:160")]
    [InlineData("xinput:0:RB")]
    [InlineData("xinput:1:RT")]
    [InlineData("dinput:12345678-9ABC-DEF0-1234-56789ABCDEF0:btn:4")]
    [InlineData("dinput:12345678-9ABC-DEF0-1234-56789ABCDEF0:pov:0:LEFT")]
    public void BindingSpec_RoundTripsIdenticallyToCppFormat(string token)
    {
        var spec = BindingSpec.Parse(token);
        Assert.NotEqual(BindingSource.None, spec.Source);
        Assert.Equal(token, spec.ToToken());
    }

    [Fact]
    public void Parse_HandlesUtf8BomCrlfClampingAndInvalidBindings()
    {
        const string ini =
            "\uFEFF; comment\r\n" +
            "[thresholds]\r\n" +
            "brake_threshold = 4.2\r\n" +
            "stop_speed_kmh = -2.0\r\n" +
            "brake_source = effective\r\n" +
            "[audio]\r\n" +
            "enabled = false\r\n" +
            "sound_file = custom.wav\r\n" +
            "cooldown_ms = 99999\r\n" +
            "[bindings]\r\n" +
            "shift_up = kb:160\r\n" +
            "gear_d = not_valid\r\n" +
            "gear_r = dinput:12345678-9abc-def0-1234-56789abcdef0:btn:3\r\n";

        var cfg = PluginConfig.Parse(ini);
        Assert.Equal(1.0f, cfg.BrakeThreshold, 3);
        Assert.Equal(0.0f, cfg.StopSpeedKmh, 3);
        Assert.Equal(BrakeSource.Effective, cfg.BrakeSource);
        Assert.False(cfg.AudioEnabled);
        Assert.Equal("custom.wav", cfg.AudioSoundFile);
        Assert.Equal(5000, cfg.AudioCooldownMs);
        Assert.Equal("kb:160", cfg.ShiftUp.ToToken());
        Assert.Equal(BindingSource.None, cfg.GearD.Source);
        Assert.Equal("dinput:12345678-9ABC-DEF0-1234-56789ABCDEF0:btn:3", cfg.GearR.ToToken());
    }

    [Fact]
    public void Serialize_RoundTripsCleanly()
    {
        var cfg = new PluginConfig
        {
            BrakeThreshold = 0.35f,
            StopSpeedKmh = 0.75f,
            BrakeSource = BrakeSource.Effective,
            AudioEnabled = false,
            AudioSoundFile = "beep.wav",
            AudioCooldownMs = 450,
            ShiftUp = BindingSpec.Parse("kb:160"),
            ShiftDown = BindingSpec.Parse("kb:162"),
            GearD = BindingSpec.Parse("xinput:0:RB"),
        };

        var reparsed = PluginConfig.Parse(cfg.Serialize());
        Assert.Equal(0.35f, reparsed.BrakeThreshold, 2);
        Assert.Equal(0.75f, reparsed.StopSpeedKmh, 2);
        Assert.Equal(BrakeSource.Effective, reparsed.BrakeSource);
        Assert.False(reparsed.AudioEnabled);
        Assert.Equal("beep.wav", reparsed.AudioSoundFile);
        Assert.Equal(450, reparsed.AudioCooldownMs);
        Assert.Equal("kb:160", reparsed.ShiftUp.ToToken());
        Assert.Equal("kb:162", reparsed.ShiftDown.ToToken());
        Assert.Equal("xinput:0:RB", reparsed.GearD.ToToken());
    }

    [Fact]
    public void DirectInputDataFormat_UsesDidfAbsAxisNotRelAxis()
    {
        // dinput.h: DIDF_ABSAXIS = 0x00000001, DIDF_RELAXIS = 0x00000002
        Assert.Equal(0x00000001u, UniversalInputCapture.DidfAbsAxis);
    }
}
