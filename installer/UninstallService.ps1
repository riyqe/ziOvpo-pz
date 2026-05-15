param(
    [string]$ServiceName = "ZiOvpoPzService"
)

$ErrorActionPreference = "SilentlyContinue"
Stop-Service -Name $ServiceName -Force -ErrorAction SilentlyContinue
$null = & sc.exe delete $ServiceName 2>&1
Start-Sleep -Milliseconds 400
exit 0
