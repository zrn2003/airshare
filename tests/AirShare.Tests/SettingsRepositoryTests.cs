using AirShare.Core.Models;
using AirShare.Data;
using Microsoft.Data.Sqlite;

namespace AirShare.Tests;

public class SettingsRepositoryTests : IDisposable
{
    private readonly string _tempDirectory;
    private readonly SettingsRepository _repository;

    public SettingsRepositoryTests()
    {
        _tempDirectory = Path.Combine(Path.GetTempPath(), "AirShareTests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(_tempDirectory);

        var database = new DatabaseInitializer(Path.Combine(_tempDirectory, "airshare.db"));
        database.Initialize();
        _repository = new SettingsRepository(database);
    }

    [Fact]
    public void Get_ReturnsDefaultSettings()
    {
        var settings = _repository.Get();

        Assert.Equal(512, settings.BufferSize);
        Assert.Equal(100, settings.MasterVolume);
        Assert.Equal(48000, settings.SampleRate);
    }

    [Fact]
    public void Save_PersistsUpdatedSettings()
    {
        _repository.Save(new AppSettings
        {
            BufferSize = 1024,
            MasterVolume = 75,
            SampleRate = 48000,
            UseDarkTheme = true
        });

        var settings = _repository.Get();

        Assert.Equal(1024, settings.BufferSize);
        Assert.Equal(75, settings.MasterVolume);
        Assert.True(settings.UseDarkTheme);
    }

    public void Dispose()
    {
        SqliteConnection.ClearAllPools();
        if (Directory.Exists(_tempDirectory))
        {
            try
            {
                Directory.Delete(_tempDirectory, recursive: true);
            }
            catch (IOException)
            {
                // Best-effort cleanup for locked test database files.
            }
        }
    }
}
