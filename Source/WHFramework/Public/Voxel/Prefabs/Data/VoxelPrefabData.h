#pragma once
#include "Asset/Primary/PrimaryAssetBase.h"

#include "VoxelPrefabData.generated.h"

/** 体素预制体数据 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelPrefabData : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UVoxelPrefabData();

public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnReset_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
	FString VoxelDatas;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector VoxelSize;
};
