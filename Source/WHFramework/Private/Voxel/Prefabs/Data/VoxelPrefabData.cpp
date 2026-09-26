#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Voxels/VoxelItemBridge.h"

namespace
{
    void WriteU16(uint8* Target, const uint16 Value)
    {
        Target[0] = static_cast<uint8>(Value);
        Target[1] = static_cast<uint8>(Value >> 8);
    }

    void WriteU32(uint8* Target, const uint32 Value)
    {
        for (int32 Byte = 0; Byte < 4; ++Byte)
        {
            Target[Byte] = static_cast<uint8>(Value >> (8 * Byte));
        }
    }

    uint16 ReadU16(const uint8* Source)
    {
        return static_cast<uint16>(Source[0]) |
            static_cast<uint16>(Source[1]) << 8;
    }

    uint32 ReadU32(const uint8* Source)
    {
        uint32 Result = 0;
        for (int32 Byte = 0; Byte < 4; ++Byte)
        {
            Result |= static_cast<uint32>(Source[Byte]) << (8 * Byte);
        }
        return Result;
    }
}

UVoxelPrefabData::UVoxelPrefabData()
{
    Type = TEXT("VoxelPrefab");
    DisplayName = FText::GetEmpty();
}

FBox UVoxelPrefabData::GetVoxelBounds() const
{
    FBox Bounds(ForceInit);
    if (!PackedCells.IsEmpty())
    {
        if (PackedCells.Num() < 4 ||
            4ull + static_cast<uint64>(ReadU32(PackedCells.GetData())) * 16ull !=
                static_cast<uint64>(PackedCells.Num()))
        {
            return Bounds;
        }
        const uint32 Count = ReadU32(PackedCells.GetData());
        for (uint32 Index = 0; Index < Count; ++Index)
        {
            const uint8* Source = PackedCells.GetData() + 4 + Index * 16;
            const FVector Position(static_cast<int32>(ReadU32(Source)),
                static_cast<int32>(ReadU32(Source + 4)),
                static_cast<int32>(ReadU32(Source + 8)));
            Bounds += Position;
            Bounds += Position + FVector(1.0);
        }
        return Bounds;
    }
    for (const FVoxelPrefabCell& Cell : Data.Cells)
    {
        const FVector Min(Cell.Offset);
        Bounds += Min;
        Bounds += Min + FVector(1.0);
    }
    return Bounds;
}

bool UVoxelPrefabData::Validate(const FVoxelRegistrySnapshot& Registry, FString& Error) const
{
    FVoxelPrefabSaveData Cells;
    return DecodeCells(Cells, Error) && ValidateCells(Cells, Registry, Error);
}

bool UVoxelPrefabData::SetPackedData(const FVoxelPrefabSaveData& Value, FString& Error)
{
    Error.Reset();
    const int64 ByteCount = 4ll + static_cast<int64>(Value.Cells.Num()) * 16ll;
    if (Value.Cells.IsEmpty() || ByteCount > MAX_int32)
    {
        Error = TEXT("Prefab has no cells or exceeds the byte-array address range");
        return false;
    }
    TArray<FPrimaryAssetId> Palette;
    TMap<FPrimaryAssetId, uint16> PaletteIndices;
    TArray<uint8> Encoded;
    Encoded.SetNumUninitialized(static_cast<int32>(ByteCount));
    WriteU32(Encoded.GetData(), static_cast<uint32>(Value.Cells.Num()));
    for (int32 Index = 0; Index < Value.Cells.Num(); ++Index)
    {
        const FVoxelPrefabCell& Cell = Value.Cells[Index];
        uint16 PaletteIndex = MAX_uint16;
        if (!Cell.bClear)
        {
            if (Cell.Item.Count != 1 || Cell.Item.State < 0 ||
                Cell.Item.State > MAX_uint16)
            {
                Error = TEXT("Prefab voxel state or count cannot be packed");
                return false;
            }
            const uint16* Existing = PaletteIndices.Find(Cell.Item.VoxelAssetID);
            if (Existing)
            {
                PaletteIndex = *Existing;
            }
            else
            {
                if (!Cell.Item.VoxelAssetID.IsValid() || Palette.Num() >= MAX_uint16)
                {
                    Error = TEXT("Prefab voxel palette is invalid or exhausted");
                    return false;
                }
                PaletteIndex = static_cast<uint16>(Palette.Add(Cell.Item.VoxelAssetID));
                PaletteIndices.Add(Cell.Item.VoxelAssetID, PaletteIndex);
            }
        }
        uint8* Target = Encoded.GetData() + 4 + Index * 16;
        WriteU32(Target, static_cast<uint32>(Cell.Offset.X));
        WriteU32(Target + 4, static_cast<uint32>(Cell.Offset.Y));
        WriteU32(Target + 8, static_cast<uint32>(Cell.Offset.Z));
        WriteU16(Target + 12, PaletteIndex);
        WriteU16(Target + 14, Cell.bClear ? 0 : static_cast<uint16>(Cell.Item.State));
    }
    PackedPalette = MoveTemp(Palette);
    PackedCells = MoveTemp(Encoded);
    Data.Cells.Reset();
    return true;
}

bool UVoxelPrefabData::DecodeCells(FVoxelPrefabSaveData& OutData, FString& Error) const
{
    if (PackedCells.IsEmpty())
    {
        OutData = Data;
        Error.Reset();
        return true;
    }
    if (PackedCells.Num() < 4)
    {
        Error = TEXT("Packed prefab header is truncated");
        return false;
    }
    const uint32 Count = ReadU32(PackedCells.GetData());
    if (Count == 0 || 4ull + static_cast<uint64>(Count) * 16ull !=
        static_cast<uint64>(PackedCells.Num()))
    {
        Error = TEXT("Packed prefab cell length is invalid");
        return false;
    }
    FVoxelPrefabSaveData Result;
    Result.Cells.SetNum(static_cast<int32>(Count));
    for (uint32 Index = 0; Index < Count; ++Index)
    {
        const uint8* Source = PackedCells.GetData() + 4 + Index * 16;
        FVoxelPrefabCell& Cell = Result.Cells[static_cast<int32>(Index)];
        Cell.Offset = FIntVector(static_cast<int32>(ReadU32(Source)),
            static_cast<int32>(ReadU32(Source + 4)),
            static_cast<int32>(ReadU32(Source + 8)));
        const uint16 PaletteIndex = ReadU16(Source + 12);
        const uint16 State = ReadU16(Source + 14);
        Cell.bClear = PaletteIndex == MAX_uint16;
        if (Cell.bClear)
        {
            if (State != 0)
            {
                Error = TEXT("Packed prefab clear cell has nonzero state");
                return false;
            }
        }
        else
        {
            if (!PackedPalette.IsValidIndex(PaletteIndex) ||
                !PackedPalette[PaletteIndex].IsValid())
            {
                Error = TEXT("Packed prefab palette index is invalid");
                return false;
            }
            Cell.Item = FVoxelItem(PackedPalette[PaletteIndex], State, 1);
        }
    }
    OutData = MoveTemp(Result);
    Error.Reset();
    return true;
}

bool UVoxelPrefabData::ValidateCells(const FVoxelPrefabSaveData& Value,
    const FVoxelRegistrySnapshot& Registry, FString& Error)
{
    Error.Reset();
    TMap<FIntVector, FVoxelBlockState> Blocks;
    Blocks.Reserve(Value.Cells.Num());
    for (const FVoxelPrefabCell& Cell : Value.Cells)
    {
        FVoxelBlockState State;
        if (!VoxelCoord::IsValid(Cell.Offset) ||
            (!Cell.bClear && (Cell.Item.Count != 1 ||
                !FVoxelItemBridge::ToBlock(Registry, Cell.Item, State) || State.IsAir())) ||
            Blocks.Contains(Cell.Offset))
        {
            Error = FString::Printf(TEXT("Invalid, duplicate, or non-unit prefab cell at %s"),
                *Cell.Offset.ToString());
            return false;
        }
        Blocks.Add(Cell.Offset, Cell.bClear ? FVoxelBlockState() : State);
    }
    for (const auto& Pair : Blocks)
    {
        if (Pair.Value.IsAir())
        {
            continue;
        }
        const FVoxelRuntimeDefinition* Definition = Registry.Find(Pair.Value.TypeId);
        if (!Definition)
        {
            Error = TEXT("Prefab references an unregistered block");
            return false;
        }
        if (Definition->Shape != EVoxelShapeKind::Door)
        {
            continue;
        }
        const bool bUpper = (Pair.Value.State & VoxelState::HalfMask) != 0;
        const FIntVector OtherPosition = Pair.Key + FIntVector(0, 0, bUpper ? -1 : 1);
        const FVoxelBlockState* Other = Blocks.Find(OtherPosition);
        const uint16 OtherState = Pair.Value.State ^ VoxelState::HalfMask;
        if (!Other || Other->TypeId != Pair.Value.TypeId || Other->State != OtherState)
        {
            Error = FString::Printf(TEXT("Door halves are missing or inconsistent at %s"),
                *Pair.Key.ToString());
            return false;
        }
    }
    // Empty means an explicitly empty asset/preview. World placement rejects an empty edit.
    return true;
}
