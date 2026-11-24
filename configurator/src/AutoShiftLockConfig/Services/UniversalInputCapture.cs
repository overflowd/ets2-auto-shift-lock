using System.Runtime.InteropServices;
using AutoShiftLockConfig.Models;

namespace AutoShiftLockConfig.Services;

/// <summary>
/// Captures the next rising-edge input across Keyboard (Win32 VK), XInput gamepads (slots 0-3 incl. LT/RT),
/// and DirectInput8 steering wheels/shifters/button boxes using the exact same token format as the C++ DLL.
/// </summary>
public sealed class UniversalInputCapture : IDisposable
{
    private const uint DirectInputVersion = 0x0800;
    private const uint Di8DevClassGameCtrl = 4;
    private const uint DiEdflAttachedOnly = 0x00000001;
    private const uint DisclBackground = 0x00000008;
    private const uint DisclNonExclusive = 0x00000002;
    public const uint DidfAbsAxis = 0x00000001;
    private const uint DidftButton = 0x0000000C;
    private const uint DidftPov = 0x00000010;
    private const uint DidftAnyInstance = 0x00FFFF00;
    private const uint DidftOptional = 0x80000000;
    private const byte TriggerThreshold = 128;

    private static readonly Guid IidIDirectInput8W = new("BF798031-483A-4DA2-AA99-5D64ED369700");

    private static readonly (ushort Mask, string Name)[] XInputDigitalButtons =
    [
        (0x1000, "A"),
        (0x2000, "B"),
        (0x4000, "X"),
        (0x8000, "Y"),
        (0x0100, "LB"),
        (0x0200, "RB"),
        (0x0020, "BACK"),
        (0x0010, "START"),
        (0x0040, "LS"),
        (0x0080, "RS"),
        (0x0001, "DPAD_UP"),
        (0x0002, "DPAD_DOWN"),
        (0x0004, "DPAD_LEFT"),
        (0x0008, "DPAD_RIGHT"),
    ];

    private sealed class DiDeviceState
    {
        public IntPtr Device;
        public string ProductGuid = string.Empty;
        public byte[] PrevButtons = new byte[128];
        public uint[] PrevPovs = [0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF];
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct XInputGamepad
    {
        public ushort Buttons;
        public byte LeftTrigger;
        public byte RightTrigger;
        public short ThumbLX;
        public short ThumbLY;
        public short ThumbRX;
        public short ThumbRY;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct XInputState
    {
        public uint PacketNumber;
        public XInputGamepad Gamepad;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct DiObjectDataFormat
    {
        public IntPtr GuidPtr;
        public uint Offset;
        public uint Type;
        public uint Flags;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct DiDataFormat
    {
        public uint Size;
        public uint ObjSize;
        public uint Flags;
        public uint DataSize;
        public uint NumObjs;
        public IntPtr Rgodf;
    }

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int EnumDevicesCallback(IntPtr deviceInstancePtr, IntPtr pvRef);

    [DllImport("user32.dll")]
    private static extern short GetAsyncKeyState(int vKey);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr GetModuleHandleW(string? lpModuleName);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr LoadLibraryW(string lpLibFileName);

    [DllImport("kernel32.dll")]
    private static extern bool FreeLibrary(IntPtr hModule);

    [DllImport("kernel32.dll", CharSet = CharSet.Ansi)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    [DllImport("dinput8.dll")]
    private static extern int DirectInput8Create(
        IntPtr hinst,
        uint dwVersion,
        ref Guid riidltf,
        out IntPtr ppvOut,
        IntPtr punkOuter);

    private delegate uint XInputGetStateDelegate(uint dwUserIndex, out XInputState pState);

    private IntPtr _xinputDll;
    private XInputGetStateDelegate? _xinputGetState;
    private IntPtr _dinput;
    private readonly List<DiDeviceState> _diDevices = [];
    private IntPtr _hwnd;

    private readonly bool[] _prevKeys = new bool[256];
    private readonly XInputState[] _prevXInput = new XInputState[4];
    private readonly bool[] _prevXInputValid = new bool[4];

    public UniversalInputCapture(IntPtr hwnd)
    {
        _hwnd = hwnd;
        InitXInput();
        InitDirectInput();
    }

    public async Task<BindingSpec?> CaptureNextAsync(CancellationToken ct)
    {
        // Snapshot currently held keys/buttons so already-held inputs don't trigger immediately
        SnapshotBaseline();

        // Wait briefly so the mouse click on the "Bind" button is released
        await Task.Delay(120, ct);
        SnapshotBaseline();

        while (!ct.IsCancellationRequested)
        {
            // ESC cancels binding capture
            if ((GetAsyncKeyState(0x1B) & 0x8000) != 0)
            {
                return null;
            }

            if (TryDetectKeyboard(out var kbSpec))
            {
                return kbSpec;
            }

            if (TryDetectXInput(out var xiSpec))
            {
                return xiSpec;
            }

            if (TryDetectDirectInput(out var diSpec))
            {
                return diSpec;
            }

            await Task.Delay(16, ct);
        }

        return null;
    }

    private unsafe void SnapshotBaseline()
    {
        for (int vk = 1; vk <= 254; vk++)
        {
            _prevKeys[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
        }

        if (_xinputGetState is not null)
        {
            for (uint slot = 0; slot < 4; slot++)
            {
                _prevXInputValid[slot] = _xinputGetState(slot, out _prevXInput[slot]) == 0;
            }
        }

        var stateBuf = stackalloc byte[144];
        foreach (var dev in _diDevices)
        {
            if (PollDevice(dev.Device, stateBuf))
            {
                for (int p = 0; p < 4; p++)
                {
                    dev.PrevPovs[p] = *(uint*)(stateBuf + p * 4);
                }
                for (int b = 0; b < 128; b++)
                {
                    dev.PrevButtons[b] = stateBuf[16 + b];
                }
            }
        }
    }

    private bool TryDetectKeyboard(out BindingSpec spec)
    {
        spec = BindingSpec.None;
        for (int vk = 1; vk <= 254; vk++)
        {
            // Skip mouse buttons, ESC, and generic modifier codes (prefer exact L/R Shift, Ctrl, Alt)
            if (vk is 1 or 2 or 4 or 5 or 6 or 0x1B or 0x10 or 0x11 or 0x12)
            {
                continue;
            }

            bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
            bool rising = down && !_prevKeys[vk];
            _prevKeys[vk] = down;
            if (rising)
            {
                spec = BindingSpec.Keyboard((uint)vk);
                return true;
            }
        }
        return false;
    }

    private bool TryDetectXInput(out BindingSpec spec)
    {
        spec = BindingSpec.None;
        if (_xinputGetState is null)
        {
            return false;
        }

        for (uint slot = 0; slot < 4; slot++)
        {
            if (_xinputGetState(slot, out var state) != 0)
            {
                _prevXInputValid[slot] = false;
                continue;
            }

            var prev = _prevXInputValid[slot] ? _prevXInput[slot].Gamepad : default;
            var curr = state.Gamepad;

            foreach (var (mask, name) in XInputDigitalButtons)
            {
                if ((curr.Buttons & mask) != 0 && (prev.Buttons & mask) == 0)
                {
                    _prevXInput[slot] = state;
                    _prevXInputValid[slot] = true;
                    spec = BindingSpec.XInput((byte)slot, name);
                    return true;
                }
            }

            if (curr.LeftTrigger >= TriggerThreshold && prev.LeftTrigger < TriggerThreshold)
            {
                _prevXInput[slot] = state;
                _prevXInputValid[slot] = true;
                spec = BindingSpec.XInput((byte)slot, "LT");
                return true;
            }

            if (curr.RightTrigger >= TriggerThreshold && prev.RightTrigger < TriggerThreshold)
            {
                _prevXInput[slot] = state;
                _prevXInputValid[slot] = true;
                spec = BindingSpec.XInput((byte)slot, "RT");
                return true;
            }

            _prevXInput[slot] = state;
            _prevXInputValid[slot] = true;
        }
        return false;
    }

    private unsafe bool TryDetectDirectInput(out BindingSpec spec)
    {
        spec = BindingSpec.None;
        var stateBuf = stackalloc byte[144];

        foreach (var dev in _diDevices)
        {
            if (!PollDevice(dev.Device, stateBuf))
            {
                continue;
            }

            for (int b = 0; b < 128; b++)
            {
                bool down = (stateBuf[16 + b] & 0x80) != 0;
                bool prevDown = (dev.PrevButtons[b] & 0x80) != 0;
                dev.PrevButtons[b] = stateBuf[16 + b];
                if (down && !prevDown)
                {
                    spec = BindingSpec.DirectInputButton(dev.ProductGuid, (uint)b);
                    return true;
                }
            }

            for (int p = 0; p < 4; p++)
            {
                uint pov = *(uint*)(stateBuf + p * 4);
                uint prevPov = dev.PrevPovs[p];
                dev.PrevPovs[p] = pov;
                if (TryEvalSinglePov(pov, out var dir) && !TryEvalSinglePov(prevPov, out _))
                {
                    spec = BindingSpec.DirectInputPov(dev.ProductGuid, (uint)p, dir);
                    return true;
                }
            }
        }
        return false;
    }

    private static bool TryEvalSinglePov(uint raw, out PovDirection dir)
    {
        dir = PovDirection.Up;
        if (raw == 0xFFFFFFFF || (raw & 0xFFFF) == 0xFFFF)
        {
            return false;
        }
        uint deg = raw % 36000;
        if (deg <= 4500 || deg >= 31500) { dir = PovDirection.Up; return true; }
        if (deg < 13500) { dir = PovDirection.Right; return true; }
        if (deg < 22500) { dir = PovDirection.Down; return true; }
        dir = PovDirection.Left;
        return true;
    }

    private void InitXInput()
    {
        foreach (var dll in new[] { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" })
        {
            _xinputDll = LoadLibraryW(dll);
            if (_xinputDll != IntPtr.Zero)
            {
                var proc = GetProcAddress(_xinputDll, "XInputGetState");
                if (proc != IntPtr.Zero)
                {
                    _xinputGetState = Marshal.GetDelegateForFunctionPointer<XInputGetStateDelegate>(proc);
                    return;
                }
                FreeLibrary(_xinputDll);
                _xinputDll = IntPtr.Zero;
            }
        }
    }

    private unsafe void InitDirectInput()
    {
        try
        {
            var hinst = GetModuleHandleW(null);
            var iid = IidIDirectInput8W;
            if (DirectInput8Create(hinst, DirectInputVersion, ref iid, out _dinput, IntPtr.Zero) != 0 || _dinput == IntPtr.Zero)
            {
                _dinput = IntPtr.Zero;
                return;
            }

            // Build a compact 144-byte data format: 4 POVs (bytes 0..15) + 128 buttons (bytes 16..143)
            var objFormats = new DiObjectDataFormat[132];
            for (int p = 0; p < 4; p++)
            {
                objFormats[p] = new DiObjectDataFormat
                {
                    GuidPtr = IntPtr.Zero,
                    Offset = (uint)(p * 4),
                    Type = DidftPov | DidftAnyInstance | DidftOptional,
                    Flags = 0,
                };
            }
            for (int b = 0; b < 128; b++)
            {
                objFormats[4 + b] = new DiObjectDataFormat
                {
                    GuidPtr = IntPtr.Zero,
                    Offset = (uint)(16 + b),
                    Type = DidftButton | DidftAnyInstance | DidftOptional,
                    Flags = 0,
                };
            }

            var pin = GCHandle.Alloc(objFormats, GCHandleType.Pinned);
            var dfPtr = Marshal.AllocHGlobal(Marshal.SizeOf<DiDataFormat>());
            try
            {
                var df = new DiDataFormat
                {
                    Size = (uint)Marshal.SizeOf<DiDataFormat>(),
                    ObjSize = (uint)Marshal.SizeOf<DiObjectDataFormat>(),
                    Flags = DidfAbsAxis,
                    DataSize = 144,
                    NumObjs = 132,
                    Rgodf = pin.AddrOfPinnedObject(),
                };
                Marshal.StructureToPtr(df, dfPtr, fDeleteOld: false);

                EnumDevicesCallback cb = (instPtr, _) =>
                {
                    if (instPtr == IntPtr.Zero) return 1;
                    // DIDEVICEINSTANCEW: offset 4 = guidInstance (16B), offset 20 = guidProduct (16B)
                    var guidInstance = Marshal.PtrToStructure<Guid>(instPtr + 4);
                    var guidProduct = Marshal.PtrToStructure<Guid>(instPtr + 20);

                    var diVtbl = *(IntPtr**)_dinput;
                    var createDevice = (delegate* unmanaged[Stdcall]<IntPtr, Guid*, IntPtr*, IntPtr, int>)diVtbl[3];
                    IntPtr dev = IntPtr.Zero;
                    if (createDevice(_dinput, &guidInstance, &dev, IntPtr.Zero) != 0 || dev == IntPtr.Zero)
                    {
                        return 1;
                    }

                    var devVtbl = *(IntPtr**)dev;
                    var setDataFormat = (delegate* unmanaged[Stdcall]<IntPtr, IntPtr, int>)devVtbl[11];
                    if (setDataFormat(dev, dfPtr) != 0)
                    {
                        var release = (delegate* unmanaged[Stdcall]<IntPtr, uint>)devVtbl[2];
                        release(dev);
                        return 1;
                    }

                    if (_hwnd != IntPtr.Zero)
                    {
                        var setCoop = (delegate* unmanaged[Stdcall]<IntPtr, IntPtr, uint, int>)devVtbl[13];
                        setCoop(dev, _hwnd, DisclBackground | DisclNonExclusive);
                    }

                    var acquire = (delegate* unmanaged[Stdcall]<IntPtr, int>)devVtbl[7];
                    acquire(dev);

                    _diDevices.Add(new DiDeviceState
                    {
                        Device = dev,
                        ProductGuid = guidProduct.ToString("D").ToUpperInvariant(),
                    });
                    return 1;
                };

                var vtbl = *(IntPtr**)_dinput;
                var enumDevices = (delegate* unmanaged[Stdcall]<IntPtr, uint, IntPtr, IntPtr, uint, int>)vtbl[4];
                enumDevices(_dinput, Di8DevClassGameCtrl, Marshal.GetFunctionPointerForDelegate(cb), IntPtr.Zero, DiEdflAttachedOnly);
                GC.KeepAlive(cb);
            }
            finally
            {
                Marshal.FreeHGlobal(dfPtr);
                pin.Free();
            }
        }
        catch
        {
            // DirectInput optional fallback: keyboard and XInput still work cleanly
        }
    }

    private static unsafe bool PollDevice(IntPtr dev, byte* stateBuf)
    {
        if (dev == IntPtr.Zero) return false;
        var vtbl = *(IntPtr**)dev;
        var poll = (delegate* unmanaged[Stdcall]<IntPtr, int>)vtbl[25];
        var acquire = (delegate* unmanaged[Stdcall]<IntPtr, int>)vtbl[7];
        var getState = (delegate* unmanaged[Stdcall]<IntPtr, uint, void*, int>)vtbl[9];

        if (poll(dev) < 0)
        {
            if (acquire(dev) < 0) return false;
            poll(dev);
        }

        if (getState(dev, 144, stateBuf) < 0)
        {
            if (acquire(dev) < 0 || getState(dev, 144, stateBuf) < 0)
            {
                return false;
            }
        }
        return true;
    }

    public unsafe void Dispose()
    {
        foreach (var d in _diDevices)
        {
            if (d.Device != IntPtr.Zero)
            {
                var vtbl = *(IntPtr**)d.Device;
                var unacquire = (delegate* unmanaged[Stdcall]<IntPtr, int>)vtbl[8];
                var release = (delegate* unmanaged[Stdcall]<IntPtr, uint>)vtbl[2];
                unacquire(d.Device);
                release(d.Device);
            }
        }
        _diDevices.Clear();

        if (_dinput != IntPtr.Zero)
        {
            var vtbl = *(IntPtr**)_dinput;
            var release = (delegate* unmanaged[Stdcall]<IntPtr, uint>)vtbl[2];
            release(_dinput);
            _dinput = IntPtr.Zero;
        }

        if (_xinputDll != IntPtr.Zero)
        {
            FreeLibrary(_xinputDll);
            _xinputDll = IntPtr.Zero;
        }
    }
}
