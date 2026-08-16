# dynqt — runtime Qt `.ui` loader with property binding 🔗

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-blue.svg)](#)

A **dynamic loading test of Qt UI files** — load a `.ui` file at runtime with
`QUiLoader` and inspect/bind its widgets, no compile-time form class needed.

## What it does

- Load any `.ui` file at runtime via `QUiLoader`.
- List every child object with its full object path
  (`parent/child/grandchild...`), so you can find a widget by name.
- **Filter** by `objectName` substring (case-insensitive).
- **`PropertyLink`** — connect/bind properties between a source object and the
  loaded widgets.
- Built against **Qt 4.8 / VS2008**, so it demonstrates the classic
  `QtUiTools` API.

## Why it matters

Loading `.ui` at runtime is the basis for plugin-driven or user-customisable
interfaces: the UI is data, not code. `dynqt` shows the minimal viable version
of that idea — layout + lookup + property binding.

## Requirements

- Qt 4.8+ (Gui + UiTools)
- C++ compiler

## Build

```bash
qmake && make
```

## License

[MIT](LICENSE) © Valentin Heinitz
