using AirShare.Core;
using AirShare.Core.Models;
using AirShare.Data;
using AirShare.Services.Native;

namespace AirShare.Services;

public sealed class SharingService
{
    private readonly AudioEngineNative _audioEngine;
    private readonly DeviceService _deviceService;
    private readonly SettingsRepository _settingsRepository;

    public SharingService(
        AudioEngineNative audioEngine,
        DeviceService deviceService,
        SettingsRepository settingsRepository)
    {
        _audioEngine = audioEngine;
        _deviceService = deviceService;
        _settingsRepository = settingsRepository;
        _audioEngine.StatsUpdated += (_, args) =>
        {
            StatsUpdated?.Invoke(this, args);
        };
    }

    public SharingState State { get; private set; } = SharingState.Idle;
    public string? LastError { get; private set; }

    public event EventHandler<SharingState>? StateChanged;
    public event EventHandler<NativeStatsEventArgs>? StatsUpdated;

    public bool IsSharing => State == SharingState.Active;

    public int StartSharing(IEnumerable<AudioDevice> selectedDevices)
    {
        var devices = selectedDevices.Where(d => d.IsSelected && d.IsConnected).ToArray();
        if (devices.Length == 0)
        {
            LastError = "Select at least one connected output device.";
            SetState(SharingState.Error);
            return AirShareNativeErrors.InvalidArg;
        }

        if (devices.Length > 8)
        {
            LastError = "AirShare supports up to 8 devices.";
            SetState(SharingState.Error);
            return AirShareNativeErrors.InvalidArg;
        }

        SetState(SharingState.Starting);

        foreach (var device in devices)
        {
            _deviceService.UpdateLatency(device.DeviceId, device.LatencyMs);
            _audioEngine.SetDeviceVolume(device.DeviceId, device.Volume / 100f);
            _deviceService.PersistDiscoveredDevices();
        }

        var settings = _settingsRepository.Get();
        _audioEngine.SetMasterVolume(settings.MasterVolume / 100f);

        var result = _audioEngine.StartSharing(
            devices.Select(d => d.DeviceId),
            settings.BufferSize);

        if (result != AirShareNativeErrors.Ok)
        {
            LastError = DescribeError(result);
            SetState(SharingState.Error);
            return result;
        }

        LastError = null;
        SetState(SharingState.Active);
        return AirShareNativeErrors.Ok;
    }

    public int StopSharing()
    {
        SetState(SharingState.Stopping);
        var result = _audioEngine.StopSharing();
        SetState(SharingState.Idle);
        return result;
    }

    public void SetMasterVolume(int volume)
    {
        var settings = _settingsRepository.Get();
        settings.MasterVolume = volume;
        _settingsRepository.Save(settings);
        _audioEngine.SetMasterVolume(volume / 100f);
    }

    private void SetState(SharingState state)
    {
        State = state;
        StateChanged?.Invoke(this, state);
    }

    private static string DescribeError(int code) => code switch
    {
        AirShareNativeErrors.NotInitialized => "Audio engine is not initialized.",
        AirShareNativeErrors.AlreadyRunning => "Audio sharing is already active.",
        AirShareNativeErrors.InvalidArg => "Invalid sharing configuration.",
        AirShareNativeErrors.Device => "Unable to open one or more audio devices.",
        AirShareNativeErrors.Engine => "Audio engine failed to start capture.",
        _ => "Unknown error while starting audio sharing."
    };
}
