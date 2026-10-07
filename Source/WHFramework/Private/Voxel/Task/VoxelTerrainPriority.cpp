#include "Voxel/Task/VoxelTerrainPriority.h"

bool FVoxelTerrainPriority::Resolve(const FVoxelTerrainGeometry& InGeometry, const FVoxelTerrainTaskRole& InRole,
	TConstArrayView<FVoxelStreamingSource> InSources, const int32 InP0RadiusCells,
	FVoxelTerrainServicePriority& OutPriority, FString& OutError)
{
	FIntVector Minimum;
	FIntVector Maximum;
	if (InSources.IsEmpty() || InP0RadiusCells <= 0 ||
		!InGeometry.Key.TryBounds(InGeometry.WorldMinZ, InGeometry.WorldMaxZ, Minimum, Maximum))
	{
		OutError = TEXT("地形优先级缺少有效观察源或空间边界");
		return false;
	}
	FVoxelTerrainServicePriority Best;
	Best.DistanceCells = MAX_dbl;
	bool bHasBest = false;
	TSet<FGuid> SourceIds;
	for (const FVoxelStreamingSource& Source : InSources)
	{
		if (!Source.Id.IsValid() || SourceIds.Contains(Source.Id) || Source.Direction.ContainsNaN())
		{
			OutError = TEXT("地形优先级Source身份重复、无效或方向非有限");
			return false;
		}
		SourceIds.Add(Source.Id);
		FVector Position(Source.Center);
		FVector Nearest(FMath::Clamp(Position.X, double(Minimum.X), double(Maximum.X)),
			FMath::Clamp(Position.Y, double(Minimum.Y), double(Maximum.Y)),
			FMath::Clamp(Position.Z, double(Minimum.Z), double(Maximum.Z)));
		if (InGeometry.Key.IsColumn())
		{
			Nearest.Z = Position.Z;
		}
		const FVector Delta = Nearest - Position;
		const double Distance = Delta.Size();
		const FVector Direction = Source.Direction.GetSafeNormal();
		const double Forward = Distance > UE_DOUBLE_SMALL_NUMBER ? FVector::DotProduct(Direction, Delta / Distance) : 1.0;
		FVoxelTerrainServicePriority Candidate;
		Candidate.DistanceCells = Distance;
		Candidate.ForwardScore = Forward;
		Candidate.SourcePriority = Source.GetSchedulingPriority();
		Candidate.SourceStableKey = Source.Id;
		if (!bHasBest || FVoxelTerrainWorkSelection::HigherPriority(Candidate, Best))
		{
			Best = Candidate;
			bHasBest = true;
		}
	}
	if (InRole.bInteractive || InRole.bCollisionGuard || InRole.bAheadPrefetch)
	{
		Best.Band = Best.DistanceCells <= InP0RadiusCells ? EVoxelTerrainServiceBand::P0 : EVoxelTerrainServiceBand::P1;
	}
	else
	{
		switch (InGeometry.Key.Representation)
		{
		case EVoxelTerrainRepresentation::Fine:
			Best.Band = EVoxelTerrainServiceBand::P2;
			break;
		case EVoxelTerrainRepresentation::VoxelProxy:
			Best.Band = EVoxelTerrainServiceBand::P3;
			break;
		case EVoxelTerrainRepresentation::Surface:
			Best.Band = EVoxelTerrainServiceBand::P4;
			break;
		case EVoxelTerrainRepresentation::Macro:
			Best.Band = EVoxelTerrainServiceBand::P5;
			break;
		default:
			Best.Band = EVoxelTerrainServiceBand::P6;
			break;
		}
	}
	OutPriority = Best;
	OutError.Reset();
	return true;
}
