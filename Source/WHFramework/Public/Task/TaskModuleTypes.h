// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SaveGame/SaveGameModuleTypes.h"
#include "Scene/SceneModuleTypes.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "TaskModuleTypes.generated.h"

class UTaskBase;
class UTaskAsset;
class UTexture2D;

namespace TaskTags
{
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_Active);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_Deliverable);
}

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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskReference
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UTaskAsset> Asset;

	/** 模板或默认实例使用无效值，独立运行实例使用有效值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid InstanceID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString TaskGUID;

	bool IsValid() const { return !Asset.IsNull() && !TaskGUID.IsEmpty(); }

	bool operator==(const FTaskReference& Other) const
	{
		return Asset.ToSoftObjectPath() == Other.Asset.ToSoftObjectPath() && InstanceID == Other.InstanceID && TaskGUID == Other.TaskGUID;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskTarget
{
	GENERATED_BODY()

	/** 场景角色稳定标识，体素区块卸载并重新加载后仍可解析 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid ActorID;

	/** 场景区域稳定标识，用于生成的城镇、地牢或建筑目标 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName AreaName;

	/** 角色或生成区域卸载时使用的世界空间备用位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset = FVector::ZeroVector;

	/** 用于绑定生成场景区域的可选项目特征分类 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag FeatureTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bResolvedByFeature = false;

	bool IsValid() const { return ActorID.IsValid() || !AreaName.IsNone() || !Location.IsNearlyZero(); }
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskMarkerData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bVisible = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Color = FLinearColor(1.f, 0.75f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Priority = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Bitmask, BitmaskEnum = "/Script/WHFramework.ESceneMarkerChannel"))
	int32 Channels = static_cast<int32>(ESceneMarkerChannel::Map | ESceneMarkerChannel::MiniMap |
		ESceneMarkerChannel::Compass | ESceneMarkerChannel::World);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOnlyShowNavigationWhenTracked = true;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ObjectiveID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag TargetTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FPrimaryAssetId TargetAssetID;

	/** 可选的稳定区域、遭遇或其他项目目标标识 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TargetName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	/** 可选的运行时闭区间，零值保持 RequiredCount 不变 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FIntPoint RequiredCountRange = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOptional = false;
};

USTRUCT()
struct WHFRAMEWORK_API FTaskRuntimeSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	FParameter Data;

	UPROPERTY()
	float ExecuteRemaining = -1.f;

	UPROPERTY()
	float CompleteRemaining = -1.f;

	UPROPERTY()
	float LeaveRemaining = -1.f;

	UPROPERTY()
	float GuideRemaining = -1.f;

	UPROPERTY()
	FTaskTarget Target;

	UPROPERTY()
	TMap<FName, int32> ObjectiveRequiredCounts;

	UPROPERTY()
	TMap<FName, FName> ObjectiveTargetNames;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskAssetSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	TSoftObjectPtr<UTaskAsset> Asset;

	UPROPERTY()
	FGuid InstanceID;

	UPROPERTY()
	FGuid AgentID;

	UPROPERTY()
	FVector AgentLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskModuleSaveData : public FSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FTaskAssetSaveData> Assets;

	UPROPERTY()
	FTaskReference CurrentTask;

	UPROPERTY()
	TMap<FString, FTaskRuntimeSaveData> TaskRecords;

};
