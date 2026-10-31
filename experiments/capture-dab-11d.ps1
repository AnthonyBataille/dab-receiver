param(
    [Parameter(Mandatory = $true)]
    [string]$RtlSdrExe,

    [string]$OutputFile = "dab_11d_10s_u8.iq"
)

$sampleCount = 20480000
$expectedBytes = 2 * $sampleCount

if (-not (Test-Path -LiteralPath $RtlSdrExe -PathType Leaf)) {
    throw "rtl_sdr.exe was not found: $RtlSdrExe"
}
if (Test-Path -LiteralPath $OutputFile) {
    throw "Output already exists: $OutputFile"
}

# Gain is set to 19.7 dB. It provides unsaturated I/Q signal, as observed experimentally.
& $RtlSdrExe -f 222064000 -s 2048000 -g 19.7 -n $sampleCount $OutputFile
if ($LASTEXITCODE -ne 0) {
    throw "rtl_sdr.exe failed with exit code $LASTEXITCODE"
}

$actualBytes = (Get-Item -LiteralPath $OutputFile).Length
if ($actualBytes -ne $expectedBytes) {
    throw "Unexpected file size: $actualBytes bytes; expected $expectedBytes"
}

Write-Host "Captured $sampleCount complex samples (10 seconds): $OutputFile"
Write-Host "Format: interleaved unsigned 8-bit I, Q; 2.048 MS/s; 222.064 MHz"