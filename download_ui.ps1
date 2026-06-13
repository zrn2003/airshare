$ErrorActionPreference = "Stop"
$outDir = "src\AirShare.App\wwwroot"
if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir | Out-Null
}

Write-Host "Downloading AirShare Dashboard..."
Invoke-WebRequest -Uri "https://contribution.usercontent.google.com/download?c=CgthaWRhX2NvZGVmeBJ8Eh1hcHBfY29tcGFuaW9uX2dlbmVyYXRlZF9maWxlcxpbCiVodG1sX2FjYmVjOTViMDQ0NDQ1ZmY5ZTRjMGFiNzY5ZGEzOTBhEgsSBxCemZeYnhsYAZIBJAoKcHJvamVjdF9pZBIWQhQxNTQwMTg2NjYwMTIxMzI2MjU5OQ&filename=&opi=89354086" -OutFile "$outDir\dashboard.html"

Write-Host "Downloading Device Manager..."
Invoke-WebRequest -Uri "https://contribution.usercontent.google.com/download?c=CgthaWRhX2NvZGVmeBJ8Eh1hcHBfY29tcGFuaW9uX2dlbmVyYXRlZF9maWxlcxpbCiVodG1sX2E3NGUxM2Q5ODljOTRiMWJiYzYwMGMwYWM3YzJkMTI4EgsSBxCemZeYnhsYAZIBJAoKcHJvamVjdF9pZBIWQhQxNTQwMTg2NjYwMTIxMzI2MjU5OQ&filename=&opi=89354086" -OutFile "$outDir\devices.html"

Write-Host "Downloading Settings..."
Invoke-WebRequest -Uri "https://contribution.usercontent.google.com/download?c=CgthaWRhX2NvZGVmeBJ8Eh1hcHBfY29tcGFuaW9uX2dlbmVyYXRlZF9maWxlcxpbCiVodG1sX2VhOTdlNTlmOGFkOTRmZTNhNjg4YmJiMWNlOGEzOGUzEgsSBxCemZeYnhsYAZIBJAoKcHJvamVjdF9pZBIWQhQxNTQwMTg2NjYwMTIxMzI2MjU5OQ&filename=&opi=89354086" -OutFile "$outDir\settings.html"

Write-Host "Downloading Analytics..."
Invoke-WebRequest -Uri "https://contribution.usercontent.google.com/download?c=CgthaWRhX2NvZGVmeBJ8Eh1hcHBfY29tcGFuaW9uX2dlbmVyYXRlZF9maWxlcxpbCiVodG1sXzAwYjM2MWJjYzEwMTRhZTdiOTExM2I4MTg0Y2Q4NTU3EgsSBxCemZeYnhsYAZIBJAoKcHJvamVjdF9pZBIWQhQxNTQwMTg2NjYwMTIxMzI2MjU5OQ&filename=&opi=89354086" -OutFile "$outDir\analytics.html"

Write-Host "UI Files downloaded successfully to $outDir!"
