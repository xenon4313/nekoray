# [English](https://github.com/xenon4313/nekoray/blob/main/README.md) | #Русский 

# NekoBox (Форк на базе sing-box 1.14)

<img src="https://pu.yufu.su/2yYFkWZi.png" width="1234" alt="Скриншот NekoBox"/>

Кроссплатформенный GUI-клиент для управления прокси-конфигурациями на базе Qt. Бэкенд: **sing-box 1.14.0**.

Этот репозиторий представляет собой улучшенный и поддерживаемый форк [r3t4rd/nekoray](https://github.com/r3t4rd/nekoray) и [MatsuriDayo/nekoray](https://github.com/MatsuriDayo/nekoray), ориентированный на высокую производительность, поддержку современных протоколов и удобный десктопный интерфейс.

**Текущая версия релиза:** `1.2-singbox-1.14.0`  
**Поддерживаемые платформы:** Windows x64 (портативный ZIP), Linux

---

## Ключевые возможности и улучшения

* **Движок sing-box 1.14.0**:
  * Полная интеграция с sing-box 1.14.0 с обновленной схемой DNS-маршрутизации.
  * Автоматическая миграция и обратная совместимость с устаревшими определениями входящих подключений (inbound: `sniffing`, `domain_strategy`).
  * Улучшенная поддержка **Hysteria 2** с обфускацией `gecko:password[:min[:max]]`.
* **Интерфейс Simple Mode (Упрощенный режим)**:
  * Тёмная тема в стиле glassmorphism («матовое стекло») со спарклайн-монитором сетевого трафика в реальном времени.
  * Мгновенное переключение серверов, автоматическое тестирование задержки (ping) и быстрый тумблер включения/выключения.
* **Селектор визуальных тем и обоев**:
  * Встроенные готовые пресеты (Taiga Aisaka, City Life, Kana Arima, Yuu Koito).
  * Загрузка пользовательских фонов с интеллектуальным масштабированием **Aspect-Fill (Cover)** (без растяжения и искажения пропорций изображения).
* **Правила маршрутизации и выбор запущенных процессов**:
  * Удобный редактор правил раздельной маршрутизации (split routing): сайты напрямую / через прокси, приложения напрямую / через прокси, а также маршрутизация под конкретные серверы.
  * **Селектор запущенных приложений (`Running…`)**: просмотр активных процессов с нативными иконками приложений, заголовками окон, количеством запущенных экземпляров и поисковой фильтрацией в реальном времени.
* **Быстрая маршрутизация из панели сведений (контекстное меню)**:
  * Клик правой кнопкой мыши по любому активному соединению во вкладке **Сведения (Details)** для мгновенного добавления приложения или домена в правила Direct (напрямую) или Proxy (через прокси).
  * Интерактивное диалоговое окно с возможностью перезапуска туннеля в один клик для применения новых правил на лету.

---

## Скачивание и установка

**Релизы на GitHub:** https://github.com/xenon4313/nekoray/releases

### 🐧 Linux

#### Arch Linux / Manjaro / EndeavourOS
Установка готового пакета `.pkg.tar.zst` через `pacman`:
```bash
sudo pacman -U nekobox-bin-1.1-x86_64.pkg.tar.zst
```
Или сборка из исходников AUR:
```bash
cd release/aur && makepkg -si
```

#### Debian / Ubuntu / Linux Mint / Pop!_OS
Установка официального `.deb`-пакета:
```bash
sudo apt install ./nekobox_1.1_amd64.deb
# или
sudo dpkg -i nekobox_1.1_amd64.deb && sudo apt -f install
```

#### Универсальный вариант для Linux (портативная сборка и установщик)
Распакуйте архив и запустите универсальный скрипт установки:
```bash
tar -xzf nekobox-1.1-linux64.tar.gz
cd nekobox-linux64
sudo ./install.sh
```
> [!TIP]
> Установщики автоматически настраивают правила Polkit (`99-nekobox.rules`) и назначают привилегии `cap_net_admin=ep` для исполняемого файла `nekobox_core`. Это позволяет активировать режим TUN/VPN **без ввода пароля суперпользователя (root)**.

### 🪟 Windows

Портативные сборки (установка не требуется). Распакуйте архив и запустите `nekobox.exe`:
```text
nekobox-5.4.1-singbox-1.14.0-windows64.zip
```
Если Windows выдаёт ошибку о недостающих DLL-библиотеках на чистой системе, установите [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).

> [!IMPORTANT]
> Не удаляйте `nekobox_core.exe` (или `nekobox_core` в Linux) и файлы баз данных `geo*`, находящиеся рядом с `nekobox`.

---

## Содержимое релиза

| Файл | Назначение |
|------|------------|
| `nekobox.exe` | GUI-клиент на Qt 6 с поддержкой Simple Mode и выбором тем |
| `nekobox_core.exe` | Ядро sing-box 1.14.0 + gRPC-интерфейс управления |
| `updater.exe` | Встроенный модуль обновлений |
| `geoip.dat` / `geosite.dat` | Списки маршрутизации, совместимые с v2ray |
| `geoip.db` / `geosite.db` | Высокоскоростные бинарные базы данных маршрутизации sing-box |
| DLL Qt / OpenSSL | Зависимости времени выполнения (портативный пакет для Windows) |
| `ver1.jpg` .. `ver4.jpg` | Предустановленные фоновые изображения для селектора тем |

---

## Стек технологий (что используется и откуда берётся)

### Приложение

| Компонент | Источник | Примечания |
|-----------|----------|------------|
| GUI (NekoBox) | этот репозиторий | C++17, CMake, Ninja, MSVC на Windows |
| Апстрим-проект | [r3t4rd/nekoray](https://github.com/r3t4rd/nekoray) / [MatsuriDayo/nekoray](https://github.com/MatsuriDayo/nekoray) | Базовый интерфейс и функциональность |
| Обертка ядра | `go/cmd/nekobox_core` | Собирает `nekobox_core.exe` |
| Апдейтер | `go/cmd/updater` | Собирает `updater.exe` |
| gRPC-мост | `go/grpc_server` | Интерфейс управления GUI ↔ core на базе Protobuf |
| Инспектор процессов | `ProcessSelectDialog` | Перечисление процессов через Win32 Toolhelp32 + Shell API |
| Отметка версии | `nekoray_version.txt` | Встраивается во время компиляции (`NKR_VERSION` / Go ldflags) |

### Ядро (прокси-движок)

| Компонент | Источник | Версия / ветка |
|-----------|----------|----------------|
| **sing-box** | [SagerNet/sing-box](https://github.com/SagerNet/sing-box) | **`1.14.0`** (с кастомными патчами для legacy inbound и gecko) |
| Апстрим sing-box | [SagerNet/sing-box](https://github.com/SagerNet/sing-box) | Базовый проект ядра |
| **libneko** | [MatsuriDayo/libneko](https://github.com/MatsuriDayo/libneko) | Общие Go-хелперы и процедуры замера скорости |
| Go toolchain | Go **1.23+** | |

**Теги сборки ядра (Core build tags)**:

```text
with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls
```

Локальные пути подмены (`replace`) в `go/cmd/nekobox_core/go.mod`:

- `github.com/sagernet/sing-box => ../../../../sing-box`
- `github.com/matsuridayo/libneko => ../../../../libneko`

### GUI-фреймворк и библиотеки C++

| Библиотека | Источник | Версия / назначение |
|------------|----------|---------------------|
| **Qt** | [Qt](https://www.qt.io/) | **Qt 6** (Widgets, Gui, Network, Svg, LinguistTools) |
| **protobuf** | [protocolbuffers/protobuf](https://github.com/protocolbuffers/protobuf) | **v21.4** (статическая сборка через `libs/deps`) |
| **gRPC / myproto** | генерируется из `.proto` | IPC между GUI и `nekobox_core` |
| **yaml-cpp** | [jbeder/yaml-cpp](https://github.com/jbeder/yaml-cpp) | **0.7.0** |
| **zxing-cpp** | [nu-book/zxing-cpp](https://github.com/nu-book/zxing-cpp) | **v2.0.0** (сканирование и импорт QR-кодов) |
| **QHotkey** | [Skycoder42/QHotkey](https://github.com/Skycoder42/QHotkey) | Встроено в `3rdparty/QHotkey` |
| **OpenSSL 3** | OpenSSL | `libcrypto-3-x64.dll`, `libssl-3-x64.dll` |
| Инструменты сборки | Microsoft / Ninja / CMake | Visual Studio 2022 Build Tools (x64) |

### Геоданные (базы данных правил маршрутизации)

| Файл | Апстрим |
|------|---------|
| `geoip.dat` | [Loyalsoldier/v2ray-rules-dat](https://github.com/Loyalsoldier/v2ray-rules-dat) |
| `geosite.dat` | [v2fly/domain-list-community](https://github.com/v2fly/domain-list-community) |
| `geoip.db` | [SagerNet/sing-geoip](https://github.com/SagerNet/sing-geoip) |
| `geosite.db` | [SagerNet/sing-geosite](https://github.com/SagerNet/sing-geosite) |

---

## Поддерживаемые протоколы и возможности

* **SOCKS5 / HTTP(S)**
* **Shadowsocks** (включая шифры 2022 AEAD)
* **VMess** и **VLESS** (с поддержкой Reality, gRPC, WebSocket)
* **Trojan**
* **Hysteria 2** (с обфускацией протокола `gecko`)
* **TUIC**
* **WireGuard**
* **Режим Tun / VPN** (стеки gVisor и system)
* **Цепочки прокси (Proxy Chains)** и пользовательские конфигурации ядра
* **Форматы подписок**: Base64, Clash, SIP002, v2rayN

---

## Флаги запуска

См. [docs/RunFlags.md](docs/RunFlags.md).

## Сборка

Техническая документация:

- [docs/readme.md](docs/readme.md) — оглавление
- [docs/Build_Windows.md](docs/Build_Windows.md) — сборка GUI для Windows
- [docs/Build_Linux.md](docs/Build_Linux.md) — сборка GUI для Linux
- [docs/Build_Core.md](docs/Build_Core.md) — сборка ядра на Go (`sing-box` + `libneko`)
- [docs/Run_Linux.md](docs/Run_Linux.md) — примечания по запуску в Linux

Типичное расположение соседних директорий для сборки ядра на Go:

```text
Working/
  nekobox/          # этот репозиторий (GUI + go/cmd/*)
  sing-box/         # SagerNet/sing-box @ 1.14.0 с патчами
  libneko/          # MatsuriDayo/libneko
```

---

## Благодарности

**Движок ядра**
* [SagerNet/sing-box](https://github.com/SagerNet/sing-box)
* [MatsuriDayo/sing-box](https://github.com/MatsuriDayo/sing-box)
* [MatsuriDayo/libneko](https://github.com/MatsuriDayo/libneko)

**Интерфейс и архитектура**
* [r3t4rd/nekoray](https://github.com/r3t4rd/nekoray) (Simple Mode и современный стиль)
* [MatsuriDayo/nekoray](https://github.com/MatsuriDayo/nekoray) (базовый апстрим-проект)
* [Qv2ray](https://github.com/Qv2ray/Qv2ray) (историческое вдохновение для UI)
* [Проект Qt](https://www.qt.io/)
* [Skycoder42/QHotkey](https://github.com/Skycoder42/QHotkey)

**Поставщики геоданных**
* [Loyalsoldier/v2ray-rules-dat](https://github.com/Loyalsoldier/v2ray-rules-dat)
* [v2fly/domain-list-community](https://github.com/v2fly/domain-list-community)
* [SagerNet/sing-geoip](https://github.com/SagerNet/sing-geoip) и [sing-geosite](https://github.com/SagerNet/sing-geosite)

---

## Лицензия

Этот проект лицензирован на условиях GPLv3 / Apache 2.0 в соответствии с лицензиями апстрим-проектов. Тексты сторонних лицензий см. в исходных файлах и каталогах подмодулей.
