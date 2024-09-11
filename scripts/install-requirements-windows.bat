@echo off
powershell -Command "Invoke-WebRequest -Uri https://aka.ms/vs/17/release/vs_buildtools.exe -OutFile vs_buildtools.exe"
powershell -Command "Start-Process -Wait -FilePath .\vs_buildtools.exe -ArgumentList '--quiet --wait --norestart --nocache --installPath C:\BuildTools --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --includeOptional'"
call "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
