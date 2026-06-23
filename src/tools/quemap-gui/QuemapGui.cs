// QuemapGui — a zero-install Windows front-end for Quetoo map tooling.
//
// Targets .NET Framework 4.8 (ships with Windows 10/11) so the built .exe runs
// anywhere with nothing installed. Written in C# 5-compatible style so it builds
// with the in-box compiler (see build.bat) without Visual Studio or the SDK.
//
// Two tabs:
//   Compile  — bulk-compile .map -> .bsp via quemap (the Windows equivalent of
//              running `make` in quetoo-data).
//   Convert  — convert .map texture coords between standard Quake3 and Valve-220,
//              ported 1:1 from the map-fu script (and quemap's texture.c).

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Text;
using System.Threading;
using System.Windows.Forms;

namespace QuemapGui
{
  internal static class Program
  {
    [STAThread]
    private static void Main()
    {
      Application.EnableVisualStyles();
      Application.SetCompatibleTextRenderingDefault(false);
      Application.Run(new MainForm());
    }
  }

  // ===================================================================
  // Shared helpers
  // ===================================================================

  internal static class Ini
  {
    private static readonly string Path_ = Path.Combine(
      Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
      "QuemapGui", "settings.ini");

    public static Dictionary<string, string> Load()
    {
      Dictionary<string, string> d = new Dictionary<string, string>();
      try
      {
        if (File.Exists(Path_))
        {
          foreach (string line in File.ReadAllLines(Path_))
          {
            int eq = line.IndexOf('=');
            if (eq > 0) d[line.Substring(0, eq)] = line.Substring(eq + 1);
          }
        }
      }
      catch { }
      return d;
    }

    public static void Save(Dictionary<string, string> d)
    {
      try
      {
        string dir = Path.GetDirectoryName(Path_);
        if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
        List<string> lines = new List<string>();
        foreach (KeyValuePair<string, string> kv in d)
          lines.Add(kv.Key + "=" + kv.Value);
        File.WriteAllLines(Path_, lines.ToArray());
      }
      catch { }
    }

    public static string Get(Dictionary<string, string> d, string key, string def)
    {
      string v;
      return d.TryGetValue(key, out v) ? v : def;
    }
  }

  internal static class MapFiles
  {
    // Resolve a ';'-separated list of files/folders into a list of .map paths.
    public static List<string> Collect(string raw, bool recurse)
    {
      List<string> maps = new List<string>();
      if (raw == null) return maps;
      raw = raw.Trim();
      if (raw.Length == 0) return maps;

      SearchOption opt = recurse ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;
      foreach (string entryRaw in raw.Split(';'))
      {
        string entry = entryRaw.Trim();
        if (entry.Length == 0) continue;
        if (Directory.Exists(entry))
        {
          try { maps.AddRange(Directory.GetFiles(entry, "*.map", opt)); }
          catch { }
        }
        else if (File.Exists(entry) && entry.EndsWith(".map", StringComparison.OrdinalIgnoreCase))
        {
          maps.Add(entry);
        }
      }
      return maps;
    }
  }

  // A flat progress bar that draws a centered, bold caption over the bar
  // (the stock ProgressBar can't host text).
  internal sealed class TextProgressBar : Control
  {
    private int _value;
    private int _maximum = 100;

    public TextProgressBar()
    {
      SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer
             | ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
    }

    public int Maximum
    {
      get { return _maximum; }
      set { _maximum = Math.Max(1, value); Invalidate(); }
    }

    public int Value
    {
      get { return _value; }
      set { _value = Math.Max(0, Math.Min(value, _maximum)); Invalidate(); }
    }

    protected override void OnTextChanged(EventArgs e) { base.OnTextChanged(e); Invalidate(); }

    protected override void OnPaint(PaintEventArgs e)
    {
      Graphics g = e.Graphics;
      Rectangle r = ClientRectangle;

      using (SolidBrush track = new SolidBrush(Color.FromArgb(221, 221, 221)))
        g.FillRectangle(track, r);

      int fw = (int) ((long) r.Width * _value / _maximum);
      if (fw > 0)
        using (SolidBrush fill = new SolidBrush(Color.FromArgb(76, 175, 80)))
          g.FillRectangle(fill, new Rectangle(r.X, r.Y, fw, r.Height));

      using (Pen border = new Pen(Color.FromArgb(150, 150, 150)))
        g.DrawRectangle(border, new Rectangle(r.X, r.Y, r.Width - 1, r.Height - 1));

      if (!string.IsNullOrEmpty(Text))
        using (Font f = new Font(Font, FontStyle.Bold))
          TextRenderer.DrawText(g, Text, f, r, Color.Black,
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
    }
  }

  // ===================================================================
  // Main window: hosts the two feature tabs
  // ===================================================================

  internal sealed class MainForm : Form
  {
    private readonly CompileControl _compile;
    private readonly ConvertControl _convert;

    public MainForm()
    {
      Text = "Quemap GUI";
      ClientSize = new Size(940, 700);
      MinimumSize = new Size(720, 520);
      Font = new Font("Segoe UI", 9f);

      TabControl tabs = new TabControl();
      tabs.Dock = DockStyle.Fill;

      TabPage tpConvert = new TabPage("Convert");
      TabPage tpCompile = new TabPage("Compile");
      tabs.TabPages.Add(tpConvert);
      tabs.TabPages.Add(tpCompile);
      Controls.Add(tabs);

      _compile = new CompileControl();
      _compile.Dock = DockStyle.Fill;
      tpCompile.Controls.Add(_compile);

      _convert = new ConvertControl();
      _convert.Dock = DockStyle.Fill;
      tpConvert.Controls.Add(_convert);

      Dictionary<string, string> s = Ini.Load();
      _compile.LoadSettings(s);
      _convert.LoadSettings(s);

      FormClosing += delegate
      {
        _compile.OnClosing();
        _convert.OnClosing();
        Dictionary<string, string> d = new Dictionary<string, string>();
        _compile.SaveSettings(d);
        _convert.SaveSettings(d);
        Ini.Save(d);
      };
    }
  }

  // ===================================================================
  // Compile tab
  // ===================================================================

  internal sealed class CompileControl : UserControl
  {
    private readonly TextBox _txtRoot;
    private readonly Label _lblQuemap;
    private readonly TextBox _txtMaps;
    private readonly CheckBox _chkDirty;
    private readonly CheckBox _chkRecurse;
    private readonly TextBox _txtOut;
    private readonly TextProgressBar _prog;
    private readonly Label _lblSummary;
    private readonly Button _btnCompile;
    private readonly Button _btnCancel;

    private string _quemapPath;
    private Thread _worker;
    private volatile bool _cancel;
    private Process _current;
    private readonly object _procLock = new object();

    // Output is produced on background threads and buffered here; a UI timer
    // flushes it in batches. Progress lines (ending in "%]") collapse into a
    // single live status line instead of spamming the log.
    private readonly object _bufLock = new object();
    private readonly StringBuilder _logBuf = new StringBuilder();
    private string _statusLine = "";
    private System.Windows.Forms.Timer _flushTimer;

    public CompileControl()
    {
      Size = new Size(900, 610);

      // Fill control added first so docked edges claim their space.
      _txtOut = new TextBox();
      _txtOut.Multiline = true;
      _txtOut.ReadOnly = true;
      _txtOut.ScrollBars = ScrollBars.Both;
      _txtOut.WordWrap = false;
      _txtOut.BackColor = Color.FromArgb(30, 30, 30);
      _txtOut.ForeColor = Color.Gainsboro;
      _txtOut.Font = new Font("Consolas", 9f);
      _txtOut.Dock = DockStyle.Fill;
      Controls.Add(_txtOut);

      Panel top = new Panel();
      top.Dock = DockStyle.Top;
      top.Height = 126;
      Controls.Add(top);

      Panel bottom = new Panel();
      bottom.Dock = DockStyle.Bottom;
      bottom.Height = 92;
      Controls.Add(bottom);

      int w = Width;

      Label lblRoot = new Label();
      lblRoot.Text = "Quetoo root:";
      lblRoot.SetBounds(10, 14, 90, 20);
      top.Controls.Add(lblRoot);

      _txtRoot = new TextBox();
      _txtRoot.SetBounds(102, 11, w - 102 - 104, 23);
      _txtRoot.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
      _txtRoot.Leave += delegate { ResolveQuemap(); };
      top.Controls.Add(_txtRoot);

      Button btnRoot = new Button();
      btnRoot.Text = "Browse…";
      btnRoot.SetBounds(w - 10 - 88, 10, 88, 25);
      btnRoot.Anchor = AnchorStyles.Top | AnchorStyles.Right;
      btnRoot.Click += delegate { BrowseRoot(); };
      top.Controls.Add(btnRoot);

      _lblQuemap = new Label();
      _lblQuemap.SetBounds(102, 40, w - 114, 18);
      _lblQuemap.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
      _lblQuemap.ForeColor = Color.DimGray;
      _lblQuemap.Text = "quemap.exe: (set the quetoo root)";
      top.Controls.Add(_lblQuemap);

      Label lblMaps = new Label();
      lblMaps.Text = "Map(s):";
      lblMaps.SetBounds(10, 70, 90, 20);
      top.Controls.Add(lblMaps);

      _txtMaps = new TextBox();
      _txtMaps.SetBounds(102, 67, w - 102 - 200, 23);
      _txtMaps.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
      top.Controls.Add(_txtMaps);

      Button btnFile = new Button();
      btnFile.Text = "File…";
      btnFile.SetBounds(w - 10 - 184, 66, 88, 25);
      btnFile.Anchor = AnchorStyles.Top | AnchorStyles.Right;
      btnFile.Click += delegate { BrowseMapFile(); };
      top.Controls.Add(btnFile);

      Button btnFolder = new Button();
      btnFolder.Text = "Folder…";
      btnFolder.SetBounds(w - 10 - 88, 66, 88, 25);
      btnFolder.Anchor = AnchorStyles.Top | AnchorStyles.Right;
      btnFolder.Click += delegate { BrowseMapFolder(); };
      top.Controls.Add(btnFolder);

      _chkDirty = new CheckBox();
      _chkDirty.Text = "Only changed (dirty) maps";
      _chkDirty.Checked = true;
      _chkDirty.SetBounds(102, 96, 200, 22);
      top.Controls.Add(_chkDirty);

      _chkRecurse = new CheckBox();
      _chkRecurse.Text = "Include subfolders";
      _chkRecurse.Checked = true;
      _chkRecurse.SetBounds(312, 96, 160, 22);
      top.Controls.Add(_chkRecurse);

      _prog = new TextProgressBar();
      _prog.SetBounds(10, 8, w - 20, 26);
      _prog.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
      bottom.Controls.Add(_prog);

      _lblSummary = new Label();
      _lblSummary.SetBounds(10, 42, w - 220, 22);
      _lblSummary.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
      _lblSummary.ForeColor = Color.DimGray;
      bottom.Controls.Add(_lblSummary);

      _btnCancel = new Button();
      _btnCancel.Text = "Cancel";
      _btnCancel.Enabled = false;
      _btnCancel.SetBounds(w - 10 - 88, 56, 88, 28);
      _btnCancel.Anchor = AnchorStyles.Top | AnchorStyles.Right;
      _btnCancel.Click += delegate { CancelCompile(); };
      bottom.Controls.Add(_btnCancel);

      _btnCompile = new Button();
      _btnCompile.Text = "Compile";
      _btnCompile.SetBounds(w - 10 - 184, 56, 88, 28);
      _btnCompile.Anchor = AnchorStyles.Top | AnchorStyles.Right;
      _btnCompile.Click += delegate { StartCompile(); };
      bottom.Controls.Add(_btnCompile);

      _flushTimer = new System.Windows.Forms.Timer();
      _flushTimer.Interval = 100;
      _flushTimer.Tick += delegate { FlushLog(); };
    }

    // ---- quemap.exe resolution ----

    private void BrowseRoot()
    {
      using (FolderBrowserDialog dlg = new FolderBrowserDialog())
      {
        dlg.Description = "Select the Quetoo root directory";
        if (Directory.Exists(_txtRoot.Text)) dlg.SelectedPath = _txtRoot.Text;
        if (dlg.ShowDialog(this) == DialogResult.OK)
        {
          _txtRoot.Text = dlg.SelectedPath;
          ResolveQuemap();
        }
      }
    }

    private void ResolveQuemap()
    {
      _quemapPath = FindQuemap(_txtRoot.Text);
      if (_quemapPath != null)
      {
        _lblQuemap.ForeColor = Color.ForestGreen;
        _lblQuemap.Text = "quemap.exe: " + _quemapPath;
      }
      else
      {
        _lblQuemap.ForeColor = Color.Firebrick;
        _lblQuemap.Text = "quemap.exe: NOT FOUND under this root";
      }
    }

    private static string FindQuemap(string root)
    {
      if (string.IsNullOrEmpty(root) || !Directory.Exists(root)) return null;
      string[] common = { Path.Combine(root, "bin\\quemap.exe"), Path.Combine(root, "quemap.exe") };
      foreach (string c in common) if (File.Exists(c)) return c;
      try
      {
        string best = null;
        DateTime bestTime = DateTime.MinValue;
        foreach (string f in Directory.GetFiles(root, "quemap.exe", SearchOption.AllDirectories))
        {
          DateTime t = File.GetLastWriteTimeUtc(f);
          if (best == null || t > bestTime) { best = f; bestTime = t; }
        }
        return best;
      }
      catch { return null; }
    }

    // ---- map selection ----

    private void BrowseMapFile()
    {
      using (OpenFileDialog dlg = new OpenFileDialog())
      {
        dlg.Title = "Select .map file(s)";
        dlg.Filter = "Quake map files (*.map)|*.map|All files (*.*)|*.*";
        dlg.Multiselect = true;
        if (dlg.ShowDialog(this) == DialogResult.OK)
          _txtMaps.Text = string.Join(";", dlg.FileNames);
      }
    }

    private void BrowseMapFolder()
    {
      using (FolderBrowserDialog dlg = new FolderBrowserDialog())
      {
        dlg.Description = "Select a folder of .map files (e.g. a 'maps' directory)";
        if (dlg.ShowDialog(this) == DialogResult.OK) _txtMaps.Text = dlg.SelectedPath;
      }
    }

    // ---- game-dir / argument resolution (mirrors quemap's maps/ requirement) ----

    private static bool ResolveMap(string mapFullPath, out string gameDir, out string relArg, out string error)
    {
      gameDir = null; relArg = null; error = null;
      string full;
      try { full = Path.GetFullPath(mapFullPath); }
      catch (Exception ex) { error = ex.Message; return false; }

      string[] parts = full.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
      int mapsIdx = -1;
      for (int i = parts.Length - 2; i >= 0; i--)
      {
        if (string.Equals(parts[i], "maps", StringComparison.OrdinalIgnoreCase)) { mapsIdx = i; break; }
      }
      if (mapsIdx < 0) { error = "not inside a 'maps' folder (quemap needs maps/<name>.map)"; return false; }

      gameDir = string.Join(Path.DirectorySeparatorChar.ToString(), parts, 0, mapsIdx);
      if (gameDir.EndsWith(":")) gameDir += Path.DirectorySeparatorChar;
      relArg = string.Join("/", parts, mapsIdx, parts.Length - mapsIdx);
      return true;
    }

    private static bool IsDirty(string mapPath)
    {
      string bsp = Path.ChangeExtension(mapPath, ".bsp");
      if (!File.Exists(bsp)) return true;
      return File.GetLastWriteTimeUtc(mapPath) > File.GetLastWriteTimeUtc(bsp);
    }

    // ---- compile driver ----

    private void StartCompile()
    {
      if (_worker != null && _worker.IsAlive) return;
      ResolveQuemap();
      if (_quemapPath == null)
      {
        MessageBox.Show(this, "Could not find quemap.exe under the quetoo root.",
          "Quemap GUI", MessageBoxButtons.OK, MessageBoxIcon.Error);
        return;
      }
      List<string> maps = MapFiles.Collect(_txtMaps.Text, _chkRecurse.Checked);
      if (maps.Count == 0)
      {
        MessageBox.Show(this, "No .map files selected.",
          "Quemap GUI", MessageBoxButtons.OK, MessageBoxIcon.Information);
        return;
      }

      bool dirtyOnly = _chkDirty.Checked;
      string quemap = _quemapPath;

      _txtOut.Clear();
      lock (_bufLock) { _logBuf.Clear(); _statusLine = ""; }
      _flushTimer.Start();
      _cancel = false;
      _btnCompile.Enabled = false;
      _btnCancel.Enabled = true;
      _prog.Value = 0;
      _prog.Maximum = maps.Count;

      _worker = new Thread(delegate () { CompileAll(quemap, maps, dirtyOnly); });
      _worker.IsBackground = true;
      _worker.Start();
    }

    private void CompileAll(string quemap, List<string> maps, bool dirtyOnly)
    {
      int done = 0, ok = 0, failed = 0, skipped = 0;
      foreach (string map in maps)
      {
        if (_cancel) break;
        done++;
        SetProgress(done, Path.GetFileNameWithoutExtension(map));

        if (dirtyOnly && !IsDirty(map))
        {
          skipped++;
          Append("--- SKIP (up to date): " + map + "\r\n");
          continue;
        }

        string gameDir, relArg, err;
        if (!ResolveMap(map, out gameDir, out relArg, out err))
        {
          failed++;
          Append("--- ERROR: " + map + "  (" + err + ")\r\n");
          continue;
        }

        string args = "-w \"" + gameDir + "\" -bsp \"" + relArg + "\"";
        Append("\r\n=== [" + done + "/" + maps.Count + "] " + relArg + " ===\r\n");
        Append("> \"" + quemap + "\" " + args + "\r\n");

        int exit = RunQuemap(quemap, args, gameDir);
        if (_cancel) { Append("--- CANCELLED\r\n"); break; }
        if (exit == 0) { ok++; Append("--- OK\r\n"); }
        else { failed++; Append("--- FAILED (exit " + exit + ")\r\n"); }
      }
      string summary = ok + " compiled, " + skipped + " skipped, " + failed + " failed"
                       + (_cancel ? " (cancelled)" : "");
      Append("\r\n=== Done: " + summary + " ===\r\n");
      FinishUi(summary);
    }

    private int RunQuemap(string quemap, string args, string workingDir)
    {
      try
      {
        ProcessStartInfo psi = new ProcessStartInfo();
        psi.FileName = quemap;
        psi.Arguments = args;
        psi.WorkingDirectory = workingDir;
        psi.UseShellExecute = false;
        psi.CreateNoWindow = true;
        psi.RedirectStandardOutput = true;
        psi.RedirectStandardError = true;

        Process p = new Process();
        p.StartInfo = psi;
        p.OutputDataReceived += delegate (object s, DataReceivedEventArgs e) { if (e.Data != null) OnStdout(e.Data); };
        p.ErrorDataReceived += delegate (object s, DataReceivedEventArgs e) { if (e.Data != null) OnStderr(e.Data); };

        p.Start();
        lock (_procLock) { _current = p; }
        p.BeginOutputReadLine();
        p.BeginErrorReadLine();
        p.WaitForExit();
        int code = p.ExitCode;
        lock (_procLock) { _current = null; }
        p.Close();
        return code;
      }
      catch (Exception ex)
      {
        Append("--- launch error: " + ex.Message + "\r\n");
        return -1;
      }
    }

    private void CancelCompile()
    {
      _cancel = true;
      lock (_procLock)
      {
        if (_current != null) { try { _current.Kill(); } catch { } }
      }
    }

    public void OnClosing() { CancelCompile(); if (_flushTimer != null) _flushTimer.Stop(); }

    // ---- thread-safe UI ----

    // Called from the worker thread for our own log lines (headers, OK/FAILED).
    private void Append(string text)
    {
      lock (_bufLock) { _logBuf.Append(text); }
    }

    // quemap stdout: progress (".. [ NN%]") becomes the live status line; the
    // rest is logged.
    private void OnStdout(string line)
    {
      if (IsProgress(line)) { lock (_bufLock) { _statusLine = line; } }
      else { lock (_bufLock) { _logBuf.Append(line).Append("\r\n"); } }
    }

    // quemap stderr (warnings/errors) is always logged.
    private void OnStderr(string line)
    {
      lock (_bufLock) { _logBuf.Append(line).Append("\r\n"); }
    }

    private static bool IsProgress(string line)
    {
      string t = line.TrimEnd();
      // Percentage ticks ("[ NN%]") and spinner frames ("[-] [\] [|] [/]").
      // The completion line ("[100%] N ms") ends in "ms", so it stays in the log.
      return t.EndsWith("%]")
          || t.EndsWith("[-]") || t.EndsWith("[\\]")
          || t.EndsWith("[|]") || t.EndsWith("[/]");
    }

    // UI thread (timer tick): flush buffered log in one append, update status.
    private void FlushLog()
    {
      string chunk, status;
      lock (_bufLock) { chunk = _logBuf.ToString(); _logBuf.Clear(); status = _statusLine; }
      if (chunk.Length > 0 && !_txtOut.IsDisposed) _txtOut.AppendText(chunk);
      if (!_lblSummary.IsDisposed) _lblSummary.Text = status;
    }

    private void SetProgress(int value, string label)
    {
      if (_prog.IsDisposed) return;
      if (_prog.InvokeRequired) { try { _prog.BeginInvoke((Action)delegate { SetProgress(value, label); }); } catch { } return; }
      _prog.Value = Math.Min(value, _prog.Maximum);
      _prog.Text = label;
    }

    private void FinishUi(string summary)
    {
      if (IsDisposed) return;
      if (InvokeRequired) { try { BeginInvoke((Action)delegate { FinishUi(summary); }); } catch { } return; }
      _flushTimer.Stop();
      FlushLog();                       // drain anything still buffered
      lock (_bufLock) { _statusLine = ""; }
      _prog.Text = "";
      _lblSummary.Text = summary;
      _btnCompile.Enabled = true;
      _btnCancel.Enabled = false;
    }

    // ---- settings ----

    public void LoadSettings(Dictionary<string, string> s)
    {
      _txtRoot.Text = Ini.Get(s, "root", "");
      _txtMaps.Text = Ini.Get(s, "maps", "");
      _chkDirty.Checked = Ini.Get(s, "dirty", "1") == "1";
      _chkRecurse.Checked = Ini.Get(s, "recurse", "1") == "1";
      if (Directory.Exists(_txtRoot.Text)) ResolveQuemap();
    }

    public void SaveSettings(Dictionary<string, string> s)
    {
      s["root"] = _txtRoot.Text;
      s["maps"] = _txtMaps.Text;
      s["dirty"] = _chkDirty.Checked ? "1" : "0";
      s["recurse"] = _chkRecurse.Checked ? "1" : "0";
    }
  }

  // ===================================================================
  // Convert tab
  // ===================================================================

  internal sealed class ConvertControl : UserControl
  {
    private readonly TextBox _txtMaps;
    private readonly RadioButton _rbToValve;
    private readonly RadioButton _rbToStd;
    private readonly RadioButton _rbSuffix;
    private readonly RadioButton _rbInplace;
    private readonly CheckBox _chkSnap;
    private readonly TextBox _txtOffInc;
    private readonly TextBox _txtRotInc;
    private readonly CheckBox _chkRecurse;
    private readonly TextBox _txtOut;
    private readonly TextProgressBar _prog;
    private readonly Label _lblSummary;
    private readonly Button _btnConvert;
    private readonly Button _btnCancel;

    private Thread _worker;
    private volatile bool _cancel;

    public ConvertControl()
    {
      Size = new Size(900, 610);

      _txtOut = new TextBox();
      _txtOut.Multiline = true;
      _txtOut.ReadOnly = true;
      _txtOut.ScrollBars = ScrollBars.Both;
      _txtOut.WordWrap = false;
      _txtOut.BackColor = Color.FromArgb(30, 30, 30);
      _txtOut.ForeColor = Color.Gainsboro;
      _txtOut.Font = new Font("Consolas", 9f);
      _txtOut.Dock = DockStyle.Fill;
      Controls.Add(_txtOut);

      Panel top = new Panel();
      top.Dock = DockStyle.Top;
      top.Height = 176;
      Controls.Add(top);

      Panel bottom = new Panel();
      bottom.Dock = DockStyle.Bottom;
      bottom.Height = 92;
      Controls.Add(bottom);

      int w = Width;

      Label lblMaps = new Label();
      lblMaps.Text = "Map(s):";
      lblMaps.SetBounds(10, 14, 70, 20);
      top.Controls.Add(lblMaps);

      _txtMaps = new TextBox();
      _txtMaps.SetBounds(82, 11, w - 82 - 200, 23);
      _txtMaps.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
      top.Controls.Add(_txtMaps);

      Button btnFile = new Button();
      btnFile.Text = "File…";
      btnFile.SetBounds(w - 10 - 184, 10, 88, 25);
      btnFile.Anchor = AnchorStyles.Top | AnchorStyles.Right;
      btnFile.Click += delegate { BrowseFile(); };
      top.Controls.Add(btnFile);

      Button btnFolder = new Button();
      btnFolder.Text = "Folder…";
      btnFolder.SetBounds(w - 10 - 88, 10, 88, 25);
      btnFolder.Anchor = AnchorStyles.Top | AnchorStyles.Right;
      btnFolder.Click += delegate { BrowseFolder(); };
      top.Controls.Add(btnFolder);

      Label lblDir = new Label();
      lblDir.Text = "Target:";
      lblDir.SetBounds(10, 48, 70, 20);
      top.Controls.Add(lblDir);

      // Each radio set lives in its own Panel: WinForms groups RadioButtons by
      // their immediate parent, so without this all radios would be one group.
      Panel pnlTarget = new Panel();
      pnlTarget.SetBounds(82, 44, 340, 26);
      top.Controls.Add(pnlTarget);

      _rbToValve = new RadioButton();
      _rbToValve.Text = "Valve-220";
      _rbToValve.Checked = true;
      _rbToValve.SetBounds(0, 2, 130, 22);
      pnlTarget.Controls.Add(_rbToValve);

      _rbToStd = new RadioButton();
      _rbToStd.Text = "Quake3 (standard)";
      _rbToStd.SetBounds(138, 2, 180, 22);
      pnlTarget.Controls.Add(_rbToStd);

      Label lblOut = new Label();
      lblOut.Text = "Output:";
      lblOut.SetBounds(10, 82, 70, 20);
      top.Controls.Add(lblOut);

      Panel pnlOutput = new Panel();
      pnlOutput.SetBounds(82, 78, 500, 26);
      top.Controls.Add(pnlOutput);

      _rbSuffix = new RadioButton();
      _rbSuffix.Text = "New file (<name>.valve/.quake3.map)";
      _rbSuffix.Checked = true;
      _rbSuffix.SetBounds(0, 2, 260, 22);
      pnlOutput.Controls.Add(_rbSuffix);

      _rbInplace = new RadioButton();
      _rbInplace.Text = "Overwrite in place";
      _rbInplace.SetBounds(278, 2, 200, 22);
      pnlOutput.Controls.Add(_rbInplace);

      _chkSnap = new CheckBox();
      _chkSnap.Text = "Snap offsets + rotation to grid";
      _chkSnap.SetBounds(82, 114, 230, 22);
      top.Controls.Add(_chkSnap);

      Label lblOff = new Label();
      lblOff.Text = "offset:";
      lblOff.SetBounds(322, 116, 44, 20);
      top.Controls.Add(lblOff);

      _txtOffInc = new TextBox();
      _txtOffInc.Text = "0.125";
      _txtOffInc.SetBounds(366, 113, 56, 23);
      top.Controls.Add(_txtOffInc);

      Label lblRot = new Label();
      lblRot.Text = "rotation°:";
      lblRot.SetBounds(434, 116, 60, 20);
      top.Controls.Add(lblRot);

      _txtRotInc = new TextBox();
      _txtRotInc.Text = "1";
      _txtRotInc.SetBounds(496, 113, 50, 23);
      top.Controls.Add(_txtRotInc);

      _chkRecurse = new CheckBox();
      _chkRecurse.Text = "Include subfolders";
      _chkRecurse.Checked = true;
      _chkRecurse.SetBounds(82, 146, 200, 22);
      top.Controls.Add(_chkRecurse);

      _prog = new TextProgressBar();
      _prog.SetBounds(10, 8, w - 20, 26);
      _prog.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
      bottom.Controls.Add(_prog);

      _lblSummary = new Label();
      _lblSummary.SetBounds(10, 42, w - 220, 22);
      _lblSummary.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
      _lblSummary.ForeColor = Color.DimGray;
      bottom.Controls.Add(_lblSummary);

      _btnCancel = new Button();
      _btnCancel.Text = "Cancel";
      _btnCancel.Enabled = false;
      _btnCancel.SetBounds(w - 10 - 88, 56, 88, 28);
      _btnCancel.Anchor = AnchorStyles.Top | AnchorStyles.Right;
      _btnCancel.Click += delegate { _cancel = true; };
      bottom.Controls.Add(_btnCancel);

      _btnConvert = new Button();
      _btnConvert.Text = "Convert";
      _btnConvert.SetBounds(w - 10 - 184, 56, 88, 28);
      _btnConvert.Anchor = AnchorStyles.Top | AnchorStyles.Right;
      _btnConvert.Click += delegate { StartConvert(); };
      bottom.Controls.Add(_btnConvert);
    }

    private void BrowseFile()
    {
      using (OpenFileDialog dlg = new OpenFileDialog())
      {
        dlg.Title = "Select .map file(s)";
        dlg.Filter = "Quake map files (*.map)|*.map|All files (*.*)|*.*";
        dlg.Multiselect = true;
        if (dlg.ShowDialog(this) == DialogResult.OK)
          _txtMaps.Text = string.Join(";", dlg.FileNames);
      }
    }

    private void BrowseFolder()
    {
      using (FolderBrowserDialog dlg = new FolderBrowserDialog())
      {
        dlg.Description = "Select a folder of .map files";
        if (dlg.ShowDialog(this) == DialogResult.OK) _txtMaps.Text = dlg.SelectedPath;
      }
    }

    private void StartConvert()
    {
      if (_worker != null && _worker.IsAlive) return;
      List<string> maps = MapFiles.Collect(_txtMaps.Text, _chkRecurse.Checked);
      if (maps.Count == 0)
      {
        MessageBox.Show(this, "No .map files selected.",
          "Quemap GUI", MessageBoxButtons.OK, MessageBoxIcon.Information);
        return;
      }

      string target = _rbToValve.Checked ? "valve" : "standard";
      bool inplace = _rbInplace.Checked;
      bool snap = _chkSnap.Checked;
      double offInc, rotInc;
      if (!double.TryParse(_txtOffInc.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out offInc) || offInc <= 0) offInc = 0.125;
      if (!double.TryParse(_txtRotInc.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out rotInc) || rotInc <= 0) rotInc = 15.0;

      _txtOut.Clear();
      _cancel = false;
      _btnConvert.Enabled = false;
      _btnCancel.Enabled = true;
      _prog.Value = 0;
      _prog.Maximum = maps.Count;

      _worker = new Thread(delegate () { ConvertAll(maps, target, inplace, snap, offInc, rotInc); });
      _worker.IsBackground = true;
      _worker.Start();
    }

    private void ConvertAll(List<string> maps, string target, bool inplace,
                            bool snap, double offInc, double rotInc)
    {
      int done = 0, okFiles = 0, failed = 0, skippedFiles = 0;
      int totalConverted = 0, totalSheared = 0, totalDegenerate = 0, totalSnapped = 0;
      string label = target == "valve" ? "Valve-220" : "standard Quake3";

      foreach (string map in maps)
      {
        if (_cancel) break;
        done++;
        SetProgress(done, Path.GetFileNameWithoutExtension(map));

        try
        {
          string text = File.ReadAllText(map);
          text = text.Replace("\r\n", "\n").Replace("\r", "\n");

          // Already fully in the target format (header and first brush agree) and
          // not snapping? Leave the file untouched. Avoids needless rewrites.
          if (!snap)
          {
            string hf, bf;
            MapConvert.DetectFormats(text, out hf, out bf);
            if (hf == target && bf == target)
            {
              Append(Path.GetFileName(map) + " -> already " + label + ", skipped\r\n");
              skippedFiles++;
              continue;
            }
          }

          MapConvert.Stats stats = new MapConvert.Stats();
          string result = MapConvert.Process(text, target, stats, snap, offInc, rotInc);

          string outFile;
          if (inplace) outFile = map;
          else
          {
            string ext = Path.GetExtension(map);
            string stem = map.Substring(0, map.Length - ext.Length);
            outFile = stem + (target == "valve" ? ".valve" : ".quake3") + ".map";
          }
          File.WriteAllText(outFile, result, new UTF8Encoding(false));

          string line = Path.GetFileName(map) + " -> " + label + ": "
            + stats.Converted + " converted, " + stats.Kept + " already in format";
          if (stats.Snapped > 0) line += ", " + stats.Snapped + " snapped";
          if (stats.Sheared > 0) line += ", " + stats.Sheared + " sheared (reset)";
          if (stats.Degenerate > 0) line += ", " + stats.Degenerate + " degenerate (skipped)";
          Append(line + "\r\n  written: " + outFile + "\r\n");
          foreach (string warn in stats.Warnings) Append("  " + warn + "\r\n");

          totalConverted += stats.Converted;
          totalSheared += stats.Sheared;
          totalDegenerate += stats.Degenerate;
          totalSnapped += stats.Snapped;
          okFiles++;
        }
        catch (Exception ex)
        {
          failed++;
          Append("--- ERROR: " + map + "  (" + ex.Message + ")\r\n");
        }
      }

      string summary = okFiles + " files, " + totalConverted + " faces converted"
        + (totalSnapped > 0 ? ", " + totalSnapped + " snapped" : "")
        + (skippedFiles > 0 ? ", " + skippedFiles + " skipped" : "")
        + (totalSheared > 0 ? ", " + totalSheared + " sheared" : "")
        + (totalDegenerate > 0 ? ", " + totalDegenerate + " degenerate" : "")
        + (failed > 0 ? ", " + failed + " failed" : "")
        + (_cancel ? " (cancelled)" : "");
      Append("\r\n=== Done: " + summary + " ===\r\n");
      FinishUi(summary);
    }

    public void OnClosing() { _cancel = true; }

    private void Append(string text)
    {
      if (_txtOut.IsDisposed) return;
      if (_txtOut.InvokeRequired) { try { _txtOut.BeginInvoke((Action)delegate { Append(text); }); } catch { } return; }
      _txtOut.AppendText(text);
    }

    private void SetProgress(int value, string label)
    {
      if (_prog.IsDisposed) return;
      if (_prog.InvokeRequired) { try { _prog.BeginInvoke((Action)delegate { SetProgress(value, label); }); } catch { } return; }
      _prog.Value = Math.Min(value, _prog.Maximum);
      _prog.Text = label;
    }

    private void FinishUi(string summary)
    {
      if (IsDisposed) return;
      if (InvokeRequired) { try { BeginInvoke((Action)delegate { FinishUi(summary); }); } catch { } return; }
      _prog.Text = "";
      _lblSummary.Text = summary;
      _btnConvert.Enabled = true;
      _btnCancel.Enabled = false;
    }

    public void LoadSettings(Dictionary<string, string> s)
    {
      _txtMaps.Text = Ini.Get(s, "cmaps", "");
      if (Ini.Get(s, "cdir", "valve") == "standard") _rbToStd.Checked = true; else _rbToValve.Checked = true;
      if (Ini.Get(s, "cmode", "suffix") == "inplace") _rbInplace.Checked = true; else _rbSuffix.Checked = true;
      _chkSnap.Checked = Ini.Get(s, "csnap", "0") == "1";
      _txtOffInc.Text = Ini.Get(s, "coff", "0.125");
      _txtRotInc.Text = Ini.Get(s, "crot", "1");
      _chkRecurse.Checked = Ini.Get(s, "crecurse", "1") == "1";
    }

    public void SaveSettings(Dictionary<string, string> s)
    {
      s["cmaps"] = _txtMaps.Text;
      s["cdir"] = _rbToStd.Checked ? "standard" : "valve";
      s["cmode"] = _rbInplace.Checked ? "inplace" : "suffix";
      s["csnap"] = _chkSnap.Checked ? "1" : "0";
      s["coff"] = _txtOffInc.Text;
      s["crot"] = _txtRotInc.Text;
      s["crecurse"] = _chkRecurse.Checked ? "1" : "0";
    }
  }

  // ===================================================================
  // Q3 <-> Valve-220 texture conversion (ported 1:1 from map-fu / texture.c)
  // ===================================================================

  internal static class MapConvert
  {
    public sealed class Stats
    {
      public int Converted;
      public int Kept;
      public int Degenerate;
      public int Sheared;
      public int Snapped;
      public readonly List<string> Warnings = new List<string>();
    }

    // Snap a value to the nearest multiple of inc (inc <= 0 disables).
    private static double SnapTo(double v, double inc)
    {
      if (inc <= 0.0) return v;
      return Math.Round(v / inc, MidpointRounding.AwayFromZero) * inc;
    }

    private sealed class ShearException : Exception { }

    // base_axis[18] from TextureAxisForPlane(): triples of (normal, s_axis, t_axis).
    private static readonly double[][] BaseAxis = new double[][] {
      new double[]{0,0,1}, new double[]{1,0,0}, new double[]{0,-1,0},   // floor
      new double[]{0,0,-1}, new double[]{1,0,0}, new double[]{0,-1,0},  // ceiling
      new double[]{1,0,0}, new double[]{0,1,0}, new double[]{0,0,-1},   // west wall
      new double[]{-1,0,0}, new double[]{0,1,0}, new double[]{0,0,-1},  // east wall
      new double[]{0,1,0}, new double[]{1,0,0}, new double[]{0,0,-1},   // south wall
      new double[]{0,-1,0}, new double[]{1,0,0}, new double[]{0,0,-1},  // north wall
    };

    private static double Dot(double[] a, double[] b)
    {
      return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }

    private static void TextureAxisForPlane(double[] normal, out double[] sAxis, out double[] tAxis)
    {
      int bestAxis = 0;
      double best = -1.0;
      for (int i = 0; i < 6; i++)
      {
        double d = Dot(normal, BaseAxis[i * 3]);
        if (d > best) { best = d; bestAxis = i; }
      }
      double[] s = BaseAxis[bestAxis * 3 + 1];
      double[] t = BaseAxis[bestAxis * 3 + 2];
      sAxis = new double[] { s[0], s[1], s[2] };
      tAxis = new double[] { t[0], t[1], t[2] };
    }

    private static int ComponentIndex(double[] axis)
    {
      if (axis[0] != 0.0) return 0;
      if (axis[1] != 0.0) return 1;
      return 2;
    }

    private static void RotateAxes(double[] sAxis, double[] tAxis, double degrees)
    {
      int sv = ComponentIndex(sAxis), tv = ComponentIndex(tAxis);
      double sinv = Math.Sin(degrees * Math.PI / 180.0);
      double cosv = Math.Cos(degrees * Math.PI / 180.0);
      double[][] axes = { sAxis, tAxis };
      foreach (double[] axis in axes)
      {
        double ns = cosv * axis[sv] - sinv * axis[tv];
        double nt = sinv * axis[sv] + cosv * axis[tv];
        axis[sv] = ns; axis[tv] = nt;
      }
    }

    private static double[] PlaneNormal(double[][] points)
    {
      double[] p0 = points[0], p1 = points[1], p2 = points[2];
      double[] t1 = { p0[0] - p1[0], p0[1] - p1[1], p0[2] - p1[2] };
      double[] t2 = { p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2] };
      double cx = t1[1] * t2[2] - t1[2] * t2[1];
      double cy = t1[2] * t2[0] - t1[0] * t2[2];
      double cz = t1[0] * t2[1] - t1[1] * t2[0];
      double length = Math.Sqrt(cx * cx + cy * cy + cz * cz);
      if (length < 1e-6) return null;
      return new double[] { cx / length, cy / length, cz / length };
    }

    private static void StandardToValve(double[] normal, double[] shift, double rotate,
                                        out double[] u, out double[] v)
    {
      double[] sAxis, tAxis;
      TextureAxisForPlane(normal, out sAxis, out tAxis);
      RotateAxes(sAxis, tAxis, rotate);
      u = new double[] { sAxis[0], sAxis[1], sAxis[2], shift[0] };
      v = new double[] { tAxis[0], tAxis[1], tAxis[2], shift[1] };
    }

    private static void ValveToStandard(double[] normal, double[] u4, double[] v4, double[] scale,
                                        out double[] shift, out double rotate, out double[] scaleOut)
    {
      double[] baseS, baseT;
      TextureAxisForPlane(normal, out baseS, out baseT);
      int sv = ComponentIndex(baseS), tv = ComponentIndex(baseT);

      double[] u = { u4[0], u4[1], u4[2] };
      double[] v = { v4[0], v4[1], v4[2] };
      double lenU = Math.Sqrt(Dot(u, u));
      double lenV = Math.Sqrt(Dot(v, v));

      shift = new double[] { u4[3], v4[3] };
      double sx = lenU > 1e-9 ? scale[0] / lenU : scale[0];
      double sy = lenV > 1e-9 ? scale[1] / lenV : scale[1];

      double[] un = lenU > 1e-9 ? new double[] { u[0] / lenU, u[1] / lenU, u[2] / lenU } : u;
      double[] perp = new double[] { 0, 0, 0 };
      perp[sv] = -baseS[tv];
      perp[tv] = baseS[sv];
      rotate = Math.Atan2(Dot(un, perp), Dot(un, baseS)) * 180.0 / Math.PI;

      double sinv = Math.Sin(rotate * Math.PI / 180.0);
      double cosv = Math.Cos(rotate * Math.PI / 180.0);
      double[] expT = { baseT[0], baseT[1], baseT[2] };
      expT[sv] = cosv * baseT[sv] - sinv * baseT[tv];
      expT[tv] = sinv * baseT[sv] + cosv * baseT[tv];
      double[] vn = lenV > 1e-9 ? new double[] { v[0] / lenV, v[1] / lenV, v[2] / lenV } : v;
      double align = Dot(vn, expT);
      if (align < 0) { sy = -sy; align = -align; }
      if (align < 0.999) throw new ShearException();

      scaleOut = new double[] { sx, sy };
    }

    // ---- formatting / parsing ----

    private static string Fmt(double x)
    {
      if (x == 0.0) return "0";
      // Round to 6 decimals matching Python's round(x, 6). .NET Framework's
      // Math.Round(double,int) isn't correctly rounded, and a direct (decimal)x
      // cast keeps only 15 significant digits (dropping the tie-break digit).
      // Parse the full 17-sig-digit round-trip form into decimal, which is then
      // correctly rounded. (No binary double lands exactly on a 6-decimal
      // midpoint, so the result is simply the true nearest 6-decimal value.)
      decimal d;
      try
      {
        d = decimal.Parse(x.ToString("G17", CultureInfo.InvariantCulture),
                          NumberStyles.Float, CultureInfo.InvariantCulture);
      }
      catch { d = (decimal)x; }
      decimal r = Math.Round(d, 6, MidpointRounding.ToEven);
      if (r == 0m) return "0";
      if (r == Math.Truncate(r)) return ((long)r).ToString(CultureInfo.InvariantCulture);
      return r.ToString("0.######", CultureInfo.InvariantCulture);
    }

    private static bool TryD(string s, out double d)
    {
      return double.TryParse(s, NumberStyles.Float | NumberStyles.AllowLeadingSign,
                             CultureInfo.InvariantCulture, out d);
    }

    private sealed class Face
    {
      public string[][] PointsRaw;
      public double[][] PointsNum;
      public string Texture;
      public string Fmt;            // "standard" | "valve"
      public double[] Shift;
      public double Rotate;
      public double[] Scale;
      public double[] U;
      public double[] V;
      public string[] Trailing;
    }

    private static Face ParseFace(string stripped)
    {
      if (!stripped.StartsWith("(")) return null;
      string[] tok = stripped.Split((char[])null, StringSplitOptions.RemoveEmptyEntries);
      if (tok.Length < 16 || tok[0] != "(") return null;

      int i = 0;
      string[][] pointsRaw = new string[3][];
      double[][] pointsNum = new double[3][];
      for (int k = 0; k < 3; k++)
      {
        if (tok[i] != "(" || i + 4 >= tok.Length || tok[i + 4] != ")") return null;
        string[] trip = { tok[i + 1], tok[i + 2], tok[i + 3] };
        double a, b, c;
        if (!TryD(trip[0], out a) || !TryD(trip[1], out b) || !TryD(trip[2], out c)) return null;
        pointsRaw[k] = trip;
        pointsNum[k] = new double[] { a, b, c };
        i += 5;
      }
      if (i >= tok.Length) return null;
      string texture = tok[i]; i++;

      Face face = new Face();
      face.PointsRaw = pointsRaw;
      face.PointsNum = pointsNum;
      face.Texture = texture;

      if (i < tok.Length && tok[i] == "[")
      {
        // Valve-220: [ ux uy uz ushift ] [ vx vy vz vshift ] rotate sx sy
        if (i + 14 >= tok.Length || tok[i + 5] != "]" || tok[i + 6] != "[" || tok[i + 11] != "]") return null;
        double[] u = new double[4], v = new double[4];
        for (int j = 0; j < 4; j++) if (!TryD(tok[i + 1 + j], out u[j])) return null;
        for (int j = 0; j < 4; j++) if (!TryD(tok[i + 7 + j], out v[j])) return null;
        double rot, sx, sy;
        if (!TryD(tok[i + 12], out rot) || !TryD(tok[i + 13], out sx) || !TryD(tok[i + 14], out sy)) return null;
        face.Fmt = "valve"; face.U = u; face.V = v; face.Rotate = rot; face.Scale = new double[] { sx, sy };
        i += 15;
      }
      else
      {
        // Standard: shift_x shift_y rotate scale_x scale_y
        if (i + 4 >= tok.Length) return null;
        double[] n = new double[5];
        for (int j = 0; j < 5; j++) if (!TryD(tok[i + j], out n[j])) return null;
        face.Fmt = "standard"; face.Shift = new double[] { n[0], n[1] }; face.Rotate = n[2];
        face.Scale = new double[] { n[3], n[4] };
        i += 5;
      }

      int rem = tok.Length - i;
      string[] trailing = new string[rem];
      Array.Copy(tok, i, trailing, 0, rem);
      face.Trailing = trailing;
      return face;
    }

    private static string PointsStr(string[][] pr)
    {
      StringBuilder sb = new StringBuilder();
      for (int k = 0; k < 3; k++)
      {
        if (k > 0) sb.Append(' ');
        sb.Append("( ").Append(pr[k][0]).Append(' ').Append(pr[k][1]).Append(' ').Append(pr[k][2]).Append(" )");
      }
      return sb.ToString();
    }

    private static string EmitStandard(Face f, double[] shift, double rotate, double[] scale)
    {
      List<string> parts = new List<string>();
      parts.Add(PointsStr(f.PointsRaw));
      parts.Add(f.Texture);
      parts.Add(Fmt(shift[0])); parts.Add(Fmt(shift[1])); parts.Add(Fmt(rotate));
      parts.Add(Fmt(scale[0])); parts.Add(Fmt(scale[1]));
      parts.AddRange(f.Trailing);
      return string.Join(" ", parts.ToArray());
    }

    private static string EmitValve(Face f, double[] u, double[] v, double rotate, double[] scale)
    {
      List<string> parts = new List<string>();
      parts.Add(PointsStr(f.PointsRaw));
      parts.Add(f.Texture);
      parts.Add("[ " + Fmt(u[0]) + " " + Fmt(u[1]) + " " + Fmt(u[2]) + " " + Fmt(u[3]) + " ]");
      parts.Add("[ " + Fmt(v[0]) + " " + Fmt(v[1]) + " " + Fmt(v[2]) + " " + Fmt(v[3]) + " ]");
      parts.Add(Fmt(rotate)); parts.Add(Fmt(scale[0])); parts.Add(Fmt(scale[1]));
      parts.AddRange(f.Trailing);
      return string.Join(" ", parts.ToArray());
    }

    private static string Indent(string raw)
    {
      int n = 0;
      while (n < raw.Length && (raw[n] == ' ' || raw[n] == '\t')) n++;
      return raw.Substring(0, n);
    }

    private static string ConvertFaceLine(string raw, Face face, string target, Stats stats,
                                          string faceId, bool snap, double offInc, double rotInc)
    {
      bool sameFmt = (face.Fmt == target);

      // Fast path: no reformatting needed when the face is already in the target
      // format and we're not snapping.
      if (!snap && sameFmt) { stats.Kept++; return raw; }

      string indent = Indent(raw);

      // Resolve the face's standard (shift, rotate, scale) params. Valve faces
      // are decomposed; a normal is needed for that and for emitting Valve.
      double[] shift, scale; double rotate;
      double[] normal = null;

      if (face.Fmt == "standard")
      {
        shift = new double[] { face.Shift[0], face.Shift[1] };
        rotate = face.Rotate;
        scale = face.Scale;
      }
      else
      {
        normal = PlaneNormal(face.PointsNum);
        if (normal == null)
        {
          stats.Degenerate++;
          stats.Warnings.Add("warning: " + faceId + ": degenerate plane, left unchanged");
          return raw;
        }
        try
        {
          ValveToStandard(normal, face.U, face.V, face.Scale, out shift, out rotate, out scale);
        }
        catch (ShearException)
        {
          shift = new double[] { 0, 0 }; rotate = 0; scale = new double[] { 1, 1 };
          stats.Sheared++;
          stats.Warnings.Add("warning: " + faceId + " ('" + face.Texture +
            "'): sheared/non-axial Valve projection reset to default alignment");
        }
      }

      // Snap offsets and rotation (scale is left alone).
      bool snapped = false;
      if (snap)
      {
        double s0 = SnapTo(shift[0], offInc);
        double s1 = SnapTo(shift[1], offInc);
        double r = SnapTo(rotate, rotInc);
        // Count as snapped only on a meaningful change. Decomposing an already-
        // Valve face recovers its rotation via atan2, which carries sub-1e-3
        // float noise off the 6-decimal axes; without this threshold that noise
        // would report phantom snaps on every repeated run.
        const double eps = 1e-3;
        if (Math.Abs(s0 - shift[0]) > eps || Math.Abs(s1 - shift[1]) > eps || Math.Abs(r - rotate) > eps)
        { stats.Snapped++; snapped = true; }
        shift = new double[] { s0, s1 };
        rotate = r;
      }

      if (!sameFmt) stats.Converted++;
      else if (!snapped) stats.Kept++;

      if (target == "standard")
      {
        return indent + EmitStandard(face, shift, rotate, scale);
      }
      else
      {
        if (normal == null) normal = PlaneNormal(face.PointsNum);
        if (normal == null)
        {
          stats.Degenerate++;
          stats.Warnings.Add("warning: " + faceId + ": degenerate plane, left unchanged");
          return raw;
        }
        double[] u, v;
        StandardToValve(normal, shift, rotate, out u, out v);
        return indent + EmitValve(face, u, v, 0.0, scale);
      }
    }

    private static void UpdateFormatHeader(List<string> lines, string target)
    {
      string label = target == "valve" ? "Quake3 (Valve)" : "Quake3";
      int limit = Math.Min(8, lines.Count);
      for (int idx = 0; idx < limit; idx++)
      {
        string s = lines[idx].Trim();
        if (s.ToLowerInvariant().StartsWith("// format:"))
        {
          lines[idx] = "// Format: " + label;
          return;
        }
      }
      lines.Insert(0, "// Format: " + label);
      lines.Insert(0, "// Game: Quetoo");
    }

    // Quick format detection without a full rewrite: reads the "// Format:"
    // header and the first brush face. Each output is "standard", "valve", or
    // null (absent/undetermined). The converter uses this to skip files already
    // in the target format (header and first brush must agree).
    public static void DetectFormats(string text, out string headerFmt, out string brushFmt)
    {
      headerFmt = null;
      brushFmt = null;
      string[] lines = text.Split('\n');

      int limit = Math.Min(8, lines.Length);
      for (int i = 0; i < limit; i++)
      {
        string s = lines[i].Trim();
        if (s.ToLowerInvariant().StartsWith("// format:"))
        {
          string val = s.Substring(10).ToLowerInvariant();
          if (val.Contains("valve")) headerFmt = "valve";
          else if (val.Contains("quake")) headerFmt = "standard";
          break;
        }
      }

      int depth = 0;
      string blockType = null;
      foreach (string raw in lines)
      {
        string s = raw.Trim();
        if (s == "{") { depth++; if (depth == 2) blockType = null; continue; }
        if (s == "}") { depth = Math.Max(0, depth - 1); continue; }
        if (depth == 2)
        {
          if (s.Length == 0 || s.StartsWith("//")) continue;
          if (blockType == null) blockType = s.StartsWith("(") ? "brush" : "other";
          if (blockType == "brush")
          {
            Face f = ParseFace(s);
            if (f != null) { brushFmt = f.Fmt; return; }
          }
        }
      }
    }

    public static string Process(string text, string target, Stats stats,
                                 bool snap, double offInc, double rotInc)
    {
      string[] lines = text.Split('\n');
      List<string> outLines = new List<string>(lines.Length + 2);
      int depth = 0;
      string blockType = null;   // null | "brush" | "other"
      int entityIdx = -1;
      int brushIdx = -1;

      foreach (string raw in lines)
      {
        string s = raw.Trim();

        if (s == "{")
        {
          depth++;
          if (depth == 1) { entityIdx++; brushIdx = -1; }
          else if (depth == 2) { blockType = null; brushIdx++; }
          outLines.Add(raw);
          continue;
        }
        if (s == "}")
        {
          if (depth == 2) blockType = null;
          depth = Math.Max(0, depth - 1);
          outLines.Add(raw);
          continue;
        }

        if (depth == 2)
        {
          if (s.Length == 0 || s.StartsWith("//")) { outLines.Add(raw); continue; }
          if (blockType == null) blockType = s.StartsWith("(") ? "brush" : "other";
          if (blockType == "brush")
          {
            Face face = ParseFace(s);
            if (face != null)
            {
              string faceId = "entity " + entityIdx + " brush " + brushIdx;
              outLines.Add(ConvertFaceLine(raw, face, target, stats, faceId, snap, offInc, rotInc));
              continue;
            }
          }
          outLines.Add(raw);
          continue;
        }

        outLines.Add(raw);
      }

      UpdateFormatHeader(outLines, target);
      return string.Join("\n", outLines.ToArray());
    }
  }
}
