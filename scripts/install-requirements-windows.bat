@echo off
echo Starting download of Visual Studio Build Tools...
powershell -Command "Invoke-WebRequest -Uri https://aka.ms/vs/17/release/vs_buildtools.exe -OutFile vs_buildtools.exe"
echo Download complete.
echo Starting installation of Visual Studio Build Tools...
powershell -Command "Start-Process -Wait -FilePath .\vs_buildtools.exe -ArgumentList '--quiet --wait --norestart --nocache --installPath %GITHUB_WORKSPACE%\BuildTools --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --includeOptional'"
echo Installation complete.
echo Setting up Visual Studio environment...
call "%GITHUB_WORKSPACE%\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
echo Environment setup complete.
