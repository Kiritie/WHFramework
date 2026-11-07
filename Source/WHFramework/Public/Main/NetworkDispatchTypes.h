#pragma once

#include "CoreMinimal.h"
#include "NetworkDispatchTypes.generated.h"

UENUM(BlueprintType)
enum class ENetworkDispatchStatus : uint8
{
	None,
	InvalidContext,
	InvalidRequest,
	NotFound,
	ModuleUnavailable,
	NotAuthorized,
	Rejected,
	RouteUnavailable,
	CapacityExceeded,
	LocalDispatched,
	RpcDispatched,
	PartiallyDispatched,
	TimedOut,
	Interrupted
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FNetworkDispatchResult
{
	GENERATED_BODY()

	// RPC 已交给引擎发送，不表示对端已收到或执行。
	UPROPERTY(BlueprintReadOnly)
	ENetworkDispatchStatus Status = ENetworkDispatchStatus::None;

	UPROPERTY(BlueprintReadOnly)
	FGuid WorldEpoch;

	UPROPERTY(BlueprintReadOnly)
	bool bLocalDispatched = false;

	UPROPERTY(BlueprintReadOnly)
	int32 RemoteDispatched = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 RemoteUnavailable = 0;

	static FNetworkDispatchResult Failure(ENetworkDispatchStatus InStatus)
	{
		FNetworkDispatchResult Result;
		Result.Status = InStatus;
		return Result;
	}
};
