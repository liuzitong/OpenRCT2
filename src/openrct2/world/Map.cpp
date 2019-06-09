/*****************************************************************************
 * Copyright (c) 2014-2019 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Map.h"

#include "../Cheats.h"
#include "../Context.h"
#include "../Game.h"
#include "../Input.h"
#include "../OpenRCT2.h"
#include "../actions/BannerRemoveAction.hpp"
#include "../actions/FootpathRemoveAction.hpp"
#include "../actions/LandLowerAction.hpp"
#include "../actions/LandRaiseAction.hpp"
#include "../actions/LandSetHeightAction.hpp"
#include "../actions/LandSetRightsAction.hpp"
#include "../actions/LargeSceneryRemoveAction.hpp"
#include "../actions/ParkEntranceRemoveAction.hpp"
#include "../actions/SmallSceneryRemoveAction.hpp"
#include "../actions/WallRemoveAction.hpp"
#include "../actions/WaterSetHeightAction.hpp"
#include "../audio/audio.h"
#include "../config/Config.h"
#include "../core/Guard.hpp"
#include "../interface/Cursors.h"
#include "../interface/Window.h"
#include "../localisation/Date.h"
#include "../localisation/Localisation.h"
#include "../management/Finance.h"
#include "../network/network.h"
#include "../object/ObjectManager.h"
#include "../object/TerrainSurfaceObject.h"
#include "../ride/RideData.h"
#include "../ride/Track.h"
#include "../ride/TrackData.h"
#include "../ride/TrackDesign.h"
#include "../scenario/Scenario.h"
#include "../util/Util.h"
#include "../windows/Intent.h"
#include "Banner.h"
#include "Climate.h"
#include "Footpath.h"
#include "LargeScenery.h"
#include "MapAnimation.h"
#include "Park.h"
#include "Scenery.h"
#include "SmallScenery.h"
#include "Surface.h"
#include "TileInspector.h"
#include "Wall.h"

#include <algorithm>
#include <iterator>

using namespace OpenRCT2;

/**
 * Replaces 0x00993CCC, 0x00993CCE
 */
const CoordsXY CoordsDirectionDelta[] = { { -32, 0 },   { 0, +32 },   { +32, 0 },   { 0, -32 },
                                          { -32, +32 }, { +32, +32 }, { +32, -32 }, { -32, -32 } };

const TileCoordsXY TileDirectionDelta[] = { { -1, 0 },  { 0, +1 },  { +1, 0 },  { 0, -1 },
                                            { -1, +1 }, { +1, +1 }, { +1, -1 }, { -1, -1 } };

uint16_t gMapSelectFlags;
uint16_t gMapSelectType;
LocationXY16 gMapSelectPositionA;
LocationXY16 gMapSelectPositionB;
LocationXYZ16 gMapSelectArrowPosition;
uint8_t gMapSelectArrowDirection;

uint8_t gMapGroundFlags;

uint16_t gWidePathTileLoopX;
uint16_t gWidePathTileLoopY;
uint16_t gGrassSceneryTileLoopPosition;

int16_t gMapSizeUnits;
int16_t gMapSizeMinus2;
int16_t gMapSize;
int16_t gMapSizeMaxXY;
int16_t gMapBaseZ;

TileElement gTileElements[MAX_TILE_TILE_ELEMENT_POINTERS * 3];
TileElement* gTileElementTilePointers[MAX_TILE_TILE_ELEMENT_POINTERS];
std::vector<CoordsXY> gMapSelectionTiles;
std::vector<PeepSpawn> gPeepSpawns;

TileElement* gNextFreeTileElement;
uint32_t gNextFreeTileElementPointerIndex;

bool gLandMountainMode;
bool gLandPaintMode;
bool gClearSmallScenery;
bool gClearLargeScenery;
bool gClearFootpath;

uint16_t gLandRemainingOwnershipSales;
uint16_t gLandRemainingConstructionSales;

LocationXYZ16 gCommandPosition;

bool gMapLandRightsUpdateSuccess;

static void clear_elements_at(int32_t x, int32_t y);
static void translate_3d_to_2d(int32_t rotation, int32_t* x, int32_t* y);

void rotate_map_coordinates(int16_t* x, int16_t* y, int32_t rotation)
{
    int32_t temp;

    switch (rotation)
    {
        case TILE_ELEMENT_DIRECTION_WEST:
            break;
        case TILE_ELEMENT_DIRECTION_NORTH:
            temp = *x;
            *x = *y;
            *y = -temp;
            break;
        case TILE_ELEMENT_DIRECTION_EAST:
            *x = -*x;
            *y = -*y;
            break;
        case TILE_ELEMENT_DIRECTION_SOUTH:
            temp = *y;
            *y = *x;
            *x = -temp;
            break;
    }
}

LocationXY16 coordinate_3d_to_2d(const LocationXYZ16* coordinate_3d, int32_t rotation)
{
    LocationXY16 coordinate_2d;

    switch (rotation)
    {
        // this function has to use right-shift (... >> 1) since dividing
        // by 2 with (... / 2) can differ by -1 and cause issues (see PR #9301)
        default:
        case 0:
            coordinate_2d.x = coordinate_3d->y - coordinate_3d->x;
            coordinate_2d.y = ((coordinate_3d->y + coordinate_3d->x) >> 1) - coordinate_3d->z;
            break;
        case 1:
            coordinate_2d.x = -coordinate_3d->y - coordinate_3d->x;
            coordinate_2d.y = ((coordinate_3d->y - coordinate_3d->x) >> 1) - coordinate_3d->z;
            break;
        case 2:
            coordinate_2d.x = -coordinate_3d->y + coordinate_3d->x;
            coordinate_2d.y = ((-coordinate_3d->y - coordinate_3d->x) >> 1) - coordinate_3d->z;
            break;
        case 3:
            coordinate_2d.x = coordinate_3d->y + coordinate_3d->x;
            coordinate_2d.y = ((-coordinate_3d->y + coordinate_3d->x) >> 1) - coordinate_3d->z;
            break;
    }
    return coordinate_2d;
}

void tile_element_iterator_begin(tile_element_iterator* it)
{
    it->x = 0;
    it->y = 0;
    it->element = map_get_first_element_at(0, 0);
}

int32_t tile_element_iterator_next(tile_element_iterator* it)
{
    if (it->element == nullptr)
    {
        it->element = map_get_first_element_at(it->x, it->y);
        return 1;
    }

    if (!it->element->IsLastForTile())
    {
        it->element++;
        return 1;
    }

    if (it->x < (MAXIMUM_MAP_SIZE_TECHNICAL - 1))
    {
        it->x++;
        it->element = map_get_first_element_at(it->x, it->y);
        return 1;
    }

    if (it->y < (MAXIMUM_MAP_SIZE_TECHNICAL - 1))
    {
        it->x = 0;
        it->y++;
        it->element = map_get_first_element_at(it->x, it->y);
        return 1;
    }

    return 0;
}

void tile_element_iterator_restart_for_tile(tile_element_iterator* it)
{
    it->element = nullptr;
}

TileElement* map_get_first_element_at(int32_t x, int32_t y)
{
    if (x < 0 || y < 0 || x > (MAXIMUM_MAP_SIZE_TECHNICAL - 1) || y > (MAXIMUM_MAP_SIZE_TECHNICAL - 1))
    {
        log_error("Trying to access element outside of range");
        return nullptr;
    }
    return gTileElementTilePointers[x + y * MAXIMUM_MAP_SIZE_TECHNICAL];
}

TileElement* map_get_nth_element_at(int32_t x, int32_t y, int32_t n)
{
    TileElement* tileElement = map_get_first_element_at(x, y);
    if (tileElement == nullptr)
    {
        return nullptr;
    }
    // Iterate through elements on this tile. This has to be walked, rather than
    // jumped directly to, because n may exceed element count for given tile,
    // and the order of tiles (unlike elements) is not synced over multiplayer.
    while (n >= 0)
    {
        if (n == 0)
        {
            return tileElement;
        }
        if (tileElement->IsLastForTile())
        {
            break;
        }
        tileElement++;
        n--;
    }
    // The element sought for is not within given tile.
    return nullptr;
}

void map_set_tile_elements(int32_t x, int32_t y, TileElement* elements)
{
    if (x < 0 || y < 0 || x > (MAXIMUM_MAP_SIZE_TECHNICAL - 1) || y > (MAXIMUM_MAP_SIZE_TECHNICAL - 1))
    {
        log_error("Trying to access element outside of range");
        return;
    }
    gTileElementTilePointers[x + y * MAXIMUM_MAP_SIZE_TECHNICAL] = elements;
}

TileElement* map_get_surface_element_at(int32_t x, int32_t y)
{
    TileElement* tileElement = map_get_first_element_at(x, y);

    if (tileElement == nullptr)
        return nullptr;

    // Find the first surface element
    while (tileElement->GetType() != TILE_ELEMENT_TYPE_SURFACE)
    {
        if (tileElement->IsLastForTile())
            return nullptr;

        tileElement++;
    }

    return tileElement;
}

TileElement* map_get_surface_element_at(const CoordsXY coords)
{
    return map_get_surface_element_at(coords.x / 32, coords.y / 32);
}

TileElement* map_get_path_element_at(int32_t x, int32_t y, int32_t z)
{
    TileElement* tileElement = map_get_first_element_at(x, y);

    if (tileElement == nullptr)
        return nullptr;

    // Find the path element at known z
    do
    {
        if (tileElement->IsGhost())
            continue;
        if (tileElement->GetType() != TILE_ELEMENT_TYPE_PATH)
            continue;
        if (tileElement->base_height != z)
            continue;

        return tileElement;
    } while (!(tileElement++)->IsLastForTile());

    return nullptr;
}

BannerElement* map_get_banner_element_at(int32_t x, int32_t y, int32_t z, uint8_t position)
{
    TileElement* tileElement = map_get_first_element_at(x, y);

    if (tileElement == nullptr)
        return nullptr;

    // Find the banner element at known z and position
    do
    {
        if (tileElement->GetType() != TILE_ELEMENT_TYPE_BANNER)
            continue;
        if (tileElement->base_height != z)
            continue;
        if (tileElement->AsBanner()->GetPosition() != position)
            continue;

        return tileElement->AsBanner();
    } while (!(tileElement++)->IsLastForTile());

    return nullptr;
}

/**
 *
 *  rct2: 0x0068AB4C
 */
void map_init(int32_t size)
{
    gNumMapAnimations = 0;
    gNextFreeTileElementPointerIndex = 0;

    for (int32_t i = 0; i < MAX_TILE_TILE_ELEMENT_POINTERS; i++)
    {
        TileElement* tile_element = &gTileElements[i];
        tile_element->ClearAs(TILE_ELEMENT_TYPE_SURFACE);
        tile_element->flags = TILE_ELEMENT_FLAG_LAST_TILE;
        tile_element->base_height = 14;
        tile_element->clearance_height = 14;
        tile_element->AsSurface()->SetWaterHeight(0);
        tile_element->AsSurface()->SetSlope(TILE_ELEMENT_SLOPE_FLAT);
        tile_element->AsSurface()->SetGrassLength(GRASS_LENGTH_CLEAR_0);
        tile_element->AsSurface()->SetOwnership(OWNERSHIP_UNOWNED);
        tile_element->AsSurface()->SetParkFences(0);
        tile_element->AsSurface()->SetSurfaceStyle(TERRAIN_GRASS);
        tile_element->AsSurface()->SetEdgeStyle(TERRAIN_EDGE_ROCK);
    }

    gGrassSceneryTileLoopPosition = 0;
    gWidePathTileLoopX = 0;
    gWidePathTileLoopY = 0;
    gMapSizeUnits = size * 32 - 32;
    gMapSizeMinus2 = size * 32 - 2;
    gMapSize = size;
    gMapSizeMaxXY = size * 32 - 33;
    gMapBaseZ = 7;
    map_update_tile_pointers();
    map_remove_out_of_range_elements();

    auto intent = Intent(INTENT_ACTION_MAP);
    context_broadcast_intent(&intent);
}

/**
 * Counts the number of surface tiles that offer land ownership rights for sale,
 * but haven't been bought yet. It updates gLandRemainingOwnershipSales and
 * gLandRemainingConstructionSales.
 */
void map_count_remaining_land_rights()
{
    gLandRemainingOwnershipSales = 0;
    gLandRemainingConstructionSales = 0;

    for (int32_t x = 0; x < MAXIMUM_MAP_SIZE_TECHNICAL; x++)
    {
        for (int32_t y = 0; y < MAXIMUM_MAP_SIZE_TECHNICAL; y++)
        {
            TileElement* element = map_get_surface_element_at(x, y);
            // Surface elements are sometimes hacked out to save some space for other map elements
            if (element == nullptr)
            {
                continue;
            }

            uint8_t flags = element->AsSurface()->GetOwnership();

            // Do not combine this condition with (flags & OWNERSHIP_AVAILABLE)
            // As some RCT1 parks have owned tiles with the 'construction rights available' flag also set
            if (!(flags & OWNERSHIP_OWNED))
            {
                if (flags & OWNERSHIP_AVAILABLE)
                {
                    gLandRemainingOwnershipSales++;
                }
                else if (
                    (flags & OWNERSHIP_CONSTRUCTION_RIGHTS_AVAILABLE) && (flags & OWNERSHIP_CONSTRUCTION_RIGHTS_OWNED) == 0)
                {
                    gLandRemainingConstructionSales++;
                }
            }
        }
    }
}

/**
 * This is meant to strip TILE_ELEMENT_FLAG_GHOST flag from all elements when
 * importing a park.
 *
 * This can only exist in hacked parks, as we remove ghost elements while saving.
 *
 * This is less invasive than removing ghost elements themselves, as they can
 * contain valid data.
 */
void map_strip_ghost_flag_from_elements()
{
    for (auto& element : gTileElements)
    {
        element.SetGhost(false);
    }
}

/**
 *
 *  rct2: 0x0068AFFD
 */
void map_update_tile_pointers()
{
    int32_t i, x, y;

    for (i = 0; i < MAX_TILE_TILE_ELEMENT_POINTERS; i++)
    {
        gTileElementTilePointers[i] = TILE_UNDEFINED_TILE_ELEMENT;
    }

    TileElement* tileElement = gTileElements;
    TileElement** tile = gTileElementTilePointers;
    for (y = 0; y < MAXIMUM_MAP_SIZE_TECHNICAL; y++)
    {
        for (x = 0; x < MAXIMUM_MAP_SIZE_TECHNICAL; x++)
        {
            *tile++ = tileElement;
            while (!(tileElement++)->IsLastForTile())
                ;
        }
    }

    gNextFreeTileElement = tileElement;
}

/**
 * Return the absolute height of an element, given its (x,y) coordinates
 *
 * ax: x
 * cx: y
 * dx: return remember to & with 0xFFFF if you don't want water affecting results
 *  rct2: 0x00662783
 */
int16_t tile_element_height(int32_t x, int32_t y)
{
    TileElement* tileElement;

    // Off the map
    if ((unsigned)x >= 8192 || (unsigned)y >= 8192)
        return 16;

    // Truncate subtile coordinates
    int32_t x_tile = x & 0xFFFFFFE0;
    int32_t y_tile = y & 0xFFFFFFE0;

    // Get the surface element for the tile
    tileElement = map_get_surface_element_at({ x_tile, y_tile });

    if (tileElement == nullptr)
    {
        return 16;
    }

    uint16_t height = (tileElement->base_height << 3);

    uint32_t slope = tileElement->AsSurface()->GetSlope();
    uint8_t extra_height = (slope & TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT) >> 4; // 0x10 is the 5th bit - sets slope to double height
    // Remove the extra height bit
    slope &= TILE_ELEMENT_SLOPE_ALL_CORNERS_UP;

    int8_t quad = 0, quad_extra = 0; // which quadrant the element is in?
                                     // quad_extra is for extra height tiles

    uint8_t xl, yl; // coordinates across this tile

    uint8_t TILE_SIZE = 31;

    xl = x & 0x1f;
    yl = y & 0x1f;

    // Slope logic:
    // Each of the four bits in slope represents that corner being raised
    // slope == 15 (all four bits) is not used and slope == 0 is flat
    // If the extra_height bit is set, then the slope goes up two z-levels

    // We arbitrarily take the SW corner to be closest to the viewer

    // One corner up
    if (slope == TILE_ELEMENT_SLOPE_N_CORNER_UP || slope == TILE_ELEMENT_SLOPE_E_CORNER_UP
        || slope == TILE_ELEMENT_SLOPE_S_CORNER_UP || slope == TILE_ELEMENT_SLOPE_W_CORNER_UP)
    {
        switch (slope)
        {
            case TILE_ELEMENT_SLOPE_N_CORNER_UP:
                quad = xl + yl - TILE_SIZE;
                break;
            case TILE_ELEMENT_SLOPE_E_CORNER_UP:
                quad = xl - yl;
                break;
            case TILE_ELEMENT_SLOPE_S_CORNER_UP:
                quad = TILE_SIZE - yl - xl;
                break;
            case TILE_ELEMENT_SLOPE_W_CORNER_UP:
                quad = yl - xl;
                break;
        }
        // If the element is in the quadrant with the slope, raise its height
        if (quad > 0)
        {
            height += quad / 2;
        }
    }

    // One side up
    switch (slope)
    {
        case TILE_ELEMENT_SLOPE_NE_SIDE_UP:
            height += xl / 2 + 1;
            break;
        case TILE_ELEMENT_SLOPE_SE_SIDE_UP:
            height += (TILE_SIZE - yl) / 2;
            break;
        case TILE_ELEMENT_SLOPE_NW_SIDE_UP:
            height += yl / 2;
            height++;
            break;
        case TILE_ELEMENT_SLOPE_SW_SIDE_UP:
            height += (TILE_SIZE - xl) / 2;
            break;
    }

    // One corner down
    if ((slope == TILE_ELEMENT_SLOPE_W_CORNER_DN) || (slope == TILE_ELEMENT_SLOPE_S_CORNER_DN)
        || (slope == TILE_ELEMENT_SLOPE_E_CORNER_DN) || (slope == TILE_ELEMENT_SLOPE_N_CORNER_DN))
    {
        switch (slope)
        {
            case TILE_ELEMENT_SLOPE_W_CORNER_DN:
                quad_extra = xl + TILE_SIZE - yl;
                quad = xl - yl;
                break;
            case TILE_ELEMENT_SLOPE_S_CORNER_DN:
                quad_extra = xl + yl;
                quad = xl + yl - TILE_SIZE - 1;
                break;
            case TILE_ELEMENT_SLOPE_E_CORNER_DN:
                quad_extra = TILE_SIZE - xl + yl;
                quad = yl - xl;
                break;
            case TILE_ELEMENT_SLOPE_N_CORNER_DN:
                quad_extra = (TILE_SIZE - xl) + (TILE_SIZE - yl);
                quad = TILE_SIZE - yl - xl - 1;
                break;
        }

        if (extra_height)
        {
            height += quad_extra / 2;
            height++;
            return height;
        }
        // This tile is essentially at the next height level
        height += 0x10;
        // so we move *down* the slope
        if (quad < 0)
        {
            height += quad / 2;
        }
    }

    // Valleys
    if ((slope == TILE_ELEMENT_SLOPE_W_E_VALLEY) || (slope == TILE_ELEMENT_SLOPE_N_S_VALLEY))
    {
        switch (slope)
        {
            case TILE_ELEMENT_SLOPE_W_E_VALLEY:
                if (xl + yl <= TILE_SIZE + 1)
                {
                    return height;
                }
                quad = TILE_SIZE - xl - yl;
                break;
            case TILE_ELEMENT_SLOPE_N_S_VALLEY:
                quad = xl - yl;
                break;
        }
        if (quad > 0)
        {
            height += quad / 2;
        }
    }

    return height;
}

int16_t tile_element_water_height(int32_t x, int32_t y)
{
    TileElement* tileElement;

    // Off the map
    if ((unsigned)x >= 8192 || (unsigned)y >= 8192)
        return 0;

    // Truncate subtile coordinates
    int32_t x_tile = x & 0xFFFFFFE0;
    int32_t y_tile = y & 0xFFFFFFE0;

    // Get the surface element for the tile
    tileElement = map_get_surface_element_at({ x_tile, y_tile });

    if (tileElement == nullptr)
    {
        return 0;
    }

    uint16_t height = (tileElement->AsSurface()->GetWaterHeight() << 4);

    return height;
}

/**
 * Checks if the tile at coordinate at height counts as connected.
 * @return 1 if connected, 0 otherwise
 */
bool map_coord_is_connected(int32_t x, int32_t y, int32_t z, uint8_t faceDirection)
{
    TileElement* tileElement = map_get_first_element_at(x, y);

    do
    {
        if (tileElement->GetType() != TILE_ELEMENT_TYPE_PATH)
            continue;

        uint8_t slopeDirection = tileElement->AsPath()->GetSlopeDirection();

        if (tileElement->AsPath()->IsSloped())
        {
            if (slopeDirection == faceDirection)
            {
                if (z == tileElement->base_height + 2)
                    return true;
            }
            else if (direction_reverse(slopeDirection) == faceDirection && z == tileElement->base_height)
            {
                return true;
            }
        }
        else
        {
            if (z == tileElement->base_height)
                return true;
        }
    } while (!(tileElement++)->IsLastForTile());

    return false;
}

/**
 *
 *  rct2: 0x006A876D
 */
void map_update_path_wide_flags()
{
    if (gScreenFlags & (SCREEN_FLAGS_TRACK_DESIGNER | SCREEN_FLAGS_TRACK_MANAGER))
    {
        return;
    }

    // Presumably update_path_wide_flags is too computationally expensive to call for every
    // tile every update, so gWidePathTileLoopX and gWidePathTileLoopY store the x and y
    // progress. A maximum of 128 calls is done per update.
    uint16_t x = gWidePathTileLoopX;
    uint16_t y = gWidePathTileLoopY;
    for (int32_t i = 0; i < 128; i++)
    {
        footpath_update_path_wide_flags(x, y);

        // Next x, y tile
        x += 32;
        if (x >= 8192)
        {
            x = 0;
            y += 32;
            if (y >= 8192)
            {
                y = 0;
            }
        }
    }
    gWidePathTileLoopX = x;
    gWidePathTileLoopY = y;
}

/**
 *
 *  rct2: 0x006A7B84
 */
int32_t map_height_from_slope(const CoordsXY coords, int32_t slope, bool isSloped)
{
    if (!isSloped)
        return 0;

    switch (slope & FOOTPATH_PROPERTIES_SLOPE_DIRECTION_MASK)
    {
        case TILE_ELEMENT_DIRECTION_WEST:
            return (31 - (coords.x & 31)) / 2;
        case TILE_ELEMENT_DIRECTION_NORTH:
            return (coords.y & 31) / 2;
        case TILE_ELEMENT_DIRECTION_EAST:
            return (coords.x & 31) / 2;
        case TILE_ELEMENT_DIRECTION_SOUTH:
            return (31 - (coords.y & 31)) / 2;
    }
    return 0;
}

bool map_is_location_valid(const CoordsXY coords)
{
    const bool is_x_valid = coords.x < (MAXIMUM_MAP_SIZE_TECHNICAL * 32) && coords.x >= 0;
    const bool is_y_valid = coords.y < (MAXIMUM_MAP_SIZE_TECHNICAL * 32) && coords.y >= 0;
    return is_x_valid && is_y_valid;
}

bool map_is_edge(const CoordsXY coords)
{
    return (coords.x < 32 || coords.y < 32 || coords.x >= gMapSizeUnits || coords.y >= gMapSizeUnits);
}

bool map_can_build_at(int32_t x, int32_t y, int32_t z)
{
    if (gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR)
        return true;
    if (gCheatsSandboxMode)
        return true;
    if (map_is_location_owned(x, y, z))
        return true;
    return false;
}

/**
 *
 *  rct2: 0x00664F72
 */
bool map_is_location_owned(int32_t x, int32_t y, int32_t z)
{
    // This check is to avoid throwing lots of messages in logs.
    if (map_is_location_valid({ x, y }))
    {
        TileElement* tileElement = map_get_surface_element_at({ x, y });
        if (tileElement != nullptr)
        {
            if (tileElement->AsSurface()->GetOwnership() & OWNERSHIP_OWNED)
                return true;

            if (tileElement->AsSurface()->GetOwnership() & OWNERSHIP_CONSTRUCTION_RIGHTS_OWNED)
            {
                z /= 8;
                if (z < tileElement->base_height || z - 2 > tileElement->base_height)
                    return true;
            }
        }
    }

    gGameCommandErrorText = STR_LAND_NOT_OWNED_BY_PARK;
    return false;
}

/**
 *
 *  rct2: 0x00664F2C
 */
bool map_is_location_in_park(const CoordsXY coords)
{
    if (map_is_location_valid(coords))
    {
        TileElement* tileElement = map_get_surface_element_at(coords);
        if (tileElement == nullptr)
            return false;
        if (tileElement->AsSurface()->GetOwnership() & OWNERSHIP_OWNED)
            return true;
    }

    gGameCommandErrorText = STR_LAND_NOT_OWNED_BY_PARK;
    return false;
}

bool map_is_location_owned_or_has_rights(int32_t x, int32_t y)
{
    if (map_is_location_valid({ x, y }))
    {
        TileElement* tileElement = map_get_surface_element_at({ x, y });
        if (tileElement == nullptr)
        {
            return false;
        }
        if (tileElement->AsSurface()->GetOwnership() & OWNERSHIP_OWNED)
            return true;
        if (tileElement->AsSurface()->GetOwnership() & OWNERSHIP_CONSTRUCTION_RIGHTS_OWNED)
            return true;
    }
    return false;
}

// 0x00981A1E
// Table of pre-calculated surface slopes (32) when raising the land tile for a given selection (5)
// 0x1F = new slope
// 0x20 = base height increases
const uint8_t tile_element_raise_styles[9][32] = {
    { 0x01, 0x1B, 0x03, 0x1B, 0x05, 0x21, 0x07, 0x21, 0x09, 0x1B, 0x0B, 0x1B, 0x0D, 0x21, 0x20, 0x0F,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x23, 0x18, 0x19, 0x1A, 0x3B, 0x1C, 0x29, 0x24, 0x1F }, // MAP_SELECT_TYPE_CORNER_0
                                                                                                        // (absolute rotation)
    { 0x02, 0x03, 0x17, 0x17, 0x06, 0x07, 0x17, 0x17, 0x0A, 0x0B, 0x22, 0x22, 0x0E, 0x20, 0x22, 0x0F,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x37, 0x18, 0x19, 0x1A, 0x23, 0x1C, 0x28, 0x26, 0x1F }, // MAP_SELECT_TYPE_CORNER_1
    { 0x04, 0x05, 0x06, 0x07, 0x1E, 0x24, 0x1E, 0x24, 0x0C, 0x0D, 0x0E, 0x20, 0x1E, 0x24, 0x1E, 0x0F,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x26, 0x18, 0x19, 0x1A, 0x21, 0x1C, 0x2C, 0x3E, 0x1F }, // MAP_SELECT_TYPE_CORNER_2
    { 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x20, 0x1D, 0x1D, 0x28, 0x28, 0x1D, 0x1D, 0x28, 0x0F,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x22, 0x18, 0x19, 0x1A, 0x29, 0x1C, 0x3D, 0x2C, 0x1F }, // MAP_SELECT_TYPE_CORNER_3
    { 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x22, 0x20, 0x20, 0x20, 0x21, 0x20, 0x28, 0x24, 0x20 }, // MAP_SELECT_TYPE_FULL
    { 0x0C, 0x0D, 0x0E, 0x20, 0x0C, 0x0D, 0x0E, 0x20, 0x0C, 0x0D, 0x0E, 0x20, 0x2C, 0x2C, 0x2C, 0x2C,
      0x0C, 0x0D, 0x0E, 0x20, 0x0C, 0x0C, 0x0E, 0x22, 0x0C, 0x0D, 0x0E, 0x21, 0x2C, 0x2C, 0x2C, 0x2C }, // MAP_SELECT_TYPE_EDGE_0
    { 0x09, 0x09, 0x0B, 0x0B, 0x0D, 0x0D, 0x20, 0x20, 0x09, 0x29, 0x0B, 0x29, 0x0D, 0x29, 0x20, 0x29,
      0x09, 0x09, 0x0B, 0x0B, 0x0D, 0x0D, 0x24, 0x22, 0x09, 0x29, 0x0B, 0x29, 0x0D, 0x29, 0x24, 0x29 }, // MAP_SELECT_TYPE_EDGE_1
    { 0x03, 0x03, 0x03, 0x23, 0x07, 0x07, 0x07, 0x23, 0x0B, 0x0B, 0x0B, 0x23, 0x20, 0x20, 0x20, 0x23,
      0x03, 0x03, 0x03, 0x23, 0x07, 0x07, 0x07, 0x23, 0x0B, 0x0B, 0x0B, 0x23, 0x20, 0x28, 0x24, 0x23 }, // MAP_SELECT_TYPE_EDGE_2
    { 0x06, 0x07, 0x06, 0x07, 0x06, 0x07, 0x26, 0x26, 0x0E, 0x20, 0x0E, 0x20, 0x0E, 0x20, 0x26, 0x26,
      0x06, 0x07, 0x06, 0x07, 0x06, 0x07, 0x26, 0x26, 0x0E, 0x20, 0x0E, 0x21, 0x0E, 0x28, 0x26, 0x26 }, // MAP_SELECT_TYPE_EDGE_3
};

// 0x00981ABE
// Basically the inverse of the table above.
// 0x1F = new slope
// 0x20 = base height increases
const uint8_t tile_element_lower_styles[9][32] = {
    { 0x2E, 0x00, 0x2E, 0x02, 0x3E, 0x04, 0x3E, 0x06, 0x2E, 0x08, 0x2E, 0x0A, 0x3E, 0x0C, 0x3E, 0x0F,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x06, 0x18, 0x19, 0x1A, 0x0B, 0x1C, 0x0C, 0x3E, 0x1F }, // MAP_SELECT_TYPE_CORNER_0
    { 0x2D, 0x2D, 0x00, 0x01, 0x2D, 0x2D, 0x04, 0x05, 0x3D, 0x3D, 0x08, 0x09, 0x3D, 0x3D, 0x0C, 0x0F,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x07, 0x18, 0x19, 0x1A, 0x09, 0x1C, 0x3D, 0x0C, 0x1F }, // MAP_SELECT_TYPE_CORNER_1
    { 0x2B, 0x3B, 0x2B, 0x3B, 0x00, 0x01, 0x02, 0x03, 0x2B, 0x3B, 0x2B, 0x3B, 0x08, 0x09, 0x0A, 0x0F,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x03, 0x18, 0x19, 0x1A, 0x3B, 0x1C, 0x09, 0x0E, 0x1F }, // MAP_SELECT_TYPE_CORNER_2
    { 0x27, 0x27, 0x37, 0x37, 0x27, 0x27, 0x37, 0x37, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0F,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x37, 0x18, 0x19, 0x1A, 0x03, 0x1C, 0x0D, 0x06, 0x1F }, // MAP_SELECT_TYPE_CORNER_3
    { 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x0D, 0x0E, 0x00 }, // MAP_SELECT_TYPE_FULL
    { 0x23, 0x23, 0x23, 0x23, 0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,
      0x23, 0x23, 0x23, 0x23, 0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03, 0x00, 0x0D, 0x0E, 0x03 }, // MAP_SELECT_TYPE_EDGE_0
    { 0x26, 0x00, 0x26, 0x02, 0x26, 0x04, 0x26, 0x06, 0x00, 0x00, 0x02, 0x02, 0x04, 0x04, 0x06, 0x06,
      0x26, 0x00, 0x26, 0x02, 0x26, 0x04, 0x26, 0x06, 0x00, 0x00, 0x02, 0x0B, 0x04, 0x0D, 0x06, 0x06 }, // MAP_SELECT_TYPE_EDGE_1
    { 0x2C, 0x00, 0x00, 0x00, 0x2C, 0x04, 0x04, 0x04, 0x2C, 0x08, 0x08, 0x08, 0x2C, 0x0C, 0x0C, 0x0C,
      0x2C, 0x00, 0x00, 0x00, 0x2C, 0x04, 0x04, 0x07, 0x2C, 0x08, 0x08, 0x0B, 0x2C, 0x0C, 0x0C, 0x0C }, // MAP_SELECT_TYPE_EDGE_2
    { 0x29, 0x29, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x29, 0x29, 0x08, 0x09, 0x08, 0x09, 0x08, 0x09,
      0x29, 0x29, 0x00, 0x01, 0x00, 0x01, 0x00, 0x07, 0x29, 0x29, 0x08, 0x09, 0x08, 0x09, 0x0E, 0x09 }, // MAP_SELECT_TYPE_EDGE_3
};

int32_t map_get_corner_height(int32_t z, int32_t slope, int32_t direction)
{
    switch (direction)
    {
        case 0:
            if (slope & TILE_ELEMENT_SLOPE_N_CORNER_UP)
            {
                z += 2;
                if (slope == (TILE_ELEMENT_SLOPE_S_CORNER_DN | TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT))
                {
                    z += 2;
                }
            }
            break;
        case 1:
            if (slope & TILE_ELEMENT_SLOPE_E_CORNER_UP)
            {
                z += 2;
                if (slope == (TILE_ELEMENT_SLOPE_W_CORNER_DN | TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT))
                {
                    z += 2;
                }
            }
            break;
        case 2:
            if (slope & TILE_ELEMENT_SLOPE_S_CORNER_UP)
            {
                z += 2;
                if (slope == (TILE_ELEMENT_SLOPE_N_CORNER_DN | TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT))
                {
                    z += 2;
                }
            }
            break;
        case 3:
            if (slope & TILE_ELEMENT_SLOPE_W_CORNER_UP)
            {
                z += 2;
                if (slope == (TILE_ELEMENT_SLOPE_E_CORNER_DN | TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT))
                {
                    z += 2;
                }
            }
            break;
    }
    return z;
}

int32_t tile_element_get_corner_height(const TileElement* tileElement, int32_t direction)
{
    int32_t z = tileElement->base_height;
    int32_t slope = tileElement->AsSurface()->GetSlope();
    return map_get_corner_height(z, slope, direction);
}

uint8_t map_get_lowest_land_height(int32_t xMin, int32_t xMax, int32_t yMin, int32_t yMax)
{
    xMin = std::max(xMin, 32);
    yMin = std::max(yMin, 32);
    xMax = std::min(xMax, (int32_t)gMapSizeMaxXY);
    yMax = std::min(yMax, (int32_t)gMapSizeMaxXY);

    uint8_t min_height = 0xFF;
    for (int32_t yi = yMin; yi <= yMax; yi += 32)
    {
        for (int32_t xi = xMin; xi <= xMax; xi += 32)
        {
            TileElement* tile_element = map_get_surface_element_at({ xi, yi });
            if (tile_element != nullptr && min_height > tile_element->base_height)
            {
                min_height = tile_element->base_height;
            }
        }
    }
    return min_height;
}

uint8_t map_get_highest_land_height(int32_t xMin, int32_t xMax, int32_t yMin, int32_t yMax)
{
    xMin = std::max(xMin, 32);
    yMin = std::max(yMin, 32);
    xMax = std::min(xMax, (int32_t)gMapSizeMaxXY);
    yMax = std::min(yMax, (int32_t)gMapSizeMaxXY);

    uint8_t max_height = 0;
    for (int32_t yi = yMin; yi <= yMax; yi += 32)
    {
        for (int32_t xi = xMin; xi <= xMax; xi += 32)
        {
            TileElement* tile_element = map_get_surface_element_at({ xi, yi });
            if (tile_element != nullptr)
            {
                uint8_t base_height = tile_element->base_height;
                if (tile_element->AsSurface()->GetSlope() & TILE_ELEMENT_SLOPE_ALL_CORNERS_UP)
                    base_height += 2;
                if (tile_element->AsSurface()->GetSlope() & TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT)
                    base_height += 2;
                if (max_height < base_height)
                    max_height = base_height;
            }
        }
    }
    return max_height;
}

bool map_is_location_at_edge(int32_t x, int32_t y)
{
    return x < 32 || y < 32 || x >= ((MAXIMUM_MAP_SIZE_TECHNICAL - 1) * 32) || y >= ((MAXIMUM_MAP_SIZE_TECHNICAL - 1) * 32);
}

/**
 *
 *  rct2: 0x0068B280
 */
void tile_element_remove(TileElement* tileElement)
{
    // Replace Nth element by (N+1)th element.
    // This loop will make tileElement point to the old last element position,
    // after copy it to it's new position
    if (!tileElement->IsLastForTile())
    {
        do
        {
            *tileElement = *(tileElement + 1);
        } while (!(++tileElement)->IsLastForTile());
    }

    // Mark the latest element with the last element flag.
    (tileElement - 1)->flags |= TILE_ELEMENT_FLAG_LAST_TILE;
    tileElement->base_height = 0xFF;

    if ((tileElement + 1) == gNextFreeTileElement)
    {
        gNextFreeTileElement--;
    }
}

/**
 *
 *  rct2: 0x00675A8E
 */
void map_remove_all_rides()
{
    tile_element_iterator it;

    tile_element_iterator_begin(&it);
    do
    {
        switch (it.element->GetType())
        {
            case TILE_ELEMENT_TYPE_PATH:
                if (it.element->AsPath()->IsQueue())
                {
                    it.element->AsPath()->SetHasQueueBanner(false);
                    it.element->AsPath()->SetRideIndex(RIDE_ID_NULL);
                }
                break;
            case TILE_ELEMENT_TYPE_ENTRANCE:
                if (it.element->AsEntrance()->GetEntranceType() == ENTRANCE_TYPE_PARK_ENTRANCE)
                    break;
                [[fallthrough]];
            case TILE_ELEMENT_TYPE_TRACK:
                footpath_queue_chain_reset();
                footpath_remove_edges_at(it.x * 32, it.y * 32, it.element);
                tile_element_remove(it.element);
                tile_element_iterator_restart_for_tile(&it);
                break;
        }
    } while (tile_element_iterator_next(&it));
}

/**
 *
 *  rct2: 0x0068AB1B
 */
void map_invalidate_map_selection_tiles()
{
    if (!(gMapSelectFlags & MAP_SELECT_FLAG_ENABLE_CONSTRUCT))
        return;

    for (const auto& position : gMapSelectionTiles)
        map_invalidate_tile_full(position.x, position.y);
}

void map_get_bounding_box(
    int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t* left, int32_t* top, int32_t* right, int32_t* bottom)
{
    int32_t x, y;
    x = ax;
    y = ay;
    uint32_t rotation = get_current_rotation();
    translate_3d_to_2d(rotation, &x, &y);
    *left = x;
    *right = x;
    *top = y;
    *bottom = y;
    x = bx;
    y = ay;
    translate_3d_to_2d(rotation, &x, &y);
    if (x < *left)
        *left = x;
    if (x > *right)
        *right = x;
    if (y > *bottom)
        *bottom = y;
    if (y < *top)
        *top = y;
    x = bx;
    y = by;
    translate_3d_to_2d(rotation, &x, &y);
    if (x < *left)
        *left = x;
    if (x > *right)
        *right = x;
    if (y > *bottom)
        *bottom = y;
    if (y < *top)
        *top = y;
    x = ax;
    y = by;
    translate_3d_to_2d(rotation, &x, &y);
    if (x < *left)
        *left = x;
    if (x > *right)
        *right = x;
    if (y > *bottom)
        *bottom = y;
    if (y < *top)
        *top = y;
}

/**
 *
 *  rct2: 0x0068AAE1
 */
void map_invalidate_selection_rect()
{
    int32_t x0, y0, x1, y1, left, right, top, bottom;

    if (!(gMapSelectFlags & MAP_SELECT_FLAG_ENABLE))
        return;

    x0 = gMapSelectPositionA.x + 16;
    y0 = gMapSelectPositionA.y + 16;
    x1 = gMapSelectPositionB.x + 16;
    y1 = gMapSelectPositionB.y + 16;
    map_get_bounding_box(x0, y0, x1, y1, &left, &top, &right, &bottom);
    left -= 32;
    right += 32;
    bottom += 32;
    top -= 32 + 2080;

    for (int32_t i = 0; i < MAX_VIEWPORT_COUNT; i++)
    {
        rct_viewport* viewport = &g_viewport_list[i];
        if (viewport->width != 0)
        {
            viewport_invalidate(viewport, left, top, right, bottom);
        }
    }
}

/**
 *
 *  rct2: 0x0068B111
 */
void map_reorganise_elements()
{
    context_setcurrentcursor(CURSOR_ZZZ);

    TileElement* new_tile_elements = (TileElement*)malloc(
        3 * (MAXIMUM_MAP_SIZE_TECHNICAL * MAXIMUM_MAP_SIZE_TECHNICAL) * sizeof(TileElement));
    TileElement* new_elements_pointer = new_tile_elements;

    if (new_tile_elements == nullptr)
    {
        log_fatal("Unable to allocate memory for map elements.");
        return;
    }

    uint32_t num_elements;

    for (int32_t y = 0; y < MAXIMUM_MAP_SIZE_TECHNICAL; y++)
    {
        for (int32_t x = 0; x < MAXIMUM_MAP_SIZE_TECHNICAL; x++)
        {
            TileElement* startElement = map_get_first_element_at(x, y);
            TileElement* endElement = startElement;
            while (!(endElement++)->IsLastForTile())
                ;

            num_elements = (uint32_t)(endElement - startElement);
            std::memcpy(new_elements_pointer, startElement, num_elements * sizeof(TileElement));
            new_elements_pointer += num_elements;
        }
    }

    num_elements = (uint32_t)(new_elements_pointer - new_tile_elements);
    std::memcpy(gTileElements, new_tile_elements, num_elements * sizeof(TileElement));
    std::memset(
        gTileElements + num_elements, 0,
        (3 * (MAXIMUM_MAP_SIZE_TECHNICAL * MAXIMUM_MAP_SIZE_TECHNICAL) - num_elements) * sizeof(TileElement));

    free(new_tile_elements);

    map_update_tile_pointers();
}

/**
 *
 *  rct2: 0x0068B044
 *  Returns true on space available for more elements
 *  Reorganises the map elements to check for space
 */
bool map_check_free_elements_and_reorganise(int32_t numElements)
{
    if (numElements != 0)
    {
        auto tileElementEnd = &gTileElements[MAX_TILE_ELEMENTS];

        // Check if is there is room for the required number of elements
        auto newTileElementEnd = gNextFreeTileElement + numElements;
        if (newTileElementEnd > tileElementEnd)
        {
            // Defragment the map element list
            map_reorganise_elements();

            // Check if there is any room again
            newTileElementEnd = gNextFreeTileElement + numElements;
            if (newTileElementEnd > tileElementEnd)
            {
                // Not enough spare elements left :'(
                gGameCommandErrorText = STR_ERR_LANDSCAPE_DATA_AREA_FULL;
                return false;
            }
        }
    }
    return true;
}

/**
 *
 *  rct2: 0x0068B1F6
 */
TileElement* tile_element_insert(int32_t x, int32_t y, int32_t z, int32_t flags)
{
    TileElement *originalTileElement, *newTileElement, *insertedElement;

    if (!map_check_free_elements_and_reorganise(1))
    {
        log_error("Cannot insert new element");
        return nullptr;
    }

    newTileElement = gNextFreeTileElement;
    originalTileElement = gTileElementTilePointers[y * MAXIMUM_MAP_SIZE_TECHNICAL + x];

    // Set tile index pointer to point to new element block
    gTileElementTilePointers[y * MAXIMUM_MAP_SIZE_TECHNICAL + x] = newTileElement;

    // Copy all elements that are below the insert height
    while (z >= originalTileElement->base_height)
    {
        // Copy over map element
        *newTileElement = *originalTileElement;
        originalTileElement->base_height = 255;
        originalTileElement++;
        newTileElement++;

        if ((newTileElement - 1)->flags & TILE_ELEMENT_FLAG_LAST_TILE)
        {
            // No more elements above the insert element
            (newTileElement - 1)->flags &= ~TILE_ELEMENT_FLAG_LAST_TILE;
            flags |= TILE_ELEMENT_FLAG_LAST_TILE;
            break;
        }
    }

    // Insert new map element
    insertedElement = newTileElement;
    newTileElement->type = 0;
    newTileElement->base_height = z;
    newTileElement->flags = flags;
    newTileElement->clearance_height = z;
    std::memset(&newTileElement->pad_04, 0, sizeof(newTileElement->pad_04));
    newTileElement++;

    // Insert rest of map elements above insert height
    if (!(flags & TILE_ELEMENT_FLAG_LAST_TILE))
    {
        do
        {
            // Copy over map element
            *newTileElement = *originalTileElement;
            originalTileElement->base_height = 255;
            originalTileElement++;
            newTileElement++;
        } while (!((newTileElement - 1)->IsLastForTile()));
    }

    gNextFreeTileElement = newTileElement;
    return insertedElement;
}

/**
 *
 *  rct2: 0x0068BB18
 */
void map_obstruction_set_error_text(TileElement* tileElement)
{
    rct_string_id errorStringId;
    Ride* ride;
    rct_scenery_entry* sceneryEntry;

    errorStringId = STR_OBJECT_IN_THE_WAY;
    switch (tileElement->GetType())
    {
        case TILE_ELEMENT_TYPE_SURFACE:
            errorStringId = STR_RAISE_OR_LOWER_LAND_FIRST;
            break;
        case TILE_ELEMENT_TYPE_PATH:
            errorStringId = STR_FOOTPATH_IN_THE_WAY;
            break;
        case TILE_ELEMENT_TYPE_TRACK:
            ride = get_ride(tileElement->AsTrack()->GetRideIndex());
            errorStringId = STR_X_IN_THE_WAY;
            set_format_arg(0, rct_string_id, ride->name);
            set_format_arg(2, uint32_t, ride->name_arguments);
            break;
        case TILE_ELEMENT_TYPE_SMALL_SCENERY:
            sceneryEntry = tileElement->AsSmallScenery()->GetEntry();
            errorStringId = STR_X_IN_THE_WAY;
            set_format_arg(0, rct_string_id, sceneryEntry->name);
            break;
        case TILE_ELEMENT_TYPE_ENTRANCE:
            switch (tileElement->AsEntrance()->GetEntranceType())
            {
                case ENTRANCE_TYPE_RIDE_ENTRANCE:
                    errorStringId = STR_RIDE_ENTRANCE_IN_THE_WAY;
                    break;
                case ENTRANCE_TYPE_RIDE_EXIT:
                    errorStringId = STR_RIDE_EXIT_IN_THE_WAY;
                    break;
                case ENTRANCE_TYPE_PARK_ENTRANCE:
                    errorStringId = STR_PARK_ENTRANCE_IN_THE_WAY;
                    break;
            }
            break;
        case TILE_ELEMENT_TYPE_WALL:
            sceneryEntry = tileElement->AsWall()->GetEntry();
            errorStringId = STR_X_IN_THE_WAY;
            set_format_arg(0, rct_string_id, sceneryEntry->name);
            break;
        case TILE_ELEMENT_TYPE_LARGE_SCENERY:
            sceneryEntry = tileElement->AsLargeScenery()->GetEntry();
            errorStringId = STR_X_IN_THE_WAY;
            set_format_arg(0, rct_string_id, sceneryEntry->name);
            break;
    }

    gGameCommandErrorText = errorStringId;
}

/**
 *
 *  rct2: 0x0068B932
 *  ax = x
 *  cx = y
 *  dl = zLow
 *  dh = zHigh
 *  ebp = clearFunc
 *  bl = bl
 */
bool map_can_construct_with_clear_at(
    int32_t x, int32_t y, int32_t zLow, int32_t zHigh, CLEAR_FUNC clearFunc, QuarterTile bl, uint8_t flags, money32* price,
    uint8_t crossingMode)
{
    int32_t al, ah, bh, cl, ch, water_height;
    al = ah = bh = cl = ch = water_height = 0;
    uint8_t slope = 0;

    gMapGroundFlags = ELEMENT_IS_ABOVE_GROUND;
    bool canBuildCrossing = false;
    if (x >= gMapSizeUnits || y >= gMapSizeUnits || x < 32 || y < 32)
    {
        gGameCommandErrorText = STR_OFF_EDGE_OF_MAP;
        return false;
    }

    if (gCheatsDisableClearanceChecks)
    {
        return true;
    }

    TileElement* tileElement = map_get_first_element_at(x / 32, y / 32);
    do
    {
        if (tileElement->GetType() != TILE_ELEMENT_TYPE_SURFACE)
        {
            if (zLow < tileElement->clearance_height && zHigh > tileElement->base_height && !(tileElement->IsGhost()))
            {
                if (tileElement->flags & (bl.GetBaseQuarterOccupied()))
                {
                    goto loc_68BABC;
                }
            }
            continue;
        }
        water_height = tileElement->AsSurface()->GetWaterHeight() * 2;
        if (water_height && water_height > zLow && tileElement->base_height < zHigh)
        {
            gMapGroundFlags |= ELEMENT_IS_UNDERWATER;
            if (water_height < zHigh)
            {
                goto loc_68BAE6;
            }
        }
    loc_68B9B7:
        if (gParkFlags & PARK_FLAGS_FORBID_HIGH_CONSTRUCTION)
        {
            al = zHigh - tileElement->base_height;
            if (al >= 0)
            {
                if (al > 18)
                {
                    gGameCommandErrorText = STR_LOCAL_AUTHORITY_WONT_ALLOW_CONSTRUCTION_ABOVE_TREE_HEIGHT;
                    return false;
                }
            }
        }

        // Only allow building crossings directly on a flat surface tile.
        if (tileElement->GetType() == TILE_ELEMENT_TYPE_SURFACE
            && (tileElement->AsSurface()->GetSlope()) == TILE_ELEMENT_SLOPE_FLAT && tileElement->base_height == zLow)
        {
            canBuildCrossing = true;
        }

        if (bl.GetZQuarterOccupied() != 0b1111)
        {
            if (tileElement->base_height >= zHigh)
            {
                // loc_68BA81
                gMapGroundFlags |= ELEMENT_IS_UNDERGROUND;
                gMapGroundFlags &= ~ELEMENT_IS_ABOVE_GROUND;
            }
            else
            {
                al = tileElement->base_height;
                ah = al;
                cl = al;
                ch = al;
                slope = tileElement->AsSurface()->GetSlope();
                if (slope & TILE_ELEMENT_SLOPE_N_CORNER_UP)
                {
                    al += 2;
                    if (slope == (TILE_ELEMENT_SLOPE_S_CORNER_DN | TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT))
                        al += 2;
                }
                if (slope & TILE_ELEMENT_SLOPE_E_CORNER_UP)
                {
                    ah += 2;
                    if (slope == (TILE_ELEMENT_SLOPE_W_CORNER_DN | TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT))
                        ah += 2;
                }
                if (slope & TILE_ELEMENT_SLOPE_S_CORNER_UP)
                {
                    cl += 2;
                    if (slope == (TILE_ELEMENT_SLOPE_N_CORNER_DN | TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT))
                        cl += 2;
                }
                if (slope & TILE_ELEMENT_SLOPE_W_CORNER_UP)
                {
                    ch += 2;
                    if (slope == (TILE_ELEMENT_SLOPE_E_CORNER_DN | TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT))
                        ch += 2;
                }
                bh = zLow + 4;
                {
                    auto baseQuarter = bl.GetBaseQuarterOccupied();
                    auto zQuarter = bl.GetZQuarterOccupied();
                    if ((!(baseQuarter & 0b0001) || ((zQuarter & 0b0001 || zLow >= al) && bh >= al))
                        && (!(baseQuarter & 0b0010) || ((zQuarter & 0b0010 || zLow >= ah) && bh >= ah))
                        && (!(baseQuarter & 0b0100) || ((zQuarter & 0b0100 || zLow >= cl) && bh >= cl))
                        && (!(baseQuarter & 0b1000) || ((zQuarter & 0b1000 || zLow >= ch) && bh >= ch)))
                    {
                        continue;
                    }
                }
            loc_68BABC:
                if (clearFunc != nullptr)
                {
                    if (!clearFunc(&tileElement, x, y, flags, price))
                    {
                        continue;
                    }
                }

                // Crossing mode 1: building track over path
                if (crossingMode == 1 && canBuildCrossing && tileElement->GetType() == TILE_ELEMENT_TYPE_PATH
                    && tileElement->base_height == zLow && !tileElement->AsPath()->IsQueue()
                    && !tileElement->AsPath()->IsSloped())
                {
                    continue;
                }
                // Crossing mode 2: building path over track
                else if (
                    crossingMode == 2 && canBuildCrossing && tileElement->GetType() == TILE_ELEMENT_TYPE_TRACK
                    && tileElement->base_height == zLow && tileElement->AsTrack()->GetTrackType() == TRACK_ELEM_FLAT)
                {
                    Ride* ride = get_ride(tileElement->AsTrack()->GetRideIndex());
                    if (ride->type == RIDE_TYPE_MINIATURE_RAILWAY)
                    {
                        continue;
                    }
                }

                if (tileElement != nullptr)
                {
                    map_obstruction_set_error_text(tileElement);
                }
                return false;

            loc_68BAE6:
                if (clearFunc != nullptr)
                {
                    if (!clearFunc(&tileElement, x, y, flags, price))
                    {
                        goto loc_68B9B7;
                    }
                }
                if (tileElement != nullptr)
                {
                    gGameCommandErrorText = STR_CANNOT_BUILD_PARTLY_ABOVE_AND_PARTLY_BELOW_WATER;
                }
                return false;
            }
        }
    } while (!(tileElement++)->IsLastForTile());
    return true;
}

/**
 *
 *  rct2: 0x0068B93A
 */
int32_t map_can_construct_at(int32_t x, int32_t y, int32_t zLow, int32_t zHigh, QuarterTile bl)
{
    return map_can_construct_with_clear_at(x, y, zLow, zHigh, nullptr, bl, 0, nullptr, CREATE_CROSSING_MODE_NONE);
}

/**
 * Updates grass length, scenery age and jumping fountains.
 *
 *  rct2: 0x006646E1
 */
void map_update_tiles()
{
    int32_t ignoreScreenFlags = SCREEN_FLAGS_SCENARIO_EDITOR | SCREEN_FLAGS_TRACK_DESIGNER | SCREEN_FLAGS_TRACK_MANAGER;
    if (gScreenFlags & ignoreScreenFlags)
        return;

    // Update 43 more tiles
    for (int32_t j = 0; j < 43; j++)
    {
        int32_t x = 0;
        int32_t y = 0;

        uint16_t interleaved_xy = gGrassSceneryTileLoopPosition;
        for (int32_t i = 0; i < 8; i++)
        {
            x = (x << 1) | (interleaved_xy & 1);
            interleaved_xy >>= 1;
            y = (y << 1) | (interleaved_xy & 1);
            interleaved_xy >>= 1;
        }

        TileElement* tileElement = map_get_surface_element_at(x, y);
        if (tileElement != nullptr)
        {
            tileElement->AsSurface()->UpdateGrassLength({ x * 32, y * 32 });
            scenery_update_tile(x * 32, y * 32);
        }

        gGrassSceneryTileLoopPosition++;
        gGrassSceneryTileLoopPosition &= 0xFFFF;
    }
}

void map_remove_provisional_elements()
{
    if (gFootpathProvisionalFlags & PROVISIONAL_PATH_FLAG_1)
    {
        footpath_provisional_remove();
        gFootpathProvisionalFlags |= PROVISIONAL_PATH_FLAG_1;
    }
    if (window_find_by_class(WC_RIDE_CONSTRUCTION) != nullptr)
    {
        ride_remove_provisional_track_piece();
        ride_entrance_exit_remove_ghost();
    }
}

void map_restore_provisional_elements()
{
    if (gFootpathProvisionalFlags & PROVISIONAL_PATH_FLAG_1)
    {
        gFootpathProvisionalFlags &= ~PROVISIONAL_PATH_FLAG_1;
        footpath_provisional_set(
            gFootpathProvisionalType, gFootpathProvisionalPosition.x, gFootpathProvisionalPosition.y,
            gFootpathProvisionalPosition.z, gFootpathProvisionalSlope);
    }
    if (window_find_by_class(WC_RIDE_CONSTRUCTION) != nullptr)
    {
        ride_restore_provisional_track_piece();
        ride_entrance_exit_place_provisional_ghost();
    }
}

/**
 * Removes elements that are out of the map size range and crops the park perimeter.
 *  rct2: 0x0068ADBC
 */
void map_remove_out_of_range_elements()
{
    int32_t mapMaxXY = gMapSizeMaxXY;

    for (int32_t y = 0; y < (MAXIMUM_MAP_SIZE_TECHNICAL * 32); y += 32)
    {
        for (int32_t x = 0; x < (MAXIMUM_MAP_SIZE_TECHNICAL * 32); x += 32)
        {
            if (x == 0 || y == 0 || x >= mapMaxXY || y >= mapMaxXY)
            {
                // Note this purposely does not use LandSetRightsAction as X Y coordinates are outside of normal range.
                auto surfaceElement = map_get_surface_element_at({ x, y });
                if (surfaceElement != nullptr)
                {
                    surfaceElement->AsSurface()->SetOwnership(OWNERSHIP_UNOWNED);
                    update_park_fences_around_tile({ x, y });
                }
                clear_elements_at(x, y);
            }
        }
    }
}

/**
 * Copies the terrain and slope from the edge of the map to the new tiles. Used when increasing the size of the map.
 *  rct2: 0x0068AC15
 */
void map_extend_boundary_surface()
{
    SurfaceElement *existingTileElement, *newTileElement;
    int32_t x, y, z, slope;

    y = gMapSize - 2;
    for (x = 0; x < MAXIMUM_MAP_SIZE_TECHNICAL; x++)
    {
        existingTileElement = map_get_surface_element_at(x, y - 1)->AsSurface();
        newTileElement = map_get_surface_element_at(x, y)->AsSurface();
        newTileElement->SetSurfaceStyle(existingTileElement->GetSurfaceStyle());
        newTileElement->SetEdgeStyle(existingTileElement->GetEdgeStyle());
        newTileElement->SetGrassLength(existingTileElement->GetGrassLength());
        newTileElement->SetOwnership(OWNERSHIP_UNOWNED);
        newTileElement->SetWaterHeight(existingTileElement->GetWaterHeight());

        z = existingTileElement->base_height;
        slope = existingTileElement->GetSlope() & TILE_ELEMENT_SLOPE_NW_SIDE_UP;
        if (slope == TILE_ELEMENT_SLOPE_NW_SIDE_UP)
        {
            z += 2;
            slope = TILE_ELEMENT_SLOPE_FLAT;
            if (existingTileElement->GetSlope() & TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT)
            {
                slope = TILE_ELEMENT_SLOPE_N_CORNER_UP;
                if (existingTileElement->GetSlope() & TILE_ELEMENT_SLOPE_S_CORNER_UP)
                {
                    slope = TILE_ELEMENT_SLOPE_W_CORNER_UP;
                    if (existingTileElement->GetSlope() & TILE_ELEMENT_SLOPE_E_CORNER_UP)
                    {
                        slope = TILE_ELEMENT_SLOPE_FLAT;
                    }
                }
            }
        }
        if (slope & TILE_ELEMENT_SLOPE_N_CORNER_UP)
            slope |= TILE_ELEMENT_SLOPE_E_CORNER_UP;
        if (slope & TILE_ELEMENT_SLOPE_W_CORNER_UP)
            slope |= TILE_ELEMENT_SLOPE_S_CORNER_UP;

        newTileElement->SetSlope(slope);
        newTileElement->base_height = z;
        newTileElement->clearance_height = z;

        update_park_fences({ x << 5, y << 5 });
    }

    x = gMapSize - 2;
    for (y = 0; y < MAXIMUM_MAP_SIZE_TECHNICAL; y++)
    {
        existingTileElement = map_get_surface_element_at(x - 1, y)->AsSurface();
        newTileElement = map_get_surface_element_at(x, y)->AsSurface();

        newTileElement->SetSurfaceStyle(existingTileElement->GetSurfaceStyle());
        newTileElement->SetEdgeStyle(existingTileElement->GetEdgeStyle());
        newTileElement->SetGrassLength(existingTileElement->GetGrassLength());
        newTileElement->SetOwnership(OWNERSHIP_UNOWNED);
        newTileElement->SetWaterHeight(existingTileElement->GetWaterHeight());

        z = existingTileElement->base_height;
        slope = existingTileElement->GetSlope() & TILE_ELEMENT_SLOPE_NE_SIDE_UP;
        if (slope == TILE_ELEMENT_SLOPE_NE_SIDE_UP)
        {
            z += 2;
            slope = TILE_ELEMENT_SLOPE_FLAT;
            if (existingTileElement->GetSlope() & TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT)
            {
                slope = TILE_ELEMENT_SLOPE_N_CORNER_UP;
                if (existingTileElement->GetSlope() & TILE_ELEMENT_SLOPE_S_CORNER_UP)
                {
                    slope = TILE_ELEMENT_SLOPE_E_CORNER_UP;
                    if (existingTileElement->GetSlope() & TILE_ELEMENT_SLOPE_W_CORNER_UP)
                    {
                        slope = TILE_ELEMENT_SLOPE_FLAT;
                    }
                }
            }
        }
        if (slope & TILE_ELEMENT_SLOPE_N_CORNER_UP)
            slope |= TILE_ELEMENT_SLOPE_W_CORNER_UP;
        if (slope & TILE_ELEMENT_SLOPE_E_CORNER_UP)
            slope |= TILE_ELEMENT_SLOPE_S_CORNER_UP;

        newTileElement->SetSlope(slope);
        newTileElement->base_height = z;
        newTileElement->clearance_height = z;

        update_park_fences({ x << 5, y << 5 });
    }
}

/**
 * Clears the provided element properly from a certain tile, and updates
 * the pointer (when needed) passed to this function to point to the next element.
 */
static void clear_element_at(int32_t x, int32_t y, TileElement** elementPtr)
{
    TileElement* element = *elementPtr;
    switch (element->GetType())
    {
        case TILE_ELEMENT_TYPE_SURFACE:
            element->base_height = 2;
            element->clearance_height = 2;
            element->AsSurface()->SetSlope(TILE_ELEMENT_SLOPE_FLAT);
            element->AsSurface()->SetSurfaceStyle(TERRAIN_GRASS);
            element->AsSurface()->SetEdgeStyle(TERRAIN_EDGE_ROCK);
            element->AsSurface()->SetGrassLength(GRASS_LENGTH_CLEAR_0);
            element->AsSurface()->SetOwnership(OWNERSHIP_UNOWNED);
            element->AsSurface()->SetParkFences(0);
            element->AsSurface()->SetWaterHeight(0);
            // Because this element is not completely removed, the pointer must be updated manually
            // The rest of the elements are removed from the array, so the pointer doesn't need to be updated.
            (*elementPtr)++;
            break;
        case TILE_ELEMENT_TYPE_ENTRANCE:
        {
            int32_t rotation = element->GetDirectionWithOffset(1);
            switch (element->AsEntrance()->GetSequenceIndex())
            {
                case 1:
                    x += CoordsDirectionDelta[rotation].x;
                    y += CoordsDirectionDelta[rotation].y;
                    break;
                case 2:
                    x -= CoordsDirectionDelta[rotation].x;
                    y -= CoordsDirectionDelta[rotation].y;
                    break;
            }
            auto parkEntranceRemoveAction = ParkEntranceRemoveAction({ x, y, element->base_height * 8 });
            GameActions::Execute(&parkEntranceRemoveAction);
            break;
        }
        case TILE_ELEMENT_TYPE_WALL:
        {
            TileCoordsXYZD wallLocation = { x >> 5, y >> 5, element->base_height, element->GetDirection() };
            auto wallRemoveAction = WallRemoveAction(wallLocation);
            GameActions::Execute(&wallRemoveAction);
        }
        break;
        case TILE_ELEMENT_TYPE_LARGE_SCENERY:
        {
            auto removeSceneryAction = LargeSceneryRemoveAction(
                x, y, element->base_height, element->GetDirection(), element->AsLargeScenery()->GetSequenceIndex());
            GameActions::Execute(&removeSceneryAction);
        }
        break;
        case TILE_ELEMENT_TYPE_BANNER:
        {
            auto bannerRemoveAction = BannerRemoveAction(
                { x, y, element->base_height * 8, element->AsBanner()->GetPosition() });
            GameActions::Execute(&bannerRemoveAction);
            break;
        }
        default:
            tile_element_remove(element);
            break;
    }
}

/**
 * Clears all elements properly from a certain tile.
 *  rct2: 0x0068AE2A
 */
static void clear_elements_at(int32_t x, int32_t y)
{
    // Remove the spawn point (if there is one in the current tile)
    gPeepSpawns.erase(
        std::remove_if(
            gPeepSpawns.begin(), gPeepSpawns.end(),
            [x, y](const auto& spawn) { return floor2(spawn.x, 32) == x && floor2(spawn.y, 32) == y; }),
        gPeepSpawns.end());

    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);

    // Remove all elements except the last one
    while (!tileElement->IsLastForTile())
        clear_element_at(x, y, &tileElement);

    // Remove the last element
    clear_element_at(x, y, &tileElement);
}

int32_t map_get_highest_z(int32_t tileX, int32_t tileY)
{
    TileElement* tileElement;
    uint32_t z;

    tileElement = map_get_surface_element_at(tileX, tileY);
    if (tileElement == nullptr)
        return -1;

    z = tileElement->base_height * 8;

    // Raise z so that is above highest point of land and water on tile
    if ((tileElement->AsSurface()->GetSlope() & TILE_ELEMENT_SLOPE_ALL_CORNERS_UP) != TILE_ELEMENT_SLOPE_FLAT)
        z += 16;
    if ((tileElement->AsSurface()->GetSlope() & TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT) != 0)
        z += 16;

    z = std::max(z, tileElement->AsSurface()->GetWaterHeight() * 16);
    return z;
}

LargeSceneryElement* map_get_large_scenery_segment(int32_t x, int32_t y, int32_t z, int32_t direction, int32_t sequence)
{
    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);
    if (tileElement == nullptr)
    {
        return nullptr;
    }
    do
    {
        if (tileElement->GetType() != TILE_ELEMENT_TYPE_LARGE_SCENERY)
            continue;
        if (tileElement->base_height != z)
            continue;
        if (tileElement->AsLargeScenery()->GetSequenceIndex() != sequence)
            continue;
        if ((tileElement->GetDirection()) != direction)
            continue;

        return tileElement->AsLargeScenery();
    } while (!(tileElement++)->IsLastForTile());
    return nullptr;
}

EntranceElement* map_get_park_entrance_element_at(int32_t x, int32_t y, int32_t z, bool ghost)
{
    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);
    if (tileElement != nullptr)
    {
        do
        {
            if (tileElement->GetType() != TILE_ELEMENT_TYPE_ENTRANCE)
                continue;

            if (tileElement->base_height != z)
                continue;

            if (tileElement->AsEntrance()->GetEntranceType() != ENTRANCE_TYPE_PARK_ENTRANCE)
                continue;

            if (!ghost && tileElement->IsGhost())
                continue;

            return tileElement->AsEntrance();
        } while (!(tileElement++)->IsLastForTile());
    }
    return nullptr;
}

EntranceElement* map_get_ride_entrance_element_at(int32_t x, int32_t y, int32_t z, bool ghost)
{
    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);
    if (tileElement != nullptr)
    {
        do
        {
            if (tileElement->GetType() != TILE_ELEMENT_TYPE_ENTRANCE)
                continue;

            if (tileElement->base_height != z)
                continue;

            if (tileElement->AsEntrance()->GetEntranceType() != ENTRANCE_TYPE_RIDE_ENTRANCE)
                continue;

            if (!ghost && tileElement->IsGhost())
                continue;

            return tileElement->AsEntrance();
        } while (!(tileElement++)->IsLastForTile());
    }
    return nullptr;
}

EntranceElement* map_get_ride_exit_element_at(int32_t x, int32_t y, int32_t z, bool ghost)
{
    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);
    if (tileElement != nullptr)
    {
        do
        {
            if (tileElement->GetType() != TILE_ELEMENT_TYPE_ENTRANCE)
                continue;

            if (tileElement->base_height != z)
                continue;

            if (tileElement->AsEntrance()->GetEntranceType() != ENTRANCE_TYPE_RIDE_EXIT)
                continue;

            if (!ghost && tileElement->IsGhost())
                continue;

            return tileElement->AsEntrance();
        } while (!(tileElement++)->IsLastForTile());
    }
    return nullptr;
}

SmallSceneryElement* map_get_small_scenery_element_at(int32_t x, int32_t y, int32_t z, int32_t type, uint8_t quadrant)
{
    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);
    if (tileElement != nullptr)
    {
        do
        {
            if (tileElement->GetType() != TILE_ELEMENT_TYPE_SMALL_SCENERY)
                continue;
            if (tileElement->AsSmallScenery()->GetSceneryQuadrant() != quadrant)
                continue;
            if (tileElement->base_height != z)
                continue;
            if (tileElement->AsSmallScenery()->GetEntryIndex() != type)
                continue;

            return tileElement->AsSmallScenery();
        } while (!(tileElement++)->IsLastForTile());
    }
    return nullptr;
}

bool map_large_scenery_get_origin(
    int32_t x, int32_t y, int32_t z, int32_t direction, int32_t sequence, int32_t* outX, int32_t* outY, int32_t* outZ,
    LargeSceneryElement** outElement)
{
    rct_scenery_entry* sceneryEntry;
    rct_large_scenery_tile* tile;
    int16_t offsetX, offsetY;

    auto tileElement = map_get_large_scenery_segment(x, y, z, direction, sequence);
    if (tileElement == nullptr)
        return false;

    sceneryEntry = tileElement->GetEntry();
    tile = &sceneryEntry->large_scenery.tiles[sequence];

    offsetX = tile->x_offset;
    offsetY = tile->y_offset;
    rotate_map_coordinates(&offsetX, &offsetY, direction);

    *outX = x - offsetX;
    *outY = y - offsetY;
    *outZ = (z * 8) - tile->z_offset;
    if (outElement != nullptr)
        *outElement = tileElement;
    return true;
}

/**
 *
 *  rct2: 0x006B9B05
 */
bool sign_set_colour(
    int32_t x, int32_t y, int32_t z, int32_t direction, int32_t sequence, uint8_t mainColour, uint8_t textColour)
{
    LargeSceneryElement* tileElement;
    rct_scenery_entry* sceneryEntry;
    rct_large_scenery_tile *sceneryTiles, *tile;
    int16_t offsetX, offsetY;
    int32_t x0, y0, z0;

    if (!map_large_scenery_get_origin(x, y, z, direction, sequence, &x0, &y0, &z0, &tileElement))
    {
        return false;
    }

    sceneryEntry = tileElement->GetEntry();
    sceneryTiles = sceneryEntry->large_scenery.tiles;

    // Iterate through each tile of the large scenery element
    sequence = 0;
    for (tile = sceneryTiles; tile->x_offset != -1; tile++, sequence++)
    {
        offsetX = tile->x_offset;
        offsetY = tile->y_offset;
        rotate_map_coordinates(&offsetX, &offsetY, direction);

        x = x0 + offsetX;
        y = y0 + offsetY;
        z = (z0 + tile->z_offset) / 8;
        tileElement = map_get_large_scenery_segment(x, y, z, direction, sequence);
        if (tileElement != nullptr)
        {
            tileElement->SetPrimaryColour(mainColour);
            tileElement->SetSecondaryColour(textColour);

            map_invalidate_tile(x, y, tileElement->base_height * 8, tileElement->clearance_height * 8);
        }
    }

    return true;
}

static void translate_3d_to_2d(int32_t rotation, int32_t* x, int32_t* y)
{
    int32_t rx, ry;

    switch (rotation & 3)
    {
        default:
        case 0:
            rx = (*y) - (*x);
            ry = (*x) + (*y);
            break;
        case 1:
            rx = -(*x) - (*y);
            ry = (*y) - (*x);
            break;
        case 2:
            rx = (*x) - (*y);
            ry = -(*x) - (*y);
            break;
        case 3:
            rx = (*x) + (*y);
            ry = (*x) - (*y);
            break;
    }
    ry /= 2;

    *x = rx;
    *y = ry;
}

CoordsXY translate_3d_to_2d_with_z(int32_t rotation, CoordsXYZ pos)
{
    CoordsXY result = {};
    switch (rotation & 3)
    {
        default:
        case 0:
            result.x = pos.y - pos.x;
            result.y = (pos.x + pos.y) / 2 - pos.z;
            break;
        case 1:
            result.x = -pos.x - pos.y;
            result.y = (pos.y - pos.x) / 2 - pos.z;
            break;
        case 2:
            result.x = pos.x - pos.y;
            result.y = (-pos.x - pos.y) / 2 - pos.z;
            break;
        case 3:
            result.x = pos.x + pos.y;
            result.y = (pos.x - pos.y) / 2 - pos.z;
            break;
    }
    return result;
}

static void map_invalidate_tile_under_zoom(int32_t x, int32_t y, int32_t z0, int32_t z1, int32_t maxZoom)
{
    if (gOpenRCT2Headless)
        return;

    int32_t x1, y1, x2, y2;

    x += 16;
    y += 16;
    translate_3d_to_2d(get_current_rotation(), &x, &y);

    x1 = x - 32;
    y1 = y - 32 - z1;
    x2 = x + 32;
    y2 = y + 32 - z0;

    for (int32_t i = 0; i < MAX_VIEWPORT_COUNT; i++)
    {
        rct_viewport* viewport = &g_viewport_list[i];
        if (viewport->width != 0 && (maxZoom == -1 || viewport->zoom <= maxZoom))
        {
            viewport_invalidate(viewport, x1, y1, x2, y2);
        }
    }
}

/**
 *
 *  rct2: 0x006EC847
 */
void map_invalidate_tile(int32_t x, int32_t y, int32_t z0, int32_t z1)
{
    map_invalidate_tile_under_zoom(x, y, z0, z1, -1);
}

/**
 *
 *  rct2: 0x006ECB60
 */
void map_invalidate_tile_zoom1(int32_t x, int32_t y, int32_t z0, int32_t z1)
{
    map_invalidate_tile_under_zoom(x, y, z0, z1, 1);
}

/**
 *
 *  rct2: 0x006EC9CE
 */
void map_invalidate_tile_zoom0(int32_t x, int32_t y, int32_t z0, int32_t z1)
{
    map_invalidate_tile_under_zoom(x, y, z0, z1, 0);
}

/**
 *
 *  rct2: 0x006EC6D7
 */
void map_invalidate_tile_full(int32_t x, int32_t y)
{
    map_invalidate_tile(x, y, 0, 2080);
}

void map_invalidate_element(int32_t x, int32_t y, TileElement* tileElement)
{
    map_invalidate_tile(x, y, tileElement->base_height * 8, tileElement->clearance_height * 8);
}

void map_invalidate_region(const LocationXY16& mins, const LocationXY16& maxs)
{
    int32_t x0, y0, x1, y1, left, right, top, bottom;

    x0 = mins.x + 16;
    y0 = mins.y + 16;

    x1 = maxs.x + 16;
    y1 = maxs.y + 16;

    map_get_bounding_box(x0, y0, x1, y1, &left, &top, &right, &bottom);

    left -= 32;
    right += 32;
    bottom += 32;
    top -= 32 + 2080;

    for (int32_t i = 0; i < MAX_VIEWPORT_COUNT; i++)
    {
        rct_viewport* viewport = &g_viewport_list[i];
        if (viewport->width != 0)
        {
            viewport_invalidate(viewport, left, top, right, bottom);
        }
    }
}

int32_t map_get_tile_side(int32_t mapX, int32_t mapY)
{
    int32_t subMapX = mapX & (32 - 1);
    int32_t subMapY = mapY & (32 - 1);
    return (subMapX < subMapY) ? ((subMapX + subMapY) < 32 ? 0 : 1) : ((subMapX + subMapY) < 32 ? 3 : 2);
}

int32_t map_get_tile_quadrant(int32_t mapX, int32_t mapY)
{
    int32_t subMapX = mapX & (32 - 1);
    int32_t subMapY = mapY & (32 - 1);
    return (subMapX > 16) ? (subMapY < 16 ? 1 : 0) : (subMapY < 16 ? 2 : 3);
}

/**
 *
 *  rct2: 0x00693BFF
 */
bool map_surface_is_blocked(int16_t x, int16_t y)
{
    TileElement* tileElement;
    if (x >= 8192 || y >= 8192)
        return true;

    tileElement = map_get_surface_element_at({ x, y });

    if (tileElement == nullptr)
    {
        return true;
    }

    int16_t water_height = tileElement->AsSurface()->GetWaterHeight();
    water_height *= 2;
    if (water_height > tileElement->base_height)
        return true;

    int16_t base_z = tileElement->base_height;
    int16_t clear_z = tileElement->base_height + 2;
    if (tileElement->AsSurface()->GetSlope() & TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT)
        clear_z += 2;

    while (!(tileElement++)->IsLastForTile())
    {
        if (clear_z >= tileElement->clearance_height)
            continue;

        if (base_z < tileElement->base_height)
            continue;

        if (tileElement->GetType() == TILE_ELEMENT_TYPE_PATH || tileElement->GetType() == TILE_ELEMENT_TYPE_WALL)
            continue;

        if (tileElement->GetType() != TILE_ELEMENT_TYPE_SMALL_SCENERY)
            return true;

        rct_scenery_entry* scenery = tileElement->AsSmallScenery()->GetEntry();
        if (scenery == nullptr)
        {
            return false;
        }
        if (scenery_small_entry_has_flag(scenery, SMALL_SCENERY_FLAG_FULL_TILE))
            return true;
    }
    return false;
}

/* Clears all map elements, to be used before generating a new map */
void map_clear_all_elements()
{
    for (int32_t y = 0; y < (MAXIMUM_MAP_SIZE_TECHNICAL * 32); y += 32)
    {
        for (int32_t x = 0; x < (MAXIMUM_MAP_SIZE_TECHNICAL * 32); x += 32)
        {
            clear_elements_at(x, y);
        }
    }
}

void game_command_modify_tile(
    int32_t* eax, int32_t* ebx, int32_t* ecx, int32_t* edx, [[maybe_unused]] int32_t* esi, int32_t* edi, int32_t* ebp)
{
    const int32_t flags = *ebx;
    const int32_t x = *ecx & 0xFF;
    const int32_t y = (*ecx >> 8) & 0xFF;
    const TILE_INSPECTOR_INSTRUCTION_TYPE instruction = static_cast<TILE_INSPECTOR_INSTRUCTION_TYPE>(*eax);

    switch (instruction)
    {
        case TILE_INSPECTOR_ANY_REMOVE:
        {
            const int16_t elementIndex = *edx;
            *ebx = tile_inspector_remove_element_at(x, y, elementIndex, flags);
            break;
        }
        case TILE_INSPECTOR_ANY_SWAP:
        {
            const int32_t firstIndex = *edx;
            const int32_t secondIndex = *edi;
            *ebx = tile_inspector_swap_elements_at(x, y, firstIndex, secondIndex, flags);
            break;
        }
        case TILE_INSPECTOR_ANY_INSERT_CORRUPT:
        {
            const int16_t elementIndex = *edx;
            *ebx = tile_inspector_insert_corrupt_at(x, y, elementIndex, flags);
            break;
        }
        case TILE_INSPECTOR_ANY_ROTATE:
        {
            const int16_t elementIndex = *edx;
            *ebx = tile_inspector_rotate_element_at(x, y, elementIndex, flags);
            break;
        }
        case TILE_INSPECTOR_ANY_PASTE:
        {
            TileElement elementToPaste;
            const int32_t data[] = { *edx, *edi };
            assert_struct_size(data, sizeof(elementToPaste));
            std::memcpy(&elementToPaste, data, 8);
            *ebx = tile_inspector_paste_element_at(x, y, elementToPaste, flags);
            break;
        }
        case TILE_INSPECTOR_ANY_SORT:
        {
            *ebx = tile_inspector_sort_elements_at(x, y, flags);
            break;
        }
        case TILE_INSPECTOR_ANY_BASE_HEIGHT_OFFSET:
        {
            const int16_t elementIndex = *edx;
            const int8_t heightOffset = *edi;
            *ebx = tile_inspector_any_base_height_offset(x, y, elementIndex, heightOffset, flags);
            break;
        }
        case TILE_INSPECTOR_SURFACE_SHOW_PARK_FENCES:
        {
            const bool showFences = *edx;
            *ebx = tile_inspector_surface_show_park_fences(x, y, showFences, flags);
            break;
        }
        case TILE_INSPECTOR_SURFACE_TOGGLE_CORNER:
        {
            const int32_t cornerIndex = *edx;
            *ebx = tile_inspector_surface_toggle_corner(x, y, cornerIndex, flags);
            break;
        }
        case TILE_INSPECTOR_SURFACE_TOGGLE_DIAGONAL:
        {
            *ebx = tile_inspector_surface_toggle_diagonal(x, y, flags);
            break;
        }
        case TILE_INSPECTOR_PATH_SET_SLOPE:
        {
            const int32_t elementIndex = *edx;
            const bool sloped = *edi;
            *ebx = tile_inspector_path_set_sloped(x, y, elementIndex, sloped, flags);
            break;
        }
        case TILE_INSPECTOR_PATH_SET_BROKEN:
        {
            const int32_t elementIndex = *edx;
            const bool broken = *edi;
            *ebx = tile_inspector_path_set_broken(x, y, elementIndex, broken, flags);
            break;
        }
        case TILE_INSPECTOR_PATH_TOGGLE_EDGE:
        {
            const int32_t elementIndex = *edx;
            const int32_t edgeIndex = *edi;
            *ebx = tile_inspector_path_toggle_edge(x, y, elementIndex, edgeIndex, flags);
            break;
        }
        case TILE_INSPECTOR_ENTRANCE_MAKE_USABLE:
        {
            const int32_t elementIndex = *edx;
            *ebx = tile_inspector_entrance_make_usable(x, y, elementIndex, flags);
            break;
        }
        case TILE_INSPECTOR_WALL_SET_SLOPE:
        {
            const int32_t elementIndex = *edx;
            const int32_t slopeValue = *edi;
            *ebx = tile_inspector_wall_set_slope(x, y, elementIndex, slopeValue, flags);
            break;
        }
        case TILE_INSPECTOR_TRACK_BASE_HEIGHT_OFFSET:
        {
            const int32_t elementIndex = *edx;
            const int8_t heightOffset = *edi;
            *ebx = tile_inspector_track_base_height_offset(x, y, elementIndex, heightOffset, flags);
            break;
        }
        case TILE_INSPECTOR_TRACK_SET_CHAIN:
        {
            const int32_t elementIndex = *edx;
            const bool entireTrackBlock = *edi;
            const bool setChain = *ebp;
            *ebx = tile_inspector_track_set_chain(x, y, elementIndex, entireTrackBlock, setChain, flags);
            break;
        }
        case TILE_INSPECTOR_TRACK_SET_BLOCK_BRAKE:
        {
            const int32_t elementIndex = *edx;
            const bool blockBrake = *edi;
            *ebx = tile_inspector_track_set_block_brake(x, y, elementIndex, blockBrake, flags);
            break;
        }
        case TILE_INSPECTOR_TRACK_SET_INDESTRUCTIBLE:
        {
            const int32_t elementIndex = *edx;
            const bool isIndestructible = *edi;
            *ebx = tile_inspector_track_set_indestructible(x, y, elementIndex, isIndestructible, flags);
            break;
        }
        case TILE_INSPECTOR_SCENERY_SET_QUARTER_LOCATION:
        {
            const int32_t elementIndex = *edx;
            const int32_t quarterIndex = *edi;
            *ebx = tile_inspector_scenery_set_quarter_location(x, y, elementIndex, quarterIndex, flags);
            break;
        }
        case TILE_INSPECTOR_SCENERY_SET_QUARTER_COLLISION:
        {
            const int32_t elementIndex = *edx;
            const int32_t quarterIndex = *edi;
            *ebx = tile_inspector_scenery_set_quarter_collision(x, y, elementIndex, quarterIndex, flags);
            break;
        }
        case TILE_INSPECTOR_BANNER_TOGGLE_BLOCKING_EDGE:
        {
            const int32_t elementIndex = *edx;
            const int32_t edgeIndex = *edi;
            *ebx = tile_inspector_banner_toggle_blocking_edge(x, y, elementIndex, edgeIndex, flags);
            break;
        }
        case TILE_INSPECTOR_CORRUPT_CLAMP:
        {
            const int32_t elementIndex = *edx;
            *ebx = tile_inspector_corrupt_clamp(x, y, elementIndex, flags);
            break;
        }
        default:
            log_error("invalid instruction");
            *ebx = MONEY32_UNDEFINED;
            break;
    }

    if (flags & GAME_COMMAND_FLAG_APPLY && gGameCommandNestLevel == 1 && !(flags & GAME_COMMAND_FLAG_GHOST)
        && *ebx != MONEY32_UNDEFINED)
    {
        LocationXYZ16 coord;
        coord.x = (x << 5) + 16;
        coord.y = (y << 5) + 16;
        coord.z = tile_element_height(coord.x, coord.y);
        network_set_player_last_action_coord(network_get_player_index(game_command_playerid), coord);
    }
}

/**
 * Gets the track element at x, y, z.
 * @param x x units, not tiles.
 * @param y y units, not tiles.
 * @param z Base height.
 */
TrackElement* map_get_track_element_at(int32_t x, int32_t y, int32_t z)
{
    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);
    do
    {
        if (tileElement->GetType() != TILE_ELEMENT_TYPE_TRACK)
            continue;
        if (tileElement->base_height != z)
            continue;

        return tileElement->AsTrack();
    } while (!(tileElement++)->IsLastForTile());

    return nullptr;
}

/**
 * Gets the track element at x, y, z that is the given track type.
 * @param x x units, not tiles.
 * @param y y units, not tiles.
 * @param z Base height.
 */
TileElement* map_get_track_element_at_of_type(int32_t x, int32_t y, int32_t z, int32_t trackType)
{
    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);
    do
    {
        if (tileElement->GetType() != TILE_ELEMENT_TYPE_TRACK)
            continue;
        if (tileElement->base_height != z)
            continue;
        if (tileElement->AsTrack()->GetTrackType() != trackType)
            continue;

        return tileElement;
    } while (!(tileElement++)->IsLastForTile());

    return nullptr;
}

/**
 * Gets the track element at x, y, z that is the given track type and sequence.
 * @param x x units, not tiles.
 * @param y y units, not tiles.
 * @param z Base height.
 */
TileElement* map_get_track_element_at_of_type_seq(int32_t x, int32_t y, int32_t z, int32_t trackType, int32_t sequence)
{
    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);
    do
    {
        if (tileElement == nullptr)
            break;
        if (tileElement->GetType() != TILE_ELEMENT_TYPE_TRACK)
            continue;
        if (tileElement->base_height != z)
            continue;
        if (tileElement->AsTrack()->GetTrackType() != trackType)
            continue;
        if (tileElement->AsTrack()->GetSequenceIndex() != sequence)
            continue;

        return tileElement;
    } while (!(tileElement++)->IsLastForTile());

    return nullptr;
}

/**
 * Gets the track element at x, y, z that is the given track type and sequence.
 * @param x x units, not tiles.
 * @param y y units, not tiles.
 * @param z Base height.
 */
TileElement* map_get_track_element_at_of_type_from_ride(int32_t x, int32_t y, int32_t z, int32_t trackType, ride_id_t rideIndex)
{
    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);
    do
    {
        if (tileElement->GetType() != TILE_ELEMENT_TYPE_TRACK)
            continue;
        if (tileElement->base_height != z)
            continue;
        if (tileElement->AsTrack()->GetRideIndex() != rideIndex)
            continue;
        if (tileElement->AsTrack()->GetTrackType() != trackType)
            continue;

        return tileElement;
    } while (!(tileElement++)->IsLastForTile());

    return nullptr;
};

/**
 * Gets the track element at x, y, z that is the given track type and sequence.
 * @param x x units, not tiles.
 * @param y y units, not tiles.
 * @param z Base height.
 */
TileElement* map_get_track_element_at_from_ride(int32_t x, int32_t y, int32_t z, ride_id_t rideIndex)
{
    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);
    do
    {
        if (tileElement->GetType() != TILE_ELEMENT_TYPE_TRACK)
            continue;
        if (tileElement->base_height != z)
            continue;
        if (tileElement->AsTrack()->GetRideIndex() != rideIndex)
            continue;

        return tileElement;
    } while (!(tileElement++)->IsLastForTile());

    return nullptr;
};

/**
 * Gets the track element at x, y, z that is the given track type and sequence.
 * @param x x units, not tiles.
 * @param y y units, not tiles.
 * @param z Base height.
 * @param direction The direction (0 - 3).
 */
TileElement* map_get_track_element_at_with_direction_from_ride(
    int32_t x, int32_t y, int32_t z, int32_t direction, ride_id_t rideIndex)
{
    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);
    do
    {
        if (tileElement->GetType() != TILE_ELEMENT_TYPE_TRACK)
            continue;
        if (tileElement->base_height != z)
            continue;
        if (tileElement->AsTrack()->GetRideIndex() != rideIndex)
            continue;
        if (tileElement->GetDirection() != direction)
            continue;

        return tileElement;
    } while (!(tileElement++)->IsLastForTile());

    return nullptr;
};

void map_offset_with_rotation(int16_t* x, int16_t* y, int16_t offsetX, int16_t offsetY, uint8_t rotation)
{
    TileCoordsXY offsets = { offsetX, offsetY };
    TileCoordsXY newCoords = { *x, *y };
    newCoords += offsets.Rotate(rotation);

    *x = (int16_t)newCoords.x;
    *y = (int16_t)newCoords.y;
}

WallElement* map_get_wall_element_at(int32_t x, int32_t y, int32_t z, int32_t direction)
{
    TileElement* tileElement = map_get_first_element_at(x >> 5, y >> 5);
    do
    {
        if (tileElement->GetType() != TILE_ELEMENT_TYPE_WALL)
            continue;
        if (tileElement->base_height != z)
            continue;
        if (tileElement->GetDirection() != direction)
            continue;

        return tileElement->AsWall();
    } while (!(tileElement++)->IsLastForTile());
    return nullptr;
}

uint16_t check_max_allowable_land_rights_for_tile(uint8_t x, uint8_t y, uint8_t base_z)
{
    TileElement* tileElement = map_get_first_element_at(x, y);
    uint16_t destOwnership = OWNERSHIP_OWNED;

    // Sometimes done deliberately.
    if (tileElement == nullptr)
    {
        return OWNERSHIP_OWNED;
    }

    do
    {
        int32_t type = tileElement->GetType();
        if (type == TILE_ELEMENT_TYPE_PATH
            || (type == TILE_ELEMENT_TYPE_ENTRANCE
                && tileElement->AsEntrance()->GetEntranceType() == ENTRANCE_TYPE_PARK_ENTRANCE))
        {
            destOwnership = OWNERSHIP_CONSTRUCTION_RIGHTS_OWNED;
            // Do not own construction rights if too high/below surface
            if (tileElement->base_height - 3 > base_z || tileElement->base_height < base_z)
            {
                destOwnership = OWNERSHIP_UNOWNED;
                break;
            }
        }
    } while (!(tileElement++)->IsLastForTile());

    return destOwnership;
}

void FixLandOwnershipTiles(std::initializer_list<TileCoordsXY> tiles)
{
    FixLandOwnershipTilesWithOwnership(tiles, OWNERSHIP_AVAILABLE);
}

void FixLandOwnershipTilesWithOwnership(std::initializer_list<TileCoordsXY> tiles, uint8_t ownership)
{
    TileElement* currentElement;
    for (const TileCoordsXY* tile = tiles.begin(); tile != tiles.end(); ++tile)
    {
        currentElement = map_get_surface_element_at((*tile).x, (*tile).y);
        currentElement->AsSurface()->SetOwnership(ownership);
        update_park_fences_around_tile({ (*tile).x * 32, (*tile).y * 32 });
    }
}
