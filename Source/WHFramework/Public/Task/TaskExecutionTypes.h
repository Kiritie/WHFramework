#pragma once

#include "CoreMinimal.h"

#include "TaskExecutionTypes.generated.h"

class UTaskBase;

UENUM(BlueprintType)
enum class ETaskState : uint8
{
	None,
	Entered,
	Executing,
	Completed,
	Leaved
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskStateChanged, ETaskState, InTaskState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTaskAssetsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskAccepted, UTaskBase*, InTask);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskAvailabilityChanged, bool, bAvailable);

UENUM(BlueprintType)
enum class ETaskExecuteType : uint8
{
	None,
	Automatic,
	Procedure
};

UENUM(BlueprintType)
enum class ETaskEnterType : uint8
{
	None,
	Automatic,
	Procedure
};

UENUM(BlueprintType)
enum class ETaskLeaveType : uint8
{
	None,
	Automatic,
	Procedure
};

UENUM(BlueprintType)
enum class ETaskCompleteType : uint8
{
	None,
	Skip,
	Automatic,
	Procedure
};

UENUM(BlueprintType)
enum class ETaskExecuteResult : uint8
{
	None,
	Succeed,
	Failed,
	Skipped
};

UENUM(BlueprintType)
enum class ETaskGuideType : uint8
{
	None,
	TimerOnce,
	TimerLoop
};

UENUM(BlueprintType)
enum class ETaskTaskState : uint8
{
	None,
	Preparing,
	Executing,
	Completed
};

UENUM(BlueprintType)
enum class ETaskStage : uint8
{
	Locked,
	Available,
	Active,
	Deliverable,
	Finished,
	Failed
};
