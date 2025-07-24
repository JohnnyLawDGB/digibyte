# DigiByte Windows Native Build Script
# Run this in Windows PowerShell, not WSL

Write-Host "DigiByte Windows Native Build Setup" -ForegroundColor Green
Write-Host "===================================" -ForegroundColor Green

# Check if running in Windows (not WSL)
if ($env:WSL_DISTRO_NAME) {
    Write-Host "ERROR: This script must be run in Windows PowerShell, not WSL!" -ForegroundColor Red
    exit 1
}

# Function to check if a command exists
function Test-CommandExists {
    param($command)
    $null = Get-Command $command -ErrorAction SilentlyContinue
    return $?
}

Write-Host "`nStep 1: Checking prerequisites..." -ForegroundColor Yellow

# Check for Visual Studio 2022
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -property installationPath
    if ($vsPath) {
        Write-Host "✓ Visual Studio found at: $vsPath" -ForegroundColor Green
    } else {
        Write-Host "✗ Visual Studio 2022 not found!" -ForegroundColor Red
        Write-Host "  Please install Visual Studio 2022 with 'Desktop development with C++' workload" -ForegroundColor Yellow
        Write-Host "  Download from: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Yellow
        exit 1
    }
} else {
    Write-Host "✗ Visual Studio 2022 not found!" -ForegroundColor Red
    exit 1
}

# Check for Python
if (Test-CommandExists python) {
    Write-Host "✓ Python is installed" -ForegroundColor Green
} else {
    Write-Host "✗ Python not found!" -ForegroundColor Red
    Write-Host "  Please install Python from: https://www.python.org/downloads/" -ForegroundColor Yellow
    exit 1
}

# Check for Git
if (Test-CommandExists git) {
    Write-Host "✓ Git is installed" -ForegroundColor Green
} else {
    Write-Host "✗ Git not found!" -ForegroundColor Red
    Write-Host "  Please install Git from: https://git-scm.com/download/win" -ForegroundColor Yellow
    exit 1
}

Write-Host "`nStep 2: Setting up build directory..." -ForegroundColor Yellow

# Get current directory (should be the DigiByte source directory)
$sourceDir = Get-Location
Write-Host "Source directory: $sourceDir" -ForegroundColor Cyan

Write-Host "`nStep 3: Installing vcpkg (package manager)..." -ForegroundColor Yellow

$vcpkgDir = "$sourceDir\vcpkg"
if (!(Test-Path $vcpkgDir)) {
    Write-Host "Cloning vcpkg..." -ForegroundColor Cyan
    git clone https://github.com/Microsoft/vcpkg.git
    Set-Location vcpkg
    .\bootstrap-vcpkg.bat
    Set-Location $sourceDir
    Write-Host "✓ vcpkg installed" -ForegroundColor Green
} else {
    Write-Host "✓ vcpkg already installed" -ForegroundColor Green
}

# Configure vcpkg for release-only builds
$tripletFile = "$vcpkgDir\triplets\x64-windows-static.cmake"
if (!(Select-String -Path $tripletFile -Pattern "VCPKG_BUILD_TYPE release" -Quiet)) {
    Add-Content -Path $tripletFile -Value "set(VCPKG_BUILD_TYPE release)"
    Write-Host "✓ Configured vcpkg for release-only builds" -ForegroundColor Green
}

Write-Host "`nStep 4: Installing dependencies..." -ForegroundColor Yellow

Set-Location $vcpkgDir

# Install required dependencies
$dependencies = @(
    "boost-filesystem",
    "boost-multi-index", 
    "boost-process",
    "boost-signals2",
    "boost-test",
    "libevent",
    "berkeleydb",
    "sqlite3",
    "zeromq"
)

foreach ($dep in $dependencies) {
    Write-Host "Installing $dep..." -ForegroundColor Cyan
    .\vcpkg.exe install ${dep}:x64-windows-static
}

Write-Host "`nStep 5: Generating Visual Studio project files..." -ForegroundColor Yellow

Set-Location $sourceDir

# Generate Visual Studio project files
Write-Host "Generating Visual Studio project files..." -ForegroundColor Cyan
python build_msvc\msvc-autogen.py

Write-Host "`nStep 6: Building DigiByte..." -ForegroundColor Yellow

# Build with MSBuild
Write-Host "Building DigiByte (this will take some time)..." -ForegroundColor Cyan
$msbuildPath = & $vsWhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1

if ($msbuildPath) {
    & $msbuildPath build_msvc\digibyte.sln -property:Configuration=Release -maxCpuCount -verbosity:minimal
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n✓ Build completed successfully!" -ForegroundColor Green
        Write-Host "`nExecutables can be found in:" -ForegroundColor Yellow
        Write-Host "  $sourceDir\build_msvc\x64\Release\" -ForegroundColor Cyan
        
        # List built executables
        $exeFiles = Get-ChildItem -Path "build_msvc\x64\Release\" -Filter "*.exe" -ErrorAction SilentlyContinue
        if ($exeFiles) {
            Write-Host "`nBuilt executables:" -ForegroundColor Yellow
            foreach ($exe in $exeFiles) {
                Write-Host "  - $($exe.Name)" -ForegroundColor Cyan
            }
        }
    } else {
        Write-Host "`n✗ Build failed!" -ForegroundColor Red
        Write-Host "Check the error messages above for details." -ForegroundColor Yellow
    }
} else {
    Write-Host "✗ MSBuild not found!" -ForegroundColor Red
}

Write-Host "`nBuild script completed." -ForegroundColor Green