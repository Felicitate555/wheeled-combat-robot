param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

function Read-Optional([string]$RelativePath) {
    $path = Join-Path $ProjectRoot $RelativePath
    if (Test-Path -LiteralPath $path) {
        return Get-Content -LiteralPath $path -Raw
    }
    return ''
}

function Require([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -notmatch $Pattern) {
        $failures.Add($Description)
    }
}

$sensorHeader = Read-Optional 'MDK-ARM\User\Sensor.h'
$sensor = Read-Optional 'MDK-ARM\User\Sensor.c'
$config = Read-Optional 'MDK-ARM\User\fight_config.h'
$user = Read-Optional 'MDK-ARM\User\User.c'
$timeHeader = Read-Optional 'MDK-ARM\User\TIME.h'
$time = Read-Optional 'MDK-ARM\User\TIME.c'
$project = Read-Optional 'MDK-ARM\ToothGeigei.uvprojx'

Require $sensorHeader 'typedef\s+struct[\s\S]*?SensorSnapshot' 'SensorSnapshot type is missing'
Require $sensorHeader 'const\s+SensorSnapshot\s*\*\s*Sensor_GetSnapshot' 'snapshot is not exposed read-only'
Require $sensorHeader 'edge_raw_mask' 'raw edge mask is missing from snapshot'
Require $sensorHeader 'enemy_raw_mask' 'raw enemy mask is missing from snapshot'
Require $sensorHeader 'enemy_filtered_mask' 'filtered enemy mask is missing from snapshot'
Require $sensorHeader 'range_raw\s*\[\s*4\s*\]' 'four raw ADC1 ranges are missing'
Require $sensorHeader 'range_filtered\s*\[\s*4\s*\]' 'four filtered ADC1 ranges are missing'
Require $sensorHeader 'adc1_frame_count' 'ADC1 frame metadata is missing'
Require $sensorHeader 'adc1_age_ms' 'ADC1 age metadata is missing'
Require $sensorHeader 'volatile\s+SensorSnapshot\s+g_sensor_debug' 'global Keil Watch sensor snapshot is missing'

Require $config '#define\s+FIGHT_CONTROL_PERIOD_MS\s+5U' 'snapshot/control period is not 5 ms'
Require $config '#define\s+TARGET_DIGITAL_COUNTER_MAX\s+3U' 'digital target integrator maximum is wrong'
Require $config '#define\s+TARGET_DIGITAL_ON_COUNT\s+2U' 'digital target activation count is wrong'
Require $config '#define\s+ADC_RANGE_EMA_SHIFT\s+2U' 'ADC EMA shift is wrong'
Require $config 'RANGE1_TARGET_ON' 'Range1 threshold is not centralized'
Require $config 'RANGE4_TARGET_OFF' 'Range4 hysteresis threshold is not centralized'

$gpioPairs = @(
    @('IR1_GPIO_Port', 'IR1_Pin'), @('IR2_GPIO_Port', 'IR2_Pin'),
    @('IR3_GPIO_Port', 'IR3_Pin'), @('IR4_GPIO_Port', 'IR4_Pin'),
    @('IR5_GPIO_Port', 'IR5_Pin'), @('IR6_GPIO_Port', 'IR6_Pin'),
    @('IR7_GPIO_Port', 'IR7_Pin'), @('IR8_GPIO_Port', 'IR8_Pin'),
    @('IR9_GPIO_Port', 'IR9_Pin'), @('IR10_GPIO_Port', 'IR10_Pin')
)
foreach ($pair in $gpioPairs) {
    $pattern = 'HAL_GPIO_ReadPin\(\s*' + $pair[0] + '\s*,\s*' + $pair[1] + '\s*\)'
    $count = ([regex]::Matches($sensor, $pattern)).Count
    if ($count -ne 1) {
        $failures.Add(("{0}/{1} must be read exactly once per GPIO frame (source occurrences={2})" -f $pair[0], $pair[1], $count))
    }
}

Require $sensor 'IR1[\s\S]*?GPIO_PIN_SET[\s\S]*?SENSOR_EDGE_FRONT_LEFT' 'front-left edge polarity/mapping is missing'
Require $sensor 'IR2[\s\S]*?GPIO_PIN_SET[\s\S]*?SENSOR_EDGE_FRONT_RIGHT' 'front-right edge polarity/mapping is missing'
Require $sensor 'IR8[\s\S]*?GPIO_PIN_SET[\s\S]*?SENSOR_EDGE_REAR_LEFT' 'rear-left edge polarity/mapping is missing'
Require $sensor 'IR6[\s\S]*?GPIO_PIN_SET[\s\S]*?SENSOR_EDGE_REAR_RIGHT' 'rear-right edge polarity/mapping is missing'
Require $sensor 'IR3[\s\S]*?GPIO_PIN_RESET[\s\S]*?SENSOR_ENEMY_D2' '2-o-clock digital sensor mapping is missing'
Require $sensor 'IR10[\s\S]*?GPIO_PIN_RESET[\s\S]*?SENSOR_ENEMY_D10' '10-o-clock digital sensor mapping is missing'

Require $sensor 'TARGET_DIGITAL_COUNTER_MAX' 'digital saturating filter is missing'
Require $sensor 'TARGET_DIGITAL_ON_COUNT' 'digital activation hysteresis is missing'
Require $sensor 'counter.*==\s*TARGET_DIGITAL_OFF_COUNT' 'digital release hysteresis is missing'
Require $sensor 'ADC_RANGE_EMA_SHIFT' 'integer ADC EMA is missing'
Require $sensor 'range_on_threshold' 'ADC target-on hysteresis is missing'
Require $sensor 'range_off_threshold' 'ADC target-off hysteresis is missing'
Require $sensor 'ADC1_DMA_ReadLatest' 'snapshot does not consume a stable published DMA frame'
Require $sensor 'ADC1_FRAME_STALE_MS' 'ADC stale-frame rejection is missing'

Require $sensor '__HAL_ADC_GET_FLAG\(\s*&hadc2\s*,\s*ADC_FLAG_EOC\s*\)' 'ADC2 completion is not polled non-blockingly'
if ($sensor -match 'HAL_ADC_PollForConversion|HAL_Delay') {
    $failures.Add('Sensor path contains a blocking ADC poll or delay')
}

Require $timeHeader 'TIME_SetADCValue' 'stage compatibility ADC setter is missing'
Require $time 'void\s+TIME_SetADCValue' 'stage compatibility ADC setter is not implemented'
Require $sensor 'TIME_SetADCValue\(' 'snapshot is not publishing compatibility ranges'
Require $sensor 'g_sensor_debug\s*=\s*g_sensor_snapshot' 'Keil Watch sensor snapshot is not published'

Require $user 'Sensor_Init\s*\(' 'Sensor initialization is not integrated'
Require $user 'Sensor_Service\s*\(' 'Sensor service is not called globally'
Require $project '<FileName>Sensor\.c</FileName>' 'Sensor.c is not in the Keil project'

if ($failures.Count -ne 0) {
    Write-Output 'Sensor Snapshot verification FAILED:'
    foreach ($failure in $failures) {
        Write-Output (" - {0}" -f $failure)
    }
    exit 1
}

Write-Output 'Sensor Snapshot verification PASSED (all static invariants satisfied).'
