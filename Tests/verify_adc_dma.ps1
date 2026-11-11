param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

function Read-ProjectFile([string]$RelativePath) {
    return Get-Content -LiteralPath (Join-Path $ProjectRoot $RelativePath) -Raw
}

function Require-Pattern(
    [string]$Text,
    [string]$Pattern,
    [string]$Description
) {
    if ($Text -notmatch $Pattern) {
        $failures.Add($Description)
    }
}

function Require-Order(
    [string]$Text,
    [string]$First,
    [string]$Second,
    [string]$Description
) {
    $firstIndex = $Text.IndexOf($First, [System.StringComparison]::Ordinal)
    $secondIndex = $Text.IndexOf($Second, [System.StringComparison]::Ordinal)
    if (($firstIndex -lt 0) -or ($secondIndex -lt 0) -or ($firstIndex -ge $secondIndex)) {
        $failures.Add($Description)
    }
}

$main = Read-ProjectFile 'Core\Src\main.c'
$adc = Read-ProjectFile 'Core\Src\adc.c'
$tim = Read-ProjectFile 'Core\Src\tim.c'
$dma = if (Test-Path -LiteralPath (Join-Path $ProjectRoot 'Core\Src\dma.c')) {
    Read-ProjectFile 'Core\Src\dma.c'
} else {
    ''
}
$interrupts = Read-ProjectFile 'Core\Src\stm32h7xx_it.c'
$time = Read-ProjectFile 'MDK-ARM\User\TIME.c'
$user = Read-ProjectFile 'MDK-ARM\User\User.c'
$sensor = Read-ProjectFile 'MDK-ARM\User\Sensor.c'

Require-Order $main 'MX_DMA_Init();' 'MX_ADC1_Init();' 'MX_DMA_Init must run before MX_ADC1_Init'

Require-Pattern $adc 'ScanConvMode\s*=\s*ADC_SCAN_ENABLE' 'ADC1 scan mode is not enabled'
Require-Pattern $adc 'NbrOfConversion\s*=\s*4' 'ADC1 conversion count is not four'
Require-Pattern $adc 'DiscontinuousConvMode\s*=\s*DISABLE' 'ADC1 discontinuous mode is not disabled'
Require-Pattern $adc 'ExternalTrigConv\s*=\s*ADC_EXTERNALTRIG_T3_TRGO' 'ADC1 is not triggered by TIM3 TRGO'
Require-Pattern $adc 'ExternalTrigConvEdge\s*=\s*ADC_EXTERNALTRIGCONVEDGE_RISING' 'ADC1 trigger edge is not rising'
Require-Pattern $adc 'ConversionDataManagement\s*=\s*ADC_CONVERSIONDATA_DMA_CIRCULAR' 'ADC1 data management is not DMA circular'
Require-Pattern $adc 'Overrun\s*=\s*ADC_OVR_DATA_OVERWRITTEN' 'ADC1 overrun policy is not overwrite'
Require-Pattern $adc 'EOCSelection\s*=\s*ADC_EOC_SEQ_CONV' 'ADC1 EOC is not end-of-sequence'
Require-Pattern $adc 'SamplingTime\s*=\s*ADC_SAMPLETIME_64CYCLES_5' 'ADC1 sampling time is not 64.5 cycles'

$rankMap = @(
    'sConfig\.Channel\s*=\s*ADC_CHANNEL_3;[\s\S]*?sConfig\.Rank\s*=\s*ADC_REGULAR_RANK_1',
    'sConfig\.Channel\s*=\s*ADC_CHANNEL_7;[\s\S]*?sConfig\.Rank\s*=\s*ADC_REGULAR_RANK_2',
    'sConfig\.Channel\s*=\s*ADC_CHANNEL_4;[\s\S]*?sConfig\.Rank\s*=\s*ADC_REGULAR_RANK_3',
    'sConfig\.Channel\s*=\s*ADC_CHANNEL_8;[\s\S]*?sConfig\.Rank\s*=\s*ADC_REGULAR_RANK_4'
)
for ($i = 0; $i -lt $rankMap.Count; $i++) {
    Require-Pattern $adc $rankMap[$i] ("ADC1 Rank {0} channel mapping is wrong" -f ($i + 1))
}

Require-Pattern $tim 'MasterOutputTrigger\s*=\s*TIM_TRGO_UPDATE' 'TIM3 TRGO is not configured as update event'

Require-Pattern $dma '__HAL_RCC_DMA1_CLK_ENABLE' 'DMA1 clock is not enabled'
Require-Pattern $adc 'hdma_adc1\.Instance\s*=\s*DMA1_Stream0' 'ADC1 is not assigned to DMA1 Stream0'
Require-Pattern $adc 'hdma_adc1\.Init\.Request\s*=\s*DMA_REQUEST_ADC1' 'ADC1 DMAMUX request is wrong'
Require-Pattern $adc 'hdma_adc1\.Init\.Mode\s*=\s*DMA_CIRCULAR' 'ADC1 DMA is not circular'
Require-Pattern $adc 'PeriphDataAlignment\s*=\s*DMA_PDATAALIGN_WORD' 'ADC1 DMA peripheral width is not word'
Require-Pattern $adc 'MemDataAlignment\s*=\s*DMA_MDATAALIGN_WORD' 'ADC1 DMA memory width is not word'
Require-Pattern $adc '__HAL_LINKDMA\(adcHandle,\s*DMA_Handle,\s*hdma_adc1\)' 'ADC1 and DMA handles are not linked'

Require-Pattern $interrupts 'void\s+DMA1_Stream0_IRQHandler\s*\(' 'DMA1 Stream0 IRQ handler is missing'
Require-Pattern $interrupts 'HAL_DMA_IRQHandler\(&hdma_adc1\)' 'DMA1 Stream0 IRQ does not dispatch to HAL'
Require-Pattern $adc 'HAL_ADC_Start_DMA\(&hadc1,[\s\S]*?ADC1_DMA_ACTIVE_WORDS\)' 'ADC1 DMA is not started with the four-word active length'
Require-Pattern $adc 'void\s+HAL_ADC_ConvCpltCallback\s*\(' 'ADC complete callback is missing'
Require-Pattern $adc 'g_adc1_dma_frame_count\+\+' 'ADC complete callback does not publish a frame count'
Require-Pattern $adc 'void\s+HAL_ADC_ErrorCallback\s*\(' 'ADC error callback is missing'
Require-Pattern $adc 'void\s+ADC1_DMA_Service\s*\(' 'main-context ADC1 DMA recovery service is missing'
Require-Pattern $sensor 'ADC1_DMA_Service\s*\(' 'Sensor service does not run ADC1 DMA recovery'

$readerMatch = [regex]::Match($adc, 'uint8_t\s+ADC1_DMA_ReadLatest[\s\S]*?void\s+HAL_ADC_ConvCpltCallback')
if ($readerMatch.Success -and ($readerMatch.Value -match 'continue\s*;')) {
    $failures.Add('ADC1 seqlock reader can reach its loop condition with an uninitialized sequence value')
}

Require-Pattern $user 'ADC1_DMA_Start\s*\(' 'ADC1 DMA startup is not integrated'
Require-Order $user 'ADC1_DMA_Start()' 'TIME_Init();' 'ADC1 DMA must be armed before TIM3 starts'

if ($time -match 'HAL_ADC_GetValue\s*\(\s*&hadc1\s*\)') {
    $failures.Add('TIME ISR still reads ADC1->DR directly')
}
Require-Pattern $time 'htim->Instance\s*!=\s*TIM3' 'timer callback is not guarded for TIM3'

if ($failures.Count -ne 0) {
    Write-Output 'ADC/DMA verification FAILED:'
    foreach ($failure in $failures) {
        Write-Output (" - {0}" -f $failure)
    }
    exit 1
}

Write-Output 'ADC/DMA verification PASSED (all static invariants satisfied).'
