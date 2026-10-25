# Guarda do POCO (regra de 24/10/2026): antes de pegar a trava e antes de cada
# input (am start, tap, swipe, keyevent), o app em primeiro plano tem de ser o
# Astra (ou o Launcher, só para o am start inicial) e não pode haver ligação.
# Falhou: o roteiro aborta, devolve a cena e solta a trava.
$PocoLaunchers = @('com.miui.home', 'com.mi.android.globallauncher', 'com.android.launcher')
function Test-PocoFree([string]$Adb, [string]$Package = 'dev.aether.editor.u07', [switch]$AllowLauncher) {
    $calls = @([regex]::Matches((& $Adb shell dumpsys telephony.registry | Out-String), 'mCallState=(\d+)') |
               ForEach-Object { $_.Groups[1].Value })
    if ($calls | Where-Object { $_ -ne '0' }) { return "ligação em curso (mCallState=$($calls -join ','))" }
    $top = [regex]::Match((& $Adb shell dumpsys activity activities | Out-String), 'topResumedActivity=ActivityRecord\{\S+ \S+ ([^/ ]+)/').Groups[1].Value
    if ($top -eq $Package) { return $null }
    if ($AllowLauncher -and ($PocoLaunchers -contains $top)) { return $null }
    return "app em primeiro plano: $top"
}
function Assert-PocoFree([string]$Adb, [string]$Package = 'dev.aether.editor.u07', [switch]$AllowLauncher) {
    $reason = Test-PocoFree -Adb $Adb -Package $Package -AllowLauncher:$AllowLauncher
    if ($reason) { throw "POCO ocupado pelo dono: $reason. Rodada abortada." }
}
