param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path

$MsBuild = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
if (-not (Test-Path $MsBuild)) {
    $MsBuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
}

Write-Host "Building native audio engine ($Configuration|x64)..."
& $MsBuild "$Root\native\AirShare.AudioEngine\AirShare.AudioEngine.vcxproj" /p:Configuration=$Configuration /p:Platform=x64 /v:m

Write-Host "Building .NET solution ($Configuration)..."
dotnet build "$Root\AirShare.sln" -c $Configuration

Write-Host "Done."
