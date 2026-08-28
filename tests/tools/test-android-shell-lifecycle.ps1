Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../../tools/android-shell-lifecycle.ps1')

$testCount = 0
function Assert-True {
    param([bool]$Value, [string]$Message)
    if (-not $Value) { throw $Message }
}
function Test-Case {
    param([string]$Name, [scriptblock]$Body)
    & $Body
    ++$script:testCount
    Write-Host "PASS: $Name"
}
function New-Policy {
    param([string]$Showing = 'false', [string]$Screen = 'SCREEN_STATE_ON', [string]$Secure = 'false')
    return "KeyguardServiceDelegate`n  showing=$Showing`n  secure=$Secure`n  screenState=$Screen`n  KeyguardStateMonitor`n    mIsShowing=$Showing"
}
function New-PolicyReader {
    param([string[]]$Policies)
    $queue = [Collections.Generic.Queue[string]]::new()
    foreach ($policy in $Policies) { $queue.Enqueue($policy) }
    return {
        if ($queue.Count -gt 1) { $queue.Dequeue() } else { $queue.Peek() }
    }.GetNewClosure()
}

Test-Case 'desbloqueado com tela ligada' {
    $state = ConvertFrom-AndroidKeyguardPolicy (New-Policy)
    Assert-True ($state.Showing -eq $false -and $state.ScreenOn) 'Estado aberto incorreto.'
}
Test-Case 'bloqueio seguro reconhecido' {
    $state = ConvertFrom-AndroidKeyguardPolicy (New-Policy -Showing true -Secure true)
    Assert-True ($state.Showing -and $state.Secure) 'Senha não pode equivaler a desbloqueado.'
}
Test-Case 'delegate e monitor divergentes não liberam' {
    $policy = "showing=false`nmIsShowing=true`nscreenState=SCREEN_STATE_ON"
    Assert-True (ConvertFrom-AndroidKeyguardPolicy $policy).Showing 'Monitor bloqueado foi ignorado.'
}
Test-Case 'CRLF e campos vizinhos não confundem o parser' {
    $policy = "  showing=false`r`n  mIsShowing=false`r`n  screenState=SCREEN_STATE_ON`r`n  mInputRestricted=true`r`n"
    Assert-True ((ConvertFrom-AndroidKeyguardPolicy $policy).Showing -eq $false) 'Parser confundiu outro campo.'
}
Test-Case 'formato desconhecido não vira falso' {
    Assert-True ($null -eq (ConvertFrom-AndroidKeyguardPolicy 'unsupported output').Showing) 'Estado desconhecido virou desbloqueado.'
}
Test-Case 'aberto não envia dismiss' {
    $reader = New-PolicyReader @((New-Policy))
    $result = Wait-AndroidKeyguardDismissed -ReadPolicy $reader -RequestDismiss { throw 'Não deveria solicitar.' }
    Assert-True ($result.dismissRequests -eq 0 -and -not $result.showing) 'Solicitação desnecessária.'
}
Test-Case 'aguarda SCREEN_STATE_ON antes de solicitar' {
    $reader = New-PolicyReader @((New-Policy true SCREEN_STATE_TURNING_ON), (New-Policy true), (New-Policy))
    $requests = [Collections.Generic.List[int]]::new()
    $result = Wait-AndroidKeyguardDismissed -ReadPolicy $reader -RequestDismiss { $requests.Add(1) } -PollMilliseconds 1
    Assert-True ($requests.Count -eq 1 -and $result.dismissRequests -eq 1) 'Solicitou durante transição.'
}
Test-Case 'aceita desbloqueio humano sem remover senha' {
    $reader = New-PolicyReader @((New-Policy true SCREEN_STATE_ON true), (New-Policy false SCREEN_STATE_ON true))
    $result = Wait-AndroidKeyguardDismissed -ReadPolicy $reader -RequestDismiss {} -PollMilliseconds 1
    Assert-True (-not $result.showing) 'Secure continua true mesmo após desbloqueio legítimo.'
}
Test-Case 'exit code zero do dismiss não prova desbloqueio' {
    $reader = New-PolicyReader @((New-Policy true SCREEN_STATE_ON true))
    $requests = [Collections.Generic.List[int]]::new()
    $blocked = $false
    try {
        Wait-AndroidKeyguardDismissed -ReadPolicy $reader -RequestDismiss { $requests.Add(1); 'OK' } -TimeoutSeconds 1 -PollMilliseconds 1 | Out-Null
    } catch {
        $blocked = $_.Exception.Data['AetherFailureKind'] -eq 'device-keyguard-blocked'
    }
    Assert-True $blocked 'Bloqueio não foi classificado corretamente.'
    Assert-True ($requests.Count -eq 1) 'Não repetir solicitações durante autenticação humana.'
}
Test-Case 'tela apagada não equivale a retomada' {
    $reader = New-PolicyReader @((New-Policy true SCREEN_STATE_OFF true))
    $blocked = $false
    try {
        Wait-AndroidKeyguardDismissed -ReadPolicy $reader -RequestDismiss { throw 'Tela apagada não deveria solicitar.' } -TimeoutSeconds 1 -PollMilliseconds 1 | Out-Null
    } catch {
        $blocked = $_.Exception.Data['AetherFailureKind'] -eq 'device-keyguard-blocked'
    }
    Assert-True $blocked 'Tela apagada não pode passar.'
}
Test-Case 'saída desconhecida interrompe a verificação' {
    $reader = New-PolicyReader @('unsupported output')
    $rejected = $false
    try {
        Wait-AndroidKeyguardDismissed -ReadPolicy $reader -RequestDismiss { throw 'Não deve solicitar.' } | Out-Null
    } catch {
        $rejected = $_.Exception.Message -match 'Não foi possível identificar'
    }
    Assert-True $rejected 'Formato desconhecido passou.'
}
Test-Case 'erro no transporte não é escondido como bloqueio' {
    $reader = New-PolicyReader @((New-Policy true))
    $rejected = $false
    try {
        Wait-AndroidKeyguardDismissed -ReadPolicy $reader -RequestDismiss { throw 'ADB indisponível' } | Out-Null
    } catch {
        $rejected = $_.Exception.Message -eq 'ADB indisponível'
    }
    Assert-True $rejected 'Erro do transporte ocultado.'
}

Write-Host "$testCount testes de lifecycle do runner passaram."
