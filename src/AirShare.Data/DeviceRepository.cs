using AirShare.Core.Models;
using Microsoft.Data.Sqlite;

namespace AirShare.Data;

public sealed class DeviceRepository
{
    private readonly DatabaseInitializer _database;

    public DeviceRepository(DatabaseInitializer database)
    {
        _database = database;
    }

    public IReadOnlyList<AudioDevice> GetAll()
    {
        var devices = new List<AudioDevice>();
        using var connection = OpenConnection();
        using var command = connection.CreateCommand();
        command.CommandText =
            """
            SELECT device_id, device_name, device_type, latency_ms, volume
            FROM Devices
            ORDER BY device_name;
            """;

        using var reader = command.ExecuteReader();
        while (reader.Read())
        {
            devices.Add(new AudioDevice
            {
                DeviceId = reader.GetString(0),
                Name = reader.IsDBNull(1) ? string.Empty : reader.GetString(1),
                Type = reader.IsDBNull(2) ? "Unknown" : reader.GetString(2),
                LatencyMs = reader.GetInt32(3),
                Volume = reader.GetInt32(4),
                IsConnected = true
            });
        }

        return devices;
    }

    public void Upsert(AudioDevice device)
    {
        using var connection = OpenConnection();
        using var command = connection.CreateCommand();
        command.CommandText =
            """
            INSERT INTO Devices (device_id, device_name, device_type, latency_ms, volume)
            VALUES ($deviceId, $name, $type, $latency, $volume)
            ON CONFLICT(device_id) DO UPDATE SET
              device_name = excluded.device_name,
              device_type = excluded.device_type,
              latency_ms = excluded.latency_ms,
              volume = excluded.volume;
            """;
        command.Parameters.AddWithValue("$deviceId", device.DeviceId);
        command.Parameters.AddWithValue("$name", device.Name);
        command.Parameters.AddWithValue("$type", device.Type);
        command.Parameters.AddWithValue("$latency", device.LatencyMs);
        command.Parameters.AddWithValue("$volume", device.Volume);
        command.ExecuteNonQuery();
    }

    public void UpdateLatency(string deviceId, int latencyMs)
    {
        using var connection = OpenConnection();
        using var command = connection.CreateCommand();
        command.CommandText = "UPDATE Devices SET latency_ms = $latency WHERE device_id = $deviceId;";
        command.Parameters.AddWithValue("$latency", latencyMs);
        command.Parameters.AddWithValue("$deviceId", deviceId);
        command.ExecuteNonQuery();
    }

    public void UpdateVolume(string deviceId, int volume)
    {
        using var connection = OpenConnection();
        using var command = connection.CreateCommand();
        command.CommandText = "UPDATE Devices SET volume = $volume WHERE device_id = $deviceId;";
        command.Parameters.AddWithValue("$volume", volume);
        command.Parameters.AddWithValue("$deviceId", deviceId);
        command.ExecuteNonQuery();
    }

    private SqliteConnection OpenConnection()
    {
        var connection = new SqliteConnection($"Data Source={_database.DatabasePath}");
        connection.Open();
        return connection;
    }
}
