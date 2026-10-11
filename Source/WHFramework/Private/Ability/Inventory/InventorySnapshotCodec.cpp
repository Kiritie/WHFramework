#include "Ability/Inventory/InventorySnapshotCodec.h"

#include "Ability/Inventory/InventoryBinaryArchive.h"
#include "Misc/Crc.h"

namespace InventorySnapshotFormat
{
    constexpr uint32 Magic = 0x32564e49;

    void WriteAsset(FInventoryBinaryWriter& InWriter, const FPrimaryAssetId& InId)
    {
        InWriter.Name(InId.PrimaryAssetType.GetName());
        InWriter.Name(InId.PrimaryAssetName);
    }

    FPrimaryAssetId ReadAsset(FInventoryBinaryReader& InReader)
    {
        const FName Type = InReader.Name();
        const FName Name = InReader.Name();
        return FPrimaryAssetId(FPrimaryAssetType(Type), Name);
    }

    void WriteItem(FInventoryBinaryWriter& InWriter, const FAbilityItem& InItem)
    {
        InWriter.U8(InItem.IsEmpty() ? 0 : 1);
        if(InItem.IsEmpty())
        {
            return;
        }
        WriteAsset(InWriter, InItem.ItemId);
        InWriter.U32(static_cast<uint32>(InItem.Count));
        InWriter.U32(static_cast<uint32>(InItem.Level));
        InWriter.Guid(InItem.InstanceId);
        InWriter.U8(static_cast<uint8>(InItem.Properties.Num()));
        for(const FInventoryItemProperty& Property : InItem.Properties)
        {
            InWriter.Name(Property.Key);
            InWriter.U32(static_cast<uint32>(Property.SchemaVersion));
            InWriter.U8(static_cast<uint8>(Property.Type));
            switch(Property.Type)
            {
                case EInventoryPropertyType::Integer:
                    InWriter.U64(static_cast<uint64>(Property.IntValue));
                    break;
                case EInventoryPropertyType::Boolean:
                    InWriter.U8(Property.BoolValue ? 1 : 0);
                    break;
                case EInventoryPropertyType::Guid:
                    InWriter.Guid(Property.GuidValue);
                    break;
                case EInventoryPropertyType::Name:
                    InWriter.Name(Property.NameValue);
                    break;
                default:
                    break;
            }
        }
    }

    FAbilityItem ReadItem(FInventoryBinaryReader& InReader)
    {
        FAbilityItem Item;
        const uint8 Present = InReader.U8();
        if(Present == 0)
        {
            return Item;
        }
        if(Present != 1)
        {
            InReader.Reject();
            return Item;
        }
        Item.ItemId = ReadAsset(InReader);
        Item.Count = static_cast<int32>(InReader.U32());
        Item.Level = static_cast<int32>(InReader.U32());
        Item.InstanceId = InReader.Guid();
        const uint8 PropertyCount = InReader.U8();
        if(PropertyCount > FInventorySnapshotCodec::MaximumProperties)
        {
            InReader.Reject();
            return Item;
        }
        for(uint8 PropertyIndex = 0; PropertyIndex < PropertyCount && InReader.IsValid(); ++PropertyIndex)
        {
            FInventoryItemProperty Property;
            Property.Key = InReader.Name();
            Property.SchemaVersion = static_cast<int32>(InReader.U32());
            Property.Type = static_cast<EInventoryPropertyType>(InReader.U8());
            switch(Property.Type)
            {
                case EInventoryPropertyType::Integer:
                    Property.IntValue = static_cast<int64>(InReader.U64());
                    break;
                case EInventoryPropertyType::Boolean:
                {
                    const uint8 Value = InReader.U8();
                    if(Value > 1)
                    {
                        InReader.Reject();
                    }
                    Property.BoolValue = Value == 1;
                    break;
                }
                case EInventoryPropertyType::Guid:
                    Property.GuidValue = InReader.Guid();
                    break;
                case EInventoryPropertyType::Name:
                    Property.NameValue = InReader.Name();
                    break;
                default:
                    InReader.Reject();
                    break;
            }
            Item.Properties.Add(MoveTemp(Property));
        }
        return Item;
    }
}

bool FInventorySnapshotCodec::InspectIdentity(TConstArrayView<uint8> InBytes, FInventorySnapshotIdentity& OutIdentity, FString& OutError)
{
    if(InBytes.Num() < 12 || InBytes.Num() > MaximumBytes)
    {
        OutError = TEXT("Inventory.SnapshotSizeLimit");
        return false;
    }
    FInventoryBinaryReader Checksum(InBytes.Slice(InBytes.Num() - 4, 4));
    if(Checksum.U32() != FCrc::MemCrc32(InBytes.GetData(), InBytes.Num() - 4))
    {
        OutError = TEXT("Inventory.SnapshotChecksum");
        return false;
    }
    FInventoryBinaryReader Reader(InBytes.Slice(0, InBytes.Num() - 4));
    if(Reader.U32() != InventorySnapshotFormat::Magic || Reader.U32() != 2)
    {
        OutError = TEXT("Inventory.UnsupportedSnapshotSchema");
        return false;
    }
    FInventorySnapshotIdentity Identity;
    Identity.WorldId = Reader.Guid();
    Identity.InventoryId = Reader.Guid();
    Reader.Guid();
    const FPrimaryAssetId ProfileId = InventorySnapshotFormat::ReadAsset(Reader);
    Identity.Generation = Reader.U32();
    Identity.Revision = Reader.U64();
    const uint32 SlotCount = Reader.U32();
    if(!Reader.IsValid() || !Identity.WorldId.IsValid() || !Identity.InventoryId.IsValid() ||
       !ProfileId.IsValid() || Identity.Generation == 0 || SlotCount > MaximumSlots)
    {
        OutError = TEXT("Inventory.SnapshotIdentityMalformed");
        return false;
    }
    OutIdentity = Identity;
    return true;
}

bool FInventorySnapshotCodec::Encode(const FInventorySnapshot& InSnapshot, const FInventoryItemRegistry& InRegistry,
    const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& InProfiles, TArray<uint8>& OutBytes, FString& OutError)
{
    OutBytes.Reset();
    const FInventoryProfileDefinition* Profile = InProfiles.Find(InSnapshot.ProfileId);
    if(!Profile || InSnapshot.SchemaVersion != 2 || InSnapshot.Slots.Num() > MaximumSlots || !Profile->ValidateState(InSnapshot, InRegistry, OutError))
    {
        if(OutError.IsEmpty())
        {
            OutError = TEXT("Inventory.SnapshotInvalid");
        }
        return false;
    }
    FInventoryBinaryWriter Writer(MaximumBytes);
    Writer.U32(InventorySnapshotFormat::Magic);
    Writer.U32(2);
    Writer.Guid(InSnapshot.WorldId);
    Writer.Guid(InSnapshot.InventoryId);
    Writer.Guid(InSnapshot.OwnerId);
    InventorySnapshotFormat::WriteAsset(Writer, InSnapshot.ProfileId);
    Writer.U32(InSnapshot.Generation);
    Writer.U64(InSnapshot.Revision);
    Writer.U32(static_cast<uint32>(InSnapshot.Slots.Num()));
    for(const FInventorySlotState& Slot : InSnapshot.Slots)
    {
        if(Slot.Item.Properties.Num() > MaximumProperties)
        {
            OutError = TEXT("Inventory.TooManyProperties");
            return false;
        }
        Writer.Name(Slot.Key.Group.GetTagName());
        Writer.U32(static_cast<uint32>(Slot.Key.Index));
        Writer.U8((Slot.bFavorite ? 1 : 0) | (Slot.bLocked ? 2 : 0));
        InventorySnapshotFormat::WriteItem(Writer, Slot.Item);
    }
    const uint32 Checksum = FCrc::MemCrc32(Writer.GetBytes().GetData(), Writer.GetBytes().Num());
    Writer.U32(Checksum);
    if(!Writer.IsValid())
    {
        OutError = TEXT("Inventory.SnapshotSizeLimit");
        return false;
    }
    OutBytes = Writer.TakeBytes();
    return true;
}

bool FInventorySnapshotCodec::Decode(TConstArrayView<uint8> InBytes, const FInventoryItemRegistry& InRegistry,
    const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& InProfiles, FInventorySnapshot& OutSnapshot, FString& OutError)
{
    if(InBytes.Num() < 12 || InBytes.Num() > MaximumBytes)
    {
        OutError = TEXT("Inventory.SnapshotSizeLimit");
        return false;
    }
    FInventoryBinaryReader ChecksumReader(InBytes.Slice(InBytes.Num() - 4, 4));
    if(ChecksumReader.U32() != FCrc::MemCrc32(InBytes.GetData(), InBytes.Num() - 4))
    {
        OutError = TEXT("Inventory.SnapshotChecksum");
        return false;
    }
    FInventoryBinaryReader Reader(InBytes.Slice(0, InBytes.Num() - 4));
    if(Reader.U32() != InventorySnapshotFormat::Magic || Reader.U32() != 2)
    {
        OutError = TEXT("Inventory.UnsupportedSnapshotSchema");
        return false;
    }
    FInventorySnapshot Snapshot;
    Snapshot.WorldId = Reader.Guid();
    Snapshot.InventoryId = Reader.Guid();
    Snapshot.OwnerId = Reader.Guid();
    Snapshot.ProfileId = InventorySnapshotFormat::ReadAsset(Reader);
    Snapshot.Generation = Reader.U32();
    Snapshot.Revision = Reader.U64();
    const uint32 SlotCount = Reader.U32();
    const FInventoryProfileDefinition* Profile = InProfiles.Find(Snapshot.ProfileId);
    if(!Reader.IsValid() || !Profile || SlotCount > MaximumSlots || SlotCount != static_cast<uint32>(Profile->Slots.Num()))
    {
        OutError = TEXT("Inventory.SnapshotProfileMismatch");
        return false;
    }
    Snapshot.Slots.Reserve(static_cast<int32>(SlotCount));
    for(uint32 SlotIndex = 0; SlotIndex < SlotCount && Reader.IsValid(); ++SlotIndex)
    {
        FInventorySlotState Slot;
        Slot.Key.Group = FGameplayTag::RequestGameplayTag(Reader.Name(), false);
        Slot.Key.Index = static_cast<int32>(Reader.U32());
        const uint8 Flags = Reader.U8();
        if((Flags & ~3) != 0)
        {
            Reader.Reject();
        }
        Slot.bFavorite = (Flags & 1) != 0;
        Slot.bLocked = (Flags & 2) != 0;
        Slot.Item = InventorySnapshotFormat::ReadItem(Reader);
        Snapshot.Slots.Add(MoveTemp(Slot));
    }
    if(!Reader.AtEnd() || !Profile->ValidateState(Snapshot, InRegistry, OutError))
    {
        if(OutError.IsEmpty())
        {
            OutError = TEXT("Inventory.SnapshotMalformed");
        }
        return false;
    }
    OutSnapshot = MoveTemp(Snapshot);
    return true;
}

bool FInventorySnapshotCodec::EncodeItems(TConstArrayView<FAbilityItem> InItems, const FInventoryItemRegistry& InRegistry,
    TArray<uint8>& OutBytes, FString& OutError)
{
    OutBytes.Reset();
    if(InItems.Num() > MaximumSlots)
    {
        OutError = TEXT("Inventory.ItemListSizeLimit");
        return false;
    }
    FInventoryBinaryWriter Writer(MaximumBytes);
    Writer.U32(0x32544949);
    Writer.U32(2);
    Writer.U32(static_cast<uint32>(InItems.Num()));
    TSet<FGuid> UniqueInstances;
    for(const FAbilityItem& Item : InItems)
    {
        if(Item.Properties.Num() > MaximumProperties || !InRegistry.ValidateItem(Item, OutError))
        {
            return false;
        }
        if(Item.InstanceId.IsValid())
        {
            if(UniqueInstances.Contains(Item.InstanceId))
            {
                OutError = TEXT("Inventory.DuplicateInstance");
                return false;
            }
            UniqueInstances.Add(Item.InstanceId);
        }
        InventorySnapshotFormat::WriteItem(Writer, Item);
    }
    const uint32 Checksum = FCrc::MemCrc32(Writer.GetBytes().GetData(), Writer.GetBytes().Num());
    Writer.U32(Checksum);
    if(!Writer.IsValid())
    {
        OutError = TEXT("Inventory.ItemListSizeLimit");
        return false;
    }
    OutBytes = Writer.TakeBytes();
    return true;
}

bool FInventorySnapshotCodec::DecodeItems(TConstArrayView<uint8> InBytes, const FInventoryItemRegistry& InRegistry,
    TArray<FAbilityItem>& OutItems, FString& OutError)
{
    if(InBytes.Num() < 16 || InBytes.Num() > MaximumBytes)
    {
        OutError = TEXT("Inventory.ItemListSizeLimit");
        return false;
    }
    FInventoryBinaryReader Checksum(InBytes.Slice(InBytes.Num() - 4, 4));
    if(Checksum.U32() != FCrc::MemCrc32(InBytes.GetData(), InBytes.Num() - 4))
    {
        OutError = TEXT("Inventory.ItemListChecksum");
        return false;
    }
    FInventoryBinaryReader Reader(InBytes.Slice(0, InBytes.Num() - 4));
    if(Reader.U32() != 0x32544949 || Reader.U32() != 2)
    {
        OutError = TEXT("Inventory.UnsupportedItemListSchema");
        return false;
    }
    const uint32 Count = Reader.U32();
    if(!Reader.IsValid() || Count > MaximumSlots)
    {
        OutError = TEXT("Inventory.ItemListSizeLimit");
        return false;
    }
    TArray<FAbilityItem> Items;
    TSet<FGuid> UniqueInstances;
    for(uint32 ItemIndex = 0; ItemIndex < Count && Reader.IsValid(); ++ItemIndex)
    {
        FAbilityItem Item = InventorySnapshotFormat::ReadItem(Reader);
        if(!Reader.IsValid() || !InRegistry.ValidateItem(Item, OutError))
        {
            if(OutError.IsEmpty())
            {
                OutError = TEXT("Inventory.ItemListMalformed");
            }
            return false;
        }
        if(Item.InstanceId.IsValid())
        {
            if(UniqueInstances.Contains(Item.InstanceId))
            {
                OutError = TEXT("Inventory.DuplicateInstance");
                return false;
            }
            UniqueInstances.Add(Item.InstanceId);
        }
        Items.Add(MoveTemp(Item));
    }
    if(!Reader.AtEnd())
    {
        OutError = TEXT("Inventory.ItemListMalformed");
        return false;
    }
    OutItems = MoveTemp(Items);
    return true;
}
