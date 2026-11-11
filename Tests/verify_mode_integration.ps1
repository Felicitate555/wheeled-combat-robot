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

$header = Read-ProjectFile 'MDK-ARM\User\Mode.h'
$mode = Read-ProjectFile 'MDK-ARM\User\Mode.c'
$config = Read-ProjectFile 'MDK-ARM\User\fight_config.h'
$car = Read-ProjectFile 'MDK-ARM\User\Car.c'
$carHeader = Read-ProjectFile 'MDK-ARM\User\Car.h'

foreach ($setting in @(
    @('FIRST_STAGE_BACK_SPEED', '75U'),
    @('FIRST_STAGE_CONFIRM_SPEED', '42U'),
    @('FIRST_STAGE_EDGE_SAFE_CONFIRM_MS', '40U'),
    @('RESTAGE_SETTLE_MS', '100U'),
    @('RESTAGE_SEARCH_SPEED', '50U'),
    @('RESTAGE_CLIMB_SPEED', '75U'),
    @('RESTAGE_CONFIRM_SPEED', '42U'),
    @('RESTAGE_EDGE_SAFE_CONFIRM_MS', '40U'),
    @('RESTAGE_CLIMB_MIN_MS', '300U'),
    @('RESTAGE_CLIMB_TIMEOUT_MS', '2000U'),
    @('RESTAGE_RAMP_RETREAT_SPEED', '40U'),
    @('RESTAGE_RAMP_RETREAT_MS', '200U'),
    @('RESTAGE_MAX_STRUCTURED_RETRIES', '3U')
)) {
    Require $config ('#define\s+' + $setting[0] + '\s+' + $setting[1]) ("unexpected Mode/Restage setting {0}" -f $setting[0])
}

Require $mode 'MODE_FIRST_STAGE_REVERSE_DELAY[\s\S]*?Car_SetBackSpeed\(FIRST_STAGE_BACK_SPEED\)' 'first-stage climb is not 75 reverse'
Require $mode 'MODE_FIRST_STAGE_WAIT_RANGE5[\s\S]*?Car_SetBackSpeed\(FIRST_STAGE_BACK_SPEED\)[\s\S]*?Car_SetBackSpeed\(FIRST_STAGE_CONFIRM_SPEED\)' 'Range5 transition does not reduce first-stage speed to 42'
Require $mode 'MODE_FIRST_STAGE_ON_STAGE_CONFIRM[\s\S]*?edge_raw_mask\s*==\s*0U[\s\S]*?FIRST_STAGE_EDGE_SAFE_CONFIRM_MS' 'first-stage four-Edge safe confirmation changed'

foreach ($state in @('MODE_RESTAGE_SETTLE', 'MODE_RESTAGE_COARSE_ALIGN',
                      'MODE_RESTAGE_UNKNOWN_ALIGN', 'MODE_RESTAGE_SEARCH',
                      'MODE_RESTAGE_CLIMB', 'MODE_RESTAGE_CONFIRM_ON_STAGE',
                      'MODE_RESTAGE_RAMP_RETREAT_STOP',
                      'MODE_RESTAGE_RAMP_RETREAT_FORWARD',
                      'MODE_RESTAGE_FAILED_STOP')) {
    Require $header ([regex]::Escape($state)) ("missing Restage state {0}" -f $state)
}

foreach ($field in @('restage_state', 'restage_retry', 'rear_search_armed',
                      'rear_edge_new_event', 'climb_started',
                      'climb_range_event', 'climb_edge_propagation',
                      'confirm_safe_ms', 'final_left_pwm', 'final_right_pwm')) {
    Require $header ([regex]::Escape($field)) ("missing RestageDebug field {0}" -f $field)
}
Require $header 'volatile\s+RestageDebug\s+g_restage_debug' 'Keil Watch Restage debug export is missing'

Require $mode 'case\s+STAGE_EXIT_FRONT:[\s\S]*?Mode_StartRestageSearch\(snapshot\)' 'FRONT loss does not reverse-search without turning'
Require $mode 'case\s+STAGE_EXIT_REAR:[\s\S]*?RESTAGE_ALIGN_180_MS' 'REAR loss lacks a bounded 180-degree coarse turn'
Require $mode 'case\s+STAGE_EXIT_LEFT:[\s\S]*?Car_Lift[\s\S]*?RESTAGE_ALIGN_90_MS' 'LEFT loss mapping is wrong'
Require $mode 'case\s+STAGE_EXIT_RIGHT:[\s\S]*?Car_Right[\s\S]*?RESTAGE_ALIGN_90_MS' 'RIGHT loss mapping is wrong'
Require $mode 'STAGE_EXIT_SINGLE_FL[\s\S]*?RESTAGE_ALIGN_45_MS[\s\S]*?STAGE_EXIT_SINGLE_RL[\s\S]*?RESTAGE_ALIGN_135_MS' 'single-sensor coarse direction mapping is incomplete'
Require $mode 'RESTAGE_UNKNOWN_MAX_RETRIES[\s\S]*?MODE_RESTAGE_FAILED_STOP' 'UNKNOWN fallback is not bounded'

Require $mode 'rear_search_armed\s*==\s*0U[\s\S]*?RESTAGE_REAR_BASELINE_MS[\s\S]*?rear_search_armed\s*=\s*1U' 'rear Search baseline is not armed from a stable state'
Require $mode 'RESTAGE_REAR_EVENT_STABLE_MS' 'rear platform transition lacks a stability interval'
Require $mode 'rear_mask\s*\^\s*g_restage\.rear_baseline_mask' 'rear platform contact is not a new transition from the armed baseline'
Require $mode 'rear_event\s*!=\s*0U[\s\S]*?Mode_StartRestageClimb\(snapshot,\s*rear_event\)' 'one new RL or RR event does not immediately start Climb'
Require $mode 'Mode_StartRestageClimb[\s\S]*?Car_SetBackSpeed\(RESTAGE_CLIMB_SPEED\)' 'rear contact does not issue immediate 75 reverse climb'

Require $mode 'climb_range_baseline\s*=\s*g_restage\.search_range5_baseline' 'Climb Range5 evidence lacks Search context'
Require $mode 'STAGE_RANGE5_EVENT_DELTA[\s\S]*?climb_range_departed\s*=\s*1U[\s\S]*?RESTAGE_CLIMB_RANGE_STABLE_MS[\s\S]*?climb_range_event\s*=\s*1U' 'Climb Range5 evidence is not excursion-to-stability'
Require $mode 'climb_edge_propagation\s*=\s*1U[\s\S]*?RESTAGE_CLIMB_EDGE_SAFE_MS[\s\S]*?climb_edge_complete\s*=\s*1U' 'secondary Climb Edge sequence evidence is missing'
Require $mode 'RESTAGE_CLIMB_MIN_MS[\s\S]*?climb_range_event[\s\S]*?climb_edge_complete[\s\S]*?Mode_StartRestageConfirm' 'Climb cannot transition through either contextual evidence path'
Require $mode 'MODE_RESTAGE_CONFIRM_ON_STAGE[\s\S]*?Car_SetBackSpeed\(RESTAGE_CONFIRM_SPEED\)[\s\S]*?edge_raw_mask\s*==\s*0U[\s\S]*?RESTAGE_EDGE_SAFE_CONFIRM_MS[\s\S]*?Car_SetDirection\(Car_Stop\)' '42-speed, 40 ms all-Edge-safe finish is incomplete'

Require $mode 'RESTAGE_CLIMB_TIMEOUT_MS[\s\S]*?Mode_StartRampRetreat' 'Climb timeout does not enter ramp-safe retreat'
Require $mode 'RESTAGE_CONFIRM_TIMEOUT_MS[\s\S]*?Mode_StartRampRetreat' 'Confirm timeout does not enter ramp-safe retreat'
Require $mode 'MODE_RESTAGE_RAMP_RETREAT_STOP[\s\S]*?Car_SetDirection\(Car_Stop\)[\s\S]*?RESTAGE_RAMP_STOP_MS[\s\S]*?MODE_RESTAGE_RAMP_RETREAT_FORWARD' 'ramp failure does not stop before moving'
Require $mode 'MODE_RESTAGE_RAMP_RETREAT_FORWARD[\s\S]*?Car_SetForwardSpeed\(RESTAGE_RAMP_RETREAT_SPEED\)[\s\S]*?RESTAGE_RAMP_RETREAT_MS[\s\S]*?Car_SetDirection\(Car_Stop\)[\s\S]*?Mode_RequestRetry' 'ramp failure does not retreat forward before retry alignment'

$restageStart = $mode.IndexOf('unsigned char Mode_GoOnStageAgain(void)', [System.StringComparison]::Ordinal)
$restageEnd = $mode.IndexOf('void Mode_FightEnter(void)', $restageStart, [System.StringComparison]::Ordinal)
if (($restageStart -lt 0) -or ($restageEnd -le $restageStart)) {
    $failures.Add('Mode_GoOnStageAgain body could not be located')
} else {
    $restage = $mode.Substring($restageStart, $restageEnd - $restageStart)
    if ($restage -match 'RANGE5_STAGE_THRESHOLD|RESTAGE_FRONT_THRESHOLD|RESTAGE_FRONT_DELTA|TIME_GetADCValue') {
        $failures.Add('Restage still uses a context-free static stage/front sensor decision')
    }
}

Require $carHeader 'Car_SetForwardSpeed\s*\(' 'ramp retreat forward-speed API is missing'
Require $carHeader 'Car_GetSignedOutput\s*\(' 'actual signed output debug API is missing'
Require $car 'Car_SetForwardSpeed[\s\S]*?Car_WriteSigned\(forward_pwm,\s*forward_pwm\)' 'forward ramp retreat does not preserve verified polarity'
Require $car 'Car_GetSignedOutput[\s\S]*?s_last_left_pwm[\s\S]*?s_last_right_pwm' 'Restage debug does not read actual final signed PWM'

if ($mode -match 'HAL_Delay') {
    $failures.Add('blocking HAL_Delay exists in Mode')
}

if ($failures.Count -ne 0) {
    Write-Output 'Mode integration verification FAILED:'
    foreach ($failure in $failures) {
        Write-Output (" - {0}" -f $failure)
    }
    exit 1
}

Write-Output 'Mode integration verification PASSED (rear-first climb, contextual finish, ramp-safe retries).'
