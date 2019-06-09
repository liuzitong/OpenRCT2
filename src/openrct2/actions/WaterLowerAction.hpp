/*****************************************************************************
 * Copyright (c) 2014-2019 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once
#include "../audio/audio.h"
#include "GameAction.h"
#include "WaterSetHeightAction.hpp"

DEFINE_GAME_ACTION(WaterLowerAction, GAME_COMMAND_LOWER_WATER, GameActionResult)
{
private:
    MapRange _range;

public:
    WaterLowerAction()
    {
    }
    WaterLowerAction(MapRange range)
        : _range(range)
    {
    }

    uint16_t GetActionFlags() const override
    {
        return GameAction::GetActionFlags();
    }

    void Serialise(DataSerialiser & stream) override
    {
        GameAction::Serialise(stream);

        stream << DS_TAG(_range);
    }

    GameActionResult::Ptr Query() const override
    {
        return QueryExecute(false);
    }

    GameActionResult::Ptr Execute() const override
    {
        return QueryExecute(true);
    }

private:
    GameActionResult::Ptr QueryExecute(bool isExecuting) const
    {
        auto res = MakeResult();

        // Keep big coordinates within map boundaries
        auto aX = std::max<decltype(_range.GetLeft())>(32, _range.GetLeft());
        auto bX = std::min<decltype(_range.GetRight())>(gMapSizeMaxXY, _range.GetRight());
        auto aY = std::max<decltype(_range.GetTop())>(32, _range.GetTop());
        auto bY = std::min<decltype(_range.GetBottom())>(gMapSizeMaxXY, _range.GetBottom());

        MapRange validRange = MapRange{ aX, aY, bX, bY };

        res->Position.x = ((validRange.GetLeft() + validRange.GetRight()) / 2) + 16;
        res->Position.y = ((validRange.GetTop() + validRange.GetBottom()) / 2) + 16;
        int16_t z = tile_element_height(res->Position.x, res->Position.y);
        int16_t waterHeight = tile_element_water_height(res->Position.x, res->Position.y);
        if (waterHeight != 0)
        {
            z = waterHeight;
        }
        res->Position.z = z;
        res->ExpenditureType = RCT_EXPENDITURE_TYPE_LANDSCAPING;

        uint8_t minHeight = GetLowestHeight();
        bool hasChanged = false;
        for (int32_t y = validRange.GetTop(); y <= validRange.GetBottom(); y += 32)
        {
            for (int32_t x = validRange.GetLeft(); x <= validRange.GetRight(); x += 32)
            {
                TileElement* tileElement = map_get_surface_element_at(x / 32, y / 32);
                if (tileElement == nullptr)
                    continue;

                SurfaceElement* surfaceElement = tileElement->AsSurface();
                uint8_t height = surfaceElement->GetWaterHeight();
                if (height == 0)
                    continue;

                height *= 2;
                if (height < minHeight)
                    continue;

                height -= 2;
                auto waterSetHeightAction = WaterSetHeightAction({ x, y }, height);
                waterSetHeightAction.SetFlags(GetFlags());
                auto result = isExecuting ? GameActions::ExecuteNested(&waterSetHeightAction)
                                          : GameActions::QueryNested(&waterSetHeightAction);
                if (result->Error == GA_ERROR::OK)
                {
                    res->Cost += result->Cost;
                    hasChanged = true;
                }
                else
                {
                    result->ErrorTitle = STR_CANT_LOWER_WATER_LEVEL_HERE;
                    return result;
                }
            }
        }

        if (isExecuting && hasChanged)
        {
            audio_play_sound_at_location(SOUND_LAYING_OUT_WATER, res->Position.x, res->Position.y, res->Position.z);
        }
        // Force ride construction to recheck area
        _currentTrackSelectionFlags |= TRACK_SELECTION_FLAG_RECHECK;

        return res;
    }

private:
    uint8_t GetLowestHeight() const
    {
        uint8_t minHeight{ 0 };
        for (int32_t y = _range.GetTop(); y <= _range.GetBottom(); y += 32)
        {
            for (int32_t x = _range.GetLeft(); x <= _range.GetRight(); x += 32)
            {
                TileElement* tile_element = map_get_surface_element_at({ x, y });
                if (tile_element == nullptr)
                    continue;

                uint8_t height = tile_element->AsSurface()->GetWaterHeight();
                if (height == 0)
                    continue;

                height *= 2;
                if (height > minHeight)
                {
                    minHeight = height;
                }
            }
        }

        return minHeight;
    }
};
