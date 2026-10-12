#include "DXActorBridge.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

// French DX Rev 1 entity ABI, also used by the original room loader. Keep the
// test oracle's mapping independent so a mistaken address cannot self-confirm.
#define FIELD(address, field) {address - 0xc000, offsetof(DXActor, field)}
static const struct { unsigned address; size_t offset; } fields[] = {
    FIELD(0xc240,ground.speed_x), FIELD(0xc250,ground.speed_y),
    FIELD(0xc260,body.x.fraction), FIELD(0xc270,body.y.fraction),
    FIELD(0xc280,body.collision.status), FIELD(0xc290,body.collision.state),
    FIELD(0xc2a0,body.collision.collision_flags), FIELD(0xc2b0,private_state[0]),
    FIELD(0xc2c0,private_state[1]), FIELD(0xc2d0,private_state[2]),
    FIELD(0xc2e0,body.collision.countdown), FIELD(0xc2f0,attack_cooldown),
    FIELD(0xc300,private_countdown2), FIELD(0xc310,body.collision.z),
    FIELD(0xc320,ground.speed_z), FIELD(0xc330,z_fraction),
    FIELD(0xc340,physics_flags), FIELD(0xc350,hitbox_flags), FIELD(0xc360,health),
    FIELD(0xc380,direction), FIELD(0xc390,private_state[4]), FIELD(0xc3a0,body.collision.type),
    FIELD(0xc3b0,sprite), FIELD(0xc3d0,inertia), FIELD(0xc3e0,home_room),
    FIELD(0xc3f0,recoil_x), FIELD(0xc400,recoil_y), FIELD(0xc410,body.collision.ignore_hits),
    FIELD(0xc420,ground.flash_timer), FIELD(0xc430,body.collision.options),
    FIELD(0xc440,private_state[3]), FIELD(0xc450,slow_countdown), FIELD(0xc460,load_order),
    FIELD(0xc470,ground.ground_status), FIELD(0xc480,ground.splash_timer),
    FIELD(0xc490,lifted), FIELD(0xc4a0,power_recoil), FIELD(0xc4d0,health_group),
    FIELD(0xc4e0,dropped_item), FIELD(0xc4f0,body.collision.ledge_count),
    FIELD(0xc5d0,body.collision.thrown_direction)
};
#undef FIELD

static int32_t signed_position(uint8_t low, uint8_t high) {
    unsigned value = low | (unsigned)high << 8;
    return value < 0x8000 ? (int32_t)value : (int32_t)value - 0x10000;
}

bool dx_actor_bridge_read(const GBContext *game, int32_t origin_x, int32_t origin_y,
                          DXActorBridgeFrame *frame) {
    if (!game || !game->wram || !game->hram || game->wram_bank != 1 || !frame ||
        origin_x < INT32_MIN + 32768 || origin_x > INT32_MAX - 32767 ||
        origin_y < INT32_MIN + 32768 || origin_y > INT32_MAX - 32767) return false;
    const uint8_t *ram = game->wram, *hram = game->hram;
    DXActorBridgeFrame next;
    memset(&next, 0, sizeof next);
    next.origin_x = origin_x; next.origin_y = origin_y;
    next.room = hram[0x76]; next.map = hram[0x77];
    next.transition = ram[0x124]; next.transition_direction = ram[0x125];
    next.transition_sequence = ram[0x16b];
    next.clock = (DXActorClock){ram[0x1b95], ram[0x16b], ram[0x124],
        ram[0x14f], ram[0x19f], ram[0x166], hram[0x67]};
    for (unsigned slot = 0; slot < 16; slot++) {
        DXActor *a = &next.roster.slots[slot];
        for (unsigned f = 0; f < sizeof fields / sizeof *fields; f++)
            *((uint8_t *)a + fields[f].offset) = ram[fields[f].address + slot];
        a->body.x.position = origin_x + signed_position(ram[0x200+slot],ram[0x220+slot]);
        a->body.y.position = origin_y + signed_position(ram[0x210+slot],ram[0x230+slot]);
        // Falling anchors have no high-byte tables in the original engine.
        a->ground.falling_x = origin_x + ram[0x4b0+slot];
        a->ground.falling_y = origin_y + ram[0x4c0+slot];
        memcpy(a->damage_hitbox, ram + 0x1580 + slot*4, 4);
        a->body.hitbox = a->hitbox_flags & 3;
        a->body.collision.frame = hram[0x67];
        a->body.collision.indoor = ram[0x1ba5];
        a->body.collision.switch_state = ram[0x16fb];
        a->body.collision.link_on_switch = ram[0x16f9];
        a->body.collision.link_immune = ram[0x1c6];
        a->ground.link_motion = ram[0x11c];
        a->ground.side_scrolling = hram[0x79] != 0;
        // hNoBGCollision is scratch for the *active* entity, not sixteen
        // persistent flags. It must be supplied by the per-handler scheduler.
        a->body.no_bg_collision = false;
    }
    memcpy(frame, &next, sizeof next);
    return true;
}

// Compare defined fields only; caller-owned structs may have arbitrary padding.
static bool equal_actor(const DXActor *a, const DXActor *b) {
    for (unsigned f = 0; f < sizeof fields / sizeof *fields; f++)
        if (*((const uint8_t *)a+fields[f].offset) != *((const uint8_t *)b+fields[f].offset)) return false;
    return a->body.x.position == b->body.x.position && a->body.y.position == b->body.y.position &&
        a->ground.falling_x == b->ground.falling_x && a->ground.falling_y == b->ground.falling_y &&
        !memcmp(a->damage_hitbox,b->damage_hitbox,4) && a->body.hitbox == b->body.hitbox &&
        a->body.no_bg_collision == b->body.no_bg_collision &&
        a->body.collision.frame == b->body.collision.frame &&
        a->body.collision.indoor == b->body.collision.indoor &&
        a->body.collision.switch_state == b->body.collision.switch_state &&
        a->body.collision.link_on_switch == b->body.collision.link_on_switch &&
        a->body.collision.link_immune == b->body.collision.link_immune &&
        a->ground.link_motion == b->ground.link_motion && a->ground.side_scrolling == b->ground.side_scrolling;
}

bool dx_actor_bridge_commit(GBContext *game, const DXActorBridgeFrame *baseline,
                            const DXActorRoster *updated) {
    if (!baseline || !updated) return false;
    DXActorBridgeFrame current;
    if (!dx_actor_bridge_read(game,baseline->origin_x,baseline->origin_y,&current) ||
        current.room != baseline->room || current.map != baseline->map ||
        current.transition != baseline->transition || current.transition_direction != baseline->transition_direction ||
        current.transition_sequence != baseline->transition_sequence ||
        memcmp(&current.clock,&baseline->clock,sizeof current.clock)) return false;
    for (unsigned slot = 0; slot < 16; slot++) {
        const DXActor *a = &updated->slots[slot], *old = &baseline->roster.slots[slot];
        if (!equal_actor(old,&current.roster.slots[slot])) return false;
        int64_t x = (int64_t)a->body.x.position - baseline->origin_x;
        int64_t y = (int64_t)a->body.y.position - baseline->origin_y;
        int64_t fx = (int64_t)a->ground.falling_x - baseline->origin_x;
        int64_t fy = (int64_t)a->ground.falling_y - baseline->origin_y;
        if (x < -32768 || x > 32767 || y < -32768 || y > 32767 || fx < 0 || fx > 255 || fy < 0 || fy > 255 ||
            a->body.hitbox != (a->hitbox_flags & 3) || a->body.no_bg_collision ||
            a->body.collision.frame != old->body.collision.frame ||
            a->body.collision.indoor != old->body.collision.indoor ||
            a->body.collision.switch_state != old->body.collision.switch_state ||
            a->body.collision.link_on_switch != old->body.collision.link_on_switch ||
            a->body.collision.link_immune != old->body.collision.link_immune ||
            a->ground.link_motion != old->ground.link_motion ||
            a->ground.side_scrolling != old->ground.side_scrolling) return false;
    }
    // No fallible operation after the first write. Validate every slot first,
    // including inactive ones which the next allocation may reuse.
    uint8_t *ram = game->wram;
    for (unsigned slot = 0; slot < 16; slot++) {
        const DXActor *a = &updated->slots[slot];
        for (unsigned f = 0; f < sizeof fields / sizeof *fields; f++)
            ram[fields[f].address+slot] = *((const uint8_t *)a+fields[f].offset);
        uint16_t x = (uint16_t)((int64_t)a->body.x.position-baseline->origin_x);
        uint16_t y = (uint16_t)((int64_t)a->body.y.position-baseline->origin_y);
        ram[0x200+slot] = (uint8_t)x; ram[0x220+slot] = (uint8_t)(x>>8);
        ram[0x210+slot] = (uint8_t)y; ram[0x230+slot] = (uint8_t)(y>>8);
        ram[0x4b0+slot] = (uint8_t)((int64_t)a->ground.falling_x-baseline->origin_x);
        ram[0x4c0+slot] = (uint8_t)((int64_t)a->ground.falling_y-baseline->origin_y);
        memcpy(ram+0x1580+slot*4,a->damage_hitbox,4);
    }
    return true;
}
