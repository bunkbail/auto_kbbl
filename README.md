# auto_kbbl - Automatic Keyboard Backlight Utility for Linux

![GitHub](https://img.shields.io/github/license/bunkbail/auto_kbbl)

A lightweight, standalone C utility that automatically turns your keyboard backlight on with key-press activity and off after a period of inactivity. It runs as a systemd service, requires no configuration, and is designed to "just work".

## Features

-   **Automatic Control**: Turns the backlight on during keyboard use and off when idle.
-   **Daemon Service**: Runs silently in the background using systemd.
-   **Efficient**: Written in C with a low memory and CPU footprint.
-   **Smart Device Detection**: Automatically finds the correct keyboard event device and LED control paths.
-   **Configurable**: All key behaviors can be customized with command-line flags.

## Prerequisites

-   A Linux distribution using `systemd` (e.g., Ubuntu, Fedora, Arch Linux, Debian).
-   A C compiler (`gcc`) and `make` to build the project.
-   `git` to clone the repository.

You can install these on a Debian/Ubuntu system with:
`sudo apt-get update && sudo apt-get install build-essential git`

## One-Liner Installation

For a fast and easy setup on a systemd-based Linux distribution, run the following command:

```bash
git clone https://github.com/bunkbail/auto_kbbl.git && cd auto_kbbl && sudo bash install.sh
```

## Manual Installation

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/bunkbail/auto_kbbl.git
    cd auto_kbbl
    ```

2.  **Compile the code:**
    ```bash
    make
    ```

3.  **Run the installation script:**
    ```bash
    sudo bash install.sh
    ```

The script will compile the code, install the binary to `/usr/local/bin`, copy the systemd service file, and start/enable the service.

## Usage

The installation script handles running the program as a service. You can manage the service using standard `systemctl` commands:

-   **Check the status of the service:**
    ```bash
    systemctl status auto_kbbl
    ```

-   **View live logs:**
    ```bash
    journalctl -u auto_kbbl -f
    ```

-   **Stop the service:**
    ```bash
    sudo systemctl stop auto_kbbl
    ```

-   **Start the service:**
    ```bash
    sudo systemctl start auto_kbbl
    ```

## Uninstallation

To completely remove the utility and its service file from your system, navigate to the cloned repository directory and run:

-   ```bash
    sudo make uninstall
    ```

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
