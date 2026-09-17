#include "Voxel/Components/VoxelMeshComponent.h"

UVoxelMeshComponent::UVoxelMeshComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	bUseAsyncCooking = false;
}
FBoxSphereBounds UVoxelMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds CalculatedBounds = Super::CalcBounds(LocalToWorld);
	CalculatedBounds.BoxExtent += FVector(4.0);
	CalculatedBounds.SphereRadius = CalculatedBounds.BoxExtent.Size();
	return CalculatedBounds;
}
bool UVoxelMeshComponent::Apply(const FVoxelMeshBuffers& B, double S, UMaterialInterface* M)
{
	check(IsInGameThread());
	if (!M || !FMath::IsFinite(S) || S <= 0 || !B.Validate())
		return false;
	TArray<FVector> P = B.Vertices;
	for (auto& V : P)
		V *= S;
	static const TArray<FVector2D> Empty;
	ClearAllMeshSections();
	CreateMeshSection_LinearColor(0, P, B.Triangles, B.Normals, B.UV0, B.UV1, B.UV2, Empty, B.Colors, B.Tangents, false, false);
	SetMaterial(0, M);
	return true;
}
