using System.Runtime.InteropServices;
using System.Text.Json;
using System.Linq;
using System.Windows;
using AirShare.Services;
using AirShare.Core.Models;
using System.Collections.Generic;

namespace AirShare.App;

[ClassInterface(ClassInterfaceType.AutoDual)]
[ComVisible(true)]
public class ScriptBridge
{
    private readonly SharingService _sharingService;
    private readonly DeviceService _deviceService;

    public ScriptBridge(SharingService sharingService, DeviceService deviceService)
    {
        _sharingService = sharingService;
        _deviceService = deviceService;
    }

    public string GetDevices()
    {
        return Application.Current.Dispatcher.Invoke(() =>
        {
            var devices = _deviceService.GetDevices();
            return JsonSerializer.Serialize(devices);
        });
    }

    public int StartSharing(string deviceIdsCsv)
    {
        return Application.Current.Dispatcher.Invoke(() =>
        {
            var deviceIds = deviceIdsCsv.Split(new[] { ',' }, System.StringSplitOptions.RemoveEmptyEntries);
            var devices = _deviceService.GetDevices().Where(d => deviceIds.Contains(d.DeviceId)).ToList();
            
            foreach (var d in devices)
            {
                d.IsSelected = true;
            }

            return _sharingService.StartSharing(devices);
        });
    }

    public string GetLastError()
    {
        return Application.Current.Dispatcher.Invoke(() =>
        {
            return _sharingService.LastError ?? "Unknown error";
        });
    }

    public int StopSharing()
    {
        return Application.Current.Dispatcher.Invoke(() =>
        {
            return _sharingService.StopSharing();
        });
    }

    public void RefreshDevices()
    {
        Application.Current.Dispatcher.Invoke(() =>
        {
            _deviceService.Refresh();
        });
    }

    public void SetDeviceVolume(string deviceId, int volume)
    {
        Application.Current.Dispatcher.Invoke(() =>
        {
            _deviceService.UpdateVolume(deviceId, volume);
        });
    }

    public void SetMasterVolume(int volume)
    {
        Application.Current.Dispatcher.Invoke(() =>
        {
            _sharingService.SetMasterVolume(volume);
        });
    }
}
