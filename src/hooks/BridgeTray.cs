using System;
using System.Drawing;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;

namespace VibePetBridge
{
    class BridgeTray : Form
    {
        private NotifyIcon tray;
        private Process nodeProcess;
        private Timer statusTimer;
        private string esp32Host = "192.168.31.6";
        private string scriptPath;
        private bool disposing = false;

        public BridgeTray()
        {
            Load += (s, e) => { BeginInvoke(new Action(Initialize)); };
        }

        private void Initialize()
        {
            scriptPath = Path.Combine(Application.StartupPath, "bridge.mjs");
            if (!File.Exists(scriptPath))
            {
                var parent = Directory.GetParent(Application.StartupPath);
                if (parent != null)
                    scriptPath = Path.Combine(parent.FullName, "bridge.mjs");
            }

            tray = new NotifyIcon();
            tray.Icon = SystemIcons.Application;
            tray.Text = "VibePet Bridge";
            tray.Visible = true;

            var menu = new ContextMenuStrip();
            menu.Items.Add("VibePet Bridge", null, null).Enabled = false;
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add("Status: starting...", null, null).Enabled = false;
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add("Exit", null, OnExit);
            tray.ContextMenuStrip = menu;

            tray.DoubleClick += (s, e) => { UpdateStatus(); };

            statusTimer = new Timer();
            statusTimer.Interval = 3000;
            statusTimer.Tick += (s, e) => UpdateStatus();
            statusTimer.Start();

            StartBridge();
            WindowState = FormWindowState.Minimized;
            ShowInTaskbar = false;
        }

        private void StartBridge()
        {
            try
            {
                nodeProcess = new Process();
                nodeProcess.StartInfo.FileName = "node";
                nodeProcess.StartInfo.Arguments = "\"" + scriptPath + "\"";
                nodeProcess.StartInfo.UseShellExecute = false;
                nodeProcess.StartInfo.CreateNoWindow = true;
                nodeProcess.StartInfo.EnvironmentVariables["ESP32_HOST"] = esp32Host;
                nodeProcess.EnableRaisingEvents = true;
                nodeProcess.Exited += (s, e) => {
                    if (!disposing) StartBridge();
                };
                nodeProcess.Start();
                tray.ContextMenuStrip.Items[2].Text = "Status: running";
            }
            catch (Exception ex)
            {
                tray.ContextMenuStrip.Items[2].Text = "Status: error";
                tray.ShowBalloonTip(3000, "VibePet Bridge", "Error: " + ex.Message, ToolTipIcon.Error);
            }
        }

        private void UpdateStatus()
        {
            try
            {
                bool running = nodeProcess != null && !nodeProcess.HasExited;
                if (running)
                {
                    tray.ContextMenuStrip.Items[2].Text = "Status: running";
                    tray.Text = "VibePet Bridge - running";
                }
                else
                {
                    tray.ContextMenuStrip.Items[2].Text = "Status: stopped";
                    tray.Text = "VibePet Bridge - stopped";
                }
            }
            catch { }
        }

        private void OnExit(object sender, EventArgs e)
        {
            disposing = true;
            statusTimer.Stop();
            if (nodeProcess != null && !nodeProcess.HasExited)
            {
                try { nodeProcess.Kill(); } catch { }
                nodeProcess.Dispose();
            }
            tray.Visible = false;
            tray.Dispose();
            Application.Exit();
        }

        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new BridgeTray());
        }
    }
}
