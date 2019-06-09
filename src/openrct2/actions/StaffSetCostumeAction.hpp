/*****************************************************************************
 * Copyright (c) 2014-2019 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "../Context.h"
#include "../interface/Window.h"
#include "../localisation/Localisation.h"
#include "../localisation/StringIds.h"
#include "../peep/Staff.h"
#include "../windows/Intent.h"
#include "../world/Sprite.h"
#include "GameAction.h"

/** rct2: 0x00982134 */
constexpr const bool peep_slow_walking_types[] = {
    false, // PEEP_SPRITE_TYPE_NORMAL
    false, // PEEP_SPRITE_TYPE_HANDYMAN
    false, // PEEP_SPRITE_TYPE_MECHANIC
    false, // PEEP_SPRITE_TYPE_SECURITY
    false, // PEEP_SPRITE_TYPE_ENTERTAINER_PANDA
    false, // PEEP_SPRITE_TYPE_ENTERTAINER_TIGER
    false, // PEEP_SPRITE_TYPE_ENTERTAINER_ELEPHANT
    false, // PEEP_SPRITE_TYPE_ENTERTAINER_ROMAN
    false, // PEEP_SPRITE_TYPE_ENTERTAINER_GORILLA
    false, // PEEP_SPRITE_TYPE_ENTERTAINER_SNOWMAN
    false, // PEEP_SPRITE_TYPE_ENTERTAINER_KNIGHT
    true,  // PEEP_SPRITE_TYPE_ENTERTAINER_ASTRONAUT
    false, // PEEP_SPRITE_TYPE_ENTERTAINER_BANDIT
    false, // PEEP_SPRITE_TYPE_ENTERTAINER_SHERIFF
    true,  // PEEP_SPRITE_TYPE_ENTERTAINER_PIRATE
    true,  // PEEP_SPRITE_TYPE_BALLOON
};

DEFINE_GAME_ACTION(StaffSetCostumeAction, GAME_COMMAND_SET_STAFF_COSTUME, GameActionResult)
{
private:
    uint16_t _spriteIndex;
    uint8_t _costume;

public:
    StaffSetCostumeAction()
    {
    }
    StaffSetCostumeAction(uint16_t spriteIndex, uint8_t costume)
        : _spriteIndex(spriteIndex)
        , _costume(costume)
    {
    }

    uint16_t GetActionFlags() const override
    {
        return GameAction::GetActionFlags() | GA_FLAGS::ALLOW_WHILE_PAUSED;
    }

    void Serialise(DataSerialiser & stream) override
    {
        GameAction::Serialise(stream);

        stream << DS_TAG(_spriteIndex) << DS_TAG(_costume);
    }

    GameActionResult::Ptr Query() const override
    {
        if (_spriteIndex >= MAX_SPRITES)
        {
            return std::make_unique<GameActionResult>(GA_ERROR::INVALID_PARAMETERS, STR_NONE);
        }

        Peep* peep = GET_PEEP(_spriteIndex);
        if (peep->type != PEEP_TYPE_STAFF || peep->staff_type != STAFF_TYPE_ENTERTAINER)
        {
            log_warning("Invalid game command for sprite %u", _spriteIndex);
            return std::make_unique<GameActionResult>(GA_ERROR::INVALID_PARAMETERS, STR_NONE);
        }

        auto spriteType = static_cast<PeepSpriteType>(_costume + 4);
        if (spriteType > std::size(peep_slow_walking_types))
        {
            log_warning("Invalid game command for sprite %u", _spriteIndex);
            return std::make_unique<GameActionResult>(GA_ERROR::INVALID_PARAMETERS, STR_NONE);
        }
        return std::make_unique<GameActionResult>();
    }

    GameActionResult::Ptr Execute() const override
    {
        Peep* peep = GET_PEEP(_spriteIndex);

        auto spriteType = static_cast<PeepSpriteType>(_costume + 4);
        peep->sprite_type = spriteType;
        peep->peep_flags &= ~PEEP_FLAGS_SLOW_WALK;
        if (peep_slow_walking_types[spriteType])
        {
            peep->peep_flags |= PEEP_FLAGS_SLOW_WALK;
        }
        peep->action_frame = 0;
        peep->UpdateCurrentActionSpriteType();
        peep->Invalidate();

        window_invalidate_by_number(WC_PEEP, _spriteIndex);
        auto intent = Intent(INTENT_ACTION_REFRESH_STAFF_LIST);
        context_broadcast_intent(&intent);

        auto res = std::make_unique<GameActionResult>();
        res->Position.x = peep->x;
        res->Position.y = peep->y;
        res->Position.z = peep->z;
        return res;
    }
};
