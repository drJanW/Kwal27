#!/usr/bin/env python3
from http.server import SimpleHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs
from datetime import datetime
import os
import shutil

BASE_DIR = "/shares/Public/Kwal/csv"
DONE_DIR = "/shares/Public/Kwal/csv/done"
HISTORY_DIR = "/shares/Public/Kwal/csv/history"

# Files that ESP32 is allowed to push back
ALLOWED_FILES = {
    "light_patterns.csv",
    "light_colors.csv",
}

class Handler(SimpleHTTPRequestHandler):
    def serve_file(self, file_path):
        if not os.path.isfile(file_path):
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"not found")
            return

        content_type = "text/plain"
        if file_path.lower().endswith(".csv"):
            content_type = "text/csv"

        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.end_headers()
        with open(file_path, "rb") as handle:
            shutil.copyfileobj(handle, self.wfile)

    def do_POST(self):
        parsed = urlparse(self.path)

        if parsed.path == "/api/upload":
            qs = parse_qs(parsed.query)
            fname = qs.get("file", [""])[0]

            # Validate filename
            if not fname or "/" in fname or ".." in fname:
                self.send_response(400); self.end_headers()
                self.wfile.write(b"bad filename")
                return

            if fname not in ALLOWED_FILES:
                self.send_response(403); self.end_headers()
                self.wfile.write(b"file not allowed")
                return

            # Read body
            content_length = int(self.headers.get("Content-Length", 0))
            if content_length == 0 or content_length > 1_000_000:
                self.send_response(400); self.end_headers()
                self.wfile.write(b"bad content length")
                return
            body = self.rfile.read(content_length)

            dest = os.path.join(BASE_DIR, fname)

            # Compare with existing — skip if identical
            if os.path.isfile(dest):
                with open(dest, "rb") as f:
                    existing = f.read()
                if existing == body:
                    self.send_response(200); self.end_headers()
                    self.wfile.write(b"unchanged")
                    print(f"[upload] {fname}: unchanged, skipped", flush=True)
                    return

                # Archive existing version before overwriting
                os.makedirs(HISTORY_DIR, exist_ok=True)
                stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                base, ext = os.path.splitext(fname)
                archive_name = f"{base}_{stamp}{ext}"
                shutil.copy2(dest, os.path.join(HISTORY_DIR, archive_name))
                print(f"[upload] {fname}: archived as {archive_name}", flush=True)

            # Write new version
            with open(dest, "wb") as f:
                f.write(body)

            print(f"[upload] {fname}: saved ({len(body)} bytes)", flush=True)
            self.send_response(200); self.end_headers()
            self.wfile.write(b"saved")
            return

        self.send_response(404); self.end_headers()
        self.wfile.write(b"not found")

    def do_GET(self):
        parsed = urlparse(self.path)

        if parsed.path.startswith("/api/move"):
            qs = parse_qs(parsed.query)
            fname = qs.get("file", [""])[0]
            if not fname or "/" in fname or ".." in fname:
                self.send_response(400); self.end_headers()
                self.wfile.write(b"bad file")
                return

            src = os.path.join(BASE_DIR, fname)
            dst = os.path.join(DONE_DIR, fname)

            if not os.path.isfile(src):
                self.send_response(404); self.end_headers()
                self.wfile.write(b"not found")
                return

            os.makedirs(DONE_DIR, exist_ok=True)
            shutil.move(src, dst)

            self.send_response(200); self.end_headers()
            self.wfile.write(b"ok")
            return

        # default: serve /shares/Public/Kwal/csv as /csv/ and /Kwal/csv/
        if parsed.path == "/csv" or parsed.path == "/csv/":
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"ok")
            return

        if parsed.path.startswith("/csv/"):
            rel_path = parsed.path[len("/csv/"):]
            return self.serve_file(os.path.join(BASE_DIR, rel_path))

        if parsed.path == "/Kwal/csv" or parsed.path == "/Kwal/csv/":
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"ok")
            return

        if parsed.path.startswith("/Kwal/csv/"):
            rel_path = parsed.path[len("/Kwal/csv/"):]
            return self.serve_file(os.path.join(BASE_DIR, rel_path))

        self.send_response(404)
        self.end_headers()
        self.wfile.write(b"not found")
        return

def main():
    import sys
    print("Starting...", flush=True)
    
    if not os.path.isdir(BASE_DIR):
        print(f"ERROR: {BASE_DIR} does not exist", flush=True)
        sys.exit(1)
    
    os.chdir(BASE_DIR)
    print(f"Working dir: {os.getcwd()}", flush=True)
    
    port = 8081  # 8080 might be in use by WD services
    try:
        server = HTTPServer(("0.0.0.0", port), Handler)
        print(f"CSV server on :{port}", flush=True)
        server.serve_forever()
    except OSError as e:
        print(f"ERROR binding port {port}: {e}", flush=True)
        sys.exit(1)

if __name__ == "__main__":
    main()