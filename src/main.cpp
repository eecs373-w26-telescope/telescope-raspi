#include <csignal>
#include <atomic>

#include "raylib.h"
#include "globals.h"
#include "display/display.h"
#include "serial/serial_reader.h"

int screenRes = 768;
Color displayColor = WHITE;

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

	StartSerialReader("/dev/ttyS0");

	InitDisplay();

	while (!WindowShouldClose() && !shouldQuit.load()) {
		DrawFrame();
	}

	StopSerialReader();
	CleanupDisplay();
	CloseWindow();
	return 0;
}
