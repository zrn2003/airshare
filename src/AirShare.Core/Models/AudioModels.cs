namespace AirShare.Core.Models;

public sealed class AudioDevice
{
    public string DeviceId { get; set; } = string.Empty;
    public string Name { get; set; } = string.Empty;
    public string Type { get; set; } = "Unknown";
    public int LatencyMs { get; set; }
    public int Volume { get; set; } = 100;
    public bool IsConnected { get; set; } = true;
    public bool IsSelected { get; set; }
}

public sealed class AppSettings
{
    public int BufferSize { get; set; } = 512;
    public int MasterVolume { get; set; } = 100;
    public int SampleRate { get; set; } = 48000;
    public bool UseDarkTheme { get; set; }
}

public sealed class DeviceLatencyStat
{
    public string DeviceId { get; set; } = string.Empty;
    public string DeviceName { get; set; } = string.Empty;
    public int LatencyMs { get; set; }
    public int BufferFillPercent { get; set; }
}

public enum SharingState
{
    Idle,
    Starting,
    Active,
    Stopping,
    Error
}
