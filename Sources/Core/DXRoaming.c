#include "DXRoaming.h"

uint8_t dx_roaming_direction(const DXRoamingContext *context) {
    int64_t x = (int64_t)context->link_x - context->x;
    int64_t y = (int64_t)context->link_y - context->y + context->z;
    uint64_t ax = x < 0 ? (uint64_t)-x : (uint64_t)x;
    uint64_t ay = y < 0 ? (uint64_t)-y : (uint64_t)y;
    return ay >= ax ? (y < 0 ? 2 : 3) : (x < 0 ? 1 : 0);
}
bool dx_roaming_decide(DXRoamingState *state, const DXRoamingContext *context,
                       DXRandomByte random_byte, void *random_context,
                       DXRoamingAction *action) {
    if (!state || !context || !random_byte || !action || state->direction > 3 ||
        (state->type != 9 && state->type != 0x0b && state->type != 0x24)) return false;
    DXRoamingState next = *state;
    DXRoamingAction decision;
    if (!context->active_state) {
        if ((next.collisions & 15) || !next.countdown) {
            uint8_t pause = (random_byte(random_context) & 15) | 16;
            // The original collision branch retains HL pointing at collisions,
            // whereas the timer-expiry branch retains the countdown pointer.
            if (next.collisions & 15) next.collisions = pause;
            else next.countdown = pause;
            next.state = 1;
            next.speed_x = next.speed_y = 0;
        }
        decision = DX_ROAM_MOVE_AND_ANIMATE;
    } else if (next.countdown) {
        decision = DX_ROAM_GROUND_ONLY;
        if (next.countdown == 10 && !next.attack_cooldown &&
            dx_roaming_direction(context) == next.direction && next.type != 0x24) {
            if (next.type == 9) decision = context->credits ? DX_ROAM_WAIT : DX_ROAM_FIRE_ROCK;
            else decision = DX_ROAM_FIRE_ARROW_AND_GROUND;
        }
    } else {
        next.countdown = (random_byte(random_context) & 31) | 32;
        next.state = 0;
        next.turn_counter = (next.turn_counter + 1) & 3;
        next.direction = next.turn_counter ? random_byte(random_context) & 3 : dx_roaming_direction(context);
        static const int8_t sx[] = {8,-8,0,0}, sy[] = {0,0,-8,8};
        next.speed_x = sx[next.direction]; next.speed_y = sy[next.direction];
        decision = DX_ROAM_WAIT;
    }
    *state = next; *action = decision;
    return true;
}
