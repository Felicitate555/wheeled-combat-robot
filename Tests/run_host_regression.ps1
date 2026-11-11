param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$BaselineRoot = '',
    [ValidateSet('All', 'Golden', 'StageMonitor', 'Restage')]
    [string]$Suite = 'All'
)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw ("command failed ({0}): {1}" -f $LASTEXITCODE, $Program)
    }
}

function Import-VcEnvironment([string]$VcVarsPath) {
    $command = ('"{0}" >nul && set' -f $VcVarsPath)
    $lines = & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) {
        throw 'vcvars64.bat failed'
    }
    foreach ($line in $lines) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $name = $line.Substring(0, $separator)
            $value = $line.Substring($separator + 1)
            [System.Environment]::SetEnvironmentVariable($name, $value, 'Process')
        }
    }
}

function Compile-Object(
    [string]$Compiler,
    [string]$Source,
    [string]$Object,
    [string[]]$Includes,
    [string[]]$Defines
) {
    $arguments = @('/nologo', '/TC', '/std:c11', '/utf-8', '/W4', '/WX', '/c')
    foreach ($include in $Includes) {
        $arguments += ('/I{0}' -f $include)
    }
    foreach ($define in $Defines) {
        $arguments += ('/D{0}' -f $define)
    }
    $arguments += $Source
    $arguments += ('/Fo{0}' -f $Object)
    Invoke-Checked $Compiler $arguments
}

function Link-Executable(
    [string]$Compiler,
    [string[]]$Objects,
    [string]$Executable
) {
    $arguments = @('/nologo') + $Objects + @('/Fe' + $Executable)
    Invoke-Checked $Compiler $arguments
}

function Run-GoldenSuite(
    [string]$Compiler,
    [string]$BuildRoot,
    [string]$ResolvedBaselineRoot,
    [string]$CurrentRoot,
    [string]$HostInclude
) {
    $baselineUser = Join-Path $ResolvedBaselineRoot 'MDK-ARM\User'
    $currentUser = Join-Path $CurrentRoot 'MDK-ARM\User'
    $goldenObject = Join-Path $BuildRoot 'fight_golden.obj'
    $finalObject = Join-Path $BuildRoot 'fight_final.obj'
    $harnessObject = Join-Path $BuildRoot 'host_golden_fight.obj'
    $executable = Join-Path $BuildRoot 'host_golden_fight.exe'
    $commonRenames = @(
        'Fight_Enter={0}_Fight_Enter',
        'Fight_Exit={0}_Fight_Exit',
        'Fight_Update={0}_Fight_Update',
        'Fight_GetMotionCommand={0}_Fight_GetMotionCommand',
        'Fight_GetState={0}_Fight_GetState',
        'Fight_GetTargetRegion={0}_Fight_GetTargetRegion',
        'Fight_ConsumeRestageRequest={0}_Fight_ConsumeRestageRequest',
        'g_fight_debug=g_{0}_fight_debug',
        'Car_ApplyFightCommand={0}_Car_ApplyFightCommand'
    )
    $goldenDefines = @($commonRenames | ForEach-Object { $_ -f 'Golden' })
    $finalDefines = @($commonRenames | ForEach-Object { $_ -f 'Final' })

    Compile-Object $Compiler (Join-Path $baselineUser 'Fight.c') $goldenObject @($HostInclude, $baselineUser) $goldenDefines
    Compile-Object $Compiler (Join-Path $currentUser 'Fight.c') $finalObject @($HostInclude, $currentUser) $finalDefines
    Compile-Object $Compiler (Join-Path $CurrentRoot 'Tests\host\host_golden_fight.c') $harnessObject @($HostInclude, $currentUser) @()
    Link-Executable $Compiler @($harnessObject, $goldenObject, $finalObject) $executable
    Invoke-Checked $executable @()
}

function Run-StageMonitorSuite(
    [string]$Compiler,
    [string]$BuildRoot,
    [string]$CurrentRoot,
    [string]$HostInclude
) {
    $currentUser = Join-Path $CurrentRoot 'MDK-ARM\User'
    $monitorObject = Join-Path $BuildRoot 'stage_monitor.obj'
    $harnessObject = Join-Path $BuildRoot 'host_stage_monitor.obj'
    $executable = Join-Path $BuildRoot 'host_stage_monitor.exe'

    Compile-Object $Compiler (Join-Path $currentUser 'StageMonitor.c') $monitorObject @($HostInclude, $currentUser) @()
    Compile-Object $Compiler (Join-Path $CurrentRoot 'Tests\host\host_stage_monitor.c') $harnessObject @($HostInclude, $currentUser) @()
    Link-Executable $Compiler @($harnessObject, $monitorObject) $executable
    Invoke-Checked $executable @()
}

function Run-RestageSuite(
    [string]$Compiler,
    [string]$BuildRoot,
    [string]$CurrentRoot,
    [string]$HostInclude
) {
    $currentUser = Join-Path $CurrentRoot 'MDK-ARM\User'
    $modeObject = Join-Path $BuildRoot 'mode.obj'
    $harnessObject = Join-Path $BuildRoot 'host_restage.obj'
    $executable = Join-Path $BuildRoot 'host_restage.exe'
    $disabledPreprocessed = Join-Path $BuildRoot 'mode_restage_disabled.i'

    Compile-Object $Compiler (Join-Path $currentUser 'Mode.c') $modeObject @($HostInclude, $currentUser) @()
    Compile-Object $Compiler (Join-Path $CurrentRoot 'Tests\host\host_restage.c') $harnessObject @($HostInclude, $currentUser) @()
    Link-Executable $Compiler @($harnessObject, $modeObject) $executable
    Invoke-Checked $executable @()

    $preprocessArguments = @(
        '/nologo', '/TC', '/EP', '/utf-8', '/DENABLE_STAGE_RESTAGE=0',
        ('/I{0}' -f $HostInclude),
        ('/I{0}' -f $currentUser),
        (Join-Path $currentUser 'Mode.c')
    )
    & $Compiler @preprocessArguments | Set-Content -LiteralPath $disabledPreprocessed -Encoding utf8
    if ($LASTEXITCODE -ne 0) {
        throw 'disabled Restage preprocessing failed'
    }
    $preprocessed = Get-Content -LiteralPath $disabledPreprocessed -Raw
    $start = $preprocessed.LastIndexOf('unsigned char Mode_Finding(void)', [System.StringComparison]::Ordinal)
    $end = $preprocessed.IndexOf('unsigned char Mode_GoOnStageAgain(void)', $start, [System.StringComparison]::Ordinal)
    if (($start -lt 0) -or ($end -le $start)) {
        throw 'could not locate preprocessed Mode_Finding'
    }
    $finding = $preprocessed.Substring($start, $end - $start)
    if (($finding -notmatch 'Fight_Update') -or
        ($finding -match 'StageMonitor_Update|StageMonitor_IsStageLostConfirmed')) {
        throw 'ENABLE_STAGE_RESTAGE=0 does not reduce Mode_Finding to normal Fight'
    }
    Write-Output 'Restage disable-switch regression PASSED (preprocessed Mode_Finding is Fight-only).'
}

$projectFull = [System.IO.Path]::GetFullPath($ProjectRoot)
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$buildRoot = Join-Path $tempBase ("robot-host-regression-{0}-{1}" -f $PID, (Get-Date -Format 'yyyyMMddHHmmssfff'))
New-Item -ItemType Directory -Path $buildRoot | Out-Null

try {
    $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'Visual Studio locator is unavailable'
    }
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -utf8
    if (-not $installation) {
        throw 'Visual C++ host compiler is unavailable'
    }
    $vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
    Import-VcEnvironment $vcvars
    $compiler = (Get-Command cl.exe -ErrorAction Stop).Source
    $hostInclude = Join-Path $projectFull 'Tests\host\include'

    if (-not $BaselineRoot) {
        $workspaceRoot = Split-Path -Parent (Split-Path -Parent $projectFull)
        $baselineZip = Join-Path $workspaceRoot 'feibiao1.zip'
        if (-not (Test-Path -LiteralPath $baselineZip)) {
            throw 'feibiao1.zip was not found; pass -BaselineRoot explicitly'
        }
        $expanded = Join-Path $buildRoot 'baseline'
        Expand-Archive -LiteralPath $baselineZip -DestinationPath $expanded
        $BaselineRoot = Join-Path $expanded 'feibiao\feibiao'
    }
    $baselineFull = [System.IO.Path]::GetFullPath($BaselineRoot)

    if (($Suite -eq 'All') -or ($Suite -eq 'Golden')) {
        Run-GoldenSuite $compiler $buildRoot $baselineFull $projectFull $hostInclude
    }
    if (($Suite -eq 'All') -or ($Suite -eq 'StageMonitor')) {
        Run-StageMonitorSuite $compiler $buildRoot $projectFull $hostInclude
    }
    if (($Suite -eq 'All') -or ($Suite -eq 'Restage')) {
        Run-RestageSuite $compiler $buildRoot $projectFull $hostInclude
    }
    Write-Output ("Host regression PASSED: {0}" -f $Suite)
}
finally {
    $resolvedBuild = [System.IO.Path]::GetFullPath($buildRoot)
    if ($resolvedBuild.StartsWith($tempBase, [System.StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolvedBuild).StartsWith('robot-host-regression-', [System.StringComparison]::Ordinal)) {
        Remove-Item -LiteralPath $resolvedBuild -Recurse -Force -ErrorAction SilentlyContinue
    }
}
