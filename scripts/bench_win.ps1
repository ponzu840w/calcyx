# bench_win.ps1 — Windows ネイティブで calcyx-gui.exe の起動時間とピーク
# RSS を計測する。WSL 側の bench.sh から powershell.exe -File 経由で呼ばれる。
#
# 出力 (stdout, key=value):
#   elapsed_s=0.123
#   peak_rss_kb=45600

param(
    [Parameter(Mandatory=$true)]
    [string]$Exe,
    [int]$ExitMs = 200,
    [int]$TimeoutMs = 5000
)

$ErrorActionPreference = "Stop"

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $Exe
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $false
$psi.EnvironmentVariables["CALCYX_BENCH_EXIT_MS"] = "$ExitMs"

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$proc = [System.Diagnostics.Process]::Start($psi)

# PeakWorkingSet64 はプロセス終了後に 0 になることがあるので、実行中に
# 50ms 毎に WorkingSet64 をサンプリングして最大値を保持する。
$peakBytes = 0
$timedOut = $false
while (-not $proc.HasExited) {
    try {
        $proc.Refresh()
        if ($proc.WorkingSet64 -gt $peakBytes) { $peakBytes = $proc.WorkingSet64 }
    } catch { break }
    if ($sw.Elapsed.TotalMilliseconds -ge $TimeoutMs) {
        try { $proc.Kill() } catch {}
        $timedOut = $true
        break
    }
    Start-Sleep -Milliseconds 50
}
$proc.WaitForExit()
$sw.Stop()

$peakKb = [int]($peakBytes / 1024)
if ($timedOut) {
    # フック未対応: 起動時間は "-"
    Write-Output "elapsed_s=-"
} else {
    Write-Output ("elapsed_s={0:N3}" -f $sw.Elapsed.TotalSeconds)
}
Write-Output "peak_rss_kb=$peakKb"
