#include <csignal>
#include <atomic>

#include "raylib.h"
#include "globals.h"
#include "display/display.h"
#include "display/drm_mirror.h"
#include "serial/serial_reader.h"
#include "serial/time_sender.h"

int screenRes = 768;
Color _displayColor = RED;
uint8_t dimAlpha = 80;
Font monoFont = {0};

static std::atomic<bool> shouldQuit{false};

static void signalHandler(int) {
	shouldQuit.store(true);
}

int main(int argc, char* argv[]) {
	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);
	std::signal(SIGTSTP, signalHandler);

	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
	InitWindow(screenRes, screenRes, "EECS373 - Telescope");
	StartDRMMirror();

	monoFont = LoadFontEx("resources/JetBrainsMono-Regular.ttf", 96, nullptr, 0);
	SetTextureFilter(monoFont.texture, TEXTURE_FILTER_POINT);

	const char* serial_device = (argc > 1) ? argv[1] : "/dev/ttyS0";
	StartSerialReader(serial_device);
	StartTimeSender();

	InitDisplay();

	while (!WindowShouldClose() && !shouldQuit.load()) {
		DrawFrame();
	}

	StopTimeSender();
	StopSerialReader();
	StopDRMMirror();
	CleanupDisplay();
	UnloadFont(monoFont);
	CloseWindow();
	return 0;
}
