#pragma once

#include "CoreMinimal.h"
#include "Ability/Inventory/InventoryProfile.h"
#include "Ability/Inventory/InventoryItemRegistry.h"

struct WHFRAMEWORK_API FInventorySnapshotIdentity
{
    FGuid WorldId;
    FGuid InventoryId;
    uint32 Generation = 0;
    uint64 Revision = 0;
};

class WHFRAMEWORK_API FInventorySnapshotCodec
{
public:
    static constexpr int32 MaximumBytes = 1024 * 1024;
    static constexpr int32 MaximumSlots = 4096;
    static constexpr int32 MaximumProperties = 16;

    static bool InspectIdentity(TConstArrayView<uint8> InBytes, FInventorySnapshotIdentity& OutIdentity, FString& OutError);

    static bool Encode(const FInventorySnapshot& InSnapshot, const FInventoryItemRegistry& InRegistry,
        const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& InProfiles, TArray<uint8>& OutBytes, FString& OutError);
    static bool Decode(TConstArrayView<uint8> InBytes, const FInventoryItemRegistry& InRegistry,
        const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& InProfiles, FInventorySnapshot& OutSnapshot, FString& OutError);
    static bool EncodeItems(TConstArrayView<FAbilityItem> InItems, const FInventoryItemRegistry& InRegistry,
        TArray<uint8>& OutBytes, FString& OutError);
    static bool DecodeItems(TConstArrayView<uint8> InBytes, const FInventoryItemRegistry& InRegistry,
        TArray<FAbilityItem>& OutItems, FString& OutError);
};
