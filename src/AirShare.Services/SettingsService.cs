using AirShare.Core.Models;
using AirShare.Data;

namespace AirShare.Services;

public sealed class SettingsService
{
    private readonly SettingsRepository _settingsRepository;

    public SettingsService(SettingsRepository settingsRepository)
    {
        _settingsRepository = settingsRepository;
    }

    public AppSettings Get() => _settingsRepository.Get();

    public void Save(AppSettings settings) => _settingsRepository.Save(settings);
}
