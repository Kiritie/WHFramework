#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Voxel/WorldObject/VoxelWorldObjectRegistry.h"
#include "VoxelWorldObjectPresenter.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(Blueprintable)
class WHFRAMEWORK_API AVoxelWorldObjectPresenter : public AActor
{
	GENERATED_BODY()

public:
	AVoxelWorldObjectPresenter();
	virtual void Initialize(UVoxelWorldObjectDefinition* InDefinition, const FVoxelWorldObjectInstance& InObject, double InBlockSize);
	virtual void RefreshState(const FVoxelWorldObjectInstance& InObject, bool bInActive, bool bInLightVisible);
	virtual void Tick(float InDeltaSeconds) override;
	const FVoxelWorldObjectInstance& GetObject() const { return Object; }
	UVoxelWorldObjectDefinition* GetDefinition() const { return Definition; }
	bool IsLightInRange() const { return bLightInRange; }

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UVoxelWorldObjectDefinition> Definition;

	FVoxelWorldObjectInstance Object;
	bool bLightInRange = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> VisualMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> EmissiveMaterials;

	float VisualAlpha = 0.f;
	float TargetVisualAlpha = 0.f;
	bool bVisualInitialized = false;
};
