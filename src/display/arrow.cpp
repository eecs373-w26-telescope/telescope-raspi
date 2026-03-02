#include "display/arrow.h"
#include "globals.h"
#include "raymath.h"

static void DrawArrow(Vector2 start, Vector2 end, float thickness, float headSize, Color color) {
	DrawLineEx(start, end, thickness, color);

	Vector2 direction = Vector2Subtract(end, start);
	float angle = atan2f(direction.y, direction.x);

	Vector2 p1 = {
		end.x - headSize * cosf(angle - PI / 6),
		end.y - headSize * sinf(angle - PI / 6)
	};
	Vector2 p2 = {
		end.x - headSize * cosf(angle + PI / 6),
		end.y - headSize * sinf(angle + PI / 6)
	};

	DrawTriangle(end, p2, p1, color);
}

void DrawDirectionArrow(float angle) {
	float radius = screenRes / 2.0f + 1;
	float arrowLen = 40.0f;
	float inset = 10.0f;
	float cx = screenRes / 2.0f;
	float cy = screenRes / 2.0f;

	// Fixed position: top-right of circle (-PI/4)
	float posAngle = -PI / 4;
	Vector2 arrowCenter = {
		cx + (radius - inset - arrowLen / 2.0f) * cosf(posAngle),
		cy + (radius - inset - arrowLen / 2.0f) * sinf(posAngle)
	};

	// Arrow points in the given direction, rotated about its center
	float halfLen = arrowLen / 2.0f;
	Vector2 arrowStart = {
		arrowCenter.x - halfLen * cosf(angle),
		arrowCenter.y - halfLen * sinf(angle)
	};
	Vector2 arrowEnd = {
		arrowCenter.x + halfLen * cosf(angle),
		arrowCenter.y + halfLen * sinf(angle)
	};
	DrawArrow(arrowStart, arrowEnd, 2, 10, displayColor);
}
