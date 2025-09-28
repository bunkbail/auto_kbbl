#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

echo "Starting installation of auto_kbbl..."

# 1. Compile the source code
echo "Compiling the binary..."
make clean
make

# 2. Install the binary
echo "Installing the binary to /usr/local/bin..."
sudo cp auto_kbbl /usr/local/bin/auto_kbbl
sudo chmod +x /usr/local/bin/auto_kbbl

# 3. Install the systemd service file
echo "Installing the systemd service file..."
sudo cp auto_kbbl.service /etc/systemd/system/auto_kbbl.service
sudo chmod 644 /etc/systemd/system/auto_kbbl.service

# 4. Reload systemd, enable and start the service
echo "Reloading systemd and starting the service..."
sudo systemctl daemon-reload
sudo systemctl enable auto_kbbl.service
sudo systemctl start auto_kbbl.service

echo ""
echo "--------------------------------------------------------"
echo "Installation complete!"
echo "auto_kbbl is now running as a background service."
echo "You can check its status with: systemctl status auto_kbbl"
echo "--------------------------------------------------------"