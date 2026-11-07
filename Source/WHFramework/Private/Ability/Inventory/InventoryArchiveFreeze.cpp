#include "Ability/Inventory/InventoryArchiveFreeze.h"

bool FInventoryArchiveFreeze::Release(const FGuid& InOperationId, const FGuid& InWorldEpoch, const FGuid& InStoreId, FString& OutError)
{
	if (!IsInGameThread() || !InOperationId.IsValid() || !InWorldEpoch.IsValid() || !InStoreId.IsValid())
	{
		OutError = TEXT("Inventory.ArchiveFreezeOwnerMismatch");
		return false;
	}
	if (!IsFrozen() && ReleasedOperationId == InOperationId && ReleasedWorldEpoch == InWorldEpoch && ReleasedStoreId == InStoreId)
	{
		OutError.Reset();
		return true;
	}
	if (OperationId != InOperationId || WorldEpoch != InWorldEpoch || StoreId != InStoreId)
	{
		OutError = TEXT("Inventory.ArchiveFreezeOwnerMismatch");
		return false;
	}
	ReleasedOperationId = OperationId;
	ReleasedWorldEpoch = WorldEpoch;
	ReleasedStoreId = StoreId;
	OperationId.Invalidate();
	WorldEpoch.Invalidate();
	StoreId.Invalidate();
	Sequence = 0;
	CheckpointCrc = 0;
	OutError.Reset();
	return true;
}
