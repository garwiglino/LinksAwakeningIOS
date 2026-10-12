#pragma once
#include "DXActors.h"
#include "gbrt.h"

// A synchronous boundary adapter, NOT a second simulation or a room cache.
// The caller supplies the coordinate origin at the exact engine boundary:
// hMapRoom alone does not describe the origin during a scrolling transition.
// Call only on the engine queue. These functions do not execute CPU cycles.
typedef struct {
    DXActorRoster roster;
    int32_t origin_x, origin_y;
    uint8_t room, map, transition, transition_direction, transition_sequence;
    DXActorClock clock;
} DXActorBridgeFrame;

bool dx_actor_bridge_read(const GBContext *game, int32_t origin_x, int32_t origin_y,
                          DXActorBridgeFrame *frame);
// Compare-before-write: reject stale input, changed shared context, unsupported
// derived-field edits, and coordinates not representable by the original ABI.
// Failure leaves all machine memory untouched. Only entity-owned fields are
// written; no HRAM, CPU registers, clock, room-cleared mask or shared Link state.
bool dx_actor_bridge_commit(GBContext *game, const DXActorBridgeFrame *baseline,
                            const DXActorRoster *updated);
