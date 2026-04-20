#pragma once

#include <cstdint>

enum class TelescopeState : uint8_t {
	INIT,
	IDLE,
	SEARCH,
	FOUND,
};

void InitStateMachine();
void UpdateStateMachine();
TelescopeState GetTelescopeState();
bool IsOverlayVisible();
