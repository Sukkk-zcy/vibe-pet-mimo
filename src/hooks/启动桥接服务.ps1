# VibePet Bridge 启动器 — 双击运行，托盘图标，右键退出

# 配置：改成你的 ESP32 IP
$env:ESP32_HOST = "192.168.31.6"

# 静默启动 Node.js 桥接（隐藏控制台窗口）
$nodePath = (Get-Command node).Source
$scriptPath = Join-Path $PSScriptRoot "bridge.mjs"

$process = Start-Process -WindowStyle Hidden -FilePath $nodePath -ArgumentList $scriptPath -PassThru

# 创建系统托盘图标
Add-Type -AssemblyName System.Windows.Forms
$icon = [System.Drawing.Icon]::ExtractAssociatedIcon((Get-Command node).Source)
$tray = New-Object System.Windows.Forms.NotifyIcon
$tray.Icon = $icon
$tray.Text = "VibePet Bridge → $env:ESP32_HOST"
$tray.Visible = $true

# 右键菜单
$menu = New-Object System.Windows.Forms.ContextMenuStrip

$statusItem = $menu.Items.Add("VibePet Bridge 运行中")
$statusItem.Enabled = $false

$menu.Items.Add("-")

$exitItem = $menu.Items.Add("退出")
$exitItem.Add_Click({
    $tray.Visible = $false
    if (!$process.HasExited) { $process.Kill() }
    [System.Windows.Forms.Application]::Exit()
    Stop-Process -Id $pid
})

$tray.ContextMenuStrip = $menu

# 双击显示状态
$tray.Add_Click({
    if ($_.Button -eq "DoubleClick") {
        $active = @()
        try {
            $resp = Invoke-WebRequest -Uri "http://localhost:17384/health" -UseBasicParsing -TimeoutSec 2
            $data = $resp | ConvertFrom-Json
            if ($data.active.Count -gt 0) {
                $tray.Text = "VibePet: $($data.active -join ', ')"
            } else {
                $tray.Text = "VibePet Bridge (空闲)"
            }
        } catch {
            $tray.Text = "VibePet Bridge (连接中...)"
        }
    }
})

# 保持运行直到退出
[System.Windows.Forms.Application]::Run()

# 清理
$tray.Visible = $false
if (!$process.HasExited) { $process.Kill() }
