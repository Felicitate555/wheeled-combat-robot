param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

function Read-ProjectFile([string]$RelativePath) {
    return Get-Content -LiteralPath (Join-Path $ProjectRoot $RelativePath) -Raw
}

function Require([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -notmatch $Pattern) {
        $failures.Add($Description)
    }
}

function Require-Before([string]$Text, [string]$First, [string]$Second, [string]$Description) {
    $firstIndex = $Text.IndexOf($First, [System.StringComparison]::Ordinal)
    $secondIndex = $Text.IndexOf($Second, [System.StringComparison]::Ordinal)
    if (($firstIndex -lt 0) -or ($secondIndex -lt 0) -or ($firstIndex -ge $secondIndex)) {
        $failures.Add($Description)
    }
}

$fightPath = Join-Path $ProjectRoot 'MDK-ARM\User\Fight.c'
$headerPath = Join-Path $ProjectRoot 'MDK-ARM\User\Fight.h'
$fight = Get-Content -LiteralPath $fightPath -Raw
$header = Get-Content -LiteralPath $headerPath -Raw
$config = Read-ProjectFile 'MDK-ARM\User\fight_config.h'
$project = Read-ProjectFile 'MDK-ARM\ToothGeigei.uvprojx'

$expectedFightHash = 'B3BD85651950228B104D0F58D265F4304E03BC5F839D0CB96B6DBBC093E41FCA'
$expectedHeaderHash = '3F916526B74941B1FAE96A7F85D91C2C261185EF89ECA4FCA317FF3F20BC9020'
$actualFightHash = (Get-FileHash -LiteralPath $fightPath -Algorithm SHA256).Hash
$actualHeaderHash = (Get-FileHash -LiteralPath $headerPath -Algorithm SHA256).Hash
if ($actualFightHash -ne $expectedFightHash) {
    $failures.Add('Fight.c differs from the feibiao1 ground-truth implementation')
}
if ($actualHeaderHash -ne $expectedHeaderHash) {
    $failures.Add('Fight.h differs from the feibiao1 ground-truth interface')
}

foreach ($setting in @(
    @('TARGET_ATTACK_ENTER_CYCLES', '2U'),
    @('SEARCH_SPEED', '38U'),
    @('TRACK_SPEED', '50U'),
    @('TURN_SPEED', '50U'),
    @('ATTACK_SPEED', '75U'),
    @('TARGET_HOLD_SPEED', '45U'),
    @('EDGE_ESCAPE_SPEED', '65U')
)) {
    Require $config ('#define\s+' + $setting[0] + '\s+' + $setting[1]) ("unexpected Fight tuning for {0}" -f $setting[0])
}

$updateStart = $fight.IndexOf('void Fight_Update(const SensorSnapshot *snapshot)', [System.StringComparison]::Ordinal)
$updateEnd = $fight.IndexOf('const FightMotionCommand *Fight_GetMotionCommand', $updateStart, [System.StringComparison]::Ordinal)
if (($updateStart -lt 0) -or ($updateEnd -le $updateStart)) {
    $failures.Add('Fight_Update body could not be located')
} else {
    $fightUpdate = $fight.Substring($updateStart, $updateEnd - $updateStart)
    Require-Before $fightUpdate 'if (snapshot->edge_raw_mask != 0U)' 'Fight_FuseTarget(snapshot)' 'raw Edge no longer preempts target fusion'
    Require-Before $fightUpdate 'Fight_HandleEdge(snapshot)' 'Fight_HandleTarget(target' 'Edge handling no longer preempts target control'
    Require-Before $fightUpdate 'Car_ApplyFightCommand(&g_fight.command)' 'Fight_UpdateDebug(snapshot)' 'final motor command is not committed before debug publication'
}

foreach ($symbol in @('Fight_CommandForSearch', 'Fight_CommandForTarget',
                       'Fight_CommandForEdge', 'Fight_HandleEdge',
                       'SEARCH_SPIN_PRIMARY', 'SEARCH_ADVANCE_ARC',
                       'SEARCH_SPIN_REVERSE', 'SEARCH_ADVANCE_ARC_REVERSE')) {
    Require ($fight + $header) ([regex]::Escape($symbol)) ("missing protected baseline symbol {0}" -f $symbol)
}
Require $project '<FileName>Fight\.c</FileName>' 'Fight.c is absent from the Keil project'

if ($failures.Count -ne 0) {
    Write-Output 'Fight baseline verification FAILED:'
    foreach ($failure in $failures) {
        Write-Output (" - {0}" -f $failure)
    }
    exit 1
}

Write-Output 'Fight baseline verification PASSED (feibiao1 byte match; only configured Attack PWM is 75).'
