#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "TaskObjectiveTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTaskTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText SubTargetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText RegionName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bApproximate = false;

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

UENUM(BlueprintType)
enum class ETaskEventScope : uint8
{
	None,
	Personal,
	Cooperative,
	WorldFact
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
	ETaskEventScope CreditScope = ETaskEventScope::None;

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
