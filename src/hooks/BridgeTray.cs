using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Net;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using System.Linq;

namespace VibePetBridge
{
    public class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            bool createdNew;
            using (new Mutex(true, "VibePetBridge", out createdNew))
            {
                if (!createdNew) { MessageBox.Show("Already running"); return; }
                Application.Run(new MainForm());
            }
        }
    }

    class MainForm : Form
    {
        private string ConfigPath { get { return Path.Combine(Application.StartupPath, "bridge-config.txt"); } }
        private string esp32Host = "192.168.31.6";
        private int bridgePort = 17384;

        private NotifyIcon tray;
        private ToolStripMenuItem statusItem;
        private HttpListener listener;
        private Thread serverThread;
        private bool running = false;

        private Dictionary<string, AgentState> agents = new Dictionary<string, AgentState>();
        private string lastActiveAgent = "";  // ★ 记住最后活跃的 agent
        private string lastPushKey = "";

        class AgentState
        {
            public string AgentId;
            public string AgentName;
            public string State;
            public string Event;
            public string Output;
            public long UpdatedAt;
        }

        private Form configForm;
        private TextBox txtEsp32Ip;
        private Label lblStatus;

        public MainForm()
        {
            LoadConfig();
            CreateConfigForm();
            CreateTray();
            StartBridge();
            WindowState = FormWindowState.Minimized;
            ShowInTaskbar = false;
        }

        private void LoadConfig()
        {
            try
            {
                if (File.Exists(ConfigPath))
                    foreach (var line in File.ReadAllLines(ConfigPath))
                    {
                        if (line.StartsWith("esp32=")) esp32Host = line.Substring(6).Trim();
                        if (line.StartsWith("port=")) int.TryParse(line.Substring(5).Trim(), out bridgePort);
                    }
            }
            catch { }
        }

        private void SaveConfig()
        {
            try { File.WriteAllText(ConfigPath, string.Format("esp32={0}\r\nport={1}\r\n", esp32Host, bridgePort)); } catch { }
        }

        private void CreateConfigForm()
        {
            configForm = new Form();
            configForm.Text = "VibePet Bridge";
            configForm.Size = new Size(380, 220);
            configForm.StartPosition = FormStartPosition.CenterScreen;
            configForm.FormBorderStyle = FormBorderStyle.FixedDialog;
            configForm.MaximizeBox = false;

            var lbl = new Label { Text = "ESP32 IP Address", Location = new Point(20, 20), Size = new Size(340, 20) };
            txtEsp32Ip = new TextBox { Text = esp32Host, Location = new Point(20, 45), Size = new Size(340, 24), Font = new Font("Consolas", 12) };
            lblStatus = new Label { Text = "", Location = new Point(20, 85), Size = new Size(340, 20), ForeColor = Color.Gray };

            var btnSave = new Button { Text = "Save & Restart", Location = new Point(20, 115), Size = new Size(160, 35) };
            btnSave.Click += (s, e) =>
            {
                if (string.IsNullOrWhiteSpace(txtEsp32Ip.Text)) return;
                esp32Host = txtEsp32Ip.Text.Trim();
                SaveConfig(); RestartBridge();
                lblStatus.Text = "Saved & restarted"; lblStatus.ForeColor = Color.Green;
            };

            var btnClose = new Button { Text = "Minimize", Location = new Point(195, 115), Size = new Size(160, 35) };
            btnClose.Click += (s, e) => configForm.Hide();

            configForm.Controls.Add(lbl); configForm.Controls.Add(txtEsp32Ip);
            configForm.Controls.Add(lblStatus); configForm.Controls.Add(btnSave); configForm.Controls.Add(btnClose);
        }

        private void CreateTray()
        {
            var menu = new ContextMenuStrip();
            statusItem = new ToolStripMenuItem("Bridge: stopped") { Enabled = false };
            menu.Items.Add("VibePet Bridge", null, null).Enabled = false;
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add(statusItem);
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add("Settings", null, (s, e) => { configForm.Show(); configForm.Activate(); });
            menu.Items.Add("Exit", null, OnExit);

            tray = new NotifyIcon();
            tray.Icon = SystemIcons.Application;
            tray.Text = "VibePet Bridge";
            tray.Visible = true;
            tray.ContextMenuStrip = menu;
            tray.DoubleClick += (s, e) => { configForm.Show(); configForm.Activate(); };
        }

        private void StartBridge()
        {
            if (running) return;
            try
            {
                listener = new HttpListener();
                listener.Prefixes.Add(string.Format("http://127.0.0.1:{0}/", bridgePort));
                listener.Start();
                running = true;
                serverThread = new Thread(HandleRequests) { IsBackground = true };
                serverThread.Start();
                UpdateStatus("running", Color.Green);
                tray.ShowBalloonTip(1000, "VibePet Bridge", string.Format(":{0} -> {1}", bridgePort, esp32Host), ToolTipIcon.Info);
            }
            catch (Exception ex)
            {
                UpdateStatus(string.Format("error: {0}", ex.Message), Color.Red);
            }
        }

        private void StopBridge()
        {
            running = false;
            try { listener.Stop(); } catch { }
            try { listener.Close(); } catch { }
            UpdateStatus("stopped", Color.Gray);
        }

        private void RestartBridge()
        {
            StopBridge(); Thread.Sleep(500); StartBridge();
        }

        // ─── 核心：选择要显示的 agent ──────────────────────
        //  规则：有活跃的 agent 就显示它，全空闲时显示最后活跃的那个
        private AgentState PickAgent()
        {
            AgentState active = null;
            AgentState idle = null;

            lock (agents)
            {
                foreach (var a in agents.Values)
                {
                    if (a.State != "idle" && a.State != "sleeping")
                    {
                        active = a;
                        break;
                    }
                }

                if (active != null)
                {
                    lastActiveAgent = active.AgentName;
                    return active;
                }

                // 全空闲 → 返回最后活跃的那个 agent
                if (!string.IsNullOrEmpty(lastActiveAgent) && agents.ContainsKey(lastActiveAgent))
                    return agents[lastActiveAgent];

                // 从来没有活跃过 → 返回第一个
                if (agents.Count > 0)
                    return agents.Values.First();
            }
            return null;
        }

        private void HandleRequests()
        {
            while (running)
            {
                try { var ctx = listener.GetContext(); ProcessRequest(ctx); }
                catch { break; }
            }
        }

        private void ProcessRequest(HttpListenerContext ctx)
        {
            var req = ctx.Request;
            var res = ctx.Response;
            var path = req.Url.AbsolutePath.ToLower();
            res.AppendHeader("access-control-allow-origin", "*");
            if (req.HttpMethod == "OPTIONS") { res.StatusCode = 204; res.Close(); return; }

            if ((path == "/api/hook" || path == "/state") && req.HttpMethod == "POST")
            {
                try
                {
                    var body = new StreamReader(req.InputStream).ReadToEnd();
                    var d = SimpleJson(body);
                    var id = Get(d, "agentId") ?? Get(d, "agent") ?? "unknown";
                    var name = Get(d, "agentName") ?? Get(d, "agent") ?? id;
                    var state = Get(d, "state") ?? "idle";
                    var evt = Get(d, "event") ?? "";
                    var output = Get(d, "output") ?? Get(d, "text") ?? "";

                    lock (agents)
                    {
                        agents[id] = new AgentState
                        {
                            AgentId = id, AgentName = name, State = state,
                            Event = evt, Output = output.Length > 200 ? output.Substring(0, 200) : output,
                            UpdatedAt = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()
                        };
                    }
                    PushToESP32();
                }
                catch { }

                byte[] ok = Encoding.UTF8.GetBytes("{\"ok\":true}");
                res.StatusCode = 200; res.ContentType = "application/json";
                res.OutputStream.Write(ok, 0, ok.Length); res.Close();
                return;
            }

            if (path == "/api/device-snapshot" && req.HttpMethod == "GET")
            {
                var sb = new StringBuilder("{\"pets\":[");
                bool first = true;
                lock (agents)
                {
                    foreach (var a in agents.Values)
                    {
                        if (!first) sb.Append(","); first = false;
                        sb.Append("{");
                        sb.Append(J("agentId", a.AgentId));
                        sb.Append(","); sb.Append(J("agentName", a.AgentName));
                        sb.Append(","); sb.Append(J("state", a.State));
                        sb.Append(","); sb.Append(J("output", a.Output));
                        sb.Append("}");
                    }
                }
                sb.Append("]}");
                byte[] data = Encoding.UTF8.GetBytes(sb.ToString());
                res.StatusCode = 200; res.ContentType = "application/json";
                res.OutputStream.Write(data, 0, data.Length); res.Close();
                return;
            }

            res.StatusCode = 404; res.Close();
        }

        private void PushToESP32()
        {
            try
            {
                var agg = PickAgent();
                if (agg == null) return;

                string key = string.Format("{0}|{1}|{2}", agg.State, agg.AgentName,
                    (agg.Output != null && agg.Output.Length > 30) ? agg.Output.Substring(0, 30) : agg.Output);
                if (key == lastPushKey) return;
                lastPushKey = key;

                string json = string.Format(
                    "{{\"agent\":\"{0}\",\"state\":\"{1}\",\"output\":\"{2}\"}}",
                    Esc(agg.AgentName), Esc(agg.State), Esc(agg.Output));
                byte[] data = Encoding.UTF8.GetBytes(json);

                var req = (HttpWebRequest)WebRequest.Create(string.Format("http://{0}:80/api/state", esp32Host));
                req.Method = "POST"; req.ContentType = "application/json";
                req.Timeout = 2000; req.Proxy = null;
                using (var s = req.GetRequestStream()) s.Write(data, 0, data.Length);
                using (var resp = req.GetResponse()) { }

                // 更新托盘
                UpdateStatus(string.Format("{0}:{1}", agg.AgentName, agg.State), Color.Green);
            }
            catch { }
        }

        private void UpdateStatus(string text, Color color)
        {
            if (InvokeRequired) { BeginInvoke(new Action(() => UpdateStatus(text, color))); return; }
            statusItem.Text = text;
            statusItem.ForeColor = color;
            tray.Text = text;
            if (lblStatus != null && !lblStatus.IsDisposed)
            { lblStatus.Text = text; lblStatus.ForeColor = color; }
        }

        private string Esc(string s)
        {
            if (s == null) return "";
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }

        private string J(string key, string value)
        {
            return string.Format("\"{0}\":\"{1}\"", key, Esc(value));
        }

        private Dictionary<string, string> SimpleJson(string json)
        {
            var r = new Dictionary<string, string>();
            var m = System.Text.RegularExpressions.Regex.Matches(json, "\"([^\"]+)\"\\s*:\\s*\"([^\"]*)\"");
            foreach (System.Text.RegularExpressions.Match match in m)
                if (match.Groups.Count >= 3) r[match.Groups[1].Value] = match.Groups[2].Value;
            return r;
        }

        private string Get(Dictionary<string, string> d, string k)
        {
            string v; d.TryGetValue(k, out v); return v;
        }

        private void OnExit(object sender, EventArgs e)
        {
            StopBridge(); tray.Visible = false; Application.Exit();
        }
    }
}
