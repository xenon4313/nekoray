# [English](https://github.com/xenon4313/nekoray/blob/main/README.md) | #Русский

# NekoBox (Форк sing-box 1.14)

<img src="https://pu.yufu.su/2yYFkWZi.png" width="1234" alt="Скриншот NekoBox"/>

Кроссплатформенный графический клиент (GUI) для управления настройками прокси на базе Qt. Бэкенд: **sing-box 1.14.0**.

Данный репозиторий представляет собой улучшенный и поддерживаемый форк [r3t4rd/nekoray](https://github.com/r3t4rd/nekoray) и [MatsuriDayo/nekoray](https://github.com/MatsuriDayo/nekoray), ориентированный на высокую производительность, поддержку современных протоколов и доработанный пользовательский интерфейс (UX) для настольных систем.

**Текущая версия релиза:** `5.4.1-singbox-1.14.0`  
**Поддерживаемые платформы:** Windows x64 (портативный ZIP), Linux

---

## Основные возможности и улучшения

* **Движок sing-box 1.14.0**:
  * Полная интеграция с sing-box 1.14.0 с обновленной схемой маршрутизации DNS.
  * Автоматическая миграция и обратная совместимость с устаревшими параметрами входящих соединений (`sniffing`, `domain_strategy`).
  * Улучшенная поддержка **Hysteria 2** с обфускацией `gecko:password[:min[:max]]`.
* **Простой режим интерфейса (Simple Mode UI)**:
  * Темный дизайн в стиле стекломорфизма (glassmorphism) со спарклайн-мониторингом трафика в реальном времени.
  * Мгновенное переключение серверов, автоматическое тестирование задержки (пинга) и быстрое включение/отключение.
* **Выбор визуальных тем и фоновых изображений**:
  * Встроенные подобранные пресеты (Taiga Aisaka, City Life, Kana Arima, Yuu Koito).
  * Загрузка пользовательских фонов с интеллектуальным масштабированием **Aspect-Fill (Cover)** (без растягивания и искажения пропорций изображения).
* **Правила маршрутизации и выбор запущенных процессов**:
  * Удобный редактор правил раздельной маршрутизации (split routing): сайты напрямую/через прокси, приложения напрямую/через прокси и раздельная маршрутизация для конкретных серверов.
  * **Инспектор запущенных приложений (`Running…`)**: анализ активных процессов с нативными иконками приложений, заголовками окон, подсчетом запущенных экземпляров и фильтрацией поиска в реальном времени.
* **Быстрая маршрутизация из панели подробностей (Контекстное меню)**:
  * Клик правой кнопкой мыши по любому активному соединению на вкладке **Детали** (Details) для мгновенного добавления приложения или домена в правила прямого подключения (Direct) или прокси (Proxy).
  * Интерактивное диалоговое окно с возможностью перезапуска туннеля в один клик для применения новых правил на лету.

---

## Скачивание

Портативные сборки (установка не требуется). Распакуйте архив и запустите `nekobox.exe`.

**Релизы:** https://github.com/r3t4rd/nekoray/releases

Примеры наименования файлов для Windows:

```text
nekobox-5.4.1-singbox-1.14.0-windows64.zip
nekobox-portable-windows64.zip
```

Если на чистой системе Windows сообщает об отсутствии необходимых DLL, установите [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).

> [!IMPORTANT]
> Не удаляйте исполняемый файл `nekobox_core.exe` и файлы баз данных `geo*`, расположенные рядом с `nekobox.exe`.

---

## Содержимое релиза

| Файл | Назначение |
|------|------------|
| `nekobox.exe` | Клиент с графическим интерфейсом Qt 6, простым режимом и выбором тем |
| `nekobox_core.exe` | Ядро sing-box 1.14.0 + панель управления gRPC |
| `updater.exe` | Вспомогательная утилита для обновления приложения |
| `geoip.dat` / `geosite.dat` | Списки маршрутизации, совместимые с v2ray |
| `geoip.db` / `geosite.db` | Высокоскоростные бинарные базы данных маршрутизации sing-box |
| DLL-библиотеки Qt / OpenSSL | Зависимости среды выполнения (портативный пакет для Windows) |
| `ver1.jpg` .. `ver4.jpg` | Предустановленные фоновые арты для выбора тем оформления |

---

## Стек технологий (используемые компоненты и источники)

### Приложение

| Компонент | Источник | Примечания |
|-----------|----------|------------|
| GUI (NekoBox) | этот репозиторий | C++17, CMake, Ninja, MSVC под Windows |
| Базовый проект (Upstream) | [r3t4rd/nekoray](https://github.com/r3t4rd/nekoray) / [MatsuriDayo/nekoray](https://github.com/MatsuriDayo/nekoray) | Исходный GUI и базовые функции |
| Обертка ядра | `go/cmd/nekobox_core` | Собирает `nekobox_core.exe` |
| Утилита обновления | `go/cmd/updater` | Собирает `updater.exe` |
| gRPC-мост | `go/grpc_server` | Интерфейс управления GUI ↔ ядро на базе Protobuf |
| Инспектор процессов | `ProcessSelectDialog` | Перечисление процессов через Win32 Toolhelp32 + Shell API |
| Метка версии | `nekoray_version.txt` | Встраивается во время компиляции (`NKR_VERSION` / Go ldflags) |

### Ядро (прокси-движок)

| Компонент | Источник | Версия / ветка |
|-----------|----------|----------------|
| **sing-box** | [SagerNet/sing-box](https://github.com/SagerNet/sing-box) | **`1.14.0`** (с кастомными патчами для legacy inbounds и gecko) |
| Исходный sing-box | [SagerNet/sing-box](https://github.com/SagerNet/sing-box) | Проект ядра |
| **libneko** | [MatsuriDayo/libneko](https://github.com/MatsuriDayo/libneko) | Общие вспомогательные функции Go и процедуры спидтеста |
| Тулчейн Go | Go **1.23+** | |

**Теги сборки ядра (build tags)**:

```text
with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls
```

Локальные пути `replace` в `go/cmd/nekobox_core/go.mod`:

- `github.com/sagernet/sing-box => ../../../../sing-box`
- `github.com/matsuridayo/libneko => ../../../../libneko`

### Фреймворк интерфейса и C++ библиотеки

| Библиотека | Источник | Версия / использование |
|------------|----------|------------------------|
| **Qt** | [Qt](https://www.qt.io/) | **Qt 6** (Widgets, Gui, Network, Svg, LinguistTools) |
| **protobuf** | [protocolbuffers/protobuf](https://github.com/protocolbuffers/protobuf) | **v21.4** (статически, через `libs/deps`) |
| **gRPC / myproto** | генерируется из `.proto` | Межпроцессное взаимодействие (IPC) GUI ↔ `nekobox_core` |
| **yaml-cpp** | [jbeder/yaml-cpp](https://github.com/jbeder/yaml-cpp) | **0.7.0** |
| **zxing-cpp** | [nu-book/zxing-cpp](https://github.com/nu-book/zxing-cpp) | **v2.0.0** (сканирование и импорт QR-кодов) |
| **QHotkey** | [Skycoder42/QHotkey](https://github.com/Skycoder42/QHotkey) | Встроено в репозиторий (`3rdparty/QHotkey`) |
| **OpenSSL 3** | OpenSSL | `libcrypto-3-x64.dll`, `libssl-3-x64.dll` |
| Инструменты сборки | Microsoft / Ninja / CMake | Visual Studio 2022 Build Tools (x64) |

### Геоданные (базы данных правил маршрутизации)

| Файл | Апстрим-источник |
|------|------------------|
| `geoip.dat` | [Loyalsoldier/v2ray-rules-dat](https://github.com/Loyalsoldier/v2ray-rules-dat) |
| `geosite.dat` | [v2fly/domain-list-community](https://github.com/v2fly/domain-list-community) |
| `geoip.db` | [SagerNet/sing-geoip](https://github.com/SagerNet/sing-geoip) |
| `geosite.db` | [SagerNet/sing-geosite](https://github.com/SagerNet/sing-geosite) |

---

## Поддерживаемые протоколы и функционал

* **SOCKS5 / HTTP(S)**
* **Shadowsocks** (включая шифры AEAD 2022)
* **VMess** и **VLESS** (с Reality, gRPC, WebSocket)
* **Trojan**
* **Hysteria 2** (с обфускацией протокола `gecko`)
* **TUIC**
* **WireGuard**
* **Режим Tun / VPN** (стеки gVisor и системный)
* **Цепочки прокси (Proxy Chains)** и пользовательские конфигурации ядра
* **Форматы подписок**: Base64, Clash, SIP002, v2rayN

---

## Флаги запуска

См. [docs/RunFlags.md](docs/RunFlags.md).

## Сборка

Техническая документация:

- [docs/readme.md](docs/readme.md) — общее оглавление
- [docs/Build_Windows.md](docs/Build_Windows.md) — сборка GUI для Windows
- [docs/Build_Linux.md](docs/Build_Linux.md) — сборка GUI для Linux
- [docs/Build_Core.md](docs/Build_Core.md) — сборка Go-ядра (`sing-box` + `libneko`)
- [docs/Run_Linux.md](docs/Run_Linux.md) — примечания по работе в среде Linux

Типичное расположение соседних директорий для сборки ядра Go:

```text
Working/
  nekobox/          # данный репозиторий (GUI + go/cmd/*)
  sing-box/         # SagerNet/sing-box @ 1.14.0 с патчами
  libneko/          # MatsuriDayo/libneko
```

---

## Авторы и благодарности

**Движок ядра**
* [SagerNet/sing-box](https://github.com/SagerNet/sing-box)
* [MatsuriDayo/sing-box](https://github.com/MatsuriDayo/sing-box)
* [MatsuriDayo/libneko](https://github.com/MatsuriDayo/libneko)

**GUI и архитектура**
* [r3t4rd/nekoray](https://github.com/r3t4rd/nekoray) (простой режим и современное оформление)
* [MatsuriDayo/nekoray](https://github.com/MatsuriDayo/nekoray) (апстрим-проект)
* [Qv2ray](https://github.com/Qv2ray/Qv2ray) (историческое вдохновение для UI)
* [Qt Project](https://www.qt.io/)
* [Skycoder42/QHotkey](https://github.com/Skycoder42/QHotkey)

**Поставщики геоданных**
* [Loyalsoldier/v2ray-rules-dat](https://github.com/Loyalsoldier/v2ray-rules-dat)
* [v2fly/domain-list-community](https://github.com/v2fly/domain-list-community)
* [SagerNet/sing-geoip](https://github.com/SagerNet/sing-geoip) и [sing-geosite](https://github.com/SagerNet/sing-geosite)

---

## Лицензия

Данный проект распространяется под лицензиями GPLv3 / Apache 2.0 в соответствии с лицензиями апстрим-проектов. Тексты сторонних лицензий можно найти в соответствующих файлах исходного кода и подмодулях.
