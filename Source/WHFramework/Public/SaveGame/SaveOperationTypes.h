#pragma once

#include "CoreMinimal.h"
#include "Common/LifecycleTypes.h"

#include "SaveOperationTypes.generated.h"

UENUM(BlueprintType)
enum class ESaveResultCode : uint8
{
	None,
	Success,
	InvalidArgument,
	NotFound,
	AlreadyExists,
	Busy,
	CaptureFailed,
	SerializeFailed,
	ReadFailed,
	WriteFailed,
	CommitFailed,
	CorruptData,
	VersionMismatch,
	ModuleMissing,
	LoadFailed,
	Unsupported,
	Unknown,
	Accepted,
	CapacityBlocked,
	SlotBusy,
	ConfirmationPending,
	Cancelled
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly)
	ESaveResultCode Code = ESaveResultCode::Unknown;

	UPROPERTY(BlueprintReadOnly)
	FText Message;

	UPROPERTY(BlueprintReadOnly)
	FGuid OperationId;

	static FSaveOperationResult Success();
	static FSaveOperationResult Accepted(const FGuid& InOperationId);

	static FSaveOperationResult Failed(ESaveResultCode InCode, const FText& InMessage);

	FORCEINLINE operator bool() const
	{
		return bSuccess;
	}
};

USTRUCT()
struct WHFRAMEWORK_API FPendingSaveLoadContext
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid OperationId;

	UPROPERTY()
	FGuid SaveId;

	UPROPERTY()
	int32 Generation = 0;

	UPROPERTY()
	FName TargetMap;

	UPROPERTY()
	EPhase LoadPhase = EPhase::All;

	bool IsValid() const
	{
		return OperationId.IsValid() && SaveId.IsValid() && Generation > 0 && !TargetMap.IsNone();
	}
};
