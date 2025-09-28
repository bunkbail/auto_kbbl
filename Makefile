CC = gcc
CFLAGS = -O2 -Wall
TARGET = auto_kbbl
INSTALL_DIR = /usr/local/bin
SERVICE_DIR = /etc/systemd/system

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	rm -f $(TARGET)

install: all
	@echo "Installing binary to $(INSTALL_DIR)..."
	sudo cp $(TARGET) $(INSTALL_DIR)
	@echo "Installing systemd service to $(SERVICE_DIR)..."
	sudo cp $(TARGET).service $(SERVICE_DIR)
	@echo "Reloading systemd daemon..."
	sudo systemctl daemon-reload
	@echo "Enabling and starting service..."
	sudo systemctl enable $(TARGET).service
	sudo systemctl start $(TARGET).service
	@echo "Installation complete."

uninstall:
	@echo "Stopping and disabling systemd service..."
	sudo systemctl stop $(TARGET).service || true
	sudo systemctl disable $(TARGET).service || true
	@echo "Removing files..."
	sudo rm -f $(INSTALL_DIR)/$(TARGET)
	sudo rm -f $(SERVICE_DIR)/$(TARGET).service
	@echo "Reloading systemd daemon..."
	sudo systemctl daemon-reload
	@echo "Uninstallation complete."