#include "Voxel/Rendering/VoxelFinePage.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Streaming/VoxelInterest.h"

uint8 FVoxelFinePagePlanner::ResolveBoundsLevel(const FVoxelGenerationBounds& InBounds,
	TConstArrayView<FVoxelStreamingSource> InSources, const FVoxelViewSettings& InSettings)
{
	uint8 Result = InSettings.Lod.MaximumFineLevel;
	bool bHasInfluence = false;
	for (const FVoxelStreamingSource& Source : InSources)
	{
		const int32 InitialSide = Source.View.InitialChunkSideCells;
		if (!Source.Has(EVoxelStreamingCapability::FineVisual) || Source.View.FineRadiusCells <= 0)
		{
			continue;
		}
		FVector AlignedMin;
		FVector AlignedMax;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			AlignedMin[Axis] = double(VoxelStreamingGeometry::FloorDivide(InBounds.Min[Axis], InitialSide) * InitialSide);
			AlignedMax[Axis] = double((VoxelStreamingGeometry::FloorDivide(int64(InBounds.Max[Axis]) - 1, InitialSide) + 1) * InitialSide);
		}
		if (AlignedMax.Z <= double(Source.Center.Z) - Source.View.FineVerticalHalfExtentCells ||
			AlignedMin.Z >= double(Source.Center.Z) + Source.View.FineVerticalHalfExtentCells)
		{
			continue;
		}
		const double InfluenceX = FMath::Clamp(double(Source.Center.X), AlignedMin.X, AlignedMax.X);
		const double InfluenceY = FMath::Clamp(double(Source.Center.Y), AlignedMin.Y, AlignedMax.Y);
		const double InfluenceDistance = FVector2D::Distance(FVector2D(Source.Center.X, Source.Center.Y),
			FVector2D(InfluenceX, InfluenceY));
		if (InfluenceDistance > Source.View.FineRadiusCells)
		{
			continue;
		}
		bHasInfluence = true;
		const double NearX = FMath::Clamp(double(Source.Center.X), double(InBounds.Min.X), double(InBounds.Max.X));
		const double NearY = FMath::Clamp(double(Source.Center.Y), double(InBounds.Min.Y), double(InBounds.Max.Y));
		const double Distance = FVector2D::Distance(FVector2D(Source.Center.X, Source.Center.Y), FVector2D(NearX, NearY));
		const uint8 InitialLevel = static_cast<uint8>(FMath::FloorLog2(uint32(Source.View.InitialChunkSideCells / 16)));
		const double FirstRadius = FMath::Max(16, Source.View.FineLevel0RadiusCells);
		const int32 Growth = Distance < FirstRadius ? 0 : 1 + FMath::FloorToInt(FMath::Log2(Distance / FirstRadius));
		const uint8 Desired = static_cast<uint8>(FMath::Min<int32>(InSettings.Lod.MaximumFineLevel, InitialLevel + Growth));
		Result = FMath::Min(Result, Desired);
	}
	return bHasInfluence ? Result : 0;
}

uint8 FVoxelFinePagePlanner::ResolveDesiredLevel(const FIntVector& InSection,
	TConstArrayView<FVoxelStreamingSource> InSources, const FVoxelViewSettings& InSettings)
{
	const FIntVector Min = InSection * 16;
	return ResolveBoundsLevel({Min, Min + FIntVector(16)}, InSources, InSettings);
}

bool FVoxelFinePagePlanner::Build(const TSet<FIntVector>& InSections,
	TConstArrayView<FVoxelStreamingSource> InSources, const FVoxelViewSettings& InSettings,
	FVoxelFinePagePlan& OutPlan, FString& OutError)
{
	for (const FVoxelStreamingSource& Source : InSources)
	{
		if (!Source.Has(EVoxelStreamingCapability::FineVisual))
		{
			continue;
		}
		if (!VoxelStreamingGeometry::ValidateInitialSide(Source.View.InitialChunkSideCells,
			InSettings.Lod.MaximumFineLevel, OutError))
		{
			return false;
		}
	}
	TMap<FVoxelFinePageKey, uint8> Leaves;
	for (const FIntVector& Section : InSections)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (int64(Section[Axis]) * 16 < MIN_int32 || (int64(Section[Axis]) + 1) * 16 > MAX_int32)
			{
				OutError = TEXT("Fine section bounds exceed the supported cell coordinate range");
				return false;
			}
		}
		Leaves.Add({Section, 0}, ResolveDesiredLevel(Section, InSources, InSettings));
	}
	for (uint8 Level = 1; Level <= InSettings.Lod.MaximumFineLevel; ++Level)
	{
		TSet<FVoxelFinePageKey> Parents;
		for (const TPair<FVoxelFinePageKey, uint8>& Pair : Leaves)
		{
			if (Pair.Key.Level != Level - 1 || Pair.Value < Level)
			{
				continue;
			}
			Parents.Add({FIntVector(VoxelGeneration::FloorDivide(Pair.Key.Coordinate.X, 2),
				VoxelGeneration::FloorDivide(Pair.Key.Coordinate.Y, 2),
				VoxelGeneration::FloorDivide(Pair.Key.Coordinate.Z, 2)), Level});
		}
		for (const FVoxelFinePageKey& Parent : Parents)
		{
			FVoxelTerrainSpatialKey Spatial{EVoxelTerrainRepresentation::Fine, Parent.Coordinate, Parent.Level};
			FIntVector Min;
			FIntVector Max;
			if (!Spatial.TryBounds(0, 16, Min, Max) || ResolveBoundsLevel({Min, Max}, InSources, InSettings) < Level)
			{
				continue;
			}
			TArray<FVoxelFinePageKey, TInlineAllocator<8>> Children;
			uint8 Desired = InSettings.Lod.MaximumFineLevel;
			for (int32 ChildIndex = 0; ChildIndex < 8; ++ChildIndex)
			{
				const FVoxelFinePageKey Child{Parent.Coordinate * 2 +
					FIntVector(ChildIndex & 1, (ChildIndex >> 1) & 1, (ChildIndex >> 2) & 1), uint8(Level - 1)};
				const uint8* ChildDesired = Leaves.Find(Child);
				if (!ChildDesired || *ChildDesired < Level)
				{
					break;
				}
				Children.Add(Child);
				Desired = FMath::Min(Desired, *ChildDesired);
			}
			if (Children.Num() != 8)
			{
				continue;
			}
			for (const FVoxelFinePageKey& Child : Children)
			{
				Leaves.Remove(Child);
			}
			Leaves.Add(Parent, Desired);
		}
	}
	FVoxelFinePagePlan Candidate;
	TArray<FIntVector> Sections;
	for (const TPair<FVoxelFinePageKey, uint8>& Pair : Leaves)
	{
		Candidate.Pages.Add(Pair.Key);
		GatherSections(Pair.Key, Sections);
		for (const FIntVector& Section : Sections)
		{
			if (!InSections.Contains(Section) || Candidate.SectionOwners.Contains(Section))
			{
				OutError = TEXT("Fine page ownership must be complete, disjoint and limited to requested sections");
				return false;
			}
			Candidate.SectionOwners.Add(Section, Pair.Key);
		}
	}
	if (Candidate.SectionOwners.Num() != InSections.Num())
	{
		OutError = TEXT("Fine page plan lost requested sections");
		return false;
	}
	OutPlan = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

void FVoxelFinePagePlanner::GatherSections(const FVoxelFinePageKey& InPage, TArray<FIntVector>& OutSections)
{
	check(InPage.Level <= 4);
	const int32 Side = 1 << InPage.Level;
	const FIntVector Min = InPage.Coordinate * Side;
	OutSections.Reset();
	OutSections.Reserve(Side * Side * Side);
	for (int32 Z = 0; Z < Side; ++Z)
	{
		for (int32 Y = 0; Y < Side; ++Y)
		{
			for (int32 X = 0; X < Side; ++X)
			{
				OutSections.Add(Min + FIntVector(X, Y, Z));
			}
		}
	}
}

void FVoxelFinePagePlanner::AppendMesh(const FVoxelSectionMeshResult& InMesh,
	const FVector& InOffsetCells, FVoxelSectionMeshResult& OutMesh)
{
	for (const FVoxelRenderBatch& Source : InMesh.Batches)
	{
		FVoxelRenderBatch* Target = OutMesh.Batches.FindByPredicate([&Source](const FVoxelRenderBatch& Batch)
		{
			return Batch.Group == Source.Group && Batch.Bank == Source.Bank &&
				Batch.MaterialVariant == Source.MaterialVariant;
		});
		if (!Target)
		{
			Target = &OutMesh.Batches.AddDefaulted_GetRef();
			Target->Group = Source.Group;
			Target->Bank = Source.Bank;
			Target->MaterialVariant = Source.MaterialVariant;
		}
		const int32 Base = Target->Mesh.Vertices.Num();
		for (const FVector& Vertex : Source.Mesh.Vertices)
		{
			Target->Mesh.Vertices.Add(Vertex + InOffsetCells);
		}
		for (const int32 Index : Source.Mesh.Triangles)
		{
			Target->Mesh.Triangles.Add(Base + Index);
		}
		Target->Mesh.Normals.Append(Source.Mesh.Normals);
		Target->Mesh.Tangents.Append(Source.Mesh.Tangents);
		Target->Mesh.UV0.Append(Source.Mesh.UV0);
		Target->Mesh.UV1.Append(Source.Mesh.UV1);
		Target->Mesh.UV2.Append(Source.Mesh.UV2);
		Target->Mesh.Colors.Append(Source.Mesh.Colors);
	}
}
