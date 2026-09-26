#include "Voxel/Rendering/DWHeightfieldTransitionBuilder.h"

#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Runtime/VoxelRegistry.h"

namespace
{
	struct FHeightfieldEdgeRecord
	{
		FVoxelHeightfieldNodeKey Key;
		int32 RangeMin = 0;
		int32 RangeMax = 0;
		int32 Step = 1;
		uint64 Revision = 0;
	};

	using FEdgeIndex = TMap<int32, TArray<FHeightfieldEdgeRecord>>;

	void IndexFootprint(const FVoxelHeightfieldNodeKey& InKey,
		const FIntPoint& InOrigin, const int32 InTileSide,
		const int32 InStep, const uint64 InRevision,
		FEdgeIndex& OutNegativeX, FEdgeIndex& OutPositiveX,
		FEdgeIndex& OutNegativeY, FEdgeIndex& OutPositiveY)
	{
		const FIntPoint Max = InOrigin + FIntPoint(InTileSide, InTileSide);
		const FHeightfieldEdgeRecord XRecord{InKey, InOrigin.Y, Max.Y,
			InStep, InRevision};
		const FHeightfieldEdgeRecord YRecord{InKey, InOrigin.X, Max.X,
			InStep, InRevision};
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
					if (A.Step == B.Step) continue;
					const int32 Min = FMath::Max(A.RangeMin, B.RangeMin);
					const int32 Max = FMath::Min(A.RangeMax, B.RangeMax);
					if (Min >= Max) continue;
					const bool bPositiveOwns = A.Step > B.Step;
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
						Edge.bUnbalanced = Edge.OwnerStep / Edge.NeighborStep > 2;
				}
			}
		}
	}

	uint64 MixSignature(const uint64 InCurrent, const uint64 InValue)
	{
		return VoxelGeneration::Mix(InCurrent ^ VoxelGeneration::Mix(InValue));
	}
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
	View.Ground = InData.GroundZ;
	View.Water = InData.WaterZ;
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
	View.Ground = InData.Height;
	View.Water = InData.WaterHeight;
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
			View.Water.Num() != Count || View.Material.Num() != Count)
		{
			OutError = TEXT("Heightfield transition tile view is invalid");
			return false;
		}
		IndexFootprint(View.Key, View.Origin, View.CellSide() * View.Step,
			View.Step, View.Revision,
			NegativeX, PositiveX, NegativeY, PositiveY);
	}
	MatchEdges(PositiveX, NegativeX, EVoxelLodEdgeDirection::PositiveX,
		EVoxelLodEdgeDirection::NegativeX, OutEdges);
	MatchEdges(PositiveY, NegativeY, EVoxelLodEdgeDirection::PositiveY,
		EVoxelLodEdgeDirection::NegativeY, OutEdges);
	OutEdges.Sort(EdgeLess);
	OutError.Reset();
	return true;
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
			ReadyNegativeX, ReadyPositiveX, ReadyNegativeY, ReadyPositiveY);
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
		const EVoxelRenderGroup InGroup, const uint16 InBank)
	{
		for (FVoxelRenderBatch& Batch : InOutMesh.Batches)
		{
			if (Batch.Group == InGroup && Batch.Bank == InBank) return Batch;
		}
		FVoxelRenderBatch& Batch = InOutMesh.Batches.AddDefaulted_GetRef();
		Batch.Group = InGroup;
		Batch.Bank = InBank;
		return Batch;
	}

	void AppendFace(FVoxelSectionMeshResult& InOutMesh,
		const FVoxelRuntimeDefinition& InDefinition,
		const FVoxelRuntimeFaceRef& InFace,
		const EVoxelLodEdgeDirection InDirection,
		const FVoxelHeightfieldTileView& InOwner,
		const int32 InWorldFixed, const int32 InStart, const int32 InEnd,
		const double InBottom, const double InTop,
		const double InTexturePeriod)
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
			InDefinition.RenderGroup, InFace.Bank).Mesh;
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
	const TAtomic<bool>* InCancel)
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
			Edge.OwnerRevision != InOwner.Revision ||
			InOwner.Ground.Num() != InOwner.Side * InOwner.Side ||
			InOwner.Water.Num() != InOwner.Ground.Num() ||
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
			constexpr int32 SafetySkirtDepthCells = 2;
			const double TexturePeriod = VoxelViewLod::TexturePeriodCells(
				InOwner.Step, InMaximumTextureStretchCells);
			for (int32 Position = Edge.RangeMin; Position < Edge.RangeMax;
				Position += InOwner.Step)
			{
				const int32 End = FMath::Min(Position + InOwner.Step, Edge.RangeMax);
				const int32 Index = EdgeCellIndex(InOwner, Edge.Direction, Position);
				const int32 Ground = InOwner.Ground[Index];
				if (Ground != MIN_int32)
				{
					FVoxelBlockState State;
					if (!InConfig.ToRuntime(InOwner.Material[Index], State))
					{
						OutError = TEXT("Pending heightfield edge material is invalid");
						return false;
					}
					const FVoxelRuntimeDefinition* Definition = InRegistry.Find(State.TypeId);
					if (!Definition)
					{
						OutError = TEXT("Pending heightfield edge material is absent from registry");
						return false;
					}
					const double Top = Ground + 1.0 + InOwner.ZBias;
					AppendFace(Mesh, *Definition,
						Definition->Face(State.State, FaceIndex(Edge.Direction)),
						Edge.Direction, InOwner, Fixed, Position, End,
						Top - SafetySkirtDepthCells, Top, TexturePeriod);
				}
				const int32 Water = InOwner.Water[Index];
				if (Water == MIN_int32 || (Ground != MIN_int32 && Water < Ground)) continue;
				const double WaterTop = Water + 1.0;
				const double GroundTop = Ground == MIN_int32
					? static_cast<double>(InMinimumGroundZ) : Ground + 1.0;
				const double Bottom = FMath::Max(GroundTop, WaterTop - SafetySkirtDepthCells);
				if (Bottom >= WaterTop) continue;
				const FVoxelRuntimeDefinition* Definition = InRegistry.Find(InConfig.Water.TypeId);
				if (!Definition)
				{
					OutError = TEXT("Pending heightfield water is absent from registry");
					return false;
				}
				AppendFace(Mesh, *Definition,
					Definition->Face(InConfig.Water.State, FaceIndex(Edge.Direction)),
					Edge.Direction, InOwner, Fixed, Position, End,
					Bottom, WaterTop, TexturePeriod);
			}
			continue;
		}
		const FVoxelHeightfieldTileView* Neighbor = InViews.Find(Edge.Neighbor);
		if (!Neighbor || Neighbor->Side < 2 || InOwner.Step <= Neighbor->Step ||
			Neighbor->Step <= 0 || InOwner.Step % Neighbor->Step != 0 ||
			!FMath::IsPowerOfTwo(InOwner.Step / Neighbor->Step) ||
			Edge.NeighborStep != Neighbor->Step ||
			Edge.NeighborRevision != Neighbor->Revision ||
			Neighbor->Ground.Num() != Neighbor->Side * Neighbor->Side ||
			Neighbor->Water.Num() != Neighbor->Ground.Num() ||
			Neighbor->Material.Num() != Neighbor->Ground.Num())
		{
			OutError = TEXT("Heightfield transition edge has stale or invalid ownership");
			return false;
		}
		const EVoxelLodEdgeDirection NeighborDirection = OppositeDirection(Edge.Direction);
		const double TexturePeriod = VoxelViewLod::TexturePeriodCells(
			Neighbor->Step, InMaximumTextureStretchCells);
		for (int32 Position = Edge.RangeMin; Position < Edge.RangeMax;
			Position += Neighbor->Step)
		{
			const int32 End = FMath::Min(Position + Neighbor->Step, Edge.RangeMax);
			const int32 OwnerIndex = EdgeCellIndex(InOwner, Edge.Direction, Position);
			const int32 NeighborIndex = EdgeCellIndex(*Neighbor, NeighborDirection, Position);
			const int32 OwnerGround = InOwner.Ground[OwnerIndex];
			const int32 NeighborGround = Neighbor->Ground[NeighborIndex];
			if (OwnerGround != MIN_int32 || NeighborGround != MIN_int32)
			{
				const bool bOwnerHigher = NeighborGround == MIN_int32 ||
					(OwnerGround != MIN_int32 &&
						static_cast<double>(OwnerGround) + InOwner.ZBias >
						static_cast<double>(NeighborGround) + Neighbor->ZBias);
				const FVoxelHeightfieldTileView& Higher = bOwnerHigher ? InOwner : *Neighbor;
				const int32 HigherIndex = bOwnerHigher ? OwnerIndex : NeighborIndex;
				const int32 LowerGround = bOwnerHigher ? NeighborGround : OwnerGround;
				const double Top = Higher.Ground[HigherIndex] + 1.0 + Higher.ZBias;
				const double Bottom = LowerGround == MIN_int32 ? InMinimumGroundZ :
					LowerGround + 1.0 + (bOwnerHigher ? Neighbor->ZBias : InOwner.ZBias);
				if (Bottom < Top - UE_DOUBLE_SMALL_NUMBER)
				{
					FVoxelBlockState State;
					const uint16 Symbol = Higher.Material[HigherIndex];
					if (!InConfig.ToRuntime(Symbol, State))
					{
						OutError = TEXT("Heightfield transition material symbol is invalid");
						return false;
					}
					const FVoxelRuntimeDefinition* Definition = InRegistry.Find(State.TypeId);
					if (!Definition)
					{
						OutError = TEXT("Heightfield transition material is absent from registry");
						return false;
					}
					const EVoxelLodEdgeDirection FaceDirection = bOwnerHigher
						? Edge.Direction : NeighborDirection;
					AppendFace(Mesh, *Definition,
						Definition->Face(State.State, FaceIndex(FaceDirection)),
						FaceDirection, InOwner, Fixed, Position, End, Bottom, Top,
						TexturePeriod);
				}
			}
			const int32 OwnerWater = InOwner.Water[OwnerIndex];
			const int32 NeighborWater = Neighbor->Water[NeighborIndex];
			const bool bOwnerWet = OwnerWater != MIN_int32 &&
				(OwnerGround == MIN_int32 || OwnerWater >= OwnerGround);
			const bool bNeighborWet = NeighborWater != MIN_int32 &&
				(NeighborGround == MIN_int32 || NeighborWater >= NeighborGround);
			if (!bOwnerWet && !bNeighborWet) continue;
			const bool bOwnerWaterHigher = bOwnerWet &&
				(!bNeighborWet || OwnerWater > NeighborWater);
			const double WaterTop = (bOwnerWaterHigher ? OwnerWater : NeighborWater) + 1.0;
			auto GroundTop = [InMinimumGroundZ](const int32 Ground)
			{
				return Ground == MIN_int32 ? static_cast<double>(InMinimumGroundZ)
					: static_cast<double>(Ground) + 1.0;
			};
			const double WaterBottom = bOwnerWet && bNeighborWet
				? FMath::Min(OwnerWater, NeighborWater) + 1.0
				: FMath::Max(GroundTop(OwnerGround), GroundTop(NeighborGround));
			if (WaterBottom >= WaterTop) continue;
			const FVoxelRuntimeDefinition* WaterDefinition =
				InRegistry.Find(InConfig.Water.TypeId);
			if (!WaterDefinition)
			{
				OutError = TEXT("Heightfield transition water is absent from registry");
				return false;
			}
			const EVoxelLodEdgeDirection WaterDirection = bOwnerWaterHigher
				? Edge.Direction : NeighborDirection;
			AppendFace(Mesh, *WaterDefinition,
				WaterDefinition->Face(InConfig.Water.State, FaceIndex(WaterDirection)),
				WaterDirection, InOwner, Fixed, Position, End, WaterBottom,
				WaterTop, TexturePeriod);
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
	OutMesh = MoveTemp(Mesh);
	OutError.Reset();
	return true;
}
