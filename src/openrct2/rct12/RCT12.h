/*****************************************************************************
 * Copyright (c) 2014-2019 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

// Structures shared between both RCT1 and RCT2.

#include "../common.h"
#include "../world/Location.hpp"

#define RCT12_MAX_RIDES_IN_PARK 255
#define RCT12_MAX_AWARDS 4
#define RCT12_MAX_NEWS_ITEMS 61
#define RCT12_MAX_STATIONS_PER_RIDE 4
#define RCT12_MAX_PEEP_SPAWNS 2
#define RCT12_MAX_PARK_ENTRANCES 4
// The number of elements in the patrol_areas array per staff member. Every bit in the array represents a 4x4 square.
// In RCT1, that's an 8-bit array. 8 * 128 = 1024 bits, which is also the number of 4x4 squares on a 128x128 map.
// For RCT2, it's a 32-bit array. 32 * 128 = 4096 bits, which is also the number of 4x4 squares on a 256x256 map.
#define RCT12_PATROL_AREA_SIZE 128
#define RCT12_STAFF_TYPE_COUNT 4
#define RCT12_NUM_COLOUR_SCHEMES 4
#define RCT12_MAX_VEHICLES_PER_RIDE 32
#define RCT12_MAX_VEHICLE_COLOURS 32
#define RCT12_SOUND_ID_NULL 0xFF

#define RCT12_EXPENDITURE_TABLE_MONTH_COUNT 16
#define RCT12_EXPENDITURE_TYPE_COUNT 14
#define RCT12_FINANCE_GRAPH_SIZE 128

#define RCT12_MAX_USER_STRINGS 1024
#define RCT12_USER_STRING_MAX_LENGTH 32

#define RCT12_PEEP_MAX_THOUGHTS 5

#define RCT12_RIDE_ID_NULL 255
#define RCT12_RIDE_MEASUREMENT_MAX_ITEMS 4800

constexpr uint16_t const RCT12_MAX_INVERSIONS = 31;
constexpr uint16_t const RCT12_MAX_GOLF_HOLES = 31;
constexpr uint16_t const RCT12_MAX_HELICES = 31;

#pragma pack(push, 1)

struct rct12_award
{
    uint16_t time;
    uint16_t type;
};
assert_struct_size(rct12_award, 4);

/**
 * A single news item / message.
 * size: 0x10C
 */
struct rct12_news_item
{
    uint8_t Type;
    uint8_t Flags;
    uint32_t Assoc;
    uint16_t Ticks;
    uint16_t MonthYear;
    uint8_t Day;
    uint8_t pad_0B;
    char Text[256];
};
assert_struct_size(rct12_news_item, 0x10C);

struct rct12_xyzd8
{
    uint8_t x, y, z, direction;
};
assert_struct_size(rct12_xyzd8, 4);

struct rct12_peep_spawn
{
    uint16_t x;
    uint16_t y;
    uint8_t z;
    uint8_t direction;
};
assert_struct_size(rct12_peep_spawn, 6);

enum class RCT12TileElementType : uint8_t
{
    Surface = (0 << 2),
    Path = (1 << 2),
    Track = (2 << 2),
    SmallScenery = (3 << 2),
    Entrance = (4 << 2),
    Wall = (5 << 2),
    LargeScenery = (6 << 2),
    Banner = (7 << 2),
    Corrupt = (8 << 2),
    EightCarsCorrupt14 = (14 << 2),
    EightCarsCorrupt15 = (15 << 2),
};
struct RCT12SurfaceElement;
struct RCT12PathElement;
struct RCT12TrackElement;
struct RCT12SmallSceneryElement;
struct RCT12LargeSceneryElement;
struct RCT12WallElement;
struct RCT12EntranceElement;
struct RCT12BannerElement;
struct RCT12CorruptElement;
struct RCT12EightCarsCorruptElement14;
struct RCT12EightCarsCorruptElement15;

struct RCT12TileElementBase
{
    uint8_t type;             // 0
    uint8_t flags;            // 1
    uint8_t base_height;      // 2
    uint8_t clearance_height; // 3
    uint8_t GetType() const;
    uint8_t GetDirection() const;
    bool IsLastForTile() const;
    bool IsGhost() const;
};
/**
 * Map element structure
 * size: 0x08
 */
struct RCT12TileElement : public RCT12TileElementBase
{
    uint8_t pad_04[4];
    template<typename TType, RCT12TileElementType TClass> TType* as() const
    {
        return (RCT12TileElementType)GetType() == TClass ? (TType*)this : nullptr;
    }

    RCT12SurfaceElement* AsSurface() const
    {
        return as<RCT12SurfaceElement, RCT12TileElementType::Surface>();
    }
    RCT12PathElement* AsPath() const
    {
        return as<RCT12PathElement, RCT12TileElementType::Path>();
    }
    RCT12TrackElement* AsTrack() const
    {
        return as<RCT12TrackElement, RCT12TileElementType::Track>();
    }
    RCT12SmallSceneryElement* AsSmallScenery() const
    {
        return as<RCT12SmallSceneryElement, RCT12TileElementType::SmallScenery>();
    }
    RCT12LargeSceneryElement* AsLargeScenery() const
    {
        return as<RCT12LargeSceneryElement, RCT12TileElementType::LargeScenery>();
    }
    RCT12WallElement* AsWall() const
    {
        return as<RCT12WallElement, RCT12TileElementType::Wall>();
    }
    RCT12EntranceElement* AsEntrance() const
    {
        return as<RCT12EntranceElement, RCT12TileElementType::Entrance>();
    }
    RCT12BannerElement* AsBanner() const
    {
        return as<RCT12BannerElement, RCT12TileElementType::Banner>();
    }
    void ClearAs(uint8_t newType);
};
assert_struct_size(RCT12TileElement, 8);
struct RCT12SurfaceElement : RCT12TileElementBase
{
private:
    uint8_t slope;        // 4 0xE0 Edge Style, 0x1F Slope
    uint8_t terrain;      // 5 0xE0 Terrain Style, 0x1F Water height
    uint8_t grass_length; // 6
    uint8_t ownership;    // 7
public:
    uint8_t GetSlope() const;
    uint32_t GetSurfaceStyle() const;
    uint32_t GetEdgeStyle() const;
    uint8_t GetGrassLength() const;
    uint8_t GetOwnership() const;
    uint32_t GetWaterHeight() const;
    uint8_t GetParkFences() const;
    bool HasTrackThatNeedsWater() const;
};
assert_struct_size(RCT12SurfaceElement, 8);
struct RCT12PathElement : RCT12TileElementBase
{
private:
    uint8_t entryIndex; // 4, 0xF0 Path type, 0x08 Ride sign, 0x04 Set when path is sloped, 0x03 Rotation
    uint8_t additions;  // 5, 0bGSSSAAAA: G = Ghost, S = station index, A = addition (0 means no addition)
    uint8_t edges;      // 6
    union
    {
        uint8_t additionStatus; // 7
        uint8_t rideIndex;
    };

public:
    uint8_t GetEntryIndex() const;
    uint8_t GetQueueBannerDirection() const;
    bool IsSloped() const;
    uint8_t GetSlopeDirection() const;
    uint8_t GetRideIndex() const;
    uint8_t GetStationIndex() const;
    bool IsWide() const;
    bool IsQueue() const;
    bool HasQueueBanner() const;
    uint8_t GetEdges() const;
    uint8_t GetCorners() const;
    uint8_t GetEdgesAndCorners() const;
    bool HasAddition() const;
    uint8_t GetAddition() const;
    uint8_t GetAdditionEntryIndex() const;
    bool AdditionIsGhost() const;
    uint8_t GetAdditionStatus() const;
    uint8_t GetRCT1PathType() const;
    uint8_t GetRCT1SupportType() const;
};
assert_struct_size(RCT12PathElement, 8);
struct RCT12TrackElement : RCT12TileElementBase
{
private:
    uint8_t trackType; // 4
    union
    {
        struct
        {
            // The lower 4 bits are the track sequence.
            // The upper 4 bits are either station bits or on-ride photo bits.
            //
            // Station bits:
            // - Bit 8 marks green light
            // - Bit 5-7 are station index.
            //
            // On-ride photo bits:
            // - Bits 7 and 8 are never set
            // - Bits 5 and 6 are set when a vehicle triggers the on-ride photo and act like a countdown from 3.
            // - If any of the bits 5-8 are set, the game counts it as a photo being taken.
            uint8_t sequence; // 5.
            uint8_t colour;   // 6
        };
        uint16_t mazeEntry; // 5
    };
    uint8_t rideIndex; // 7
public:
    uint8_t GetTrackType() const;
    uint8_t GetSequenceIndex() const;
    uint8_t GetRideIndex() const;
    uint8_t GetColourScheme() const;
    uint8_t GetStationIndex() const;
    bool HasChain() const;
    bool HasCableLift() const;
    bool IsInverted() const;
    uint8_t GetBrakeBoosterSpeed() const;
    bool HasGreenLight() const;
    uint8_t GetSeatRotation() const;
    uint16_t GetMazeEntry() const;
    bool IsTakingPhoto() const;
    uint8_t GetPhotoTimeout() const;
    bool IsHighlighted() const;
    // Used in RCT1, will be reintroduced at some point.
    // (See https://github.com/OpenRCT2/OpenRCT2/issues/7059)
    uint8_t GetDoorAState() const;
    uint8_t GetDoorBState() const;
};
assert_struct_size(RCT12TrackElement, 8);
struct RCT12SmallSceneryElement : RCT12TileElementBase
{
private:
    uint8_t entryIndex; // 4
    uint8_t age;        // 5
    uint8_t colour_1;   // 6
    uint8_t colour_2;   // 7
public:
    uint8_t GetEntryIndex() const;
    uint8_t GetAge() const;
    uint8_t GetSceneryQuadrant() const;
    colour_t GetPrimaryColour() const;
    colour_t GetSecondaryColour() const;
    bool NeedsSupports() const;
};
assert_struct_size(RCT12SmallSceneryElement, 8);
struct RCT12LargeSceneryElement : RCT12TileElementBase
{
private:
    uint16_t entryIndex; // 4
    uint8_t colour[2];   // 6
public:
    uint32_t GetEntryIndex() const;
    uint16_t GetSequenceIndex() const;
    colour_t GetPrimaryColour() const;
    colour_t GetSecondaryColour() const;
    BannerIndex GetBannerIndex() const;
};
assert_struct_size(RCT12LargeSceneryElement, 8);
struct RCT12WallElement : RCT12TileElementBase
{
private:
    uint8_t entryIndex; // 4
    union
    {
        uint8_t colour_3;         // 5
        BannerIndex banner_index; // 5
    };
    uint8_t colour_1;  // 6 0b_2221_1111 2 = colour_2 (uses flags for rest of colour2), 1 = colour_1
    uint8_t animation; // 7 0b_dfff_ft00 d = direction, f = frame num, t = across track flag (not used)
public:
    uint8_t GetEntryIndex() const;
    uint8_t GetSlope() const;
    colour_t GetPrimaryColour() const;
    colour_t GetSecondaryColour() const;
    colour_t GetTertiaryColour() const;
    uint8_t GetAnimationFrame() const;
    BannerIndex GetBannerIndex() const;
    bool IsAcrossTrack() const;
    bool AnimationIsBackwards() const;
    uint32_t GetRawRCT1WallTypeData() const;
    int32_t GetRCT1WallType(int32_t edge) const;
    colour_t GetRCT1WallColour() const;
};
assert_struct_size(RCT12WallElement, 8);
struct RCT12EntranceElement : RCT12TileElementBase
{
private:
    uint8_t entranceType; // 4
    uint8_t index;        // 5. 0bUSSS????, S = station index.
    uint8_t pathType;     // 6
    uint8_t rideIndex;    // 7
public:
    uint8_t GetEntranceType() const;
    uint8_t GetRideIndex() const;
    uint8_t GetStationIndex() const;
    uint8_t GetSequenceIndex() const;
    uint8_t GetPathType() const;
};
assert_struct_size(RCT12EntranceElement, 8);
struct RCT12BannerElement : RCT12TileElementBase
{
private:
    BannerIndex index; // 4
    uint8_t position;  // 5
    uint8_t flags;     // 6
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
    uint8_t unused; // 7
#pragma clang diagnostic pop
public:
    BannerIndex GetIndex() const;
    uint8_t GetPosition() const;
    uint8_t GetAllowedEdges() const;
};
assert_struct_size(RCT12BannerElement, 8);

struct RCT12CorruptElement : RCT12TileElementBase
{
    uint8_t pad[4];
};
assert_struct_size(RCT12CorruptElement, 8);

struct RCT12EightCarsCorruptElement14 : RCT12TileElementBase
{
    uint8_t pad[4];
};
assert_struct_size(RCT12EightCarsCorruptElement14, 8);

struct RCT12EightCarsCorruptElement15 : RCT12TileElementBase
{
    uint8_t pad[4];
};
assert_struct_size(RCT12EightCarsCorruptElement15, 8);

struct RCT12SpriteBase
{
    uint8_t sprite_identifier;       // 0x00
    uint8_t type;                    // 0x01
    uint16_t next_in_quadrant;       // 0x02
    uint16_t next;                   // 0x04
    uint16_t previous;               // 0x06
    uint8_t linked_list_type_offset; // 0x08
    uint8_t sprite_height_negative;  // 0x09
    uint16_t sprite_index;           // 0x0A
    uint16_t flags;                  // 0x0C
    int16_t x;                       // 0x0E
    int16_t y;                       // 0x10
    int16_t z;                       // 0x12
    uint8_t sprite_width;            // 0x14
    uint8_t sprite_height_positive;  // 0x15
    int16_t sprite_left;             // 0x16
    int16_t sprite_top;              // 0x18
    int16_t sprite_right;            // 0x1A
    int16_t sprite_bottom;           // 0x1C
    uint8_t sprite_direction;        // 0x1E
};
assert_struct_size(RCT12SpriteBase, 0x1F);

struct RCT12SpriteBalloon : RCT12SpriteBase
{
    uint8_t pad_1F[0x24 - 0x1F];
    uint16_t popped;      // 0x24
    uint8_t time_to_move; // 0x26
    uint8_t frame;        // 0x27
    uint8_t pad_28[4];
    uint8_t colour; // 0x2C
};
assert_struct_size(RCT12SpriteBalloon, 0x2D);

struct RCT12SpriteDuck : RCT12SpriteBase
{
    uint8_t pad_1F[0x26 - 0x1F];
    uint16_t frame; // 0x26
    uint8_t pad_28[0x30 - 0x28];
    int16_t target_x; // 0x30
    int16_t target_y; // 0x32
    uint8_t pad_34[0x14];
    uint8_t state; // 0x48
};
assert_struct_size(RCT12SpriteDuck, 0x49);

struct RCT12SpriteLitter : RCT12SpriteBase
{
    uint8_t pad_1F[0x24 - 0x1F];
    uint32_t creationTick; // 0x24
};
assert_struct_size(RCT12SpriteLitter, 0x28);

struct RCT12SpriteParticle : RCT12SpriteBase
{
    uint8_t pad_1F[0x26 - 0x1F];
    uint16_t frame; // 0x26
};
assert_struct_size(RCT12SpriteParticle, 0x28);

struct RCT12SpriteJumpingFountain : RCT12SpriteBase
{
    uint8_t pad_1F[0x26 - 0x1F];
    uint8_t num_ticks_alive; // 0x26
    uint8_t frame;           // 0x27
    uint8_t pad_28[0x2F - 0x28];
    uint8_t fountain_flags; // 0x2F
    int16_t target_x;       // 0x30
    int16_t target_y;       // 0x32
    uint8_t pad_34[0x46 - 0x34];
    uint16_t iteration; // 0x46
};
assert_struct_size(RCT12SpriteJumpingFountain, 0x48);

struct RCT12SpriteMoneyEffect : RCT12SpriteBase
{
    uint8_t pad_1F[0x24 - 0x1F];
    uint16_t move_delay;   // 0x24
    uint8_t num_movements; // 0x26
    uint8_t vertical;
    money32 value; // 0x28
    uint8_t pad_2C[0x44 - 0x2C];
    int16_t offset_x; // 0x44
    uint16_t wiggle;  // 0x46
};
assert_struct_size(RCT12SpriteMoneyEffect, 0x48);

struct RCT12SpriteCrashedVehicleParticle : RCT12SpriteBase
{
    uint8_t pad_1F[0x24 - 0x1F];
    uint16_t time_to_live; // 0x24
    uint16_t frame;        // 0x26
    uint8_t pad_28[0x2C - 0x28];
    uint8_t colour[2];            // 0x2C
    uint16_t crashed_sprite_base; // 0x2E
    int16_t velocity_x;           // 0x30
    int16_t velocity_y;           // 0x32
    int16_t velocity_z;           // 0x34
    uint8_t pad_36[0x38 - 0x36];
    int32_t acceleration_x; // 0x38
    int32_t acceleration_y; // 0x3C
    int32_t acceleration_z; // 0x40
};
assert_struct_size(RCT12SpriteCrashedVehicleParticle, 0x44);

struct RCT12SpriteCrashSplash : RCT12SpriteBase
{
    uint8_t pad_1F[0x26 - 0x1F];
    uint16_t frame; // 0x26
};
assert_struct_size(RCT12SpriteCrashSplash, 0x28);

struct RCT12SpriteSteamParticle : RCT12SpriteBase
{
    uint8_t pad_1F[0x24 - 0x1F];
    uint16_t time_to_move; // 0x24
    uint16_t frame;        // 0x26
};
assert_struct_size(RCT12SpriteSteamParticle, 0x28);

struct RCT12PeepThought
{
    uint8_t type;
    uint8_t item;
    uint8_t freshness;
    uint8_t fresh_timeout;
};
assert_struct_size(RCT12PeepThought, 4);

struct RCT12RideMeasurement
{
    uint8_t ride_index;                                 // 0x0000
    uint8_t flags;                                      // 0x0001
    uint32_t last_use_tick;                             // 0x0002
    uint16_t num_items;                                 // 0x0006
    uint16_t current_item;                              // 0x0008
    uint8_t vehicle_index;                              // 0x000A
    uint8_t current_station;                            // 0x000B
    int8_t vertical[RCT12_RIDE_MEASUREMENT_MAX_ITEMS];  // 0x000C
    int8_t lateral[RCT12_RIDE_MEASUREMENT_MAX_ITEMS];   // 0x12CC
    uint8_t velocity[RCT12_RIDE_MEASUREMENT_MAX_ITEMS]; // 0x258C
    uint8_t altitude[RCT12_RIDE_MEASUREMENT_MAX_ITEMS]; // 0x384C
};
assert_struct_size(RCT12RideMeasurement, 0x4B0C);

#pragma pack(pop)
