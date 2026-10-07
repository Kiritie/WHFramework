#include "Voxel/Rendering/DWHeightfieldTransitionBuilder.h"
#include "Voxel/Rendering/VoxelBoundaryRaster.h"
#include "Voxel/Streaming/VoxelFineSpatial.h"

#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Runtime/VoxelRegistry.h"

namespace
{
	struct FHeightfieldEdgeRecord
	{
		FVoxelHeightfieldNodeKey Key;
		int32 RangeMin = 0;
		int32 RangeMax = 0;
		int32 Step = 1;
		int32 TileSide = 16;
		uint64 Revision = 0;
		uint64 GeometryRevision = 0;
	};

	using FEdgeIndex = TMap<int32, TArray<FHeightfieldEdgeRecord>>;

	void IndexFootprint(const FVoxelHeightfieldNodeKey& InKey,
		const FIntPoint& InOrigin, const int32 InTileSide,
		const int32 InStep, const uint64 InRevision,
		FEdgeIndex& OutNegativeX, FEdgeIndex& OutPositiveX,
		FEdgeIndex& OutNegativeY, FEdgeIndex& OutPositiveY, const uint64 InGeometryRevision = 0)
	{
		const FIntPoint Max = InOrigin + FIntPoint(InTileSide, InTileSide);
		const FHeightfieldEdgeRecord XRecord{InKey, InOrigin.Y, Max.Y,
			InStep, InTileSide, InRevision, InGeometryRevision};
		const FHeightfieldEdgeRecord YRecord{InKey, InOrigin.X, Max.X,
			InStep, InTileSide, InRevision, InGeometryRevision};
		OutNegativeX.FindOrAdd(InOrigin.X).Add(XRecord);
		OutPositiveX.FindOrAdd(Max.X).Add(XRecord);
		OutNegativeY.FindOrAdd(InOrigin.Y).Add(YRecord);
		OutPositiveY.FindOrAdd(Max.Y).Add(YRecord);
	}

	bool KeyLess(const FVoxelHeightfieldNodeKey& A, const FVoxelHeightfieldNodeKey& B)
	{
		if (A.Representation != B.Representation)
		{
			return static_cast<uint8>(A.Representation) < static_cast<uint8>(B.Representation);
		}
		if (A.Coordinate.X != B.Coordinate.X) return A.Coordinate.X < B.Coordinate.X;
		if (A.Coordinate.Y != B.Coordinate.Y) return A.Coordinate.Y < B.Coordinate.Y;
		return A.Level < B.Level;
	}

	bool EdgeLess(const FVoxelHeightfieldTransitionEdge& A,
		const FVoxelHeightfieldTransitionEdge& B)
	{
		if (KeyLess(A.Owner, B.Owner)) return true;
		if (KeyLess(B.Owner, A.Owner)) return false;
		if (A.Direction != B.Direction)
		{
			return static_cast<uint8>(A.Direction) < static_cast<uint8>(B.Direction);
		}
		if (A.bPending != B.bPending) return !A.bPending;
		if (A.RangeMin != B.RangeMin) return A.RangeMin < B.RangeMin;
		if (A.RangeMax != B.RangeMax) return A.RangeMax < B.RangeMax;
		return KeyLess(A.Neighbor, B.Neighbor);
	}

	void MatchEdges(const FEdgeIndex& Positive, const FEdgeIndex& Negative,
		const EVoxelLodEdgeDirection PositiveDirection,
		const EVoxelLodEdgeDirection NegativeDirection,
		TArray<FVoxelHeightfieldTransitionEdge>& OutEdges)
	{
		for (const TPair<int32, TArray<FHeightfieldEdgeRecord>>& Pair : Positive)
		{
			const TArray<FHeightfieldEdgeRecord>* Other = Negative.Find(Pair.Key);
			if (!Other) continue;
			for (const FHeightfieldEdgeRecord& A : Pair.Value)
			{
				for (const FHeightfieldEdgeRecord& B : *Other)
				{
					const int32 Min = FMath::Max(A.RangeMin, B.RangeMin);
					const int32 Max = FMath::Min(A.RangeMax, B.RangeMax);
					if (Min >= Max) continue;
					const bool bPositiveOwns = A.Step > B.Step || (A.Step == B.Step && KeyLess(A.Key, B.Key));
					FVoxelHeightfieldTransitionEdge& Edge = OutEdges.AddDefaulted_GetRef();
					Edge.Owner = bPositiveOwns ? A.Key : B.Key;
					Edge.Neighbor = bPositiveOwns ? B.Key : A.Key;
					Edge.Direction = bPositiveOwns ? PositiveDirection : NegativeDirection;
					Edge.RangeMin = Min;
					Edge.RangeMax = Max;
					Edge.OwnerStep = bPositiveOwns ? A.Step : B.Step;
					Edge.NeighborStep = bPositiveOwns ? B.Step : A.Step;
						Edge.OwnerRevision = bPositiveOwns ? A.Revision : B.Revision;
						Edge.NeighborRevision = bPositiveOwns ? B.Revision : A.Revision;
						Edge.OwnerGeometryRevision = bPositiveOwns ? A.GeometryRevision : B.GeometryRevision;
						Edge.NeighborGeometryRevision = bPositiveOwns ? B.GeometryRevision : A.GeometryRevision;
						Edge.bUnbalanced = FMath::Max(A.TileSide, B.TileSide) / FMath::Min(A.TileSide, B.TileSide) > 2;
				}
			}
		}
	}

	uint64 MixSignature(const uint64 InCurrent, const uint64 InValue)
	{
		return VoxelGeneration::Mix(InCurrent ^ VoxelGeneration::Mix(InValue));
	}
}

bool FVoxelFrozenHeightfieldInputs::SameInputs(const FVoxelFrozenHeightfieldInputs& InOther) const
{
	return Owner == InOther.Owner && WorldEpoch == InOther.WorldEpoch && GeneratorIdentity == InOther.GeneratorIdentity &&
		RegistryRevision == InOther.RegistryRevision && ShapeRevision == InOther.ShapeRevision &&
		Sources == InOther.Sources && Edges == InOther.Edges;
}

uint8 FVoxelHeightfieldTileView::GetWaterKind(const int32 InIndex) const
{
	if (SurfaceFlags.IsEmpty()) return WaterKind[InIndex];
	if (Water[InIndex] == MIN_int32 || Water[InIndex] < Ground[InIndex]) return static_cast<uint8>(EVoxelWaterKind::None);
	const uint8 Flags = SurfaceFlags[InIndex];
	if ((Flags & VoxelSurface_Ocean) != 0) return static_cast<uint8>(EVoxelWaterKind::Ocean);
	if ((Flags & VoxelSurface_Lake) != 0) return static_cast<uint8>(EVoxelWaterKind::Lake);
	if ((Flags & VoxelSurface_River) != 0) return static_cast<uint8>(EVoxelWaterKind::River);
	return static_cast<uint8>(EVoxelWaterKind::Edited);
}

FVoxelHeightfieldTileView FVoxelHeightfieldTransitionBuilder::MakeView(
	const FVoxelSurfaceTileData& InData)
{
	FVoxelHeightfieldTileView View;
	View.Key = {EVoxelHeightfieldRepresentation::Surface,
		InData.Key.Coordinate, InData.Key.Level};
	View.Origin = InData.Key.Coordinate * InData.GetTileSide();
	View.Side = InData.Side;
	View.Step = InData.Step;
	View.Revision = InData.Revision;
	View.GeometryRevision = InData.Geometry.Key.Representation == EVoxelTerrainRepresentation::None ? 0 : GetTypeHash(InData.Geometry);
	View.Ground = InData.GroundZ;
	View.Water = InData.WaterZ;
	View.SurfaceFlags = InData.Flags;
	View.Material = InData.SurfaceMaterial;
	View.ZBias = -0.02;
	return View;
}

FVoxelHeightfieldTileView FVoxelHeightfieldTransitionBuilder::MakeView(
	const FVoxelMacroTileData& InData)
{
	FVoxelHeightfieldTileView View;
	View.Key = {EVoxelHeightfieldRepresentation::Macro,
		InData.Key.Coordinate, InData.Key.Level};
	View.Origin = InData.Key.Coordinate * InData.GetTileSide();
	View.Side = InData.Side;
	View.Step = InData.Step;
	View.Revision = InData.Revision;
	View.GeometryRevision = InData.Geometry.Key.Representation == EVoxelTerrainRepresentation::None ? 0 : GetTypeHash(InData.Geometry);
	View.Ground = InData.Height;
	View.Water = InData.WaterHeight;
	View.WaterKind = InData.WaterKind;
	View.Material = InData.SurfaceClass;
	View.ZBias = -0.05;
	return View;
}

bool FVoxelHeightfieldTransitionBuilder::BuildEdges(
	TConstArrayView<FVoxelHeightfieldTileView> InTiles,
	TArray<FVoxelHeightfieldTransitionEdge>& OutEdges,
	FString& OutError)
{
	OutEdges.Reset();
	FEdgeIndex NegativeX;
	FEdgeIndex PositiveX;
	FEdgeIndex NegativeY;
	FEdgeIndex PositiveY;
	for (const FVoxelHeightfieldTileView& View : InTiles)
	{
		const int32 Count = View.Side * View.Side;
		if (View.Key.Representation == EVoxelHeightfieldRepresentation::None ||
			View.Side < 2 || View.Step <= 0 || View.Ground.Num() != Count ||
			View.Water.Num() != Count || View.WaterKindCount() != Count ||
			View.Material.Num() != Count)
		{
			OutError = TEXT("Heightfield transition tile view is invalid");
			return false;
		}
		IndexFootprint(View.Key, View.Origin, View.CellSide() * View.Step,
			View.Step, View.Revision,
			NegativeX, PositiveX, NegativeY, PositiveY, View.GeometryRevision);
	}
	MatchEdges(PositiveX, NegativeX, EVoxelLodEdgeDirection::PositiveX,
		EVoxelLodEdgeDirection::NegativeX, OutEdges);
	MatchEdges(PositiveY, NegativeY, EVoxelLodEdgeDirection::PositiveY,
		EVoxelLodEdgeDirection::NegativeY, OutEdges);
	OutEdges.Sort(EdgeLess);
	OutError.Reset();
	return true;
}

void FVoxelHeightfieldTransitionBuilder::GatherOwnerDependencies(TConstArrayView<FVoxelHeightfieldTileView> InTiles,
	TConstArrayView<FVoxelHeightfieldTileFootprint> InPendingTiles,
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldNodeKey>>& OutDependencies)
{
	OutDependencies.Reset();
	FEdgeIndex NegativeX;
	FEdgeIndex PositiveX;
	FEdgeIndex NegativeY;
	FEdgeIndex PositiveY;
	for (const auto& View : InTiles)
	{
		OutDependencies.FindOrAdd(View.Key).Add(View.Key);
		IndexFootprint(View.Key, View.Origin, View.CellSide() * View.Step, View.Step, View.Revision,
			NegativeX, PositiveX, NegativeY, PositiveY, View.GeometryRevision);
	}
	for (const auto& Tile : InPendingTiles)
	{
		IndexFootprint(Tile.Key, Tile.Origin, Tile.TileSide, Tile.Step, 0,
			NegativeX, PositiveX, NegativeY, PositiveY);
	}
	const auto Gather = [&OutDependencies](const FEdgeIndex& Positive, const FEdgeIndex& Negative)
	{
		for (const auto& Pair : Positive)
		{
			const auto* Across = Negative.Find(Pair.Key);
			if (!Across) continue;
			for (const auto& A : Pair.Value)
			{
				for (const auto& B : *Across)
				{
					if (FMath::Max(A.RangeMin, B.RangeMin) >= FMath::Min(A.RangeMax, B.RangeMax)) continue;
					if (auto* Keys = OutDependencies.Find(A.Key)) Keys->AddUnique(B.Key);
					if (auto* Keys = OutDependencies.Find(B.Key)) Keys->AddUnique(A.Key);
				}
			}
		}
	};
	Gather(PositiveX, NegativeX);
	Gather(PositiveY, NegativeY);
	for (auto& Pair : OutDependencies) Pair.Value.Sort(KeyLess);
}

void FVoxelHeightfieldTransitionBuilder::ExcludeCoveredIntervals(
	TConstArrayView<FVoxelHeightfieldTileView> InTiles,
	TConstArrayView<FBox> InFineBoxes,
	TConstArrayView<FBox> InProxySurfaceBoxes,
	TArray<FVoxelHeightfieldTransitionEdge>& InOutEdges)
{
	TMap<FVoxelHeightfieldNodeKey, FVoxelHeightfieldTileView> Views;
	for (const FVoxelHeightfieldTileView& View : InTiles)
	{
		Views.Add(View.Key, View);
	}
	TArray<FVoxelHeightfieldTransitionEdge> Visible;
	auto Exclude = [](const FVoxelHeightfieldTransitionEdge& Edge,
		const FVoxelHeightfieldTileView& Owner,
		TConstArrayView<FBox> Boxes,
		TArray<FIntPoint>& Intervals)
	{
		const bool bX = Edge.Direction == EVoxelLodEdgeDirection::NegativeX ||
			Edge.Direction == EVoxelLodEdgeDirection::PositiveX;
		const int32 Fixed = bX
			? Owner.Origin.X + (Edge.Direction == EVoxelLodEdgeDirection::PositiveX
				? Owner.CellSide() * Owner.Step : 0)
			: Owner.Origin.Y + (Edge.Direction == EVoxelLodEdgeDirection::PositiveY
				? Owner.CellSide() * Owner.Step : 0);
		for (const FBox& Box : Boxes)
		{
			const double BoxMinFixed = bX ? Box.Min.X : Box.Min.Y;
			const double BoxMaxFixed = bX ? Box.Max.X : Box.Max.Y;
			if (BoxMinFixed > Fixed || BoxMaxFixed < Fixed) continue;
			const int32 Min = FMath::Max(Edge.RangeMin,
				FMath::CeilToInt(bX ? Box.Min.Y : Box.Min.X));
			const int32 Max = FMath::Min(Edge.RangeMax,
				FMath::CeilToInt(bX ? Box.Max.Y : Box.Max.X));
			if (Min < Max) Intervals.Add(FIntPoint(Min, Max));
		}
	};
	for (const FVoxelHeightfieldTransitionEdge& Edge : InOutEdges)
	{
		const FVoxelHeightfieldTileView* Owner = Views.Find(Edge.Owner);
		if (!Owner) continue;
		TArray<FIntPoint> Intervals;
		Exclude(Edge, *Owner, InFineBoxes, Intervals);
		Exclude(Edge, *Owner, InProxySurfaceBoxes, Intervals);
		Intervals.Sort([](const FIntPoint& A, const FIntPoint& B)
		{
			return A.X == B.X ? A.Y < B.Y : A.X < B.X;
		});
		int32 Cursor = Edge.RangeMin;
		for (const FIntPoint& Interval : Intervals)
		{
			if (Cursor < Interval.X)
			{
				FVoxelHeightfieldTransitionEdge& Segment = Visible.Add_GetRef(Edge);
				Segment.RangeMin = Cursor;
				Segment.RangeMax = Interval.X;
			}
			Cursor = FMath::Max(Cursor, Interval.Y);
			if (Cursor >= Edge.RangeMax) break;
		}
		if (Cursor < Edge.RangeMax)
		{
			FVoxelHeightfieldTransitionEdge& Segment = Visible.Add_GetRef(Edge);
			Segment.RangeMin = Cursor;
			Segment.RangeMax = Edge.RangeMax;
		}
	}
	InOutEdges = MoveTemp(Visible);
}

void FVoxelHeightfieldTransitionBuilder::AppendPendingEdges(
	TConstArrayView<FVoxelHeightfieldTileView> InReadyTiles,
	TConstArrayView<FVoxelHeightfieldTileFootprint> InPendingTiles,
	TArray<FVoxelHeightfieldTransitionEdge>& InOutEdges)
{
	FEdgeIndex ReadyNegativeX;
	FEdgeIndex ReadyPositiveX;
	FEdgeIndex ReadyNegativeY;
	FEdgeIndex ReadyPositiveY;
	FEdgeIndex PendingNegativeX;
	FEdgeIndex PendingPositiveX;
	FEdgeIndex PendingNegativeY;
	FEdgeIndex PendingPositiveY;
	for (const FVoxelHeightfieldTileView& View : InReadyTiles)
	{
		IndexFootprint(View.Key, View.Origin, View.CellSide() * View.Step,
			View.Step, View.Revision,
			ReadyNegativeX, ReadyPositiveX, ReadyNegativeY, ReadyPositiveY, View.GeometryRevision);
	}
	for (const FVoxelHeightfieldTileFootprint& Footprint : InPendingTiles)
	{
		if (Footprint.TileSide <= 0 || Footprint.Step <= 0) continue;
		IndexFootprint(Footprint.Key, Footprint.Origin, Footprint.TileSide,
			Footprint.Step, 0,
			PendingNegativeX, PendingPositiveX, PendingNegativeY, PendingPositiveY);
	}
	TArray<FVoxelHeightfieldTransitionEdge> Pending;
	auto MatchPending = [&Pending](const FEdgeIndex& Ready,
		const FEdgeIndex& Wanted,
		const FEdgeIndex& ReadyAcross,
		const EVoxelLodEdgeDirection Direction)
	{
		for (const auto& Pair : Ready)
		{
			const TArray<FHeightfieldEdgeRecord>* Targets = Wanted.Find(Pair.Key);
			if (!Targets) continue;
			const TArray<FHeightfieldEdgeRecord>* Occupied = ReadyAcross.Find(Pair.Key);
			for (const FHeightfieldEdgeRecord& A : Pair.Value)
			{
			for (const FHeightfieldEdgeRecord& B : *Targets)
			{
				const int32 Start = FMath::Max(A.RangeMin, B.RangeMin);
				const int32 End = FMath::Min(A.RangeMax, B.RangeMax);
				if (Start >= End) continue;
				TArray<FIntPoint> Blocked;
				if (Occupied)
				{
					for (const FHeightfieldEdgeRecord& Other : *Occupied)
					{
						const int32 Min = FMath::Max(Start, Other.RangeMin);
						const int32 Max = FMath::Min(End, Other.RangeMax);
						if (Min < Max) Blocked.Add(FIntPoint(Min, Max));
					}
				}
				Blocked.Sort([](const FIntPoint& L, const FIntPoint& R)
				{
					return L.X == R.X ? L.Y < R.Y : L.X < R.X;
				});
				auto Append = [&Pending, &A, Direction](const int32 Min, const int32 Max)
				{
					if (Min >= Max) return;
					FVoxelHeightfieldTransitionEdge& Edge = Pending.AddDefaulted_GetRef();
					Edge.Owner = A.Key;
					Edge.Direction = Direction;
					Edge.RangeMin = Min;
					Edge.RangeMax = Max;
					Edge.OwnerStep = A.Step;
					Edge.NeighborStep = 0;
					Edge.OwnerRevision = A.Revision;
					Edge.OwnerGeometryRevision = A.GeometryRevision;
					Edge.bPending = true;
				};
				int32 Cursor = Start;
				for (const FIntPoint& Interval : Blocked)
				{
					Append(Cursor, Interval.X);
					Cursor = FMath::Max(Cursor, Interval.Y);
				}
				Append(Cursor, End);
			}
			}
		}
	};
	MatchPending(ReadyPositiveX, PendingNegativeX, ReadyNegativeX,
		EVoxelLodEdgeDirection::PositiveX);
	MatchPending(ReadyNegativeX, PendingPositiveX, ReadyPositiveX,
		EVoxelLodEdgeDirection::NegativeX);
	MatchPending(ReadyPositiveY, PendingNegativeY, ReadyNegativeY,
		EVoxelLodEdgeDirection::PositiveY);
	MatchPending(ReadyNegativeY, PendingPositiveY, ReadyPositiveY,
		EVoxelLodEdgeDirection::NegativeY);
	Pending.Sort(EdgeLess);
	TArray<FVoxelHeightfieldTransitionEdge> Merged;
	for (const FVoxelHeightfieldTransitionEdge& Edge : Pending)
	{
		if (!Merged.IsEmpty())
		{
			FVoxelHeightfieldTransitionEdge& Last = Merged.Last();
			if (Last.Owner == Edge.Owner && Last.Direction == Edge.Direction &&
				Last.RangeMax >= Edge.RangeMin)
			{
				Last.RangeMax = FMath::Max(Last.RangeMax, Edge.RangeMax);
				continue;
			}
		}
		Merged.Add(Edge);
	}
	InOutEdges.Append(MoveTemp(Merged));
	InOutEdges.Sort(EdgeLess);
}

uint64 FVoxelHeightfieldTransitionBuilder::BuildSignature(
	const FVoxelHeightfieldNodeKey& InOwner,
	const uint64 InOwnerRevision,
	TConstArrayView<FVoxelHeightfieldTransitionEdge> InEdges)
{
	TArray<FVoxelHeightfieldTransitionEdge> Sorted;
	Sorted.Append(InEdges.GetData(), InEdges.Num());
	Sorted.Sort(EdgeLess);
	uint64 Signature = MixSignature(GetTypeHash(InOwner), InOwnerRevision);
	for (const FVoxelHeightfieldTransitionEdge& Edge : Sorted)
	{
		Signature = MixSignature(Signature, GetTypeHash(Edge.Neighbor));
		Signature = MixSignature(Signature, Edge.NeighborRevision);
		Signature = MixSignature(Signature, Edge.OwnerGeometryRevision);
		Signature = MixSignature(Signature, Edge.NeighborGeometryRevision);
		Signature = MixSignature(Signature, static_cast<uint8>(Edge.Direction));
		Signature = MixSignature(Signature, static_cast<uint32>(Edge.RangeMin));
		Signature = MixSignature(Signature, static_cast<uint32>(Edge.RangeMax));
		Signature = MixSignature(Signature, static_cast<uint32>(Edge.OwnerStep));
		Signature = MixSignature(Signature, static_cast<uint32>(Edge.NeighborStep));
		Signature = MixSignature(Signature, Edge.bPending ? 1 : 0);
		Signature = MixSignature(Signature, Edge.bUnbalanced ? 1 : 0);
	}
	return Signature;
}

namespace
{
	EVoxelLodEdgeDirection OppositeDirection(const EVoxelLodEdgeDirection InDirection)
	{
		switch (InDirection)
		{
		case EVoxelLodEdgeDirection::NegativeX: return EVoxelLodEdgeDirection::PositiveX;
		case EVoxelLodEdgeDirection::PositiveX: return EVoxelLodEdgeDirection::NegativeX;
		case EVoxelLodEdgeDirection::NegativeY: return EVoxelLodEdgeDirection::PositiveY;
		case EVoxelLodEdgeDirection::PositiveY: return EVoxelLodEdgeDirection::NegativeY;
		default: return EVoxelLodEdgeDirection::None;
		}
	}

	uint8 FaceIndex(const EVoxelLodEdgeDirection InDirection)
	{
		switch (InDirection)
		{
		case EVoxelLodEdgeDirection::PositiveX: return 0;
		case EVoxelLodEdgeDirection::NegativeX: return 1;
		case EVoxelLodEdgeDirection::PositiveY: return 2;
		case EVoxelLodEdgeDirection::NegativeY: return 3;
		default: return 0;
		}
	}

	int32 EdgeCellIndex(const FVoxelHeightfieldTileView& InView,
		const EVoxelLodEdgeDirection InDirection, const int32 InPosition)
	{
		const bool bXEdge = InDirection == EVoxelLodEdgeDirection::NegativeX ||
			InDirection == EVoxelLodEdgeDirection::PositiveX;
		const int32 Along = FMath::Clamp(VoxelGeneration::FloorDivide(
			InPosition - (bXEdge ? InView.Origin.Y : InView.Origin.X), InView.Step),
			0, InView.CellSide() - 1);
		const int32 X = bXEdge
			? (InDirection == EVoxelLodEdgeDirection::PositiveX ? InView.CellSide() - 1 : 0)
			: Along;
		const int32 Y = bXEdge ? Along
			: (InDirection == EVoxelLodEdgeDirection::PositiveY ? InView.CellSide() - 1 : 0);
		return InView.Index(X, Y);
	}

	FVoxelRenderBatch& FindOrAddBatch(FVoxelSectionMeshResult& InOutMesh,
		const EVoxelRenderGroup InGroup, const uint16 InBank, const uint8 InVariant,
		FVoxelHeightfieldMesher::FCapacityScope* InCapacity)
	{
		for (FVoxelRenderBatch& Batch : InOutMesh.Batches)
		{
			if (Batch.Group == InGroup && Batch.Bank == InBank && Batch.MaterialVariant == InVariant) return Batch;
		}
		if (InCapacity && !InCapacity->PrepareBatches(InOutMesh)) return InCapacity->RejectedBatch();
		FVoxelRenderBatch& Batch = InOutMesh.Batches.AddDefaulted_GetRef();
		Batch.Group = InGroup;
		Batch.Bank = InBank;
		Batch.MaterialVariant = InVariant;
		if (InCapacity) InCapacity->Bind(Batch);
		return Batch;
	}

	void AppendFace(FVoxelSectionMeshResult& InOutMesh,
		const FVoxelRuntimeDefinition& InDefinition,
		const FVoxelRuntimeFaceRef& InFace,
		const EVoxelLodEdgeDirection InDirection,
		const FVoxelHeightfieldTileView& InOwner,
		const int32 InWorldFixed, const int32 InStart, const int32 InEnd,
		const double InBottom, const double InTop,
		const double InTexturePeriod, FVoxelHeightfieldMesher::FCapacityScope* InCapacity)
	{
		const bool bXEdge = InDirection == EVoxelLodEdgeDirection::NegativeX ||
			InDirection == EVoxelLodEdgeDirection::PositiveX;
		const double Fixed = InWorldFixed - (bXEdge ? InOwner.Origin.X : InOwner.Origin.Y);
		const double Start = InStart - (bXEdge ? InOwner.Origin.Y : InOwner.Origin.X);
		const double End = InEnd - (bXEdge ? InOwner.Origin.Y : InOwner.Origin.X);
		FVector Vertices[4];
		switch (InDirection)
		{
		case EVoxelLodEdgeDirection::PositiveX:
			Vertices[0] = FVector(Fixed, End, InTop);
			Vertices[1] = FVector(Fixed, Start, InTop);
			Vertices[2] = FVector(Fixed, Start, InBottom);
			Vertices[3] = FVector(Fixed, End, InBottom);
			break;
		case EVoxelLodEdgeDirection::NegativeX:
			Vertices[0] = FVector(Fixed, Start, InTop);
			Vertices[1] = FVector(Fixed, End, InTop);
			Vertices[2] = FVector(Fixed, End, InBottom);
			Vertices[3] = FVector(Fixed, Start, InBottom);
			break;
		case EVoxelLodEdgeDirection::PositiveY:
			Vertices[0] = FVector(Start, Fixed, InTop);
			Vertices[1] = FVector(End, Fixed, InTop);
			Vertices[2] = FVector(End, Fixed, InBottom);
			Vertices[3] = FVector(Start, Fixed, InBottom);
			break;
		case EVoxelLodEdgeDirection::NegativeY:
			Vertices[0] = FVector(End, Fixed, InTop);
			Vertices[1] = FVector(Start, Fixed, InTop);
			Vertices[2] = FVector(Start, Fixed, InBottom);
			Vertices[3] = FVector(End, Fixed, InBottom);
			break;
		default:
			return;
		}
		FVoxelMeshBuffers& Mesh = FindOrAddBatch(InOutMesh,
			InDefinition.RenderGroup, InFace.Bank, 0, InCapacity).Mesh;
		if (InCapacity && InCapacity->Count(Mesh, 4, 6)) return;
		const int32 Base = Mesh.Vertices.Num();
		const FVector Normal = FVector::CrossProduct(Vertices[1] - Vertices[0],
			Vertices[2] - Vertices[0]).GetSafeNormal();
		const FVector Tangent = (Vertices[1] - Vertices[0]).GetSafeNormal();
		for (const FVector& Vertex : Vertices)
		{
			Mesh.Vertices.Add(Vertex);
			Mesh.Normals.Add(Normal);
			Mesh.UV0.Add(FVector2D(bXEdge ? Vertex.Y : Vertex.X,
				-Vertex.Z) / InTexturePeriod);
			Mesh.UV1.Add(FVector2D(InFace.Layer, InFace.Frames));
			Mesh.UV2.Add(FVector2D(InFace.FPS, 0.0));
			Mesh.Colors.Add(InFace.Tint);
			Mesh.Tangents.Add(FProcMeshTangent(Tangent, false));
		}
		Mesh.Triangles.Append({Base, Base + 2, Base + 1,
			Base, Base + 3, Base + 2});
	}

	bool SampleWaterEdge(const FVoxelHeightfieldTileView& View, const EVoxelLodEdgeDirection Direction,
		const double Position, double& OutHeight, uint8& OutKind)
	{
		const bool bX = Direction == EVoxelLodEdgeDirection::NegativeX || Direction == EVoxelLodEdgeDirection::PositiveX;
		const int32 Origin = bX ? View.Origin.Y : View.Origin.X;
		const double Coordinate = (Position - Origin) / View.Step;
		const int32 Along = FMath::Clamp(FMath::FloorToInt(Coordinate), 0, View.CellSide() - 1);
		const double Alpha = FMath::Clamp(Coordinate - Along, 0.0, 1.0);
		const int32 Fixed = Direction == EVoxelLodEdgeDirection::PositiveX || Direction == EVoxelLodEdgeDirection::PositiveY ? View.CellSide() : 0;
		const int32 First = bX ? View.Index(Fixed, Along) : View.Index(Along, Fixed);
		const int32 Second = bX ? View.Index(Fixed, Along + 1) : View.Index(Along + 1, Fixed);
		const auto IsWet = [&View](const int32 Index)
		{
			return View.GetWaterKind(Index) != uint8(EVoxelWaterKind::None) && View.Water[Index] != MIN_int32 &&
				View.Water[Index] >= View.Ground[Index];
		};
		const bool bFirstWet = IsWet(First), bSecondWet = IsWet(Second);
		if (!bFirstWet && !bSecondWet) return false;
		const int32 CellFixed = FMath::Min(Fixed, View.CellSide() - 1);
		const int32 CellX = bX ? CellFixed : Along, CellY = bX ? Along : CellFixed;
		const int32 CellIndices[4] = {View.Index(CellX, CellY), View.Index(CellX + 1, CellY),
			View.Index(CellX + 1, CellY + 1), View.Index(CellX, CellY + 1)};
		// 使用生成此边缘的实际水 quad 材质选择顺序（00、10、11、01）。
		for (const int32 Index : CellIndices)
			if (IsWet(Index)) { OutKind = View.GetWaterKind(Index); break; }
		if (bFirstWet && bSecondWet)
		{
			OutHeight = FMath::Lerp(double(View.Water[First]) + 1.0, double(View.Water[Second]) + 1.0, Alpha);
			return true;
		}
		// 与 BuildWaterGrid 的实际湿/干边缘一致：断点在边中点，湿侧保持自身顶点高度。
		if ((bFirstWet && Alpha > .5 + 1.e-9) || (bSecondWet && Alpha < .5 - 1.e-9)) return false;
		const int32 Wet = bFirstWet ? First : Second;
		OutHeight = double(View.Water[Wet]) + 1.0;
		return true;
	}

	void AppendWaterInterface(FVoxelSectionMeshResult& OutMesh, const FVoxelRuntimeDefinition& Definition,
		const FVoxelRuntimeFaceRef& Face, const FVoxelHeightfieldTileView& Owner,
		const EVoxelLodEdgeDirection Direction, const int32 FixedWorld, const double StartWorld, const double EndWorld,
		const double TopStart, const double TopEnd, const double BottomStart, const double BottomEnd,
		const uint8 WaterKind, const double TexturePeriod, FVoxelHeightfieldMesher::FCapacityScope* InCapacity)
	{
		const bool bX = Direction == EVoxelLodEdgeDirection::NegativeX || Direction == EVoxelLodEdgeDirection::PositiveX;
		const bool bPositive = Direction == EVoxelLodEdgeDirection::PositiveX || Direction == EVoxelLodEdgeDirection::PositiveY;
		const int32 Axis = bX ? 0 : 1, Along = bX ? 1 : 0;
		const double Fixed = FixedWorld - (bX ? Owner.Origin.X : Owner.Origin.Y);
		const double Start = StartWorld - (bX ? Owner.Origin.Y : Owner.Origin.X);
		const double End = EndWorld - (bX ? Owner.Origin.Y : Owner.Origin.X);
		FVector Points[4] = {FVector(0.0), FVector(0.0), FVector(0.0), FVector(0.0)};
		for (auto& Point : Points) Point[Axis] = Fixed;
		Points[0][Along] = End; Points[0].Z = TopEnd;
		Points[1][Along] = Start; Points[1].Z = TopStart;
		Points[2][Along] = Start; Points[2].Z = BottomStart;
		Points[3][Along] = End; Points[3].Z = BottomEnd;
		if (bX != bPositive) { Swap(Points[0], Points[1]); Swap(Points[2], Points[3]); }
		auto& Batch = FindOrAddBatch(OutMesh, Definition.RenderGroup, Face.Bank, WaterKind, InCapacity);
		auto& Mesh = Batch.Mesh;
		const bool bFirstTriangle = FVector::CrossProduct(Points[2] - Points[0], Points[1] - Points[0]).SizeSquared() > 1.e-18;
		const bool bSecondTriangle = FVector::CrossProduct(Points[3] - Points[0], Points[2] - Points[0]).SizeSquared() > 1.e-18;
		if (InCapacity && InCapacity->Count(Mesh, 4, 3 * (int32(bFirstTriangle) + int32(bSecondTriangle)))) return;
		const int32 Base = Mesh.Vertices.Num();
		FVector Normal(0.0); Normal[Axis] = bPositive ? 1.0 : -1.0;
		FVector Tangent(0.0); Tangent[Along] = 1.0;
		for (const auto& Point : Points)
		{
			Mesh.Vertices.Add(Point); Mesh.Normals.Add(Normal);
			Mesh.UV0.Add(FVector2D(Point[Along], -Point.Z) / TexturePeriod);
			Mesh.UV1.Add(FVector2D(Face.Layer, Face.Frames)); Mesh.UV2.Add(FVector2D(Face.FPS, 0.0));
			Mesh.Colors.Add(Face.Tint); Mesh.Tangents.Add(FProcMeshTangent(Tangent, false));
		}
		if (bFirstTriangle)
			Mesh.Triangles.Append({Base, Base + 2, Base + 1});
		if (bSecondTriangle)
			Mesh.Triangles.Append({Base, Base + 3, Base + 2});
	}
}

bool FVoxelHeightfieldTransitionBuilder::BuildMesh(
	const FVoxelHeightfieldTileView& InOwner,
	TConstArrayView<FVoxelHeightfieldTransitionEdge> InEdges,
	const TMap<FVoxelHeightfieldNodeKey, FVoxelHeightfieldTileView>& InViews,
	const FVoxelGenerationRuntimeConfig& InConfig,
	const FVoxelRegistrySnapshot& InRegistry,
	const int32 InMinimumGroundZ,
	const double InMaximumTextureStretchCells,
	FVoxelSectionMeshResult& OutMesh,
	FString& OutError,
	const TAtomic<bool>* InCancel, FVoxelHeightfieldMesher::FCapacityScope* InCapacity)
{
	FVoxelSectionMeshResult Mesh;
	for (const FVoxelHeightfieldTransitionEdge& Edge : InEdges)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		if (!(Edge.Owner == InOwner.Key) ||
			Edge.Direction == EVoxelLodEdgeDirection::None ||
			Edge.RangeMin >= Edge.RangeMax || InOwner.Side < 2 || InOwner.Step <= 0 ||
			Edge.OwnerStep != InOwner.Step ||
			Edge.OwnerRevision != InOwner.Revision || Edge.OwnerGeometryRevision != InOwner.GeometryRevision ||
			InOwner.Ground.Num() != InOwner.Side * InOwner.Side ||
			InOwner.Water.Num() != InOwner.Ground.Num() ||
			InOwner.WaterKindCount() != InOwner.Ground.Num() ||
			InOwner.Material.Num() != InOwner.Ground.Num())
		{
			OutError = TEXT("Heightfield transition owner is stale or invalid");
			return false;
		}
		const bool bXEdge = Edge.Direction == EVoxelLodEdgeDirection::NegativeX ||
			Edge.Direction == EVoxelLodEdgeDirection::PositiveX;
		const int32 AlongOrigin = bXEdge ? InOwner.Origin.Y : InOwner.Origin.X;
		const int32 AlongEnd = AlongOrigin + InOwner.CellSide() * InOwner.Step;
		if (Edge.RangeMin < AlongOrigin || Edge.RangeMax > AlongEnd)
		{
			OutError = TEXT("Heightfield transition interval exceeds owner boundary");
			return false;
		}
		const int32 Fixed = bXEdge
			? InOwner.Origin.X + (Edge.Direction == EVoxelLodEdgeDirection::PositiveX
				? InOwner.CellSide() * InOwner.Step : 0)
			: InOwner.Origin.Y + (Edge.Direction == EVoxelLodEdgeDirection::PositiveY
				? InOwner.CellSide() * InOwner.Step : 0);
		if (Edge.bPending)
		{
			OutError = TEXT("DependencyNotReady: Heightfield 缺少完整双侧冻结边界");
			return false;
		}
		const FVoxelHeightfieldTileView* Neighbor = InViews.Find(Edge.Neighbor);
		if (!Neighbor || Neighbor->Side < 2 || InOwner.Step < Neighbor->Step ||
			Neighbor->Step <= 0 || InOwner.Step % Neighbor->Step != 0 ||
			!FMath::IsPowerOfTwo(InOwner.Step / Neighbor->Step) ||
			Edge.NeighborStep != Neighbor->Step ||
			Edge.NeighborRevision != Neighbor->Revision || Edge.NeighborGeometryRevision != Neighbor->GeometryRevision ||
			Neighbor->Ground.Num() != Neighbor->Side * Neighbor->Side ||
			Neighbor->Water.Num() != Neighbor->Ground.Num() ||
			Neighbor->WaterKindCount() != Neighbor->Ground.Num() ||
			Neighbor->Material.Num() != Neighbor->Ground.Num())
		{
			OutError = TEXT("Heightfield transition edge has stale or invalid ownership");
			return false;
		}
		const EVoxelLodEdgeDirection NeighborDirection = OppositeDirection(Edge.Direction);
		const double TexturePeriod = VoxelViewLod::TexturePeriodCells(
			Neighbor->Step, InMaximumTextureStretchCells);
		// 只消费本窗口命中的边缘区间；不复制完整高度剖面或邻接面的临时数组。
		for (int64 Position = Edge.RangeMin; Position < Edge.RangeMax;)
		{
			if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
			const int32 OwnerIndex = EdgeCellIndex(InOwner, Edge.Direction, int32(Position));
			const int32 NeighborIndex = EdgeCellIndex(*Neighbor, NeighborDirection, int32(Position));
			const int64 OwnerOrigin = bXEdge ? InOwner.Origin.Y : InOwner.Origin.X;
			const int64 NeighborOrigin = bXEdge ? Neighbor->Origin.Y : Neighbor->Origin.X;
			const int64 OwnerEnd = OwnerOrigin + (FVoxelFineSpatial::FloorDivide(Position - OwnerOrigin, InOwner.Step) + 1) * InOwner.Step;
			const int64 NeighborEnd = NeighborOrigin + (FVoxelFineSpatial::FloorDivide(Position - NeighborOrigin, Neighbor->Step) + 1) * Neighbor->Step;
			const int32 End = int32(FMath::Min<int64>(Edge.RangeMax, FMath::Min(OwnerEnd, NeighborEnd)));
			if (End <= Position) { OutError = TEXT("Invalid ground edge breakpoint"); return false; }
			const double OwnerHeight = InOwner.Ground[OwnerIndex] == MIN_int32 ? InMinimumGroundZ : InOwner.Ground[OwnerIndex] + 1.0 + InOwner.ZBias;
			const double NeighborHeight = Neighbor->Ground[NeighborIndex] == MIN_int32 ? InMinimumGroundZ : Neighbor->Ground[NeighborIndex] + 1.0 + Neighbor->ZBias;
			if (OwnerHeight != NeighborHeight)
			{
				const bool bOwnerHigher = OwnerHeight > NeighborHeight;
				const auto Direction = bOwnerHigher ? Edge.Direction : NeighborDirection;
				FVoxelBlockState State;
				if (!InConfig.ToRuntime(bOwnerHigher ? InOwner.Material[OwnerIndex] : Neighbor->Material[NeighborIndex], State))
				{ OutError = TEXT("Boundary material is invalid"); return false; }
				const auto* Definition = InRegistry.Find(State.TypeId);
				if (!Definition) { OutError = TEXT("Boundary material is absent from registry"); return false; }
				AppendFace(Mesh, *Definition, Definition->Face(State.State, FaceIndex(Direction)), Direction,
					InOwner, Fixed, int32(Position), End, FMath::Min(OwnerHeight, NeighborHeight), FMath::Max(OwnerHeight, NeighborHeight), TexturePeriod, InCapacity);
			}
			Position = End;
		}
		const auto* WaterDefinition = InRegistry.Find(InConfig.Water.TypeId);
		if (!WaterDefinition) { OutError = TEXT("Heightfield water material is absent from registry"); return false; }
		const double OwnerOrigin = bXEdge ? InOwner.Origin.Y : InOwner.Origin.X;
		const double NeighborOrigin = bXEdge ? Neighbor->Origin.Y : Neighbor->Origin.X;
		const double OwnerHalfStep = InOwner.Step * .5, NeighborHalfStep = Neighbor->Step * .5;
		for (double Position = Edge.RangeMin; Position < Edge.RangeMax;)
		{
			if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
			const double OwnerCut = OwnerOrigin + (FMath::FloorToDouble((Position - OwnerOrigin) / OwnerHalfStep + 1.e-9) + 1) * OwnerHalfStep;
			const double NeighborCut = NeighborOrigin + (FMath::FloorToDouble((Position - NeighborOrigin) / NeighborHalfStep + 1.e-9) + 1) * NeighborHalfStep;
			const double End = FMath::Min(double(Edge.RangeMax), FMath::Min(OwnerCut, NeighborCut));
			if (End <= Position) { OutError = TEXT("Invalid water edge breakpoint"); return false; }
			double OwnerA = 0, OwnerB = 0, NeighborA = 0, NeighborB = 0, MidHeight = 0;
			uint8 OwnerKind = 0, NeighborKind = 0, Kind = 0;
			const double Middle = (Position + End) * .5;
			const bool bWet = SampleWaterEdge(InOwner, Edge.Direction, Middle, MidHeight, OwnerKind) &&
				SampleWaterEdge(*Neighbor, NeighborDirection, Middle, MidHeight, NeighborKind);
			if (bWet && SampleWaterEdge(InOwner, Edge.Direction, Position, OwnerA, Kind) &&
				SampleWaterEdge(InOwner, Edge.Direction, End, OwnerB, Kind) &&
				SampleWaterEdge(*Neighbor, NeighborDirection, Position, NeighborA, Kind) &&
				SampleWaterEdge(*Neighbor, NeighborDirection, End, NeighborB, Kind))
			{
				const auto Append = [&](const double Start, const double Stop, const double OA, const double OB,
					const double NA, const double NB)
				{
					if (FMath::IsNearlyEqual(OA, NA, 1.e-9) && FMath::IsNearlyEqual(OB, NB, 1.e-9)) return;
					const bool bOwnerHigher = OA + OB > NA + NB;
					const auto Direction = bOwnerHigher ? Edge.Direction : NeighborDirection;
					AppendWaterInterface(Mesh, *WaterDefinition, WaterDefinition->Face(InConfig.Water.State, 4), InOwner,
						Direction, Fixed, Start, Stop, bOwnerHigher ? OA : NA, bOwnerHigher ? OB : NB,
						bOwnerHigher ? NA : OA, bOwnerHigher ? NB : OB, bOwnerHigher ? OwnerKind : NeighborKind, TexturePeriod, InCapacity);
				};
				const double FirstDifference = OwnerA - NeighborA, LastDifference = OwnerB - NeighborB;
				if (FirstDifference * LastDifference < 0.0)
				{
					const double Alpha = FirstDifference / (FirstDifference - LastDifference);
					const double Cross = FMath::Lerp(Position, End, Alpha);
					const double Height = FMath::Lerp(OwnerA, OwnerB, Alpha);
					Append(Position, Cross, OwnerA, Height, NeighborA, Height);
					Append(Cross, End, Height, OwnerB, Height, NeighborB);
				}
				else Append(Position, End, OwnerA, OwnerB, NeighborA, NeighborB);
			}
			Position = End;
		}
	}
	for (const FVoxelRenderBatch& Batch : Mesh.Batches)
	{
		if (!Batch.Mesh.Validate())
		{
			OutError = TEXT("Heightfield transition mesh validation failed");
			return false;
		}
	}
	if (!Mesh.ValidateAndCache()) { OutError = TEXT("Invalid heightfield seam mesh"); return false; }
	OutMesh = MoveTemp(Mesh);
	OutError.Reset();
	return true;
}

FVoxelGameThreadContinuation FVoxelHeightfieldTransitionBuilder::GatherOwnerDependenciesSteps(TConstArrayView<FVoxelHeightfieldTileView> InTiles,
	TConstArrayView<FVoxelHeightfieldTileFootprint> InPendingTiles, TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldNodeKey>>& OutDependencies)
{
	OutDependencies.Reset();
	FEdgeIndex NegativeX;
	FEdgeIndex PositiveX;
	FEdgeIndex NegativeY;
	FEdgeIndex PositiveY;
	for (const auto& View : InTiles)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		OutDependencies.FindOrAdd(View.Key).Add(View.Key);
		IndexFootprint(
			View.Key, View.Origin, View.CellSide() * View.Step, View.Step, View.Revision, NegativeX, PositiveX, NegativeY, PositiveY, View.GeometryRevision);
	}
	for (const auto& Tile : InPendingTiles)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		IndexFootprint(Tile.Key, Tile.Origin, Tile.TileSide, Tile.Step, 0, NegativeX, PositiveX, NegativeY, PositiveY);
	}
	const auto Gather = [&OutDependencies](const FEdgeIndex& Positive, const FEdgeIndex& Negative) -> FVoxelGameThreadContinuation
	{
		for (const auto& Pair : Positive)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const auto* Across = Negative.Find(Pair.Key);
			if (!Across)
				continue;
			for (const auto& A : Pair.Value)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				for (const auto& B : *Across)
				{
					co_await FVoxelGameThreadContinuation::Yield();
					if (FMath::Max(A.RangeMin, B.RangeMin) >= FMath::Min(A.RangeMax, B.RangeMax))
						continue;
					if (auto* Keys = OutDependencies.Find(A.Key))
						Keys->AddUnique(B.Key);
					if (auto* Keys = OutDependencies.Find(B.Key))
						Keys->AddUnique(A.Key);
				}
			}
		}
	};
	co_await Gather(PositiveX, NegativeX);
	co_await Gather(PositiveY, NegativeY);
	for (auto& Pair : OutDependencies)
		Pair.Value.Sort(KeyLess);
}
