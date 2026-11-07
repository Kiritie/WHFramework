#pragma once

#include "CoreMinimal.h"

/** Original inventory freeze identity, independent of the World module's UObject lifetime. */
class WHFRAMEWORK_API FInventoryArchiveFreeze
{
public:
	bool Release(const FGuid& InOperationId, const FGuid& InWorldEpoch, const FGuid& InStoreId, FString& OutError);
	bool IsFrozen() const { check(IsInGameThread()); return OperationId.IsValid(); }
	const FGuid& GetOperationId() const { check(IsInGameThread()); return OperationId; }
	const FGuid& GetWorldEpoch() const { check(IsInGameThread()); return WorldEpoch; }
	const FGuid& GetStoreId() const { check(IsInGameThread()); return StoreId; }
	uint64 GetSequence() const { check(IsInGameThread()); return Sequence; }
	uint32 GetCheckpointCrc() const { check(IsInGameThread()); return CheckpointCrc; }

private:
	friend class UInventoryModule;
	FGuid OperationId;
	FGuid WorldEpoch;
	FGuid StoreId;
	FGuid ReleasedOperationId;
	FGuid ReleasedWorldEpoch;
	FGuid ReleasedStoreId;
	uint64 Sequence = 0;
	uint32 CheckpointCrc = 0;
};
