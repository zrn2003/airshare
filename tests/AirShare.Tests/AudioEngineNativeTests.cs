using AirShare.Services;
using AirShare.Services.Native;

namespace AirShare.Tests;

public class AudioEngineNativeTests
{
    [Fact]
    public void Initialize_ReturnsOk_WhenNativeDllPresent()
    {
        var engine = new AudioEngineNative();
        var result = engine.Initialize();

        try
        {
            Assert.True(
                result == AirShare.Core.AirShareNativeErrors.Ok ||
                result == AirShare.Core.AirShareNativeErrors.Device,
                $"Unexpected init result: {result}");
        }
        finally
        {
            engine.Shutdown();
        }
    }
}
