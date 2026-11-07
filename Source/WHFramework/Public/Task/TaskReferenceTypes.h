#pragma once

#include "CoreMinimal.h"

#include "TaskReferenceTypes.generated.h"

class UTaskAsset;

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
	FGuid OwnerActorID;

	UPROPERTY()
	FName InstanceKey;

	UPROPERTY()
	FVector AgentLocation = FVector::ZeroVector;
};
