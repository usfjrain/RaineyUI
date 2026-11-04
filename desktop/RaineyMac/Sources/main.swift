import Cocoa
import WebKit

final class AppDelegate: NSObject, NSApplicationDelegate, WKScriptMessageHandler {
    private var window: NSWindow!
    private var webView: WKWebView!

    func applicationDidFinishLaunching(_ notification: Notification) {
        let contentController = WKUserContentController()
        contentController.add(self, name: "rainey")

        let configuration = WKWebViewConfiguration()
        configuration.userContentController = contentController
        configuration.preferences.javaScriptCanOpenWindowsAutomatically = false

        webView = WKWebView(frame: .zero, configuration: configuration)
        webView.setValue(false, forKey: "drawsBackground")

        window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 1280, height: 720),
            styleMask: [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView],
            backing: .buffered,
            defer: false
        )
        window.title = "Rainey"
        window.minSize = NSSize(width: 960, height: 540)
        window.center()
        window.contentView = webView
        window.makeKeyAndOrderFront(nil)

        if let url = Bundle.main.url(forResource: "index", withExtension: "html") {
            webView.loadFileURL(url, allowingReadAccessTo: Bundle.main.resourceURL ?? url.deletingLastPathComponent())
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }

    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        guard
            message.name == "rainey",
            let body = message.body as? [String: Any],
            let action = body["action"] as? String
        else { return }

        switch action {
        case "pickFolder":
            guard
                let systemId = body["systemId"] as? String,
                let extensions = body["extensions"] as? [String]
            else { return }
            pickFolder(systemId: systemId, extensions: extensions)
        case "launch":
            guard let path = body["path"] as? String else { return }
            launch(path: path)
        case "saveLibrary":
            guard let payload = body["payload"] as? String else { return }
            saveLibrary(payload)
        case "loadLibrary":
            loadLibrary()
        case "showInFinder":
            guard let path = body["path"] as? String else { return }
            NSWorkspace.shared.activateFileViewerSelecting([URL(fileURLWithPath: path)])
        default:
            break
        }
    }

    private func pickFolder(systemId: String, extensions: [String]) {
        let panel = NSOpenPanel()
        panel.title = "Choose ROM Folder"
        panel.prompt = "Scan"
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false

        panel.beginSheetModal(for: window) { [weak self] response in
            guard let self = self else { return }
            if response == .OK, let folder = panel.url {
                let games = self.scan(folder: folder, systemId: systemId, extensions: extensions)
                let folderPayload = ["path": folder.path, "name": folder.lastPathComponent]
                self.callJavaScript(function: "window.raineyNative.onScanResult", argument: [
                    "systemId": systemId,
                    "folder": folderPayload,
                    "games": games
                ])
            } else {
                self.callJavaScript(function: "window.raineyNative.onMessage", argument: [
                    "kind": "info",
                    "message": "Folder selection cancelled."
                ])
            }
        }
    }

    private func scan(folder: URL, systemId: String, extensions: [String]) -> [[String: Any]] {
        let normalizedExtensions = Set(extensions.map { $0.lowercased().trimmingCharacters(in: CharacterSet(charactersIn: ".")) })
        let manager = FileManager.default
        let keys: [URLResourceKey] = [.isDirectoryKey, .isRegularFileKey, .localizedNameKey]
        let enumerator = manager.enumerator(at: folder, includingPropertiesForKeys: keys, options: [.skipsHiddenFiles])
        var games: [[String: Any]] = []

        while let fileURL = enumerator?.nextObject() as? URL {
            let values = try? fileURL.resourceValues(forKeys: Set(keys))
            if values?.isDirectory == true { continue }
            guard values?.isRegularFile == true else { continue }
            let ext = fileURL.pathExtension.lowercased()
            guard normalizedExtensions.contains(ext) else { continue }

            let title = fileURL.deletingPathExtension().lastPathComponent
                .replacingOccurrences(of: "_", with: " ")
                .replacingOccurrences(of: ".", with: " ")
                .trimmingCharacters(in: .whitespacesAndNewlines)

            games.append([
                "id": "\(systemId)-\(fileURL.path.hashValue)",
                "system": systemId,
                "title": title.isEmpty ? fileURL.lastPathComponent : title,
                "ext": ext,
                "path": fileURL.path,
                "favorite": false,
                "recent": false,
                "year": "Unknown",
                "genre": "Unknown",
                "developer": "Unknown"
            ])
        }

        return games.sorted { first, second in
            (first["title"] as? String ?? "") < (second["title"] as? String ?? "")
        }
    }

    private func launch(path: String) {
        let url = URL(fileURLWithPath: path)
        guard FileManager.default.fileExists(atPath: path) else {
            callJavaScript(function: "window.raineyNative.onMessage", argument: [
                "kind": "error",
                "message": "This game file no longer exists. Re-scan the folder or choose the file again."
            ])
            return
        }

        let opened = NSWorkspace.shared.open(url)
        callJavaScript(function: "window.raineyNative.onMessage", argument: [
            "kind": opened ? "success" : "error",
            "message": opened
                ? "Opening game with the default macOS app for this file type."
                : "macOS could not find an app for this game. Install an emulator and associate this file type."
        ])
    }

    private func saveLibrary(_ payload: String) {
        do {
            let url = try libraryURL()
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try payload.write(to: url, atomically: true, encoding: .utf8)
        } catch {
            callJavaScript(function: "window.raineyNative.onMessage", argument: [
                "kind": "error",
                "message": "Could not save the desktop library: \(error.localizedDescription)"
            ])
        }
    }

    private func loadLibrary() {
        do {
            let url = try libraryURL()
            let payload = try String(contentsOf: url, encoding: .utf8)
            callJavaScript(function: "window.raineyNative.onLibraryLoaded", rawJSON: payload)
        } catch {
            callJavaScript(function: "window.raineyNative.onLibraryLoaded", rawJSON: "{}")
        }
    }

    private func libraryURL() throws -> URL {
        let base = try FileManager.default.url(
            for: .applicationSupportDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: true
        )
        return base.appendingPathComponent("Rainey Desktop", isDirectory: true).appendingPathComponent("library.json")
    }

    private func callJavaScript(function: String, argument: Any) {
        guard
            JSONSerialization.isValidJSONObject(argument),
            let data = try? JSONSerialization.data(withJSONObject: argument),
            let json = String(data: data, encoding: .utf8)
        else { return }
        callJavaScript(function: function, rawJSON: json)
    }

    private func callJavaScript(function: String, rawJSON: String) {
        DispatchQueue.main.async { [weak self] in
            self?.webView.evaluateJavaScript("\(function)(\(rawJSON));", completionHandler: nil)
        }
    }
}

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.activate(ignoringOtherApps: true)
app.run()
