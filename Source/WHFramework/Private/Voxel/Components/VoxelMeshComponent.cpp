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

	ClearAllMeshSections();
	SetProcMeshSection(0, FProcMeshSection());
	FProcMeshSection& Section = *GetProcMeshSection(0);
	Section.ProcVertexBuffer.SetNumUninitialized(InMesh.Vertices.Num());
	for (int32 Index = 0; Index < InMesh.Vertices.Num(); ++Index)
	{
		FProcMeshVertex& Vertex = Section.ProcVertexBuffer[Index];
		Vertex.Position = InMesh.Vertices[Index];
		Vertex.Normal = InMesh.Normals[Index];
		Vertex.UV0 = InMesh.UV0[Index];
		Vertex.UV1 = InMesh.UV1[Index];
		Vertex.UV2 = InMesh.UV2[Index];
		Vertex.UV3 = FVector2D::ZeroVector;
		Vertex.Color = InMesh.Colors[Index].ToFColor(false);
		Vertex.Tangent = InMesh.Tangents[Index];
		Section.SectionLocalBox += Vertex.Position;
	}
	Section.ProcIndexBuffer.Reserve(InMesh.Triangles.Num());
	for (int32 Index = 0; Index < InMesh.Triangles.Num(); Index += 3)
	{
		const int32 First = InMesh.Triangles[Index];
		const int32 Second = InMesh.Triangles[Index + 1];
		const int32 Third = InMesh.Triangles[Index + 2];
		if (First == Second || First == Third || Second == Third) continue;
		Section.ProcIndexBuffer.Append({uint32(First), uint32(Second), uint32(Third)});
	}
	Section.bEnableCollision = false;
	SetProcMeshSection(0, Section);

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
