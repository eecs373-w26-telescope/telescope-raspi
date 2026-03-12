#include "state_machine/state_machine.h"

static TelescopeState current_state = TelescopeState::INIT;

void InitStateMachine() {
	current_state = TelescopeState::INIT;
}

void UpdateStateMachine() {
	switch (current_state) {

	case TelescopeState::INIT:
		// hardware initiazation, gps fix, imu direction, etc.
		break;

	case TelescopeState::SETUP:
		// configure telescope pamamaters
		break;

	case TelescopeState::IDLE:
		// TODO: transition to SEARCH when user starts search
		break;

	case TelescopeState::SEARCH:
		// TODO: transition to FOUND when target acquired
		// TODO: transition to IDLE if search cancelled/timed out
		break;

	case TelescopeState::FOUND:
		// TODO: transition to IDLE when tracking lost
		break;
	}
}

TelescopeState GetTelescopeState() {
	return current_state;
}
