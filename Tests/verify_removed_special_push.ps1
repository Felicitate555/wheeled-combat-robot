param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

$legacyAngleToken = -join @(67, 79, 82, 78, 69, 82 | ForEach-Object { [char]$_ })
$legacyReloadToken = -join @(65, 84, 84, 65, 67, 75, 95, 82, 69, 76, 79, 65, 68 | ForEach-Object { [char]$_ })
$formalFiles = Get-ChildItem -LiteralPath $ProjectRoot -Recurse -File | Where-Object {
    $_.Extension -in '.c', '.h', '.ps1', '.uvprojx'
}

foreach ($file in $formalFiles) {
    $text = Get-Content -LiteralPath $file.FullName -Raw
    if ($text.IndexOf($legacyAngleToken, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
        $failures.Add(("legacy angle-push token remains in {0}" -f $file.FullName))
    }
    if ($text.IndexOf($legacyReloadToken, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
        $failures.Add(("legacy reload-attack token remains in {0}" -f $file.FullName))
    }
}

if ($failures.Count -ne 0) {
    Write-Output 'Legacy special-push removal verification FAILED:'
    foreach ($failure in $failures) {
        Write-Output (" - {0}" -f $failure)
    }
    exit 1
}

Write-Output 'Legacy special-push removal verification PASSED (no executable states, parameters, functions, or test remnants).'
