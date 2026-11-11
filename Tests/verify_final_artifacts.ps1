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

$map = Read-File 'MDK-ARM\ToothGeigei\ToothGeigei.map'
$scatter = Read-File 'MDK-ARM\ToothGeigei_DMA.sct'
$project = Read-File 'MDK-ARM\ToothGeigei.uvprojx'
$ioc = Read-File 'ToothGeigei.ioc'
$build = Read-File 'MDK-ARM\final-competition-build.log'
$adcHeader = Read-File 'Core\Inc\adc.h'
$tim = Read-File 'Core\Src\tim.c'

Require $build '0 Error\(s\), 0 Warning\(s\)' 'final Keil build is not clean'
Require $map 'g_adc1_dma_buffer\s+0x2404ffe0\s+Data\s+32\s+adc\.o\(ADC_DMA_BUFFER\)' 'map does not place the 32-byte ADC buffer at 0x2404FFE0'
Require $map 'Execution Region RW_ADC_DMA \(Exec base: 0x2404ffe0,[^\r\n]*Size: 0x00000020, Max: 0x00000020,[^\r\n]*UNINIT\)' 'map DMA execution region is not the exclusive 32-byte AXI region'
Require $map 'Execution Region RW_IRAM2 \(Exec base: 0x24000000,[^\r\n]*Max: 0x0004ffe0' 'normal AXI SRAM overlaps the reserved DMA line'
Require $scatter 'RW_ADC_DMA\s+0x2404FFE0\s+UNINIT\s+0x00000020' 'custom scatter file does not reserve the expected DMA line'
Require $scatter '\*\(ADC_DMA_BUFFER\)' 'custom scatter file does not select the ADC DMA section'
Require $project '<umfTarg>0</umfTarg>[\s\S]*?<useFile>1</useFile>[\s\S]*?<ScatterFile>\.\\ToothGeigei_DMA\.sct</ScatterFile>' 'Keil target is not configured to use the custom scatter file'
Require $adcHeader '#define\s+ADC1_DMA_BUFFER_ADDRESS\s+0x2404FFE0UL' 'runtime DMA address contract differs from the linker contract'

Require $ioc 'Dma\.ADC1\.0\.Instance=DMA1_Stream0' '.ioc does not preserve ADC1 DMA stream selection'
Require $ioc 'Dma\.ADC1\.0\.Mode=DMA_CIRCULAR' '.ioc does not preserve DMA circular mode'
Require $ioc 'Dma\.ADC1\.0\.PeriphDataAlignment=DMA_PDATAALIGN_WORD' '.ioc does not preserve DMA peripheral width'
Require $ioc 'Dma\.ADC1\.0\.MemDataAlignment=DMA_MDATAALIGN_WORD' '.ioc does not preserve DMA memory width'
Require $ioc 'ADC1\.ExternalTrigConv=ADC_EXTERNALTRIG_T3_TRGO' '.ioc does not preserve TIM3 TRGO ADC trigger'
Require $ioc 'TIM3\.MasterOutputTrigger=TIM_TRGO_UPDATE' '.ioc does not preserve TIM3 update TRGO'
Require $tim 'htim3\.Init\.Prescaler\s*=\s*250-1[\s\S]*?htim3\.Init\.Period\s*=\s*1000-1' 'TIM3 1 ms time base changed'

$sourceFiles = @()
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'Core') -Include '*.c','*.h' -File -Recurse
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'MDK-ARM\User') -Include '*.c','*.h' -File -Recurse
foreach ($file in $sourceFiles) {
    $text = Get-Content -LiteralPath $file.FullName -Raw
    if ($text -match 'SCB_EnableDCache\s*\(|HAL_MPU_ConfigRegion\s*\(|HAL_MPU_Enable\s*\(') {
        $failures.Add(("D-Cache/MPU was enabled unexpectedly in {0}" -f $file.FullName))
    }
    if ($text -match 'MX_USART|HAL_UART_Init\s*\(') {
        $failures.Add(("UART/GPIO debug was introduced unexpectedly in {0}" -f $file.FullName))
    }
}

foreach ($artifact in @('MDK-ARM\ToothGeigei\ToothGeigei.axf',
                         'MDK-ARM\ToothGeigei\ToothGeigei.hex',
                         'MDK-ARM\ToothGeigei\ToothGeigei.map')) {
    if (-not (Test-Path -LiteralPath (Join-Path $ProjectRoot $artifact))) {
        $failures.Add(("missing final build artifact {0}" -f $artifact))
    }
}

$buildInputs = @()
$buildInputs += Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'Core') -Include '*.c','*.h' -File -Recurse
$buildInputs += Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'MDK-ARM\User') -Include '*.c','*.h' -File -Recurse
$buildInputs += Get-Item -LiteralPath (Join-Path $ProjectRoot 'MDK-ARM\ToothGeigei.uvprojx')
$newestInput = $buildInputs | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
foreach ($artifact in @('MDK-ARM\ToothGeigei\ToothGeigei.axf',
                         'MDK-ARM\ToothGeigei\ToothGeigei.hex',
                         'MDK-ARM\ToothGeigei\ToothGeigei.map')) {
    $path = Join-Path $ProjectRoot $artifact
    if ((Test-Path -LiteralPath $path) -and
        ((Get-Item -LiteralPath $path).LastWriteTimeUtc -lt $newestInput.LastWriteTimeUtc)) {
        $failures.Add(("stale build artifact {0}; newest source is {1}" -f $artifact, $newestInput.FullName))
    }
}

if ($failures.Count -ne 0) {
    Write-Output 'Final artifact verification FAILED:'
    foreach ($failure in $failures) {
        Write-Output (" - {0}" -f $failure)
    }
    exit 1
}

Write-Output 'Final artifact verification PASSED (build, map, cache, CubeMX and output checks).'
