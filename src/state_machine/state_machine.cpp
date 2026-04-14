#include "state_machine/state_machine.h"
#include "protocol/shared_state.h"
#include <mutex>

static TelescopeState current_state = TelescopeState::INIT;

void InitStateMachine() {
	current_state = TelescopeState::INIT;
}

void UpdateStateMachine() {
	std::lock_guard<std::mutex> lock(g_shared_state.mtx);
	if (g_shared_state.state_sync_received) {
		current_state = static_cast<TelescopeState>(g_shared_state.state_sync.state);
	}
}

TelescopeState GetTelescopeState() {
	return current_state;
}
