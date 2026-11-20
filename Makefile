CC = gcc
CXX = clang++
CFLAGS = -Wall -Wextra -std=c11 -O2
CXXFLAGS = -std=c++11 -O2 -fobjc-arc
TARGET_CLI = unmad_fs
TARGET_GUI = unmad_gui
APP_BUNDLE = UnmadFS.app
DISK = disk.img

.PHONY: all cli gui run test hex clean

all: $(TARGET_CLI) $(TARGET_GUI) $(APP_BUNDLE)

unmad_fs_core.o: unmad_fs_core.c unmad_fs_core.h
	$(CC) $(CFLAGS) -c unmad_fs_core.c -o unmad_fs_core.o

$(TARGET_CLI): unmad_fs.c unmad_fs_core.o
	$(CC) $(CFLAGS) unmad_fs.c unmad_fs_core.o -o $(TARGET_CLI)

$(TARGET_GUI): gui.mm gui_assets.h unmad_fs_core.o
	$(CXX) $(CXXFLAGS) gui.mm unmad_fs_core.o -framework Cocoa -framework WebKit -o $(TARGET_GUI)

$(APP_BUNDLE): $(TARGET_GUI)
	mkdir -p $(APP_BUNDLE)/Contents/MacOS
	mkdir -p $(APP_BUNDLE)/Contents/Resources
	cp $(TARGET_GUI) $(APP_BUNDLE)/Contents/MacOS/UnmadFS
	@echo '<?xml version="1.0" encoding="UTF-8"?>' > $(APP_BUNDLE)/Contents/Info.plist
	@echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' >> $(APP_BUNDLE)/Contents/Info.plist
	@echo '<plist version="1.0"><dict>' >> $(APP_BUNDLE)/Contents/Info.plist
	@echo '  <key>CFBundleExecutable</key><string>UnmadFS</string>' >> $(APP_BUNDLE)/Contents/Info.plist
	@echo '  <key>CFBundleIdentifier</key><string>com.unmad.fs.app</string>' >> $(APP_BUNDLE)/Contents/Info.plist
	@echo '  <key>CFBundleName</key><string>UnmadFS</string>' >> $(APP_BUNDLE)/Contents/Info.plist
	@echo '  <key>CFBundlePackageType</key><string>APPL</string>' >> $(APP_BUNDLE)/Contents/Info.plist
	@echo '  <key>CFBundleShortVersionString</key><string>1.0</string>' >> $(APP_BUNDLE)/Contents/Info.plist
	@echo '</dict></plist>' >> $(APP_BUNDLE)/Contents/Info.plist

cli: $(TARGET_CLI)

gui: $(TARGET_GUI)
	./$(TARGET_GUI)

run: $(TARGET_CLI)
	./$(TARGET_CLI)

test: $(TARGET_CLI)
	bash run_tests.sh

hex:
	@if [ -f $(DISK) ]; then \
		xxd $(DISK) | head -n 10; \
	else \
		echo "Error: $(DISK) not found. Run 'make run' or 'make test' first."; \
	fi

clean:
	rm -rf $(TARGET_CLI) $(TARGET_GUI) $(APP_BUNDLE) *.o $(DISK)
