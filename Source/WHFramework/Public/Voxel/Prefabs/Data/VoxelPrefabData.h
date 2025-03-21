#pragma once
#include "Asset/Primary/PrimaryAssetBase.h"

#include "VoxelPrefabData.generated.h"

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
	FString VoxelDatas;
};
