# Graph Storage

Graph sessions are stored through AxiomFS under `graphs/` with the extension
`.mi23graph`.

The file format is readable JSON:

```json
{
  "version": 1,
  "name": "Parabola",
  "functions": [
    {
      "expression": "x^2",
      "enabled": true
    }
  ],
  "window": {
    "xMin": -10,
    "xMax": 10,
    "yMin": -10,
    "yMax": 10
  },
  "mode": {
    "angle": "radian"
  }
}
```

The Graphing app exposes a storage menu from the graph view with `DEL`:

- Save Graph
- Load Graph
- Delete Graph
- New Graph

Unnamed sessions are saved as `Graph_001.mi23graph` style names. Filenames are
sanitized and duplicate names are auto-incremented instead of overwritten.

Current limitations:

- Direct opening from the File Browser is not wired yet; graph files are visible
  and show properties there.
- The app supports the existing five graph function slots. Broader multi-session
  management and companion app transfer are planned future work.
- The desktop companion app is intentionally not part of this milestone.
