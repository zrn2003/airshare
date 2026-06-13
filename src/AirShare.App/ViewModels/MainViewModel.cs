using System.Collections.ObjectModel;
using System.Windows;
using System.Windows.Threading;
using AirShare.Core.Models;
using AirShare.Services;
using AirShare.Services.Native;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace AirShare.App.ViewModels;

public partial class MainViewModel : ObservableObject
{
    private readonly DeviceService _deviceService;
    private readonly SharingService _sharingService;
    private readonly SettingsService _settingsService;
    private readonly Dispatcher _dispatcher;

    [ObservableProperty]
    private string _statusMessage = "Ready";

    [ObservableProperty]
    private bool _isSharing;

    [ObservableProperty]
    private AppPage _selectedPage = AppPage.Dashboard;

    [ObservableProperty]
    private int _connectedDeviceCount;

    [ObservableProperty]
    private int _masterVolume = 100;

    [ObservableProperty]
    private int _bufferSize = 512;

    [ObservableProperty]
    private bool _useDarkTheme;

    [ObservableProperty]
    private string? _errorMessage;

    public ObservableCollection<DeviceItemViewModel> Devices { get; } = new();
    public ObservableCollection<DeviceLatencyStat> ActiveLatencyStats { get; } = new();

    public MainViewModel(
        DeviceService deviceService,
        SharingService sharingService,
        SettingsService settingsService)
    {
        _deviceService = deviceService;
        _sharingService = sharingService;
        _settingsService = settingsService;
        _dispatcher = Application.Current.Dispatcher;

        _sharingService.StateChanged += (_, state) =>
        {
            _dispatcher.Invoke(() =>
            {
                IsSharing = state == SharingState.Active;
                StatusMessage = state switch
                {
                    SharingState.Active => "Audio sharing active",
                    SharingState.Starting => "Starting audio sharing...",
                    SharingState.Stopping => "Stopping audio sharing...",
                    SharingState.Error => _sharingService.LastError ?? "Sharing error",
                    _ => "Ready"
                };
                ErrorMessage = state == SharingState.Error ? _sharingService.LastError : null;
            });
        };

        _sharingService.StatsUpdated += (_, args) =>
        {
            _dispatcher.Invoke(() => UpdateLatencyStat(args));
        };

        _deviceService.DevicesChanged += (_, _) => _dispatcher.Invoke(LoadDevices);

        LoadSettings();
        LoadDevices();
    }

    [RelayCommand]
    private void RefreshDevices()
    {
        _deviceService.Refresh();
        StatusMessage = "Device list refreshed";
    }

    [RelayCommand]
    private void StartSharing()
    {
        ErrorMessage = null;
        var selected = Devices.Where(d => d.IsSelected).Select(d => d.ToModel()).ToArray();
        if (selected.Length == 0)
        {
            ErrorMessage = "Select at least one device.";
            return;
        }

        ActiveLatencyStats.Clear();
        foreach (var device in selected)
        {
            ActiveLatencyStats.Add(new DeviceLatencyStat
            {
                DeviceId = device.DeviceId,
                DeviceName = device.Name,
                LatencyMs = device.LatencyMs
            });
        }

        _sharingService.StartSharing(selected);
    }

    [RelayCommand]
    private void StopSharing()
    {
        _sharingService.StopSharing();
        ActiveLatencyStats.Clear();
    }

    [RelayCommand]
    private void Navigate(string pageName)
    {
        if (Enum.TryParse<AppPage>(pageName, out var page))
        {
            SelectedPage = page;
        }
    }

    [RelayCommand]
    private void SaveSettings()
    {
        _settingsService.Save(new AppSettings
        {
            BufferSize = BufferSize,
            MasterVolume = MasterVolume,
            SampleRate = 48000,
            UseDarkTheme = UseDarkTheme
        });

        if (_sharingService.IsSharing)
        {
            _sharingService.SetMasterVolume(MasterVolume);
        }

        StatusMessage = "Settings saved";
    }

    partial void OnMasterVolumeChanged(int value)
    {
        if (_sharingService.IsSharing)
        {
            _sharingService.SetMasterVolume(value);
        }
    }

    partial void OnUseDarkThemeChanged(bool value)
    {
        ApplyTheme(value);
    }

    private void LoadSettings()
    {
        var settings = _settingsService.Get();
        BufferSize = settings.BufferSize;
        MasterVolume = settings.MasterVolume;
        UseDarkTheme = settings.UseDarkTheme;
        ApplyTheme(UseDarkTheme);
    }

    private void LoadDevices()
    {
        var selectedIds = Devices.Where(d => d.IsSelected).Select(d => d.DeviceId).ToHashSet();
        Devices.Clear();

        foreach (var device in _deviceService.GetDevices())
        {
            Devices.Add(new DeviceItemViewModel(device)
            {
                IsSelected = selectedIds.Contains(device.DeviceId)
            });
        }

        ConnectedDeviceCount = Devices.Count(d => d.IsConnected);
    }

    private void UpdateLatencyStat(NativeStatsEventArgs args)
    {
        var stat = ActiveLatencyStats.FirstOrDefault(s => s.DeviceId == args.DeviceId);
        if (stat is null)
        {
            var device = Devices.FirstOrDefault(d => d.DeviceId == args.DeviceId);
            stat = new DeviceLatencyStat
            {
                DeviceId = args.DeviceId,
                DeviceName = device?.Name ?? args.DeviceId
            };
            ActiveLatencyStats.Add(stat);
        }

        stat.LatencyMs = args.LatencyMs;
        stat.BufferFillPercent = args.BufferFillPercent;
        OnPropertyChanged(nameof(ActiveLatencyStats));
    }

    private static void ApplyTheme(bool dark)
    {
        if (dark)
        {
            SetBrush("BackgroundBrush", System.Windows.Media.Color.FromRgb(11, 19, 38));
            SetBrush("SurfaceBrush", System.Windows.Media.Color.FromRgb(11, 19, 38));
            SetBrush("CardBrush", System.Windows.Media.Color.FromRgb(23, 31, 51));
            SetBrush("TextBrush", System.Windows.Media.Color.FromRgb(218, 226, 253));
            SetBrush("MutedBrush", System.Windows.Media.Color.FromRgb(187, 202, 191));
            SetBrush("SurfaceContainerLowBrush", System.Windows.Media.Color.FromRgb(19, 27, 46));
            SetBrush("SurfaceContainerHighBrush", System.Windows.Media.Color.FromRgb(34, 42, 61));
        }
        else
        {
            SetBrush("BackgroundBrush", System.Windows.Media.Color.FromRgb(249, 249, 249));
            SetBrush("SurfaceBrush", System.Windows.Media.Color.FromRgb(249, 249, 249));
            SetBrush("CardBrush", System.Windows.Media.Color.FromRgb(255, 255, 255));
            SetBrush("TextBrush", System.Windows.Media.Color.FromRgb(26, 28, 28));
            SetBrush("MutedBrush", System.Windows.Media.Color.FromRgb(64, 71, 82));
            SetBrush("SurfaceContainerLowBrush", System.Windows.Media.Color.FromRgb(243, 243, 243));
            SetBrush("SurfaceContainerHighBrush", System.Windows.Media.Color.FromRgb(232, 232, 232));
        }
    }

    private static void SetBrush(string key, System.Windows.Media.Color color)
    {
        Application.Current.Resources[key] = new System.Windows.Media.SolidColorBrush(color);
    }
}

public partial class DeviceItemViewModel : ObservableObject
{
    public DeviceItemViewModel(AudioDevice device)
    {
        DeviceId = device.DeviceId;
        Name = device.Name;
        Type = device.Type;
        LatencyMs = device.LatencyMs;
        Volume = device.Volume;
        IsConnected = device.IsConnected;
    }

    public string DeviceId { get; }
    public string Name { get; }
    public string Type { get; }

    [ObservableProperty]
    private bool _isSelected;

    [ObservableProperty]
    private int _latencyMs;

    [ObservableProperty]
    private int _volume;

    [ObservableProperty]
    private bool _isConnected;

    public AudioDevice ToModel() => new()
    {
        DeviceId = DeviceId,
        Name = Name,
        Type = Type,
        LatencyMs = LatencyMs,
        Volume = Volume,
        IsConnected = IsConnected,
        IsSelected = IsSelected
    };
}
