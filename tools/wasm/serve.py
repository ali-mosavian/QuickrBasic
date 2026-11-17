"""python3 tools/wasm/serve.py [DIR] [PORT]: DIR (build/web) over HTTP on PORT (8000), cross-origin
isolated (COOP/COEP), as the IDE's page needs for its shared memory."""

import sys
from typing import ClassVar
from functools import partial
from http.server import ThreadingHTTPServer
from http.server import SimpleHTTPRequestHandler


class Isolated(SimpleHTTPRequestHandler):
    extensions_map: ClassVar = {
        **SimpleHTTPRequestHandler.extensions_map,
        ".mjs": "text/javascript",
        ".wasm": "application/wasm",
    }

    def end_headers(self) -> None:
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


class Server(ThreadingHTTPServer):
    request_queue_size = 64  # the page asks for its files at once; the default 5 refuses some


def main() -> None:
    root = sys.argv[1] if len(sys.argv) > 1 else "build/web"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8000
    print(f"http://localhost:{port}/")
    Server(("127.0.0.1", port), partial(Isolated, directory=root)).serve_forever()


if __name__ == "__main__":
    main()
