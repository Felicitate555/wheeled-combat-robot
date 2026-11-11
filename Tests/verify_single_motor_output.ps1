param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

function Read-File([string]$RelativePath) {
    return Get-Content -LiteralPath (Join-Path $ProjectRoot $RelativePath) -Raw
}

function Require([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -notmatch $Pattern) {
        $failures.Add($Description)
    }
}

$carHeader = Read-File 'MDK-ARM\User\Car.h'
$car = Read-File 'MDK-ARM\User\Car.c'
$fight = Read-File 'MDK-ARM\User\Fight.c'

Require $carHeader 'Car_ApplyFightCommand\s*\(' 'fight command application API is missing'
Require $car 'static\s+void\s+Car_WriteSigned' 'signed wheel writer is missing'
Require $car 'MOTOR_PWM_MAX' 'signed wheel command is not clamped to PWM range'
Require $car 'static\s+uint8_t\s+Car_IsDirectionReversal\s*\(' 'direction-reversal detector is missing'
Require $car 'if\s*\(\s*Car_IsDirectionReversal\s*\(\s*s_last_left_pwm\s*,\s*left_pwm\s*\)\s*!=\s*0U\s*\)\s*\{\s*TIM2->CCR1\s*=\s*0U\s*;' 'left PWM is not cleared only for a true direction reversal'
Require $car 'if\s*\(\s*Car_IsDirectionReversal\s*\(\s*s_last_right_pwm\s*,\s*right_pwm\s*\)\s*!=\s*0U\s*\)\s*\{\s*TIM2->CCR3\s*=\s*0U\s*;' 'right PWM is not cleared only for a true direction reversal'
Require $car 'TIM2->CCR1\s*=\s*left_duty\s*;[\s\S]*?TIM2->CCR3\s*=\s*right_duty\s*;[\s\S]*?s_last_left_pwm\s*=\s*left_pwm\s*;[\s\S]*?s_last_right_pwm\s*=\s*right_pwm\s*;' 'last signed wheel commands are not updated after the PWM write'

$leftClearCount = ([regex]::Matches($car, 'TIM2->CCR1\s*=\s*0U\s*;')).Count
$rightClearCount = ([regex]::Matches($car, 'TIM2->CCR3\s*=\s*0U\s*;')).Count
if (($leftClearCount -ne 1) -or ($rightClearCount -ne 1)) {
    $failures.Add(("PWM zero writes must exist only in the two direction-reversal guards (left={0}, right={1})" -f $leftClearCount, $rightClearCount))
}

# Confirm the image/bench-verified polarity is preserved:
# left +: A1=1,A2=0; left -: A1=0,A2=1;
# right +: B1=0,B2=1; right -: B1=1,B2=0.
Require $car 'left_pwm\s*>\s*0[\s\S]*?A1_GPIO_Port\s*,\s*A1_Pin\s*,\s*GPIO_PIN_SET[\s\S]*?A2_GPIO_Port\s*,\s*A2_Pin\s*,\s*GPIO_PIN_RESET' 'positive left-wheel polarity changed'
Require $car 'left_pwm\s*<\s*0[\s\S]*?A1_GPIO_Port\s*,\s*A1_Pin\s*,\s*GPIO_PIN_RESET[\s\S]*?A2_GPIO_Port\s*,\s*A2_Pin\s*,\s*GPIO_PIN_SET' 'negative left-wheel polarity changed'
Require $car 'right_pwm\s*>\s*0[\s\S]*?B1_GPIO_Port\s*,\s*B1_Pin\s*,\s*GPIO_PIN_RESET[\s\S]*?B2_GPIO_Port\s*,\s*B2_Pin\s*,\s*GPIO_PIN_SET' 'positive right-wheel polarity changed'
Require $car 'right_pwm\s*<\s*0[\s\S]*?B1_GPIO_Port\s*,\s*B1_Pin\s*,\s*GPIO_PIN_SET[\s\S]*?B2_GPIO_Port\s*,\s*B2_Pin\s*,\s*GPIO_PIN_RESET' 'negative right-wheel polarity changed'

if ($car -match 'HAL_TIM_Base_Start_IT\s*\(\s*&htim2\s*\)') {
    $failures.Add('TIM2 PWM timer is still incorrectly started with Base IT')
}

$applyCount = ([regex]::Matches($fight, 'Car_ApplyFightCommand\s*\(')).Count
if ($applyCount -ne 3) {
    $failures.Add(("expected exactly three lifecycle call sites (Enter, Exit, Update), found {0}" -f $applyCount))
}
Require $fight 'void\s+Fight_Update[\s\S]*?Car_ApplyFightCommand\s*\(\s*&g_fight\.command\s*\)[\s\S]*?Fight_UpdateDebug' 'Fight_Update does not commit exactly one final decision before debug publication'

$cFiles = Get-ChildItem -LiteralPath $ProjectRoot -Filter '*.c' -File -Recurse
foreach ($file in $cFiles) {
    if ($file.FullName -eq (Join-Path $ProjectRoot 'MDK-ARM\User\Car.c')) {
        continue
    }
    $text = Get-Content -LiteralPath $file.FullName -Raw
    if ($text -match 'TIM2->CCR[13]|HAL_GPIO_WritePin\(\s*(A1|A2|B1|B2)_GPIO_Port') {
        $failures.Add(("motor hardware write escaped Car.c: {0}" -f $file.FullName))
    }
}

if ($failures.Count -ne 0) {
    Write-Output 'Single motor-output verification FAILED:'
    foreach ($failure in $failures) {
        Write-Output (" - {0}" -f $failure)
    }
    exit 1
}

Write-Output 'Single motor-output verification PASSED (all static invariants satisfied).'
