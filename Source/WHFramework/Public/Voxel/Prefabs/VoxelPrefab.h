#pragma once
#include "Common/Base/WHActor.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelPrefab.generated.h"

class UVoxelPrefabData;
class UVoxelMeshComponent;

/** Display-only prefab host. World edits must use UVoxelModule::ApplyPrefab. */
UCLASS()
class WHFRAMEWORK_API AVoxelPrefab : public AWHActor
{
	GENERATED_BODY()
public:
	AVoxelPrefab();
	bool SetPreviewAsset(UVoxelPrefabData* Asset, double BlockSize, FString& Error);
	bool SetPreviewData(const FVoxelPrefabSaveData& Data, double BlockSize, FString& Error);
	void ClearPreview();
	virtual void OnDespawn_Implementation(EObjectDespawnMode Mode) override;

protected:
	virtual void LoadData(const FParameter& Value, EPhase Phase) override;
	virtual FParameter ToData() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(Transient)
	TObjectPtr<UVoxelPrefabData> PreviewAsset;
	UPROPERTY(Transient)
	FVoxelPrefabSaveData PreviewData;
	UPROPERTY(Transient)
	double PreviewBlockSize = 100.0;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVoxelMeshComponent>> PreviewMeshes;
};
