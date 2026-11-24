using System.Media;
using AutoShiftLockConfig.Models;
using AutoShiftLockConfig.Services;

namespace AutoShiftLockConfig;

public sealed class MainForm : Form
{
    private readonly TextBox _pluginsPathBox = new() { ReadOnly = false, Anchor = AnchorStyles.Left | AnchorStyles.Right };
    private readonly Label _installStatusLabel = new() { AutoSize = true };

    private readonly Dictionary<string, (Label ValueLabel, Button BindBtn, Button ClearBtn)> _bindingRows = new();
    private readonly NumericUpDown _brakePctInput = new() { Minimum = 5, Maximum = 100, Value = 25, Width = 75 };
    private readonly NumericUpDown _stopSpeedInput = new() { Minimum = 0.0m, Maximum = 10.0m, DecimalPlaces = 2, Increment = 0.1m, Value = 0.50m, Width = 75 };
    private readonly ComboBox _brakeSourceCombo = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 195 };

    private readonly CheckBox _audioEnabledCheck = new() { Text = "Play warning sound when shift is locked", AutoSize = true, Checked = true };
    private readonly TextBox _soundFileBox = new() { Text = "lock_warning.wav", Anchor = AnchorStyles.Left | AnchorStyles.Right };
    private readonly NumericUpDown _cooldownInput = new() { Minimum = 0, Maximum = 5000, Increment = 50, Value = 350, Width = 75 };

    private readonly Label _hintLabel = new()
    {
        Text = "Setup reminder: In ETS2 Options → Keys & Buttons, clear your existing Shift Up / Shift Down bindings, and set Transmission to 'Real Automatic'.",
        AutoSize = true,
        MaximumSize = new Size(540, 0),
    };
    private readonly Label _statusBarLabel = new() { AutoSize = true };
    private readonly Button _themeToggleBtn = new() { AutoSize = true, Padding = new Padding(10, 3, 10, 3) };
    private readonly Button _saveBtn = new() { Text = "Save Settings", AutoSize = true, Padding = new Padding(14, 6, 14, 6) };

    private PluginConfig _config = new();
    private AppTheme _currentTheme;
    private StatusKind _statusKind = StatusKind.Neutral;
    private StatusKind _installKind = StatusKind.Warning;
    private CancellationTokenSource? _captureCts;

    private enum StatusKind
    {
        Neutral,
        Success,
        Warning,
        Danger,
    }

    public MainForm()
    {
        Text = "ETS2 Auto Shift-Lock — Configuration";
        MinimumSize = new Size(740, 660);
        Size = new Size(780, 700);
        StartPosition = FormStartPosition.CenterScreen;
        Font = new Font("Segoe UI", 9.5f);

        LoadAppIcon();
        _currentTheme = ThemeManager.LoadPreference();

        BuildLayout();
        AutoDetectEts2AndLoad();
        ApplyTheme();
    }

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        ThemeManager.ApplyWindowTitleBarTheme(Handle, _currentTheme);
    }

    private void LoadAppIcon()
    {
        try
        {
            var exePath = Environment.ProcessPath;
            if (!string.IsNullOrEmpty(exePath) && File.Exists(exePath))
            {
                var extracted = Icon.ExtractAssociatedIcon(exePath);
                if (extracted is not null)
                {
                    Icon = extracted;
                    return;
                }
            }
        }
        catch
        {
            // Keep default icon if extraction fails
        }
    }

    private void BuildLayout()
    {
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(14),
            ColumnCount = 1,
            RowCount = 6,
            AutoScroll = true,
        };
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));

        root.Controls.Add(BuildHeaderPanel(), 0, 0);
        root.Controls.Add(BuildInstallGroup(), 0, 1);
        root.Controls.Add(BuildBindingsGroup(), 0, 2);
        root.Controls.Add(BuildThresholdsGroup(), 0, 3);
        root.Controls.Add(BuildAudioGroup(), 0, 4);
        root.Controls.Add(BuildFooterPanel(), 0, 5);

        Controls.Add(root);
    }

    private Panel BuildHeaderPanel()
    {
        var panel = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            ColumnCount = 2,
            RowCount = 1,
            AutoSize = true,
            Padding = new Padding(0, 0, 0, 8),
        };
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));

        var title = new Label
        {
            Text = "ETS2 Auto Shift-Lock",
            Font = new Font("Segoe UI Semibold", 13.5f),
            AutoSize = true,
            Anchor = AnchorStyles.Left,
            Padding = new Padding(0, 2, 0, 0),
        };

        _themeToggleBtn.Click += (_, _) =>
        {
            _currentTheme = _currentTheme == AppTheme.Dark ? AppTheme.Light : AppTheme.Dark;
            ThemeManager.SavePreference(_currentTheme);
            ApplyTheme();
        };

        panel.Controls.Add(title, 0, 0);
        panel.Controls.Add(_themeToggleBtn, 1, 0);
        return panel;
    }

    private GroupBox BuildInstallGroup()
    {
        var grp = new GroupBox
        {
            Text = "1. ETS2 Plugins Folder & Installation",
            Dock = DockStyle.Top,
            AutoSize = true,
            Padding = new Padding(10),
        };

        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 3,
            RowCount = 2,
            AutoSize = true,
        };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));

        var browseBtn = new Button { Text = "Browse...", AutoSize = true, Padding = new Padding(8, 2, 8, 2) };
        browseBtn.Click += (_, _) => BrowsePluginsDir();

        var installBtn = new Button { Text = "Install / Update Plugin", AutoSize = true, Padding = new Padding(8, 2, 8, 2) };
        installBtn.Click += (_, _) => InstallPlugin();

        _pluginsPathBox.TextChanged += (_, _) => RefreshInstallStatus();

        grid.Controls.Add(_pluginsPathBox, 0, 0);
        grid.Controls.Add(browseBtn, 1, 0);
        grid.Controls.Add(installBtn, 2, 0);
        grid.Controls.Add(_installStatusLabel, 0, 1);
        grid.SetColumnSpan(_installStatusLabel, 3);

        grp.Controls.Add(grid);
        return grp;
    }

    private GroupBox BuildBindingsGroup()
    {
        var grp = new GroupBox
        {
            Text = "2. Shift Bindings (Keyboard, Gamepad, Steering Wheel)",
            Dock = DockStyle.Top,
            AutoSize = true,
            Padding = new Padding(10),
        };

        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 4,
            RowCount = 5,
            AutoSize = true,
        };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 195));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));

        var actions = new (string Key, string Label)[]
        {
            ("shift_up", "Shift Up (R → N → D):"),
            ("shift_down", "Shift Down (D → N → R):"),
            ("gear_d", "Direct Drive (D):"),
            ("gear_n", "Direct Neutral (N):"),
            ("gear_r", "Direct Reverse (R):"),
        };

        for (int i = 0; i < actions.Length; i++)
        {
            var (key, labelText) = actions[i];
            var titleLbl = new Label { Text = labelText, AutoSize = true, Anchor = AnchorStyles.Left, Padding = new Padding(0, 6, 0, 0) };
            var valueLbl = new Label
            {
                Text = "Unbound",
                AutoSize = true,
                Anchor = AnchorStyles.Left,
                Padding = new Padding(0, 6, 0, 0),
            };
            var bindBtn = new Button { Text = "Press to Bind", AutoSize = true, Padding = new Padding(6, 2, 6, 2) };
            var clearBtn = new Button { Text = "Clear", AutoSize = true, Padding = new Padding(6, 2, 6, 2) };

            bindBtn.Click += async (_, _) => await CaptureBindingAsync(key);
            clearBtn.Click += (_, _) =>
            {
                SetBinding(key, BindingSpec.None);
                RefreshBindingLabels();
            };

            _bindingRows[key] = (valueLbl, bindBtn, clearBtn);

            grid.Controls.Add(titleLbl, 0, i);
            grid.Controls.Add(valueLbl, 1, i);
            grid.Controls.Add(bindBtn, 2, i);
            grid.Controls.Add(clearBtn, 3, i);
        }

        grp.Controls.Add(grid);
        return grp;
    }

    private GroupBox BuildThresholdsGroup()
    {
        var grp = new GroupBox
        {
            Text = "3. Brake & Stop Thresholds",
            Dock = DockStyle.Top,
            AutoSize = true,
            Padding = new Padding(10),
        };

        _brakeSourceCombo.Items.Add("Foot Brake Pedal (Recommended)");
        _brakeSourceCombo.Items.Add("Effective Brake");
        _brakeSourceCombo.SelectedIndex = 0;

        var flow = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            WrapContents = true,
            FlowDirection = FlowDirection.LeftToRight,
        };

        flow.Controls.Add(new Label { Text = "Brake threshold (%):", AutoSize = true, Padding = new Padding(0, 6, 4, 0) });
        flow.Controls.Add(_brakePctInput);
        flow.Controls.Add(new Label { Text = "Stop tolerance (km/h):", AutoSize = true, Padding = new Padding(14, 6, 4, 0) });
        flow.Controls.Add(_stopSpeedInput);
        flow.Controls.Add(new Label { Text = "Brake source:", AutoSize = true, Padding = new Padding(14, 6, 4, 0) });
        flow.Controls.Add(_brakeSourceCombo);

        grp.Controls.Add(flow);
        return grp;
    }

    private GroupBox BuildAudioGroup()
    {
        var grp = new GroupBox
        {
            Text = "4. Audio Warning",
            Dock = DockStyle.Top,
            AutoSize = true,
            Padding = new Padding(10),
        };

        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 4,
            RowCount = 2,
            AutoSize = true,
        };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));

        var browseWavBtn = new Button { Text = "Choose WAV...", AutoSize = true };
        browseWavBtn.Click += (_, _) => BrowseWavFile();

        var testBtn = new Button { Text = "Test Sound", AutoSize = true };
        testBtn.Click += (_, _) => TestWarningSound();

        var cooldownPanel = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Left };
        cooldownPanel.Controls.Add(_audioEnabledCheck);
        cooldownPanel.Controls.Add(new Label { Text = "Cooldown (ms):", AutoSize = true, Padding = new Padding(16, 4, 4, 0) });
        cooldownPanel.Controls.Add(_cooldownInput);

        grid.Controls.Add(cooldownPanel, 0, 0);
        grid.SetColumnSpan(cooldownPanel, 4);
        grid.Controls.Add(new Label { Text = "Sound file:", AutoSize = true, Padding = new Padding(0, 6, 4, 0) }, 0, 1);
        grid.Controls.Add(_soundFileBox, 1, 1);
        grid.Controls.Add(browseWavBtn, 2, 1);
        grid.Controls.Add(testBtn, 3, 1);

        grp.Controls.Add(grid);
        return grp;
    }

    private Panel BuildFooterPanel()
    {
        var panel = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            ColumnCount = 2,
            RowCount = 2,
            AutoSize = true,
            Padding = new Padding(0, 8, 0, 0),
        };
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));

        _saveBtn.Click += (_, _) => SaveCurrentConfig();

        panel.Controls.Add(_hintLabel, 0, 0);
        panel.Controls.Add(_saveBtn, 1, 0);
        panel.Controls.Add(_statusBarLabel, 0, 1);
        panel.SetColumnSpan(_statusBarLabel, 2);

        return panel;
    }

    private void ApplyTheme()
    {
        ThemeManager.ApplyToControlTree(this, _currentTheme, primaryButton: _saveBtn);
        if (IsHandleCreated)
        {
            ThemeManager.ApplyWindowTitleBarTheme(Handle, _currentTheme);
        }

        var palette = ThemeManager.GetPalette(_currentTheme);
        _themeToggleBtn.Text = _currentTheme == AppTheme.Dark ? "Light Mode" : "Dark Mode";
        _hintLabel.ForeColor = palette.TextSecondary;
        _installStatusLabel.ForeColor = ResolveStatusColor(_installKind, palette);
        _statusBarLabel.ForeColor = ResolveStatusColor(_statusKind, palette);
        RefreshBindingLabels();
    }

    private static Color ResolveStatusColor(StatusKind kind, ThemePalette p) => kind switch
    {
        StatusKind.Success => p.TextSuccess,
        StatusKind.Warning => p.TextWarning,
        StatusKind.Danger => p.TextDanger,
        _ => p.TextSecondary,
    };

    private void SetStatusMessage(string message, StatusKind kind)
    {
        _statusKind = kind;
        _statusBarLabel.Text = message;
        _statusBarLabel.ForeColor = ResolveStatusColor(kind, ThemeManager.GetPalette(_currentTheme));
    }

    private void SetInstallMessage(string message, StatusKind kind)
    {
        _installKind = kind;
        _installStatusLabel.Text = message;
        _installStatusLabel.ForeColor = ResolveStatusColor(kind, ThemeManager.GetPalette(_currentTheme));
    }

    private void AutoDetectEts2AndLoad()
    {
        var found = Ets2PathDetector.FindEts2PluginsDirectories();
        if (found.Count > 0)
        {
            _pluginsPathBox.Text = found[0];
            LoadConfigFromPluginsDir(found[0]);
        }
        else
        {
            SetInstallMessage(
                "ETS2 was not auto-detected — please browse to your bin\\win_x64\\plugins folder.",
                StatusKind.Warning);
        }
        RefreshBindingLabels();
    }

    private void BrowsePluginsDir()
    {
        using var dlg = new FolderBrowserDialog
        {
            Description = "Select your ETS2 bin\\win_x64\\plugins folder",
            UseDescriptionForTitle = true,
        };
        if (dlg.ShowDialog(this) == DialogResult.OK)
        {
            _pluginsPathBox.Text = dlg.SelectedPath;
            LoadConfigFromPluginsDir(dlg.SelectedPath);
        }
    }

    private void LoadConfigFromPluginsDir(string pluginsDir)
    {
        var iniPath = Path.Combine(pluginsDir, Ets2PathDetector.ConfigIniName);
        _config = PluginConfig.Load(iniPath);
        _brakePctInput.Value = Math.Clamp((decimal)Math.Round(_config.BrakeThreshold * 100f), 5, 100);
        _stopSpeedInput.Value = Math.Clamp((decimal)_config.StopSpeedKmh, 0m, 10m);
        _brakeSourceCombo.SelectedIndex = _config.BrakeSource == BrakeSource.Effective ? 1 : 0;
        _audioEnabledCheck.Checked = _config.AudioEnabled;
        _soundFileBox.Text = _config.AudioSoundFile;
        _cooldownInput.Value = Math.Clamp(_config.AudioCooldownMs, 0, 5000);
        RefreshBindingLabels();
        RefreshInstallStatus();
    }

    private void RefreshInstallStatus()
    {
        var dir = _pluginsPathBox.Text.Trim();
        if (string.IsNullOrEmpty(dir))
        {
            SetInstallMessage("No folder selected.", StatusKind.Warning);
            return;
        }

        var dllExists = File.Exists(Path.Combine(dir, Ets2PathDetector.PluginDllName));
        if (dllExists)
        {
            SetInstallMessage("Status: Plugin DLL is installed.", StatusKind.Success);
        }
        else
        {
            SetInstallMessage("Status: DLL not installed yet — click 'Install / Update Plugin'.", StatusKind.Warning);
        }
    }

    private void InstallPlugin()
    {
        var targetDir = _pluginsPathBox.Text.Trim();
        if (string.IsNullOrEmpty(targetDir))
        {
            MessageBox.Show(this, "Please select your ETS2 plugins folder first.", "Warning", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        var appDir = AppContext.BaseDirectory;
        var candidateSourceDirs = new[]
        {
            Path.Combine(appDir, "plugin"),
            appDir,
        };

        var sourceDir = candidateSourceDirs.FirstOrDefault(d => File.Exists(Path.Combine(d, Ets2PathDetector.PluginDllName)));
        if (sourceDir is null)
        {
            MessageBox.Show(
                this,
                $"Plugin file ({Ets2PathDetector.PluginDllName}) was not found in the 'plugin' folder next to this app.",
                "Error",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
            return;
        }

        try
        {
            Ets2PathDetector.InstallPluginFiles(sourceDir, targetDir);
            RefreshInstallStatus();
            SetStatusMessage($"Plugin installed successfully to: {targetDir}", StatusKind.Success);
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, $"Installation failed: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private async Task CaptureBindingAsync(string actionKey)
    {
        _captureCts?.Cancel();
        _captureCts = new CancellationTokenSource(TimeSpan.FromSeconds(8));
        var (lbl, _, _) = _bindingRows[actionKey];
        var oldText = lbl.Text;
        lbl.Text = "Listening... Press any key or button (ESC to cancel)";
        lbl.ForeColor = ThemeManager.GetPalette(_currentTheme).TextDanger;
        SetAllBindButtonsEnabled(false);

        try
        {
            using var capture = new UniversalInputCapture(Handle);
            var captured = await capture.CaptureNextAsync(_captureCts.Token);
            if (captured is not null)
            {
                SetBinding(actionKey, captured);
                SetStatusMessage($"Bound: {captured.ToDisplayText()}", StatusKind.Success);
            }
            else
            {
                lbl.Text = oldText;
                SetStatusMessage("Binding capture canceled.", StatusKind.Neutral);
            }
        }
        catch (OperationCanceledException)
        {
            lbl.Text = oldText;
            SetStatusMessage("Timed out — no input detected.", StatusKind.Warning);
        }
        finally
        {
            SetAllBindButtonsEnabled(true);
            RefreshBindingLabels();
        }
    }

    private void SetAllBindButtonsEnabled(bool enabled)
    {
        foreach (var (_, bindBtn, clearBtn) in _bindingRows.Values)
        {
            bindBtn.Enabled = enabled;
            clearBtn.Enabled = enabled;
        }
    }

    private void SetBinding(string key, BindingSpec spec)
    {
        switch (key)
        {
            case "shift_up": _config.ShiftUp = spec; break;
            case "shift_down": _config.ShiftDown = spec; break;
            case "gear_d": _config.GearD = spec; break;
            case "gear_n": _config.GearN = spec; break;
            case "gear_r": _config.GearR = spec; break;
        }
    }

    private BindingSpec GetBinding(string key) => key switch
    {
        "shift_up" => _config.ShiftUp,
        "shift_down" => _config.ShiftDown,
        "gear_d" => _config.GearD,
        "gear_n" => _config.GearN,
        "gear_r" => _config.GearR,
        _ => BindingSpec.None,
    };

    private void RefreshBindingLabels()
    {
        var palette = ThemeManager.GetPalette(_currentTheme);
        foreach (var (key, row) in _bindingRows)
        {
            var spec = GetBinding(key);
            row.ValueLabel.Text = spec.ToDisplayText();
            row.ValueLabel.ForeColor = spec.Source == Models.BindingSource.None ? palette.TextSecondary : palette.TextAccent;
        }
    }

    private void BrowseWavFile()
    {
        using var dlg = new OpenFileDialog
        {
            Filter = "WAV Audio Files (*.wav)|*.wav|All Files (*.*)|*.*",
            Title = "Choose Warning Sound",
        };
        if (dlg.ShowDialog(this) == DialogResult.OK)
        {
            _soundFileBox.Text = dlg.FileName;
        }
    }

    private void TestWarningSound()
    {
        var raw = _soundFileBox.Text.Trim();
        var resolved = Path.IsPathRooted(raw)
            ? raw
            : Path.Combine(_pluginsPathBox.Text.Trim(), raw);

        if (!File.Exists(resolved))
        {
            resolved = Path.Combine(AppContext.BaseDirectory, "plugin", Ets2PathDetector.DefaultWavName);
        }

        if (File.Exists(resolved))
        {
            try
            {
                using var player = new SoundPlayer(resolved);
                player.Play();
                return;
            }
            catch
            {
                // Fall back to system beep
            }
        }
        SystemSounds.Exclamation.Play();
    }

    private void SaveCurrentConfig()
    {
        var dir = _pluginsPathBox.Text.Trim();
        if (string.IsNullOrEmpty(dir))
        {
            MessageBox.Show(this, "Please select your ETS2 plugins folder before saving.", "Warning", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        _config.BrakeThreshold = (float)_brakePctInput.Value / 100f;
        _config.StopSpeedKmh = (float)_stopSpeedInput.Value;
        _config.BrakeSource = _brakeSourceCombo.SelectedIndex == 1 ? BrakeSource.Effective : BrakeSource.Input;
        _config.AudioEnabled = _audioEnabledCheck.Checked;
        _config.AudioSoundFile = string.IsNullOrWhiteSpace(_soundFileBox.Text) ? Ets2PathDetector.DefaultWavName : _soundFileBox.Text.Trim();
        _config.AudioCooldownMs = (int)_cooldownInput.Value;

        var iniPath = Path.Combine(dir, Ets2PathDetector.ConfigIniName);
        try
        {
            _config.Save(iniPath);
            SetStatusMessage($"Settings saved ({DateTime.Now:HH:mm:ss}): {iniPath}", StatusKind.Success);
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, $"Failed to save settings: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }
}
