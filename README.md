# Sheet pile solver -- interactive demo

## Build the solver library

    gcc -std=c11 -O2 -Iinclude -fPIC -shared -o bin/libsheetpile.so \
        src/soil_springs.c src/beam_fe.c src/linalg.c src/solver.c src/api.c -lm

(This is what the web backend calls. `make` separately builds a CLI
binary from src/main.c if you want a quick sanity check outside the browser.)

## Run

    python3 bin/backend.py

Then open http://127.0.0.1:8765/ in a browser.

## Layout

- include/, src/ -- the C solver: soil spring law (tri-linear
  active/neutral/passive), beam FE assembly, dense linear solver,
  Newton-Raphson driver, and api.c (flat FFI entry point for ctypes).
- bin/backend.py -- stdlib-only Python HTTP server. Serves the static
  frontend and a POST /solve endpoint that calls libsheetpile.so
  in-process via ctypes (no subprocess spawn -- this is what keeps
  round-trip latency under ~1ms).
- web/ -- static frontend: index.html (two tabs), style.css, app.js
  (vanilla JS, no build step, no frameworks).
