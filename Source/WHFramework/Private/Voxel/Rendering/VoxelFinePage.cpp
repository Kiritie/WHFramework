#include "Voxel/Rendering/VoxelFinePage.h"

#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

uint8 FVoxelFinePagePlanner::ResolveDesiredLevel(
	const FIntVector& InSection,
	TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelViewSettings& InSettings)
{
	constexpr int32 SectionSide = 16;

	const FVector SectionCenter =
		FVector(InSection * SectionSide) +
		FVector(SectionSide * 0.5);

	double NearestHorizontalDistance = MAX_dbl;
	int32 Level0Radius = MAX_int32;

	for (const FVoxelStreamingSource& Source : InSources)
	{
		if (!Source.Has(EVoxelStreamingCapability::FineVisual))
		{
			continue;
		}

		const double HorizontalDistance = FVector2D::Distance(
			FVector2D(SectionCenter.X, SectionCenter.Y),
			FVector2D(Source.Center.X, Source.Center.Y));

		if (HorizontalDistance < NearestHorizontalDistance)
		{
			NearestHorizontalDistance = HorizontalDistance;
			Level0Radius = FMath::Max(
				SectionSide,
				Source.View.FineLevel0RadiusCells);
		}
	}

	if (Level0Radius == MAX_int32 ||
		NearestHorizontalDistance < Level0Radius)
	{
		return 0;
	}

	const double Ratio =
		NearestHorizontalDistance /
		FMath::Max(1.0, static_cast<double>(Level0Radius));

	const int32 Level =
		1 + FMath::FloorToInt(
			FMath::Log2(FMath::Max(1.0, Ratio)));

	return static_cast<uint8>(FMath::Clamp(
		Level,
		0,
		static_cast<int32>(InSettings.Lod.MaximumFineLevel)));
}

void FVoxelFinePagePlanner::Build(
	const TSet<FIntVector>& InFineSections,
	TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelViewSettings& InSettings,
	FVoxelFinePagePlan& OutPlan)
{
	OutPlan = {};

	TMap<FVoxelFinePageKey, uint8> Leaves;

	for (const FIntVector& Section : InFineSections)
	{
		const FVoxelFinePageKey Key { Section, 0 };
		Leaves.Add(
			Key,
			ResolveDesiredLevel(
				Section,
				InSources,
				InSettings));
	}

	auto CanMergePage = [&](const FVoxelFinePageKey& Page)
	{
		const FVoxelGenerationBounds Bounds = Page.GetBounds();
		const FVector2D Min(Bounds.Min.X, Bounds.Min.Y);
		const FVector2D Max(Bounds.Max.X, Bounds.Max.Y);

		for (const FVoxelStreamingSource& Source : InSources)
		{
			if (!Source.Has(EVoxelStreamingCapability::FineVisual))
			{
				continue;
			}

			const FVector2D Center(Source.Center.X, Source.Center.Y);
			const FVector2D Nearest(
				FMath::Clamp(Center.X, Min.X, Max.X),
				FMath::Clamp(Center.Y, Min.Y, Max.Y));

			const double NearDistance = FVector2D::Distance(Center, Nearest);
			const int32 LevelStartRadius = FMath::Max(
				16,
				Source.View.FineLevel0RadiusCells) <<
				(Page.Level - 1);

			// 只要 Page 任意部分侵入某个 Source 更精细的距离带，就必须继续拆。
			if (NearDistance < LevelStartRadius)
			{
				return false;
			}
		}

		return true;
	};

	for (uint8 ParentLevel = 1;
		 ParentLevel <= InSettings.Lod.MaximumFineLevel;
		 ++ParentLevel)
	{
		TMap<FVoxelFinePageKey, TArray<FVoxelFinePageKey>> Families;

		for (const TPair<FVoxelFinePageKey, uint8>& Pair : Leaves)
		{
			if (Pair.Key.Level != ParentLevel - 1 ||
				Pair.Value < ParentLevel)
			{
				continue;
			}

			const FVoxelFinePageKey Parent {
				FIntVector(
					VoxelGeneration::FloorDivide(Pair.Key.Coordinate.X, 2),
					VoxelGeneration::FloorDivide(Pair.Key.Coordinate.Y, 2),
					VoxelGeneration::FloorDivide(Pair.Key.Coordinate.Z, 2)),
				ParentLevel
			};

			Families.FindOrAdd(Parent).Add(Pair.Key);
		}

		for (const TPair<FVoxelFinePageKey, TArray<FVoxelFinePageKey>>& Family : Families)
		{
			if (Family.Value.Num() != 8 ||
				!CanMergePage(Family.Key))
			{
				continue;
			}

			bool bComplete = true;
			uint8 Desired = MAX_uint8;

			for (const FVoxelFinePageKey& Child : Family.Value)
			{
				const uint8* ChildDesired = Leaves.Find(Child);
				if (!ChildDesired || *ChildDesired < ParentLevel)
				{
					bComplete = false;
					break;
				}
				Desired = FMath::Min(Desired, *ChildDesired);
			}

			if (!bComplete)
			{
				continue;
			}

			for (const FVoxelFinePageKey& Child : Family.Value)
			{
				Leaves.Remove(Child);
			}

			Leaves.Add(Family.Key, Desired);
		}
	}

	for (const TPair<FVoxelFinePageKey, uint8>& Pair : Leaves)
	{
		OutPlan.Pages.Add(Pair.Key);

		const FVoxelGenerationBounds Bounds = Pair.Key.GetBounds();
		const FIntVector MinSection(
			VoxelGeneration::FloorDivide(Bounds.Min.X, 16),
			VoxelGeneration::FloorDivide(Bounds.Min.Y, 16),
			VoxelGeneration::FloorDivide(Bounds.Min.Z, 16));
		const int32 SectionsPerSide = Pair.Key.GetSide() / 16;

		for (int32 Z = 0; Z < SectionsPerSide; ++Z)
		{
			for (int32 Y = 0; Y < SectionsPerSide; ++Y)
			{
				for (int32 X = 0; X < SectionsPerSide; ++X)
				{
					const FIntVector Section =
						MinSection + FIntVector(X, Y, Z);
					if (InFineSections.Contains(Section))
					{
						OutPlan.SectionOwners.Add(Section, Pair.Key);
					}
				}
			}
		}
	}
}

void FVoxelFinePagePlanner::GatherSections(const FVoxelFinePageKey& InPage, TArray<FIntVector>& OutSections)
{
	OutSections.Reset();
	const FIntVector Min = InPage.Coordinate * (1 << InPage.Level);
	const int32 Side = 1 << InPage.Level;
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

void FVoxelFinePagePlanner::AppendMesh(const FVoxelSectionMeshResult& InMesh, const FVector& InOffsetCells, FVoxelSectionMeshResult& OutMesh)
{
	for (const FVoxelRenderBatch& Source : InMesh.Batches)
	{
		FVoxelRenderBatch* Target = OutMesh.Batches.FindByPredicate([&Source](const FVoxelRenderBatch& Batch)
		{
			return Batch.Group == Source.Group && Batch.Bank == Source.Bank && Batch.MaterialVariant == Source.MaterialVariant;
		});
		if (!Target)
		{
			Target = &OutMesh.Batches.AddDefaulted_GetRef();
			Target->Group = Source.Group;
			Target->Bank = Source.Bank;
			Target->MaterialVariant = Source.MaterialVariant;
		}
		const int32 Base = Target->Mesh.Vertices.Num();
		for (const FVector& Vertex : Source.Mesh.Vertices) Target->Mesh.Vertices.Add(Vertex + InOffsetCells);
		for (const int32 Index : Source.Mesh.Triangles) Target->Mesh.Triangles.Add(Base + Index);
		Target->Mesh.Normals.Append(Source.Mesh.Normals);
		Target->Mesh.Tangents.Append(Source.Mesh.Tangents);
		Target->Mesh.UV0.Append(Source.Mesh.UV0);
		Target->Mesh.UV1.Append(Source.Mesh.UV1);
		Target->Mesh.UV2.Append(Source.Mesh.UV2);
		Target->Mesh.Colors.Append(Source.Mesh.Colors);
	}
}
