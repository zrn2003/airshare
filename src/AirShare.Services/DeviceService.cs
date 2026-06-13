using AirShare.Core.Models;
using AirShare.Data;
using AirShare.Services.Native;

namespace AirShare.Services;

public sealed class DeviceService
{
    private readonly AudioEngineNative _audioEngine;
    private readonly DeviceRepository _deviceRepository;

    public DeviceService(AudioEngineNative audioEngine, DeviceRepository deviceRepository)
    {
        _audioEngine = audioEngine;
        _deviceRepository = deviceRepository;
        _audioEngine.DeviceChanged += (_, _) => DevicesChanged?.Invoke(this, EventArgs.Empty);
    }

    public event EventHandler? DevicesChanged;

    public IReadOnlyList<AudioDevice> GetDevices()
    {
        _audioEngine.RefreshDevices();
        var nativeDevices = _audioEngine.EnumerateDevices();
        var savedDevices = _deviceRepository.GetAll().ToDictionary(d => d.DeviceId);

        return nativeDevices
            .Select(native =>
            {
                savedDevices.TryGetValue(native.Id, out var saved);
                return new AudioDevice
                {
                    DeviceId = native.Id,
                    Name = string.IsNullOrWhiteSpace(native.Name) ? "Unknown Device" : native.Name,
                    Type = string.IsNullOrWhiteSpace(native.Type) ? "Unknown" : native.Type,
                    LatencyMs = native.LatencyMs > 0 ? native.LatencyMs : (saved?.LatencyMs ?? 0),
                    Volume = native.HardwareVolume,
                    IsConnected = native.IsConnected == 1
                };
            })
            .OrderBy(d => d.Name)
            .ToArray();
    }

    public void Refresh()
    {
        _audioEngine.RefreshDevices();
        PersistDiscoveredDevices();
        DevicesChanged?.Invoke(this, EventArgs.Empty);
    }

    public void PersistDiscoveredDevices()
    {
        foreach (var device in GetDevices())
        {
            _deviceRepository.Upsert(device);
        }
    }

    public void UpdateVolume(string deviceId, int volume)
    {
        _deviceRepository.UpdateVolume(deviceId, volume);
        _audioEngine.SetDeviceVolume(deviceId, volume / 100f);
    }

    public void UpdateLatency(string deviceId, int latencyMs)
    {
        _deviceRepository.UpdateLatency(deviceId, latencyMs);
        _audioEngine.SetDeviceLatencyOffset(deviceId, latencyMs);
    }
}
