using System.Windows;
using AirShare.App.ViewModels;
using AirShare.App.Views;
using AirShare.Core;
using AirShare.Data;
using AirShare.Services;
using AirShare.Services.Native;
using Microsoft.Extensions.DependencyInjection;

namespace AirShare.App;

public partial class App : Application
{
    private ServiceProvider? _services;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        var services = new ServiceCollection();
        services.AddSingleton<DatabaseInitializer>();
        services.AddSingleton<DeviceRepository>();
        services.AddSingleton<SettingsRepository>();
        services.AddSingleton<AudioEngineNative>();
        services.AddSingleton<DeviceService>();
        services.AddSingleton<SettingsService>();
        services.AddSingleton<SharingService>();
        services.AddSingleton<MainViewModel>();
        services.AddSingleton<MainWindow>();

        _services = services.BuildServiceProvider();

        var database = _services.GetRequiredService<DatabaseInitializer>();
        database.Initialize();

        var audioEngine = _services.GetRequiredService<AudioEngineNative>();
        if (audioEngine.Initialize() != AirShareNativeErrors.Ok)
        {
            MessageBox.Show(
                "Failed to initialize the AirShare audio engine.",
                "AirShare",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
            Shutdown(1);
            return;
        }

        var mainWindow = _services.GetRequiredService<MainWindow>();
        mainWindow.Show();
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _services?.GetService<AudioEngineNative>()?.Shutdown();
        _services?.Dispose();
        base.OnExit(e);
    }
}
