# Helpers do runner; sem ADB, instalação ou execução automática ao importar.
# dumpsys não é API estável: saída desconhecida falha explicitamente, nunca equivale a desbloqueado.
function ConvertFrom-AndroidKeyguardPolicy {
    param([AllowEmptyString()][string]$Policy)
    $showing = @([regex]::Matches($Policy, '(?m)^\s*(?:showing|mIsShowing)=(true|false)\s*$'))
    $screen = [regex]::Match($Policy, '(?m)^\s*screenState=(\S+)\s*$')
    $secure = [regex]::Match($Policy, '(?m)^\s*secure=(true|false)\s*$')
    return [pscustomobject]@{
        # Delegate e monitor podem divergir durante a transição. Só liberar quando ambos negam.
        Showing = if ($showing.Count -eq 0) { $null } else {
            @($showing | Where-Object { $_.Groups[1].Value -eq 'true' }).Count -gt 0
        }
        ScreenOn = $screen.Success -and $screen.Groups[1].Value -eq 'SCREEN_STATE_ON'
        ScreenState = if ($screen.Success) { $screen.Groups[1].Value } else { 'unknown' }
        Secure = if ($secure.Success) { $secure.Groups[1].Value -eq 'true' } else { $null }
    }
}

function Wait-AndroidKeyguardDismissed {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$ReadPolicy,
        [Parameter(Mandatory = $true)][scriptblock]$RequestDismiss,
        [ValidateRange(1, 300)][int]$TimeoutSeconds = 20,
        [ValidateRange(1, 2000)][int]$PollMilliseconds = 250
    )
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $attempts = 0
    do {
        $state = ConvertFrom-AndroidKeyguardPolicy -Policy (& $ReadPolicy)
        if ($null -eq $state.Showing -or $state.ScreenState -eq 'unknown') {
            throw 'Não foi possível identificar o keyguard em dumpsys window policy; desbloqueio não comprovado.'
        }
        if (-not $state.Showing -and $state.ScreenOn) {
            return [ordered]@{
                elapsedMs = [Math]::Round($timer.Elapsed.TotalMilliseconds, 3)
                dismissRequests = $attempts
                showing = $false
                screenState = $state.ScreenState
            }
        }
        # Awake sozinho não garante tela ligada/desbloqueada. O comando solicita dismissal;
        # exit code zero não comprova sucesso. PIN/biometria continuam exclusivamente humanos.
        if ($state.Showing -and $state.ScreenOn -and $attempts -eq 0) {
            & $RequestDismiss | Out-Null
            ++$attempts
        }
        Start-Sleep -Milliseconds $PollMilliseconds
    } while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds)

    $blockedException = [InvalidOperationException]::new(
        "Aparelho bloqueado: desbloqueio não confirmado em $TimeoutSeconds s " +
        "(keyguard=$($state.Showing), screen=$($state.ScreenState), secure=$($state.Secure)). " +
        'Desbloqueie manualmente; a retomada gráfica não foi validada.')
    $blockedException.Data['AetherFailureKind'] = 'device-keyguard-blocked'
    throw $blockedException
}
