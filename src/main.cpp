#include <csignal>
#include <atomic>

#include "raylib.h"
#include "globals.h"
#include "display/display.h"
#include "serial/serial_reader.h"

int screenRes = 768;
Color displayColor = WHITE;
uint8_t dimAlpha = 80;
Font monoFont = {0};

static std::atomic<bool> shouldQuit{false};

static void signalHandler(int) {
	shouldQuit.store(true);
}

int main() {
	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);
	std::signal(SIGTSTP, signalHandler);

	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
	InitWindow(screenRes, screenRes, "EECS373 - Telescope");

	monoFont = LoadFontEx("resources/JetBrainsMono-Regular.ttf", 96, nullptr, 0);
	SetTextureFilter(monoFont.texture, TEXTURE_FILTER_POINT);

	StartSerialReader("/dev/ttyS0");

	InitDisplay();

	while (!WindowShouldClose() && !shouldQuit.load()) {
		DrawFrame();
	}

	StopSerialReader();
	CleanupDisplay();
	UnloadFont(monoFont);
	CloseWindow();
	return 0;
}
