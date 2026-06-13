using Microsoft.Data.Sqlite;

namespace AirShare.Data;

public sealed class DatabaseInitializer
{
    public DatabaseInitializer(string? databasePath = null)
    {
        DatabasePath = databasePath ??
            Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "AirShare",
                "airshare.db");
    }

    public string DatabasePath { get; }

    public void Initialize()
    {
        var directory = Path.GetDirectoryName(DatabasePath)!;
        Directory.CreateDirectory(directory);

        using var connection = new SqliteConnection($"Data Source={DatabasePath}");
        connection.Open();

        using var command = connection.CreateCommand();
        command.CommandText =
            """
            CREATE TABLE IF NOT EXISTS Devices (
              id INTEGER PRIMARY KEY,
              device_id TEXT UNIQUE NOT NULL,
              device_name TEXT,
              device_type TEXT,
              latency_ms INTEGER DEFAULT 0,
              volume INTEGER DEFAULT 100
            );

            CREATE TABLE IF NOT EXISTS Settings (
              id INTEGER PRIMARY KEY CHECK (id = 1),
              buffer_size INTEGER DEFAULT 512,
              master_volume INTEGER DEFAULT 100,
              sample_rate INTEGER DEFAULT 48000,
              use_dark_theme INTEGER DEFAULT 0
            );

            INSERT OR IGNORE INTO Settings (id, buffer_size, master_volume, sample_rate, use_dark_theme)
            VALUES (1, 512, 100, 48000, 0);
            """;
        command.ExecuteNonQuery();
    }
}
