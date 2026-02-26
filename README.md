# Iptux Plus (Modernized LAN Messenger)

[![Snapcraft](https://snapcraft.io/iptux/badge.svg)](https://snapcraft.io/iptux)
[![CI](https://github.com/iptux-src/iptux/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/iptux-src/iptux/actions/workflows/ci.yml?query=branch%3Amaster)
[![CodeFactor](https://www.codefactor.io/repository/github/iptux-src/iptux/badge)](https://www.codefactor.io/repository/github/iptux-src/iptux)
[![Weblate Translation Status](https://hosted.weblate.org/widgets/iptux/-/iptux/svg-badge.svg)](https://hosted.weblate.org/engage/iptux/)

Iptux Plus is a modern, high-speed over-LAN cross-platform messenger that allows seamless peer-to-peer file sharing, direct messaging, and localized group chat. It operates independently of any external servers, making it highly secure and completely private.

This project is a complete GTK4 & Libadwaita modernization of the classic popular `iptux` client, redesigned with an integrated Apple-style user interface, enhanced performance, and new group chat capabilities.

## Key Features

* **Serverless Architecture**: Communicates exclusively over your local network using TCP/UDP port 2425 protocols.
* **High-Speed File Sharing**: Send native files and folders safely to network peers at the maximum speed your network router allows.
* **Modern GTK4 Aesthetics**: Fully responsive layout utilizing Adwaita interface guidelines, providing a macOS/iMessage style native experience.
* **Real-time Group Chat**: Start multi-participant conversations effortlessly.
* **Zero-config Detection**: Instantly discovers everyone online using UDP broadcast packets on the subnet.

## Installation

### Linux (Debian and Ubuntu) - Iptux Plus (Recommended)

To compile and install the modern Iptux Plus interface:

```sh
# Install GTK4, Libadwaita, and related build tools
sudo apt-get update
sudo apt-get install git libgtk-4-dev libadwaita-1-dev libglib2.0-dev libjsoncpp-dev g++ meson libsigc++-2.0-dev appstream gettext

# Clone the repository
git clone git://github.com/iptux-src/iptux.git
cd iptux

# Setup the modern build environment
meson setup build-gtk4
ninja -C build-gtk4

# Install system-wide
sudo ninja -C build-gtk4 install

# Run the modern application
iptux-plus
```

### macOS - Iptux Plus

To build Iptux Plus on macOS, you must use Homebrew to establish the correct dependency environment.

```sh
# Install required libraries
brew install meson gettext gtk4 libadwaita jsoncpp libsigc++@2 appstream

# Clone the repository
git clone git://github.com/iptux-src/iptux.git
cd iptux

# Compile and run
meson setup build-gtk4
ninja -C build-gtk4
sudo ninja -C build-gtk4 install
iptux-plus
```

---

## Classic Iptux (GTK3 Legacy Interface)

For environments lacking GTK4 support, or if you prefer the classic split-window aesthetic of legacy iptux, you can compile the core legacy binary instead.

### Linux (Debian and Ubuntu)
```sh
sudo apt-get install git libgtk-3-dev libglib2.0-dev libjsoncpp-dev g++ meson libsigc++-2.0-dev libayatana-appindicator3-dev appstream gettext
git clone git://github.com/iptux-src/iptux.git
cd iptux
meson setup build
meson compile -C build
sudo meson install -C build
iptux
```

### Classic Packaged Versions

<p align="left">
  <a href="https://snapcraft.io/iptux"><img src="https://snapcraft.io/static/images/badges/en/snap-store-white.svg" height="64" alt="Get it from the Snap Store"></a>
  <a href="https://flathub.org/apps/io.github.iptux_src.iptux"><img src="https://flathub.org/api/badge?svg&locale=en&light" height="64" alt="Get it on Flathub"></a>
</p>

## Usage & Connectivity

Ensure your system's firewall configuration permits traffic on **TCP/UDP port 2425**. If this port is actively blocked, you will not be able to discover or message local peers.

If another instance or application is using this port, you can customize the start port using:
```sh
iptux-plus --port 2426
```

### Compatibility List

For information regarding protocol interoperability with other IPMsg clients on Windows or Linux, please review the compatibility matrix:
https://github.com/iptux-src/iptux/wiki/Compatible-List

## Development and Testing

If you wish to test messaging locally without multiple machines, simply launch two clients binding to separate localhost loopback interfaces:

```sh
# Setup development build pointing to local resources
meson setup -Ddev=true build-gtk4

# Run two isolated simulated network clients
iptux-plus -b 127.0.0.2 &
iptux-plus -b 127.0.0.3 &
```
*(Note: Sending files across loopbacks 127.0.0.2 -> 127.0.0.3 has known testing limitations and is not strictly supported for debugging transfers).*

## Contributing

We welcome translations, bug reporting, and feature additions.

* **Translations**: Help regionalize Iptux at [Weblate](https://hosted.weblate.org/projects/iptux/#languages).
* **Updating Translaton Templates**: Run `meson compile update-po -C build` to refresh `po/iptux.pot`.
* **Issue Tracking**: Submit technical issues on the [GitHub Tracker](https://github.com/iptux-src/iptux/issues).

## Analytics

![Alt](https://repobeats.axiom.co/api/embed/8944a2744839c5ea58b0ea10f46a1d31c7fefa07.svg "Repobeats analytics image")
