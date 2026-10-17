// TRENDY STUDIO – Desktop-Hülle (Testversion). Die DAW selbst steckt in app/index.html.
const { app, BrowserWindow, Menu, session, shell } = require("electron");
const path = require("path");

const ERLAUBT = ["media", "midi", "midiSysex", "clipboard-read", "clipboard-sanitized-write", "fullscreen"];

function fenster() {
  const win = new BrowserWindow({
    width: 1440, height: 900, minWidth: 960, minHeight: 600,
    title: "TRENDY STUDIO", backgroundColor: "#c4c4bb",
    // backgroundThrottling aus: Der Taktgeber läuft auch weiter, wenn das Fenster im Hintergrund ist
    webPreferences: { contextIsolation: true, sandbox: true, backgroundThrottling: false },
  });
  win.loadFile(path.join(__dirname, "app", "index.html"));
  win.webContents.setWindowOpenHandler(({ url }) => { if (/^https?:/.test(url)) shell.openExternal(url); return { action: "deny" }; });
}

app.setName("TRENDY STUDIO");
app.whenReady().then(() => {
  session.defaultSession.setPermissionRequestHandler((wc, perm, cb) => cb(ERLAUBT.includes(perm)));
  session.defaultSession.setPermissionCheckHandler((wc, perm) => ERLAUBT.includes(perm));
  // Nur App- und Fenstermenü nativ: Bearbeiten, Darstellung usw. liefert die DAW selbst, inklusive ⌘-Kürzeln.
  Menu.setApplicationMenu(Menu.buildFromTemplate([
    { label: "TRENDY STUDIO", submenu: [{ role: "about", label: "Über TRENDY STUDIO" }, { type: "separator" }, { role: "hide", label: "TRENDY STUDIO ausblenden" }, { role: "hideOthers", label: "Andere ausblenden" }, { role: "unhide", label: "Alle einblenden" }, { type: "separator" }, { role: "quit", label: "TRENDY STUDIO beenden" }] },
    { label: "Fenster", submenu: [{ role: "minimize", label: "Im Dock ablegen" }, { role: "togglefullscreen", label: "Vollbild" }, { role: "toggleDevTools", label: "Entwicklerwerkzeuge" }] },
  ]));
  app.setAboutPanelOptions({ applicationName: "TRENDY STUDIO", applicationVersion: app.getVersion(), copyright: "Produziert von Andreas Engel, Darmstadt, Deutschland" });
  fenster();
  app.on("activate", () => { if (BrowserWindow.getAllWindows().length === 0) fenster(); });
});
app.on("window-all-closed", () => { if (process.platform !== "darwin") app.quit(); });
