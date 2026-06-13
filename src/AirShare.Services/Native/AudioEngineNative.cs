using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using NAudio.CoreAudioApi;
using NAudio.Wave;
using NAudio.Wave.SampleProviders;
using AirShare.Core;

namespace AirShare.Services.Native;

public class NativeDeviceInfo
{
    public string Id { get; set; } = "";
    public string Name { get; set; } = "";
    public string Type { get; set; } = "";
    public int LatencyMs { get; set; }
    public int HardwareVolume { get; set; }
    public int IsConnected { get; set; }
}

public sealed class NativeDeviceEventArgs : EventArgs
{
    public NativeDeviceEventArgs(NativeDeviceInfo device, bool added) 
    { 
        Device = device; 
        Added = added; 
    }
    public NativeDeviceInfo Device { get; }
    public bool Added { get; }
}

public sealed class NativeStatsEventArgs : EventArgs
{
    public NativeStatsEventArgs(string deviceId, int latencyMs, int bufferFillPercent) 
    { 
        DeviceId = deviceId; 
        LatencyMs = latencyMs; 
        BufferFillPercent = bufferFillPercent; 
    }
    public string DeviceId { get; }
    public int LatencyMs { get; }
    public int BufferFillPercent { get; }
}

public sealed class AudioEngineNative : IDisposable
{
    public event EventHandler<NativeDeviceEventArgs>? DeviceChanged;
    public event EventHandler<NativeStatsEventArgs>? StatsUpdated;

    private WasapiLoopbackCapture? _capture;
    private readonly Dictionary<string, WasapiOut> _outputs = new();
    private readonly Dictionary<string, BufferedWaveProvider> _buffers = new();
    private readonly Dictionary<string, MediaFoundationResampler> _resamplers = new();
    private readonly Dictionary<string, VolumeSampleProvider> _volumeProviders = new();
    
    private float _masterVolume = 1.0f;
    private readonly Dictionary<string, float> _deviceVolumes = new();
    private readonly Dictionary<string, int> _latencyOffsets = new();
    
    private bool _isSharing;
    private readonly object _lock = new();
    private Thread? _statsThread;

    public int Initialize() => AirShareNativeErrors.Ok;
    public void Shutdown() => StopSharing();
    public void Dispose() => Shutdown();

    public IReadOnlyList<NativeDeviceInfo> EnumerateDevices(int maxCount = 32)
    {
        var enumerator = new MMDeviceEnumerator();
        var devices = enumerator.EnumerateAudioEndPoints(DataFlow.Render, DeviceState.Active);
        var list = new List<NativeDeviceInfo>();
        foreach (var d in devices)
        {
            var idLower = d.ID.ToLowerInvariant();
            var nameLower = d.FriendlyName.ToLowerInvariant();
            bool isBluetooth = idLower.Contains("bth") || idLower.Contains("bluetooth") || nameLower.Contains("bluetooth") || nameLower.Contains("airpods");
            int estimatedLatency = isBluetooth ? 8 : 0;

            int hwVolume = 100;
            try { hwVolume = (int)(d.AudioEndpointVolume.MasterVolumeLevelScalar * 100); } catch {}

            list.Add(new NativeDeviceInfo
            {
                Id = d.ID,
                Name = d.FriendlyName,
                Type = d.DataFlow.ToString(),
                LatencyMs = estimatedLatency,
                HardwareVolume = hwVolume,
                IsConnected = 1
            });
        }
        return list;
    }

    public int RefreshDevices() => AirShareNativeErrors.Ok;

    public int StartSharing(IEnumerable<string> deviceIds, int bufferSizeFrames)
    {
        lock (_lock)
        {
            if (_isSharing) return AirShareNativeErrors.AlreadyRunning;

            try
            {
                _capture = new WasapiLoopbackCapture();
                var format = _capture.WaveFormat; 

                var enumerator = new MMDeviceEnumerator();
                var endpoints = enumerator.EnumerateAudioEndPoints(DataFlow.Render, DeviceState.Active);
                var defaultEndpoint = enumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);

                // Find the maximum latency among all selected endpoints to use as the baseline
                int maxLatency = 0;
                foreach (var id in deviceIds)
                {
                    if (id == defaultEndpoint.ID) continue;
                    int l = _latencyOffsets.TryGetValue(id, out var v) ? v : 0;
                    if (l > maxLatency) maxLatency = l;
                }

                foreach (var id in deviceIds)
                {
                    // Prevent infinite feedback loop: DO NOT render to the device we are capturing from!
                    if (id == defaultEndpoint.ID) 
                    {
                        continue;
                    }
                    var endpoint = endpoints.FirstOrDefault(d => d.ID == id);
                    if (endpoint == null) continue;

                    var wasapiOut = new WasapiOut(endpoint, AudioClientShareMode.Shared, true, 50);
                    
                    var buffer = new BufferedWaveProvider(format)
                    {
                        BufferDuration = TimeSpan.FromSeconds(2),
                        DiscardOnBufferOverflow = true
                    };
                    _buffers[id] = buffer;

                    // Initial padding to sync latency across devices + 100ms base padding to prevent starvation/crackling
                    int deviceLatency = _latencyOffsets.TryGetValue(id, out var l) ? l : 0;
                    int offsetMs = maxLatency - deviceLatency + 100;
                    if (offsetMs > 0)
                    {
                        int bytesToDelay = (int)((offsetMs / 1000.0) * format.AverageBytesPerSecond);
                        bytesToDelay -= bytesToDelay % format.BlockAlign;
                        if (bytesToDelay > 0)
                        {
                            buffer.AddSamples(new byte[bytesToDelay], 0, bytesToDelay);
                        }
                    }

                    var volumeProvider = new VolumeSampleProvider(buffer.ToSampleProvider())
                    {
                        Volume = 1.0f
                    };
                    _volumeProviders[id] = volumeProvider;

                    // NAudio gracefully handles sample rate conversion using MediaFoundationResampler
                    var resampler = new MediaFoundationResampler(volumeProvider.ToWaveProvider(), wasapiOut.OutputWaveFormat)
                    {
                        ResamplerQuality = 60
                    };
                    _resamplers[id] = resampler;

                    wasapiOut.Init(resampler);
                    _outputs[id] = wasapiOut;
                }

                _capture.DataAvailable += (s, e) =>
                {
                    lock (_lock)
                    {
                        if (!_isSharing) return;
                        foreach (var buffer in _buffers.Values)
                        {
                            buffer.AddSamples(e.Buffer, 0, e.BytesRecorded);
                        }
                    }
                };

                foreach (var output in _outputs.Values) output.Play();
                _capture.StartRecording();

                _isSharing = true;
                
                _statsThread = new Thread(StatsLoop) { IsBackground = true };
                _statsThread.Start();

                return AirShareNativeErrors.Ok;
            }
            catch (Exception)
            {
                StopSharing();
                return AirShareNativeErrors.Engine;
            }
        }
    }

    private void StatsLoop()
    {
        while (_isSharing)
        {
            lock (_lock)
            {
                foreach (var kvp in _buffers)
                {
                    var buffer = kvp.Value;
                    int fillPercent = (int)((buffer.BufferedDuration.TotalMilliseconds / buffer.BufferDuration.TotalMilliseconds) * 100);
                    StatsUpdated?.Invoke(this, new NativeStatsEventArgs(kvp.Key, 0, fillPercent));
                }
            }
            Thread.Sleep(500);
        }
    }

    public int StopSharing()
    {
        lock (_lock)
        {
            if (!_isSharing) return AirShareNativeErrors.Ok;
            _isSharing = false;

            if (_capture != null)
            {
                _capture.StopRecording();
                _capture.Dispose();
                _capture = null;
            }

            foreach (var output in _outputs.Values)
            {
                output.Stop();
                output.Dispose();
            }

            _outputs.Clear();
            _buffers.Clear();
            _resamplers.Clear();
            _volumeProviders.Clear();
            return AirShareNativeErrors.Ok;
        }
    }

    public bool IsSharing() => _isSharing;

    public int SetMasterVolume(float gain)
    {
        lock (_lock)
        {
            _masterVolume = gain;
            return AirShareNativeErrors.Ok;
        }
    }

    public int SetDeviceVolume(string deviceId, float gain)
    {
        try
        {
            var enumerator = new MMDeviceEnumerator();
            var endpoint = enumerator.EnumerateAudioEndPoints(DataFlow.Render, DeviceState.Active).FirstOrDefault(d => d.ID == deviceId);
            if (endpoint != null)
            {
                endpoint.AudioEndpointVolume.MasterVolumeLevelScalar = gain;
            }
            return AirShareNativeErrors.Ok;
        }
        catch
        {
            return AirShareNativeErrors.Engine;
        }
    }

    private void UpdateVolumes()
    {
        // Deprecated: Hardware volume is now used exclusively via SetDeviceVolume
    }

    public int GetDeviceLatency(string deviceId, out int latencyMs) 
    { 
        latencyMs = 0; 
        return AirShareNativeErrors.Ok; 
    }

    public int SetDeviceLatencyOffset(string deviceId, int latencyMs) 
    { 
        lock (_lock)
        {
            _latencyOffsets[deviceId] = latencyMs;
        }
        return AirShareNativeErrors.Ok; 
    }

    public string GetVersion() => "2.0.0-naudio";
}
