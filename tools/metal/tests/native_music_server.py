#!/usr/bin/env python3
"""Local radio fixtures for the actual Core Audio backend integration tests.

This helper is deliberately not named test_*. It never contacts a station or
opens a microphone. With --binary it generates fixtures, starts an ephemeral
loopback server, runs the native test executable and retains a JSON receipt.
Without --binary it serves until interrupted, for an independently run client.
"""

from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import threading
import time
import wave
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit


def fixtures(directory: Path) -> dict[str, tuple[bytes, str, float]]:
    """Create our own four-second tones; encoder tools are fixture tools only."""
    directory.mkdir(parents=True, exist_ok=True)
    pcm = directory / "native_music.wav"
    if not pcm.exists():
        with wave.open(str(pcm), "wb") as output:
            output.setparams((2, 2, 44100, 0, "NONE", "not compressed"))
            output.writeframes(b"".join(
                struct.pack("<hh", sample, sample)
                for i in range(44100 * 4)
                for sample in [int(1000 * math.sin(i * 2 * math.pi * 440 / 44100))]
            ))
    commands = []
    specifications = [
        ("mp3", "audio/mpeg", ["lame", "--silent", "-b", "128", str(pcm), str(directory / "mp3")]),
        ("aac", "audio/aac", ["afconvert", str(pcm), str(directory / "aac"), "-f", "adts", "-d", "aac", "-b", "128000"]),
        ("heaac", "audio/aacp", ["afconvert", str(pcm), str(directory / "heaac"), "-f", "adts", "-d", "aach", "-b", "64000"]),
        ("vorbis", "audio/ogg", ["ffmpeg", "-nostdin", "-v", "error", "-y", "-i", str(pcm), "-c:a", "vorbis", "-strict", "experimental", "-f", "ogg", str(directory / "vorbis")]),
        ("opus", "audio/ogg", ["ffmpeg", "-nostdin", "-v", "error", "-y", "-i", str(pcm), "-c:a", "libopus", "-f", "ogg", str(directory / "opus")]),
    ]
    streams = {}
    for name, mime, argv in specifications:
        destination = directory / name
        if not destination.exists() or not destination.stat().st_size:
            if not shutil.which(argv[0]):
                if name in ("vorbis", "opus"):
                    continue
                raise RuntimeError(f"Missing required fixture encoder {argv[0]}")
            result = subprocess.run(argv, capture_output=True, text=True, check=False)
            commands.append(dict(argv=argv, exit=result.returncode, stdout=result.stdout, stderr=result.stderr))
            if result.returncode:
                raise RuntimeError(f"Fixture generation failed: {commands[-1]}")
        data = destination.read_bytes()
        # MP3 CBR128, other encoders may use far fewer bytes for this simple tone.
        rate = 16000.0 if name == "mp3" else len(data) / 4.0
        streams[name] = (data, mime, rate)
    manifest = directory / "radio.m3u8"
    if shutil.which("ffmpeg") and not manifest.exists():
        (directory / "hls").mkdir(exist_ok=True)
        argv = ["ffmpeg", "-nostdin", "-v", "error", "-y", "-i", str(pcm),
                "-c:a", "aac", "-b:a", "128000", "-f", "hls", "-hls_time", "1",
                "-hls_playlist_type", "vod", "-hls_segment_filename", str(directory / "hls" / "segment%d.ts"),
                str(manifest)]
        result = subprocess.run(argv, capture_output=True, text=True, check=False)
        commands.append(dict(argv=argv, exit=result.returncode, stdout=result.stdout, stderr=result.stderr))
        if result.returncode:
            raise RuntimeError(f"HLS fixture generation failed: {commands[-1]}")
        # ffmpeg writes only segment basenames into this manifest. Keep actual
        # media under /hls/ and exercise ordinary relative HLS segment URLs.
        manifest.write_text(manifest.read_text().replace("\nsegment", "\nhls/segment"))
    (directory / "fixture-commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    return streams


class RadioServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, streams, directory):
        super().__init__(("127.0.0.1", 0), RadioHandler)
        self.streams = streams
        self.directory = directory
        self.requests = []
        self.counts = {}
        self.lock = threading.Lock()


class RadioHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_args):
        pass

    @property
    def base(self):
        return f"http://127.0.0.1:{self.server.server_port}"

    def response(self, mime, data):
        self.send_response(200)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = urlsplit(self.path).path
        with self.server.lock:
            self.server.counts[self.path] = self.server.counts.get(self.path, 0) + 1
            attempt = self.server.counts[self.path]
            self.server.requests.append(dict(path=path, attempt=attempt, headers=dict(self.headers)))
        try:
            if path == "/stats":
                with self.server.lock:
                    data = json.dumps(self.server.counts).encode()
                self.response("application/json", data)
                return
            if path == "/formats":
                self.response("application/json", json.dumps(list(self.server.streams)).encode())
                return
            if path == "/radio.m3u8":
                manifest = self.server.directory / "radio.m3u8"
                if manifest.exists():
                    self.response("application/vnd.apple.mpegurl", manifest.read_bytes())
                else:
                    self.send_error(404)
                return
            if path.startswith("/hls/"):
                filename = Path(path).name
                if filename.startswith("segment") and filename.endswith(".ts"):
                    segment = self.server.directory / "hls" / filename
                    if segment.exists():
                        self.response("video/mp2t", segment.read_bytes())
                        return
                self.send_error(404)
                return
            if path == "/redirect":
                self.send_response(302)
                self.send_header("Location", "/mp3")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            if path == "/test.pls":
                self.response("audio/x-scpls", b"[playlist]\nNumberOfEntries=1\nFile1=/mp3\nTitle1=Native fixture\nLength1=-1\nVersion=2\n")
                return
            if path == "/test.m3u":
                self.response("audio/x-mpegurl", b"#EXTM3U\n#EXTINF:-1,Native fixture\n/mp3\n")
                return
            if path == "/query.m3u":
                self.response("audio/x-mpegurl", f"#EXTM3U\n{self.base}/mp3?token=a=b\n".encode())
                return
            if path in ("/failover404.pls", "/failover500.pls"):
                missing = "/no-such-stream" if path == "/failover404.pls" else "/server-error"
                self.response("audio/x-scpls", f"[playlist]\nNumberOfEntries=2\nFile1={missing}\nFile2=/mp3\nVersion=2\n".encode())
                return
            if path == "/nested.m3u":
                self.response("application/vnd.apple.mpegurl", b"#EXTM3U\ntest.pls\n")
                return
            if path == "/cycle.m3u":
                self.response("audio/x-mpegurl", b"#EXTM3U\ncycle.m3u\n")
                return
            if path in ("/cycle-a.pls", "/cycle-b.pls"):
                other = "/cycle-b.pls" if path == "/cycle-a.pls" else "/cycle-a.pls"
                self.response("audio/x-scpls", f"[playlist]\nNumberOfEntries=2\nFile1={other}\nFile2={path}\nVersion=2\n".encode())
                return
            if path == "/invalid.pls":
                self.response("audio/x-scpls", b"[playlist]\nNumberOfEntries=1\nTitle1=No stream URL\n")
                return
            if path == "/large.m3u":
                self.response("audio/x-mpegurl", b"#EXTM3U\n" + b"#" * (1024 * 1024) + b"\n/mp3\n")
                return
            if path == "/unavailable":
                self.send_error(503)
                return
            if path == "/server-error":
                self.send_error(500)
                return
            if path == "/finite.mp3":
                data, mime, _rate = self.server.streams["mp3"]
                self.response(mime, data)
                return
            if path == "/hang":
                self.send_response(200)
                self.send_header("Content-Type", "audio/mpeg")
                self.send_header("Connection", "close")
                self.end_headers()
                time.sleep(4)
                return
            name = path.lstrip("/")
            if name in ("icy", "icy-latin1", "legacy", "disconnect", "stall", "mime-error"):
                name = "mp3"
            if name not in self.server.streams:
                self.send_error(404)
                return
            data, mime, rate = self.server.streams[name]
            icy = path in ("/icy", "/icy-latin1", "/legacy") and self.headers.get("Icy-MetaData") == "1"
            if path == "/legacy":
                self.wfile.write(b"ICY 200 OK\r\n")
            else:
                self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream" if path == "/mime-error" else mime)
            self.send_header("icy-name", "Native Probe Station")
            self.send_header("icy-url", self.base + "/station")
            self.send_header("Connection", "close")
            if icy:
                self.send_header("icy-metaint", "4093")
            self.end_headers()
            title = "Probe Artist - Probe Song" if path != "/icy-latin1" else "Probe Artist - Caf\xe9"
            encoding = "latin-1" if path == "/icy-latin1" else "utf-8"
            payload = f"StreamTitle='{title}';StreamUrl='{self.base}/song';".encode(encoding)
            block = bytes([(len(payload) + 15) // 16]) + payload.ljust(((len(payload) + 15) // 16) * 16, b"\0")
            sent = 0
            began = time.monotonic()
            stalled = False
            while time.monotonic() - began < 90:
                if path == "/disconnect" and attempt == 1 and sent >= 24000:
                    return
                if path == "/stall" and not stalled and sent >= 32000:
                    stalled = True
                    time.sleep(1)
                count = min(256, 4093 - sent % 4093) if icy else 256
                offset = sent % len(data)
                chunk = (data + data)[offset:offset + count]
                self.wfile.write(chunk)
                sent += count
                if icy and sent % 4093 == 0:
                    # Separate length byte, body halves and padding exercise
                    # metadata ownership independently of network chunk shape.
                    self.wfile.write(block[:1])
                    self.wfile.flush()
                    time.sleep(0.001)
                    self.wfile.write(block[1:9])
                    self.wfile.flush()
                    time.sleep(0.001)
                    self.wfile.write(block[9:])
                self.wfile.flush()
                time.sleep(count / rate)
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            self.close_connection = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures-dir", type=Path, required=True)
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--ready-file", type=Path)
    parser.add_argument("test_arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    server = RadioServer(fixtures(args.fixtures_dir), args.fixtures_dir)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = f"http://127.0.0.1:{server.server_port}"
    if args.ready_file:
        args.ready_file.write_text(base + "\n")
    print(base, flush=True)
    receipt = dict(base=base, fixtures=list(server.streams))
    code = 0
    try:
        if args.binary:
            environment = dict(os.environ, AL_NATIVE_MUSIC_TEST_URL=base,
                               AL_NATIVE_MUSIC_TEST_FORMATS=",".join(server.streams) + (",hls" if (args.fixtures_dir / "radio.m3u8").exists() else ""),
                               AL_NATIVE_MUSIC_TEST_TRACE="1")
            extra = args.test_arguments
            if extra and extra[0] == "--":
                extra = extra[1:]
            argv = [str(args.binary.resolve()), *(extra or ["[native_music]"])]
            result = subprocess.run(argv, env=environment, capture_output=True, text=True, check=False)
            receipt.update(argv=argv, exit=result.returncode, stdout=result.stdout, stderr=result.stderr)
            print(result.stdout, end="")
            print(result.stderr, end="", file=__import__("sys").stderr)
            code = result.returncode
        else:
            while True:
                time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        server.shutdown()
        server.server_close()
        receipt["requests"] = server.requests
        if args.receipt:
            args.receipt.write_text(json.dumps(receipt, indent=2) + "\n")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
