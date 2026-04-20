#!/bin/bash
# install.sh - Installs the Telescope HUD service on Raspberry Pi

set -e

# Get the absolute path of the directory where this script is located
INSTALL_DIR=$(cd "$(dirname "$0")" && pwd)

echo "==> Configuring Telescope HUD Service..."

# 1. Ensure the launch script is executable
if [ -f "$INSTALL_DIR/run_cage.sh" ]; then
    chmod +x "$INSTALL_DIR/run_cage.sh"
    echo "  - Set executable permissions on run_cage.sh"
else
    echo "  ! Error: run_cage.sh not found in $INSTALL_DIR"
    exit 1
fi

# 2. Symlink the service file to systemd
SERVICE_FILE="telescope.service"
if [ -f "$INSTALL_DIR/$SERVICE_FILE" ]; then
    echo "  - Symlinking $SERVICE_FILE to /etc/systemd/system/"
    ln -sf "$INSTALL_DIR/$SERVICE_FILE" /etc/systemd/system/telescope.service
else
    echo "  ! Error: $SERVICE_FILE not found in $INSTALL_DIR"
    exit 1
fi

# 3. Reload systemd and enable service
echo "  - Reloading systemd daemon..."
systemctl daemon-reload

echo "  - Enabling telescope.service..."
systemctl enable telescope.service

# 4. Attempt to restart
echo "  - Restarting telescope.service..."
systemctl restart telescope.service

echo ""
echo "==> Installation complete!"
echo "Check status with: systemctl status telescope.service"
echo "View logs with:   tail -f /var/log/telescope.log"
