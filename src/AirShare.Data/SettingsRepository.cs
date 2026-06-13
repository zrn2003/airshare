using AirShare.Core.Models;
using Microsoft.Data.Sqlite;

namespace AirShare.Data;

public sealed class SettingsRepository
{
    private readonly DatabaseInitializer _database;

    public SettingsRepository(DatabaseInitializer database)
    {
        _database = database;
    }

    public AppSettings Get()
    {
        using var connection = OpenConnection();
        using var command = connection.CreateCommand();
        command.CommandText =
            """
            SELECT buffer_size, master_volume, sample_rate, use_dark_theme
            FROM Settings
            WHERE id = 1;
            """;

        using var reader = command.ExecuteReader();
        if (!reader.Read())
        {
            return new AppSettings();
        }

        return new AppSettings
        {
            BufferSize = Math.Max(2048, reader.GetInt32(0)),
            MasterVolume = reader.GetInt32(1),
            SampleRate = Math.Max(48000, reader.GetInt32(2)),
            UseDarkTheme = reader.GetInt32(3) == 1
        };
    }

    public void Save(AppSettings settings)
    {
        using var connection = OpenConnection();
        using var command = connection.CreateCommand();
        command.CommandText =
            """
            UPDATE Settings
            SET buffer_size = $bufferSize,
                master_volume = $masterVolume,
                sample_rate = $sampleRate,
                use_dark_theme = $darkTheme
            WHERE id = 1;
            """;
        command.Parameters.AddWithValue("$bufferSize", settings.BufferSize);
        command.Parameters.AddWithValue("$masterVolume", settings.MasterVolume);
        command.Parameters.AddWithValue("$sampleRate", settings.SampleRate);
        command.Parameters.AddWithValue("$darkTheme", settings.UseDarkTheme ? 1 : 0);
        command.ExecuteNonQuery();
    }

    private SqliteConnection OpenConnection()
    {
        var connection = new SqliteConnection($"Data Source={_database.DatabasePath}");
        connection.Open();
        return connection;
    }
}
