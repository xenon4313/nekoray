# Руководство по сборке NekoBox под Linux

В этом руководстве описан процесс полной сборки **NekoBox** и ядра **nekobox_core** (на базе sing-box 1.14.0) из исходного кода.

---

## 1. Системные зависимости

### Arch Linux / Manjaro
```bash
sudo pacman -S --needed base-devel git cmake ninja qt6-base qt6-svg protobuf yaml-cpp libx11 go libcap
```

### Debian 12 / Ubuntu 24.04+
```bash
sudo apt update
sudo apt install -y build-essential git cmake ninja-build \
    qt6-base-dev qt6-base-dev-tools libqt6svg6-dev \
    libprotobuf-dev protobuf-compiler libyaml-cpp-dev \
    libx11-dev libcap2-bin golang
```

---

## 2. Структура каталогов

Для корректной работы локальных связей `go.mod` рекомендуется следующая структура каталогов:
```
neko/
├── nekoray/     # Frontend (C++ / Qt6)
├── sing-box/    # Backend (Go / sing-box 1.14 core)
├── libneko/     # Библиотека интеграции
└── sing-quic/   # QUIC компоненты
```

---

## 3. Сборка ядра `nekobox_core`

Перейдите в каталог обертки ядра и соберите исполняемый файл:

```bash
cd nekoray/go/cmd/nekobox_core

go build -v -trimpath \
    -ldflags "-w -s -X github.com/matsuridayo/libneko/neko_common.Version_neko=5.4.1" \
    -tags "with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls" \
    -o nekobox_core .
```

---

## 4. Сборка графического интерфейса `nekobox`

Сборка C++ GUI с помощью CMake и Ninja:

```bash
cd nekoray
mkdir -p build && cd build

cmake -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DNKR_PACKAGE_MAKER=OFF ..

ninja -j$(nproc)
```

Исполняемый файл появится в `build/nekobox`.

---

## 5. Создание установочных пакетов

В репозитории подготовлены автоматические скрипты для сборки дистрибутивов:

### Arch Linux (.pkg.tar.zst)
```bash
cd pkg-build
makepkg -f
```

### Debian / Ubuntu (.deb)
```bash
./build_deb.sh
```

### Портативный архив (.tar.gz / .zip)
Соберите бинарники в каталог `nekobox-linux64/` вместе с базами `geoip.db` и `geosite.db`, затем запакуйте:
```bash
tar -czf nekobox-5.4.1-singbox-1.14.0-linux64.tar.gz -C nekobox-linux64 .
```
