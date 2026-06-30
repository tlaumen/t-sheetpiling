"""
Local backend for the sheet pile solver demo.

Serves the static frontend (web/index.html, app.js, style.css) and a
single POST /solve endpoint that calls the C solver, compiled into
bin/libsheetpile.{so,dylib,dll}, in-process via ctypes (no subprocess
spawn).

Run with:  python3 server/backend.py
Then open: http://127.0.0.1:8765/
"""
import ctypes
import json
import os
import platform
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(HERE)
BIN_DIR = os.path.join(PROJECT_ROOT, "bin")
STATIC_DIR = os.path.join(PROJECT_ROOT, "web")


def find_library():
    """Picks whichever shared library is present in bin/, in an order
    that matches the current platform's native extension first."""
    candidates = {
        "Windows": ["libsheetpile.dll"],
        "Darwin": ["libsheetpile.dylib", "libsheetpile.so"],
    }.get(platform.system(), ["libsheetpile.so"])
    for name in candidates:
        path = os.path.join(BIN_DIR, name)
        if os.path.isfile(path):
            return path
    raise FileNotFoundError(
        "No shared library found in bin/. Run `make lib` (or the "
        "equivalent manual gcc/cl.exe command in the README) first."
    )


lib = ctypes.CDLL(find_library())

D = ctypes.c_double
DP = ctypes.POINTER(ctypes.c_double)
I = ctypes.c_int
IP = ctypes.POINTER(ctypes.c_int)

lib.solve_api.restype = ctypes.c_int
lib.solve_api.argtypes = [
    D, D, D,                                  # pile_EI, pile_top, pile_tip
    DP, DP, DP, DP, DP, DP, DP, DP, I,        # left layers (8 arrays) + n_left
    DP, DP, DP, DP, DP, DP, DP, DP, I,        # right layers (8 arrays) + n_right
    DP, I,                                    # support_levels, n_supports
    I,                                        # n_nodes
    DP, DP, DP, DP, DP, DP, DP,               # out: z,w,theta,M,V,p_left,p_right
    IP, IP                                    # out: converged, iterations
]


def darr(values):
    arr = (ctypes.c_double * len(values))(*values)
    return arr, ctypes.cast(arr, DP)


def run_solve(payload):
    pile = payload["pile"]
    left_layers = payload["left"]
    right_layers = payload["right"]
    supports = payload["supports"]
    n_nodes = int(payload.get("n_nodes", 61))

    def cols(layers, key):
        return [float(l[key]) for l in layers]

    left_arrs = {k: darr(cols(left_layers, k)) for k in
                 ("top", "bottom", "gamma", "phi", "c", "k1", "k2", "k3")}
    right_arrs = {k: darr(cols(right_layers, k)) for k in
                  ("top", "bottom", "gamma", "phi", "c", "k1", "k2", "k3")}
    support_levels = [float(s["level"]) for s in supports]
    sup_arr, sup_ptr = darr(support_levels if support_levels else [0.0])

    out_z, out_z_p = darr([0.0] * n_nodes)
    out_w, out_w_p = darr([0.0] * n_nodes)
    out_th, out_th_p = darr([0.0] * n_nodes)
    out_M, out_M_p = darr([0.0] * n_nodes)
    out_V, out_V_p = darr([0.0] * n_nodes)
    out_pL, out_pL_p = darr([0.0] * n_nodes)
    out_pR, out_pR_p = darr([0.0] * n_nodes)
    converged = ctypes.c_int(0)
    iterations = ctypes.c_int(0)

    rc = lib.solve_api(
        float(pile["EI"]), float(pile["top"]), float(pile["tip"]),
        left_arrs["top"][1], left_arrs["bottom"][1], left_arrs["gamma"][1],
        left_arrs["phi"][1], left_arrs["c"][1],
        left_arrs["k1"][1], left_arrs["k2"][1], left_arrs["k3"][1], len(left_layers),
        right_arrs["top"][1], right_arrs["bottom"][1], right_arrs["gamma"][1],
        right_arrs["phi"][1], right_arrs["c"][1],
        right_arrs["k1"][1], right_arrs["k2"][1], right_arrs["k3"][1], len(right_layers),
        sup_ptr, len(support_levels),
        n_nodes,
        out_z_p, out_w_p, out_th_p, out_M_p, out_V_p, out_pL_p, out_pR_p,
        ctypes.byref(converged), ctypes.byref(iterations)
    )

    if rc != 0:
        return {"error": f"solver returned code {rc}"}

    return {
        "z": list(out_z), "w": list(out_w), "theta": list(out_th),
        "M": list(out_M), "V": list(out_V),
        "p_left": list(out_pL), "p_right": list(out_pR),
        "converged": bool(converged.value), "iterations": iterations.value,
    }


MIME = {".html": "text/html", ".js": "application/javascript", ".css": "text/css"}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _send_json(self, obj, status=200):
        body = json.dumps(obj).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        if self.path == "/solve":
            length = int(self.headers.get("Content-Length", "0"))
            try:
                payload = json.loads(self.rfile.read(length))
                result = run_solve(payload)
                self._send_json(result)
            except Exception as e:
                self._send_json({"error": str(e)}, status=400)
        else:
            self._send_json({"error": "not found"}, status=404)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/":
            path = "/index.html"
        fpath = os.path.join(STATIC_DIR, path.lstrip("/"))
        if not os.path.abspath(fpath).startswith(os.path.abspath(STATIC_DIR)):
            self._send_json({"error": "forbidden"}, status=403)
            return
        if not os.path.isfile(fpath):
            self._send_json({"error": "not found"}, status=404)
            return
        ext = os.path.splitext(fpath)[1]
        with open(fpath, "rb") as f:
            body = f.read()
        self.send_response(200)
        self.send_header("Content-Type", MIME.get(ext, "application/octet-stream"))
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


if __name__ == "__main__":
    print("Serving on http://127.0.0.1:8765/")
    ThreadingHTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
