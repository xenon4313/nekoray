# NekoBox (sing-box 1.14 Fork)

<img src="https://pu.yufu.su/2yYFkWZi.png" width="1234" alt="NekoBox screenshot"/>

Qt-based cross-platform GUI proxy configuration manager. Backend: **sing-box 1.14.0**.

This repository is an enhanced, maintained fork of [r3t4rd/nekoray](https://github.com/r3t4rd/nekoray) and [MatsuriDayo/nekoray](https://github.com/MatsuriDayo/nekoray), tailored for high performance, modern protocol support, and refined desktop UX.

**Current release version:** `5.4.1-singbox-1.14.0`  
**Supported platforms:** Windows x64 (portable ZIP), Linux

---

## Key Features & Enhancements

* **sing-box 1.14.0 Engine**:
  * Full integration with sing-box 1.14.0 with upgraded DNS routing schema.
  * Automatic migration and backward compatibility with legacy inbound definitions (`sniffing`, `domain_strategy`).
  * Enhanced **Hysteria 2** support with `gecko:password[:min[:max]]` obfuscation.
* **Simple Mode UI**:
  * Dark glassmorphism layout with live sparkline traffic monitor.
  * Instant server switching, automated latency testing, and quick power toggle.
* **Visual Theme & Wallpaper Selector**:
  * Built-in curated presets (Taiga Aisaka, City Life, Kana Arima, Yuu Koito).
  * Custom background loading with intelligent **Aspect-Fill (Cover)** scaling (no image stretching or distortion).
* **Connection Rules & Running Process Selector**:
  * Friendly rule editor for split routing: Direct/Proxy sites, Direct/Proxy apps, and per-server routing.
  * **Running Apps Picker (`Running…`)**: Inspect live processes with native application icons, window titles, instance counts, and real-time search filtering.
* **Details Panel Quick-Routing (Context Menu)**:
  * Right-click any active connection in the **Details** tab to immediately add an application or domain to Direct or Proxy rules.
  * Interactive prompt with one-click tunnel restart to apply new rules on the fly.

---

## Download

Portable builds (no installer required). Extract and run `nekobox.exe`.

**Releases:** https://github.com/r3t4rd/nekoray/releases

Windows asset naming example:

```text
nekobox-5.4.1-singbox-1.14.0-windows64.zip
nekobox-portable-windows64.zip
```

If Windows reports missing DLLs on a clean machine, install the [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).

> [!IMPORTANT]
> Do not remove `nekobox_core.exe` or the `geo*` database files located next to `nekobox.exe`.

---

## What’s inside a release

| File | Role |
|------|------|
| `nekobox.exe` | Qt 6 GUI client with Simple Mode & Theme Selector |
| `nekobox_core.exe` | sing-box 1.14.0 core + gRPC control plane |
| `updater.exe` | In-app update helper |
| `geoip.dat` / `geosite.dat` | v2ray-compatible routing lists |
| `geoip.db` / `geosite.db` | High-speed sing-box binary routing databases |
| Qt / OpenSSL DLLs | Runtime dependencies (Windows portable package) |
| `ver1.jpg` .. `ver4.jpg` | Preset artwork backgrounds for Theme Selector |

---

## Stack (what we use and where it comes from)

### Application

| Component | Source | Notes |
|-----------|--------|--------|
| GUI (NekoBox) | this repository | C++17, CMake, Ninja, MSVC on Windows |
| Upstream project | [r3t4rd/nekoray](https://github.com/r3t4rd/nekoray) / [MatsuriDayo/nekoray](https://github.com/MatsuriDayo/nekoray) | Base GUI and features |
| Core wrapper | `go/cmd/nekobox_core` | Builds `nekobox_core.exe` |
| Updater | `go/cmd/updater` | Builds `updater.exe` |
| gRPC bridge | `go/grpc_server` | Protobuf-based GUI ↔ core control interface |
| Process Inspector | `ProcessSelectDialog` | Win32 Toolhelp32 + Shell API process enumeration |
| Version stamp | `nekoray_version.txt` | Embedded at compile time (`NKR_VERSION` / Go ldflags) |

### Core (proxy engine)

| Component | Source | Version / branch |
|-----------|--------|------------------|
| **sing-box** | [SagerNet/sing-box](https://github.com/SagerNet/sing-box) | **`1.14.0`** (with custom legacy inbound & gecko patches) |
| Upstream sing-box | [SagerNet/sing-box](https://github.com/SagerNet/sing-box) | Core project |
| **libneko** | [MatsuriDayo/libneko](https://github.com/MatsuriDayo/libneko) | Shared Go helpers & speedtest routines |
| Go toolchain | Go **1.23+** | |

**Core build tags**:

```text
with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls
```

Local `replace` paths in `go/cmd/nekobox_core/go.mod`:

- `github.com/sagernet/sing-box => ../../../../sing-box`
- `github.com/matsuridayo/libneko => ../../../../libneko`

### GUI framework & C++ libraries

| Library | Source | Version / usage |
|---------|--------|-----------------|
| **Qt** | [Qt](https://www.qt.io/) | **Qt 6** (Widgets, Gui, Network, Svg, LinguistTools) |
| **protobuf** | [protocolbuffers/protobuf](https://github.com/protocolbuffers/protobuf) | **v21.4** (static, via `libs/deps`) |
| **gRPC / myproto** | generated from `.proto` | GUI ↔ `nekobox_core` IPC |
| **yaml-cpp** | [jbeder/yaml-cpp](https://github.com/jbeder/yaml-cpp) | **0.7.0** |
| **zxing-cpp** | [nu-book/zxing-cpp](https://github.com/nu-book/zxing-cpp) | **v2.0.0** (QR code scanning & import) |
| **QHotkey** | [Skycoder42/QHotkey](https://github.com/Skycoder42/QHotkey) | Vendored under `3rdparty/QHotkey` |
| **OpenSSL 3** | OpenSSL | `libcrypto-3-x64.dll`, `libssl-3-x64.dll` |
| Build Tools | Microsoft / Ninja / CMake | Visual Studio 2022 Build Tools (x64) |

### Geodata (routing rule databases)

| File | Upstream |
|------|----------|
| `geoip.dat` | [Loyalsoldier/v2ray-rules-dat](https://github.com/Loyalsoldier/v2ray-rules-dat) |
| `geosite.dat` | [v2fly/domain-list-community](https://github.com/v2fly/domain-list-community) |
| `geoip.db` | [SagerNet/sing-geoip](https://github.com/SagerNet/sing-geoip) |
| `geosite.db` | [SagerNet/sing-geosite](https://github.com/SagerNet/sing-geosite) |

---

## Supported Protocols & Features

* **SOCKS5 / HTTP(S)**
* **Shadowsocks** (including 2022 AEAD ciphers)
* **VMess** & **VLESS** (with Reality, gRPC, WebSocket)
* **Trojan**
* **Hysteria 2** (with `gecko` protocol obfuscation)
* **TUIC**
* **WireGuard**
* **Tun / VPN Mode** (gVisor & system stack)
* **Proxy Chains** & Custom Core configurations
* **Subscription Formats**: Base64, Clash, SIP002, v2rayN

---

## Run Flags

See [docs/RunFlags.md](docs/RunFlags.md).

## Build

Technical documentation:

- [docs/readme.md](docs/readme.md) — index
- [docs/Build_Windows.md](docs/Build_Windows.md) — Windows GUI
- [docs/Build_Linux.md](docs/Build_Linux.md) — Linux GUI
- [docs/Build_Core.md](docs/Build_Core.md) — Go core (`sing-box` + `libneko`)
- [docs/Run_Linux.md](docs/Run_Linux.md) — Linux runtime notes

Typical sibling directory layout for building the Go core:

```text
Working/
  nekobox/          # this repository (GUI + go/cmd/*)
  sing-box/         # SagerNet/sing-box @ 1.14.0 with patches
  libneko/          # MatsuriDayo/libneko
```

---

## Credits

**Core Engine**
* [SagerNet/sing-box](https://github.com/SagerNet/sing-box)
* [MatsuriDayo/sing-box](https://github.com/MatsuriDayo/sing-box)
* [MatsuriDayo/libneko](https://github.com/MatsuriDayo/libneko)

**GUI & Architecture**
* [r3t4rd/nekoray](https://github.com/r3t4rd/nekoray) (Simple Mode & modern styling)
* [MatsuriDayo/nekoray](https://github.com/MatsuriDayo/nekoray) (upstream project)
* [Qv2ray](https://github.com/Qv2ray/Qv2ray) (historical UI inspiration)
* [Qt Project](https://www.qt.io/)
* [Skycoder42/QHotkey](https://github.com/Skycoder42/QHotkey)

**Geodata Providers**
* [Loyalsoldier/v2ray-rules-dat](https://github.com/Loyalsoldier/v2ray-rules-dat)
* [v2fly/domain-list-community](https://github.com/v2fly/domain-list-community)
* [SagerNet/sing-geoip](https://github.com/SagerNet/sing-geoip) & [sing-geosite](https://github.com/SagerNet/sing-geosite)

---

## License

This project is licensed under GPLv3 / Apache 2.0 in compliance with upstream licenses. See individual source files and submodule directories for third-party license texts.
