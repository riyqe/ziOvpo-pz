param(
    [Parameter(Mandatory = $true)]
    [string]$ServiceAppDir,
    [string]$ServiceName = "ZiOvpoPzService",
    [string]$DisplayName = "ZiOvpo PZ Service"
)

$ErrorActionPreference = "Stop"

$logDir = Join-Path $env:ProgramData "ZiOvpoPz"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$log = Join-Path $logDir "install-service.log"

function Write-ZiLog([string]$Message) {
    $ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Add-Content -LiteralPath $log -Value "[$ts] $Message" -Encoding utf8
}

try {
    $exe = Join-Path $ServiceAppDir "ZiOvpoPzService.exe"
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "ZiOvpoPzService.exe not found: $exe"
    }

    Write-ZiLog "Configure service $ServiceName; exe=$exe"

    Stop-Service -Name $ServiceName -Force -ErrorAction SilentlyContinue
    $null = & sc.exe delete $ServiceName 2>&1
    Start-Sleep -Milliseconds 800

    $binPath = "`"$exe`""
    New-Service -Name $ServiceName -BinaryPathName $binPath -DisplayName $DisplayName -StartupType Automatic
    Set-Service -Name $ServiceName -Description "ZiOvpo PZ background antivirus service."
    Start-Service -Name $ServiceName

    Write-ZiLog "Service started OK."
    exit 0
}
catch {
    Write-ZiLog "ERROR: $($_.Exception.Message)"
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
