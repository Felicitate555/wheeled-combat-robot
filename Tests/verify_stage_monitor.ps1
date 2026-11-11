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

$header = Read-ProjectFile 'MDK-ARM\User\StageMonitor.h'
$monitor = Read-ProjectFile 'MDK-ARM\User\StageMonitor.c'
$mode = Read-ProjectFile 'MDK-ARM\User\Mode.c'
$config = Read-ProjectFile 'MDK-ARM\User\fight_config.h'
$project = Read-ProjectFile 'MDK-ARM\ToothGeigei.uvprojx'

foreach ($direction in @('STAGE_EXIT_UNKNOWN', 'STAGE_EXIT_FRONT', 'STAGE_EXIT_REAR',
                          'STAGE_EXIT_LEFT', 'STAGE_EXIT_RIGHT',
                          'STAGE_EXIT_SINGLE_FL', 'STAGE_EXIT_SINGLE_FR',
                          'STAGE_EXIT_SINGLE_RL', 'STAGE_EXIT_SINGLE_RR')) {
    Require $header ([regex]::Escape($direction)) ("missing exit direction {0}" -f $direction)
}
foreach ($state in @('STAGE_MONITOR_OFF', 'STAGE_MONITOR_ARMING',
                      'STAGE_MONITOR_ARMED', 'STAGE_MONITOR_EXIT_CANDIDATE',
                      'STAGE_MONITOR_LOST_CONFIRMED')) {
    Require $header ([regex]::Escape($state)) ("missing monitor state {0}" -f $state)
}
foreach ($field in @('first_edge', 'coalesced_first_edge', 'edge_union',
                      'last_edge', 'pre_exit_motion', 'exit_time',
                      'range5_transition_seen', 'edge_propagation_seen',
                      'confidence_reason')) {
    Require $header ([regex]::Escape($field)) ("missing StageExitEvent field {0}" -f $field)
}
foreach ($field in @('monitor_state', 'armed', 'candidate_active', 'first_edge',
                      'coalesced_first_edge', 'range5_baseline', 'range5_min',
                      'range5_max', 'pre_exit_left_pwm', 'pre_exit_right_pwm',
                      'lost_confirmed', 'exit_direction', 'confidence_reason',
                      'final_left_pwm', 'final_right_pwm')) {
    Require $header ([regex]::Escape($field)) ("missing StageMonitorDebug field {0}" -f $field)
}
Require $header 'volatile\s+StageMonitorDebug\s+g_stage_debug' 'Keil Watch StageMonitor debug export is missing'

Require $header 'typedef\s+struct[\s\S]*?tick_ms[\s\S]*?range5_raw[\s\S]*?edge_raw_mask[\s\S]*?left_pwm[\s\S]*?right_pwm[\s\S]*?fight_state[\s\S]*?StageHistorySample' 'history sample does not contain all required observations'
Require $monitor 'StageHistorySample\s+history\s*\[\s*STAGE_HISTORY_SAMPLE_COUNT\s*\]' '60-sample history ring is missing'
Require $monitor 'sample->fight_state\s*=\s*Fight_GetState\(\)' 'history does not store the actual Fight state'

foreach ($setting in @(
    @('STAGE_MONITOR_HISTORY_MS', '300U'),
    @('STAGE_HISTORY_SAMPLE_COUNT', '60U'),
    @('STAGE_ARM_SAFE_MS', '100U'),
    @('STAGE_EDGE_COHERENT_MS', '10U'),
    @('STAGE_EDGE_PAIR_WINDOW_MS', '35U'),
    @('STAGE_EDGE_SAFE_AFTER_EVENT_MS', '30U'),
    @('STAGE_EVENT_MAX_MS', '300U'),
    @('STAGE_RANGE5_EVENT_DELTA', '3000U')
)) {
    Require $config ('#define\s+' + $setting[0] + '\s+' + $setting[1]) ("unexpected StageMonitor setting {0}" -f $setting[0])
}

Require $monitor 'STAGE_MONITOR_ARMING[\s\S]*?STAGE_ARM_SAFE_MS[\s\S]*?STAGE_MONITOR_ARMED' 'monitor does not arm after a continuous safe interval'
Require $monitor 'g_stage\.state\s*==\s*STAGE_MONITOR_ARMED[\s\S]*?edge_raw_mask\s*==\s*0U[\s\S]*?StageMonitor_StartCandidate' 'candidate is not gated by ARMED plus a new Edge event'
Require $monitor 'STAGE_EDGE_PAIR_WINDOW_MS[\s\S]*?StageMonitor_CoalesceFirstEdge' 'single-to-pair coalescing window is not applied'
Require $monitor 'first_coherent_exit_edge\s*!=\s*0U[\s\S]*?return' 'FIRST direction can be overwritten by later propagation'
Require $monitor 'StageMonitor_PropagationTargetMask[\s\S]*?STAGE_EDGE_PROPAGATION_MS[\s\S]*?edge_propagation_seen\s*=\s*1U' 'cross-chassis propagation evidence is incomplete'

Require-Before $monitor 'pre_cycle_motion = g_stage.last_final_motion' 'StageMonitor_RecordHistory(snapshot, final_motion)' 'pre-event motion is not captured before the current EdgeEscape command is recorded'
Require $monitor 'StageMonitor_StartCandidate\(snapshot,\s*&pre_cycle_motion\)' 'candidate does not preserve the prior final signed PWM'
Require $monitor 'range5_departed_seen[\s\S]*?STAGE_RANGE5_EVENT_DELTA[\s\S]*?STAGE_RANGE5_RETURN_DELTA[\s\S]*?range5_transition_seen\s*=\s*1U' 'Range5 evidence is not a complete transition sequence'
Require $monitor 'STAGE_EDGE_SAFE_AFTER_EVENT_MS[\s\S]*?STAGE_CONFIDENCE_EDGE_PROPAGATION[\s\S]*?STAGE_CONFIDENCE_EDGE_UNION[\s\S]*?STAGE_CONFIDENCE_RANGE5_AND_MOTION' 'loss confirmation does not require safe recovery plus high-confidence evidence'
Require $monitor 'STAGE_EVENT_DISMISS_MS[\s\S]*?StageMonitor_ReturnToArmed' 'ordinary successful EdgeEscape candidates are not dismissed'

if ($monitor -match 'RANGE5_STAGE_THRESHOLD') {
    $failures.Add('StageMonitor contains a context-free static Range5 threshold')
}
if ($monitor -match 'Car_|HAL_Delay|malloc|calloc|free|\bfloat\b|\bdouble\b|\bsin\s*\(|\bcos\s*\(|\batan') {
    $failures.Add('StageMonitor violates the side-listener/nonblocking integer-only boundary')
}

$findingStart = $mode.IndexOf('unsigned char Mode_Finding(void)', [System.StringComparison]::Ordinal)
$findingEnd = $mode.IndexOf('unsigned char Mode_GoOnStageAgain(void)', $findingStart, [System.StringComparison]::Ordinal)
if (($findingStart -lt 0) -or ($findingEnd -le $findingStart)) {
    $failures.Add('Mode_Finding body could not be located')
} else {
    $finding = $mode.Substring($findingStart, $findingEnd - $findingStart)
    Require-Before $finding 'Fight_Update(snapshot)' 'StageMonitor_Update(snapshot, Fight_GetMotionCommand())' 'StageMonitor runs before normal Fight'
    Require-Before $finding 'StageMonitor_Update(snapshot, Fight_GetMotionCommand())' 'StageMonitor_IsStageLostConfirmed()' 'Mode switches before monitor confirmation'
    if ($finding -match 'Fight_ConsumeRestageRequest|LatchUnknown') {
        $failures.Add('legacy Edge hard timeout still directly requests Restage')
    }
}

Require $config '#ifndef\s+ENABLE_STAGE_RESTAGE[\s\S]*?#define\s+ENABLE_STAGE_RESTAGE\s+1' 'one-switch Restage insurance is missing or disabled'
Require $project '<FileName>StageMonitor\.c</FileName>' 'StageMonitor.c is absent from the Keil project'

if ($failures.Count -ne 0) {
    Write-Output 'StageMonitor verification FAILED:'
    foreach ($failure in $failures) {
        Write-Output (" - {0}" -f $failure)
    }
    exit 1
}

Write-Output 'StageMonitor verification PASSED (armed side-listener, FIRST pair coalescing, conservative evidence).'
