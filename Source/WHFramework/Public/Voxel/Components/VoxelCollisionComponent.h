#pragma once
#include "Components/PrimitiveComponent.h"
#include "VoxelCollisionComponent.generated.h"
class UBodySetup;
UCLASS()
class WHFRAMEWORK_API UVoxelCollisionComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	explicit UVoxelCollisionComponent(const FObjectInitializer& ObjectInitializer);
	bool Apply(const TArray<FBox>& Boxes, double BlockSize);
	virtual UBodySetup* GetBodySetup() override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBodySetup> Setup;
	FBox LocalBounds = FBox(FVector::ZeroVector, FVector::ZeroVector);
};
