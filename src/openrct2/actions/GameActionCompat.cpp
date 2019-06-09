/*****************************************************************************
 * Copyright (c) 2014-2019 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "GameAction.h"
#include "GuestSetNameAction.hpp"
#include "MazeSetTrackAction.hpp"
#include "PlaceParkEntranceAction.hpp"
#include "PlacePeepSpawnAction.hpp"
#include "RideCreateAction.hpp"
#include "RideDemolishAction.hpp"
#include "RideSetName.hpp"
#include "RideSetStatus.hpp"
#include "SetParkEntranceFeeAction.hpp"
#include "StaffSetNameAction.hpp"
#include "WallRemoveAction.hpp"

#pragma region PlaceParkEntranceAction
money32 place_park_entrance(int16_t x, int16_t y, int16_t z, uint8_t direction)
{
    auto gameAction = PlaceParkEntranceAction(x, y, z, direction);
    auto result = GameActions::Execute(&gameAction);
    if (result->Error == GA_ERROR::OK)
    {
        return 0;
    }
    else
    {
        return MONEY32_UNDEFINED;
    }
}

/**
 *
 *  rct2: 0x00666F4E
 */
money32 park_entrance_place_ghost(int32_t x, int32_t y, int32_t z, int32_t direction)
{
    park_entrance_remove_ghost();

    auto gameAction = PlaceParkEntranceAction(x, y, z, direction);
    gameAction.SetFlags(GAME_COMMAND_FLAG_GHOST);

    auto result = GameActions::Execute(&gameAction);
    if (result->Error == GA_ERROR::OK)
    {
        gParkEntranceGhostPosition.x = x;
        gParkEntranceGhostPosition.y = y;
        gParkEntranceGhostPosition.z = z;
        gParkEntranceGhostDirection = direction;
        gParkEntranceGhostExists = true;
    }
    return result->Cost;
}
#pragma endregion

#pragma region SetParkEntranceFeeAction
void park_set_entrance_fee(money32 fee)
{
    auto gameAction = SetParkEntranceFeeAction((money16)fee);
    GameActions::Execute(&gameAction);
}
#pragma endregion

#pragma region RideCreateAction
/**
 *
 *  rct2: 0x006B4800
 */
void ride_construct_new(ride_list_item listItem)
{
    int32_t rideEntryIndex = ride_get_entry_index(listItem.type, listItem.entry_index);
    int32_t colour1 = ride_get_random_colour_preset_index(listItem.type);
    int32_t colour2 = ride_get_unused_preset_vehicle_colour(rideEntryIndex);

    auto gameAction = RideCreateAction(listItem.type, listItem.entry_index, colour1, colour2);

    gameAction.SetCallback([](const GameAction* ga, const RideCreateGameActionResult* result) {
        if (result->Error != GA_ERROR::OK)
            return;

        auto ride = get_ride(result->rideIndex);
        ride_construct(ride);
    });

    GameActions::Execute(&gameAction);
}

money32 ride_create_command(int32_t type, int32_t subType, int32_t flags, ride_id_t* outRideIndex, uint8_t* outRideColour)
{
    int32_t rideEntryIndex = ride_get_entry_index(type, subType);
    int32_t colour1 = ride_get_random_colour_preset_index(type);
    int32_t colour2 = ride_get_unused_preset_vehicle_colour(rideEntryIndex);

    auto gameAction = RideCreateAction(type, subType, colour1, colour2);
    gameAction.SetFlags(flags);

    auto r = GameActions::Execute(&gameAction);
    const RideCreateGameActionResult* res = static_cast<RideCreateGameActionResult*>(r.get());

    // Callee's of this function expect MONEY32_UNDEFINED in case of failure.
    if (res->Error != GA_ERROR::OK)
    {
        return MONEY32_UNDEFINED;
    }

    *outRideIndex = res->rideIndex;
    *outRideColour = colour1;

    return res->Cost;
}

#pragma endregion

#pragma region RideSetStatusAction

void ride_set_status(Ride* ride, int32_t status)
{
    auto gameAction = RideSetStatusAction(ride->id, status);
    GameActions::Execute(&gameAction);
}

#pragma endregion

#pragma region RideSetNameAction
void ride_set_name(Ride* ride, const char* name, uint32_t flags)
{
    auto gameAction = RideSetNameAction(ride->id, name);
    gameAction.SetFlags(flags);
    GameActions::Execute(&gameAction);
}
#pragma endregion

#pragma region RideModifyAction
void ride_action_modify(Ride* ride, int32_t modifyType, int32_t flags)
{
    auto gameAction = RideDemolishAction(ride->id, modifyType);
    gameAction.SetFlags(flags);

    GameActions::Execute(&gameAction);
}
#pragma endregion

#pragma region GuestSetName

void guest_set_name(uint16_t spriteIndex, const char* name)
{
    auto gameAction = GuestSetNameAction(spriteIndex, name);
    GameActions::Execute(&gameAction);
}
#pragma endregion

#pragma region StaffSetName

void staff_set_name(uint16_t spriteIndex, const char* name)
{
    auto gameAction = StaffSetNameAction(spriteIndex, name);
    GameActions::Execute(&gameAction);
}
#pragma endregion

#pragma region PlacePeepSpawn
bool place_peep_spawn(CoordsXYZD location)
{
    auto gameAction = PlacePeepSpawnAction(location);
    auto result = GameActions::Execute(&gameAction);
    return result->Error == GA_ERROR::OK;
}
#pragma endregion

#pragma region MazeSetTrack
money32 maze_set_track(
    uint16_t x, uint16_t y, uint16_t z, uint8_t flags, bool initialPlacement, uint8_t direction, ride_id_t rideIndex,
    uint8_t mode)
{
    auto gameAction = MazeSetTrackAction(x, y, z, initialPlacement, direction, rideIndex, mode);
    gameAction.SetFlags(flags);

    GameActionResult::Ptr res;

    if (!(flags & GAME_COMMAND_FLAG_APPLY))
        res = GameActions::Query(&gameAction);
    else
        res = GameActions::Execute(&gameAction);

    // NOTE: ride_construction_tooldown_construct requires them to be set.
    // Refactor result type once theres no C code referencing this function.
    gGameCommandErrorText = res->ErrorMessage;
    gGameCommandErrorTitle = res->ErrorTitle;

    if (res->Error != GA_ERROR::OK)
    {
        return MONEY32_UNDEFINED;
    }

    return res->Cost;
}
#pragma endregion
