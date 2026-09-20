#include "Voxel/Components/VoxelMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

UVoxelMeshComponent::UVoxelMeshComponent(
	const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer)
{
	SetCollisionEnabled(
		ECollisionEnabled::NoCollision);

	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);

	bUseAsyncCooking = false;
}

FBoxSphereBounds UVoxelMeshComponent::CalcBounds(
	const FTransform& InLocalToWorld) const
{
	FBoxSphereBounds CalculatedBounds =
		Super::CalcBounds(
			InLocalToWorld);

	CalculatedBounds.BoxExtent +=
		FVector(4.0);

	CalculatedBounds.SphereRadius =
		CalculatedBounds.
			BoxExtent.Size();

	return CalculatedBounds;
}

bool UVoxelMeshComponent::Apply(
	const FVoxelMeshBuffers& InMesh,
	const double InBlockSize,
	UMaterialInterface* InMaterial)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_MeshApplyGT);
	check(IsInGameThread());
	const double StartTime = FPlatformTime::Seconds();

	if (!InMaterial ||
		!FMath::IsFinite(
			InBlockSize) ||
		InBlockSize <= 0.0 ||
		!InMesh.Validate())
	{
		return false;
	}

	/*
	 * 顶点保持 Voxel Local Unit。
	 * 不再在 GameThread 复制一份 FVector 数组逐点乘 BlockSize。
	 */
	SetRelativeScale3D(
		FVector(
			InBlockSize));

	static const TArray<FVector2D>
		EmptyUV3;

	ClearAllMeshSections();

	CreateMeshSection_LinearColor(
		0,
		InMesh.Vertices,
		InMesh.Triangles,
		InMesh.Normals,
		InMesh.UV0,
		InMesh.UV1,
		InMesh.UV2,
		EmptyUV3,
		InMesh.Colors,
		InMesh.Tangents,
		false,
		false);

	SetMaterial(
		0,
		InMaterial);

#if !UE_BUILD_SHIPPING
	const double Milliseconds =
		(FPlatformTime::Seconds() - StartTime) * 1000.0;
	if (Milliseconds >= 5.0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Voxel mesh GT apply %.2fms; vertices=%d triangles=%d"),
			Milliseconds,
			InMesh.Vertices.Num(),
			InMesh.Triangles.Num() / 3);
	}
#endif

	return true;
}
