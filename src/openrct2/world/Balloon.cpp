/*****************************************************************************
 * Copyright (c) 2014-2019 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "../Game.h"
#include "../audio/audio.h"
#include "../network/network.h"
#include "../scenario/Scenario.h"
#include "../util/Util.h"
#include "Sprite.h"

bool rct_sprite::IsBalloon()
{
    return this->balloon.sprite_identifier == SPRITE_IDENTIFIER_MISC && this->balloon.type == SPRITE_MISC_BALLOON;
}

rct_balloon* rct_sprite::AsBalloon()
{
    rct_balloon* result = nullptr;
    if (IsBalloon())
    {
        result = (rct_balloon*)this;
    }
    return result;
}

void rct_balloon::Update()
{
    invalidate_sprite_2((rct_sprite*)this);
    if (popped == 1)
    {
        frame++;
        if (frame >= 5)
        {
            sprite_remove((rct_sprite*)this);
        }
    }
    else
    {
        time_to_move++;
        if (time_to_move >= 3)
        {
            time_to_move = 0;
            frame++;
            sprite_move(x, y, z + 1, (rct_sprite*)this);

            int32_t maxZ = 1967 - ((x ^ y) & 31);
            if (z >= maxZ)
            {
                Pop();
            }
        }
    }
}

void rct_balloon::Press()
{
    if (popped != 1)
    {
        // There is a random chance that pressing the balloon will not pop it
        // and instead shift it slightly
        uint32_t random = scenario_rand();
        if ((sprite_index & 7) || (random & 0xFFFF) < 0x2000)
        {
            Pop();
        }
        else
        {
            int16_t shift = ((random & 0x80000000) ? -6 : 6);
            sprite_move(x + shift, y, z, (rct_sprite*)this);
        }
    }
}

void rct_balloon::Pop()
{
    popped = 1;
    frame = 0;
    audio_play_sound_at_location(SOUND_BALLOON_POP, x, y, z);
}

void create_balloon(int32_t x, int32_t y, int32_t z, int32_t colour, bool isPopped)
{
    rct_sprite* sprite = create_sprite(2);
    if (sprite != nullptr)
    {
        sprite->balloon.sprite_width = 13;
        sprite->balloon.sprite_height_negative = 22;
        sprite->balloon.sprite_height_positive = 11;
        sprite->balloon.sprite_identifier = SPRITE_IDENTIFIER_MISC;
        sprite_move(x, y, z, sprite);
        sprite->balloon.type = SPRITE_MISC_BALLOON;
        sprite->balloon.time_to_move = 0;
        sprite->balloon.frame = 0;
        sprite->balloon.colour = colour;
        sprite->balloon.popped = (isPopped ? 1 : 0);
    }
}

void balloon_update(rct_balloon* balloon)
{
    balloon->Update();
}
