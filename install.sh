#!/bin/bash
# install.sh - Installs the original single-monitor DRM service

set -e

echo "==> Installing telescope.service..."
# Copy the service file created in the project directory to the system directory
sudo cp telescope.service /etc/systemd/system/telescope.service

echo "==> Reloading systemd and restarting service..."
sudo systemctl daemon-reload
sudo systemctl enable telescope.service
sudo systemctl restart telescope.service

echo ""
echo "==> Installation complete!"
echo "Status: $(systemctl is-active telescope.service)"
