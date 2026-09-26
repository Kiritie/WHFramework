#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Rendering/VoxelHeightfieldMesher.h"
#include "Voxel/Rendering/DWHeightfieldTransitionBuilder.h"
#include "Voxel/Rendering/DWVolumeTransitionPlanner.h"
#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"
#include "Voxel/Rendering/VoxelMeshClipper.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

namespace
{
	class FVoxelTestOverlaySource final : public IVoxelOverlaySource
	{
	public:
		virtual bool EnumerateModifiedSections(
			const FVoxelGenerationBounds& InBounds,
			TArray<FIntVector>& OutSections,
			const TAtomic<bool>* InCancel = nullptr) const override
		{
			for (const TPair<FIntVector, FVoxelOverlaySnapshot>& Pair : Overlays)
			{
				if (InCancel && InCancel->Load())
				{
					OutSections.Reset();
					return false;
				}
				const FVoxelGenerationBounds SectionBounds{
					Pair.Key * VoxelBlock::Size,
					(Pair.Key + FIntVector(1)) * VoxelBlock::Size
				};
				if (SectionBounds.Intersects(InBounds))
				{
					OutSections.Add(Pair.Key);
				}
			}
			return true;
		}

		virtual bool ReadOverlay(
			const FIntVector& InSection,
			FVoxelOverlaySnapshot& OutOverlay) const override
		{
			const FVoxelOverlaySnapshot* Found = Overlays.Find(InSection);
			if (!Found)
			{
				return false;
			}
			OutOverlay = *Found;
			return true;
		}

		void Set(const FIntVector& InPosition, const FVoxelBlockState InState)
		{
			const FIntVector Section(
				VoxelGeneration::FloorDivide(InPosition.X, VoxelBlock::Size),
				VoxelGeneration::FloorDivide(InPosition.Y, VoxelBlock::Size),
				VoxelGeneration::FloorDivide(InPosition.Z, VoxelBlock::Size));
			auto PositiveMod = [](const int32 InValue)
			{
				const int32 Value = InValue % VoxelBlock::Size;
				return Value < 0 ? Value + VoxelBlock::Size : Value;
			};
			const int32 Index =
				PositiveMod(InPosition.X) +
				PositiveMod(InPosition.Y) * VoxelBlock::Size +
				PositiveMod(InPosition.Z) * VoxelBlock::Size * VoxelBlock::Size;
			FVoxelOverlaySnapshot& Overlay = Overlays.FindOrAdd(Section);
			Overlay.Section = Section;
			Overlay.Revision = 1;
			Overlay.Blocks.Add(Index, InState);
		}

	private:
		TMap<FIntVector, FVoxelOverlaySnapshot> Overlays;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownEmptyMeshTest,
	"WHFramework.Voxel.Rendering.KnownEmptyMesh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownEmptyMeshTest::RunTest(const FString& Parameters)
{
	FVoxelRegistrySnapshot Registry = *VoxelTest::MakeRegistry();
	Registry.Definitions[1].Shape = EVoxelShapeKind::FullCube;
	Registry.Definitions[1].bOccludes = true;
	FVoxelSectionSnapshot Snapshot;
	Snapshot.Blocks.Init(0, VoxelBlock::Volume);
	TestTrue(TEXT("Air is ready without any neighbor data or mesh task"), FVoxelSectionMesher::IsKnownEmpty(Snapshot, Registry));
	const uint32 Solid = FVoxelBlockState(1, 0).Pack();
	Snapshot.Blocks.Init(Solid, VoxelBlock::Volume);
	TestFalse(TEXT("Unknown boundaries cannot prove solid geometry is enclosed"), FVoxelSectionMesher::IsKnownEmpty(Snapshot, Registry));
	for (int32 Face = 0; Face < 6; ++Face)
	{
		Snapshot.Known[Face] = true;
		Snapshot.Halo[Face].Init(Solid, 256);
	}
	TestTrue(TEXT("Fully enclosed solid is ready without meshing"), FVoxelSectionMesher::IsKnownEmpty(Snapshot, Registry));
	FVoxelBoundaryTransitionContext Transition;
	Transition.Owner = {FIntVector::ZeroValue, 1};
	FVoxelBoundaryTransitionPatch& Patch = Transition.Patches.AddDefaulted_GetRef();
	Patch.Face.Owner = Transition.Owner;
	Patch.Face.Neighbor = {FIntVector(2, 0, 0), 0};
	Patch.Face.Direction = EVoxelVolumeFaceDirection::PositiveX;
	Patch.Face.Min = Patch.Face.Neighbor.GetBounds().Min;
	Patch.Face.Max = Patch.Face.Neighbor.GetBounds().Max;
	Patch.Neighbor.Key = Patch.Face.Neighbor;
	Patch.Neighbor.Face = 1;
	Patch.Neighbor.States.Init(Solid, 256);
	TestTrue(TEXT("Empty check uses a valid fine boundary"), Transition.Validate());
	TestTrue(TEXT("Enclosed fine boundary skips transition meshing"),
		FVoxelSectionMesher::IsKnownEmpty(Snapshot, Registry, &Transition));
	Patch.Neighbor.States[17] = 0;
	TestFalse(TEXT("Fine boundary exposure must not be hidden by solid coarse halo"),
		FVoxelSectionMesher::IsKnownEmpty(Snapshot, Registry, &Transition));
	FVoxelShapeRegistry Shapes;
	FVoxelSectionMeshResult Mesh;
	TestTrue(TEXT("Fine exposure builds actual transition geometry"),
		FVoxelSectionMesher::Build(Snapshot, Registry, Shapes, Mesh, nullptr, 1.0, &Transition));
	TestTrue(TEXT("Fine exposure is not an empty transition"), !Mesh.Batches.IsEmpty());
	for (int32 Face = 0; Face < 6; ++Face)
	{
		Snapshot.Halo[Face][17] = 0;
		TestFalse(TEXT("An exposed face in every direction requires geometry"), FVoxelSectionMesher::IsKnownEmpty(Snapshot, Registry));
		Snapshot.Halo[Face][17] = Solid;
	}
	Snapshot.Blocks[2048] = 0;
	TestFalse(TEXT("An internal cavity must not be discarded"), FVoxelSectionMesher::IsKnownEmpty(Snapshot, Registry));
	Snapshot.Blocks[2048] = Solid;
	Registry.Definitions[1].bOccludes = false;
	TestFalse(TEXT("Nonoccluding materials still require meshing"), FVoxelSectionMesher::IsKnownEmpty(Snapshot, Registry));
	return true;
}

namespace
{
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> MakeSurfaceRegistry()
	{
		FVoxelRegistrySnapshot Registry = *VoxelTest::MakeRegistry();
		for (FVoxelRuntimeDefinition& Definition : Registry.Definitions)
		{
			Definition.bSolid = Definition.TypeId != 0 && Definition.TypeId != 6 && Definition.TypeId != 7;
		}
		return MakeShared<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe>(MoveTemp(Registry));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelMeshCoverageClipTest,
	"WHFramework.Voxel.Rendering.CoverageClip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMeshCoverageClipTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Registry = VoxelTest::MakeRegistry();
	const TArray<int32> Heights = { 10, 10, 10, 10 };
	const TArray<uint16> Materials = { 1, 1, 1, 1 };
	FVoxelSectionMeshResult Source;
	FString Error;
	if (!TestTrue(TEXT("Coarse test surface builds"), FVoxelHeightfieldMesher::BuildBlockyTerrain(
		2, 8, Heights, Materials, *Config, *Registry, Source, Error))) return false;
	TArray<FBox> Boxes = { FBox(FVector(2, 2, -100), FVector(6, 6, 100)) };
	auto Check = [this, &Source, &Boxes](const double ExpectedArea)
	{
		FVoxelSectionMeshResult Clipped;
		VoxelMeshClipper::Subtract(Source, Boxes, Clipped);
		double Area = 0.0;
		for (const FVoxelRenderBatch& Batch : Clipped.Batches)
		{
			const FVoxelMeshBuffers& Mesh = Batch.Mesh;
			TestTrue(TEXT("Clipped mesh attributes remain valid"), Mesh.Validate());
			for (int32 Index = 0; Index < Mesh.Vertices.Num(); ++Index)
			{
				TestTrue(TEXT("Clipping preserves the coarse face texture mapping"),
					Mesh.UV0[Index].Equals(FVector2D(Mesh.Vertices[Index].X, Mesh.Vertices[Index].Y) / 4.0));
			}
			for (int32 Triangle = 0; Triangle < Mesh.Triangles.Num(); Triangle += 3)
			{
				const FVector& A = Mesh.Vertices[Mesh.Triangles[Triangle]];
				const FVector& B = Mesh.Vertices[Mesh.Triangles[Triangle + 1]];
				const FVector& C = Mesh.Vertices[Mesh.Triangles[Triangle + 2]];
				Area += FVector::CrossProduct(B - A, C - A).Size() * 0.5;
				for (const FBox& Box : Boxes)
				{
					TestFalse(TEXT("No retained triangle lies in finer coverage"), Box.IsInside((A + B + C) / 3.0));
				}
			}
		}
		TestTrue(TEXT("Only covered area is removed, remainder has no holes"), FMath::IsNearlyEqual(Area, ExpectedArea));
	};
	Check(48.0);
	Boxes.Add(FBox(FVector(4, 0, -100), FVector(8, 8, 100)));
	Check(24.0);
	Boxes = { FBox(FVector(0, 0, -100), FVector(8, 8, 100)) };
	Check(0.0);
	Boxes.Reset();
	Check(64.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelCoverageResultBudgetTest,
	"WHFramework.Voxel.Rendering.CoverageResultBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoverageResultBudgetTest::RunTest(const FString& InParameters)
{
	const auto Source = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	Source->Batches.AddDefaulted_GetRef().Mesh.Vertices.SetNum(4096);
	const auto Payload = MakeShared<FVoxelViewCoverageResult, ESPMode::ThreadSafe>();
	Payload->Meshes.Add(Source);
	FVoxelTaskResult Result;
	Result.CustomPayload = Payload;
	TestTrue(TEXT("Reusing a large mesh fits the publisher's minimum task reservation"), Result.ResultBytes() <= 1024);
	const uint64 SharedBytes = Result.ResultBytes();
	Payload->OwnedMeshBytes = Source->Bytes();
	TestEqual(TEXT("Newly clipped geometry is charged to the scheduler result budget"),
		Result.ResultBytes(), SharedBytes + Source->Bytes());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelEmptyRepresentationReadinessTest,
	"WHFramework.Voxel.Rendering.EmptyRepresentationReadiness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEmptyRepresentationReadinessTest::RunTest(const FString& InParameters)
{
	FVoxelPrimaryFineReadiness State;
	State.Required = 2;
	State.Ready = 2;
	TestFalse(TEXT("Mesh data alone does not complete presentation"), State.IsComplete());
	State.Presented = 1;
	State.Renderable = 1;
	TestFalse(TEXT("A visible neighbour does not hide an unfinished replacement"), State.IsComplete());
	State.Presented = 2;
	State.Renderable = 0;
	TestTrue(TEXT("Successfully committed empty regions complete readiness"), State.IsComplete());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelUnknownBoundaryTest,
	"WHFramework.Voxel.Rendering.UnknownBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelUnknownBoundaryTest::RunTest(const FString& InParameters)
{
	FVoxelRegistrySnapshot Registry = *VoxelTest::MakeRegistry();
	Registry.Definitions[1].bSolid = true;
	Registry.Definitions[1].bOccludes = true;
	FVoxelShapeRegistry Shapes;
	Shapes.BuildDefaults();
	FVoxelSectionSnapshot Snapshot;
	Snapshot.Blocks.Init(FVoxelBlockState(1, 0).Pack(), 4096);
	auto CountTriangles = [this, &Registry, &Shapes, &Snapshot]()
	{
		FVoxelSectionMeshResult Mesh;
		TestTrue(TEXT("Boundary test mesh builds"), FVoxelSectionMesher::Build(Snapshot, Registry, Shapes, Mesh));
		int32 Count = 0;
		for (const FVoxelRenderBatch& Batch : Mesh.Batches) Count += Batch.Mesh.Triangles.Num() / 3;
		return Count;
	};
	TestEqual(TEXT("Unknown neighbours do not produce an enclosing box"), CountTriangles(), 0);
	Snapshot.Known[4] = true;
	Snapshot.Halo[4].Init(0, 256);
	TestEqual(TEXT("Only the confirmed air side is rendered"), CountTriangles(), 2);
	Snapshot.Halo[4].Init(FVoxelBlockState(1, 0).Pack(), 256);
	TestEqual(TEXT("Confirmed solid neighbour suppresses the shared face"), CountTriangles(), 0);
	Snapshot.Blocks[8 + 8 * 16 + 8 * 256] = 0;
	TestEqual(TEXT("Known internal air cavity still renders its six sides"), CountTriangles(), 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoarseTextureScaleTest, "WHFramework.Voxel.Rendering.CoarseTextureScale", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoarseTextureScaleTest::RunTest(const FString& InParameters)
{
	FVoxelRegistrySnapshot Registry = *VoxelTest::MakeRegistry();
	Registry.Definitions[1].bSolid = true;
	Registry.Definitions[1].bOccludes = true;
	FVoxelShapeRegistry Shapes;
	Shapes.BuildDefaults();
	FVoxelSectionSnapshot Snapshot;
	Snapshot.Blocks.Init(FVoxelBlockState(1, 0).Pack(), 4096);
	for (int32 Face = 0; Face < 6; ++Face)
	{
		Snapshot.Known[Face] = true;
		Snapshot.Halo[Face].Init(0, 256);
	}
	FVoxelSectionMeshResult Fine;
	FVoxelSectionMeshResult Coarse;
	TestTrue(TEXT("Fine mesh builds"), FVoxelSectionMesher::Build(Snapshot, Registry, Shapes, Fine));
	TestTrue(TEXT("Coarse mesh builds"), FVoxelSectionMesher::Build(Snapshot, Registry, Shapes, Coarse, nullptr, 8.0 / VoxelViewLod::TexturePeriodCells(8, 4.0)));
	bool bFineRepeats = false;
	for (const FVoxelRenderBatch& Batch : Fine.Batches)
	{
		for (const FVector2D& UV : Batch.Mesh.UV0)
		{
			bFineRepeats |= UV.X > 1.0 || UV.Y > 1.0;
		}
	}
	TestTrue(TEXT("Fine mesh retains per-cell texture detail"), bFineRepeats);
	for (const FVoxelRenderBatch& Batch : Coarse.Batches)
	{
		for (const FVector2D& UV : Batch.Mesh.UV0)
		{
			TestTrue(TEXT("Coarse texture period is bounded rather than stretched across the merged face"), UV.X >= 0.0 && UV.X <= 32.0 && UV.Y >= 0.0 && UV.Y <= 32.0);
		}
	}
	TestEqual(TEXT("Greedy merge preserves per-cell UV extent"), Coarse.Batches[0].Mesh.UV0[1].Size(), Fine.Batches[0].Mesh.UV0[1].Size() * 2.0);
	TestEqual(TEXT("LOD step four uses one tile per coarse cell"), VoxelViewLod::TexturePeriodCells(4, 4.0), 4.0);
	TestEqual(TEXT("Very coarse geometry cannot increase texture stretching"), VoxelViewLod::TexturePeriodCells(256, 4.0), 4.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelProxyOverlaySnapshotTest,
	"WHFramework.Voxel.Rendering.ProxyOverlaySnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelProxyOverlaySnapshotTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelOverlaySnapshotSet Overlays;
	for (const FIntVector Section : { FIntVector(-2, 0, 0), FIntVector(0, 0, 0) })
	{
		FVoxelOverlaySnapshot& Overlay = Overlays.Sections.FindOrAdd(Section);
		Overlay.Section = Section;
		Overlay.Blocks.Add(272, FVoxelBlockState());
		Overlay.Blocks.Add(273, FVoxelBlockState());
	}
	const FVoxelOverlaySnapshotSet Captured = Overlays;
	Overlays.Sections.FindChecked(FIntVector(-2, 0, 0)).Blocks[273] = FVoxelBlockState(1, 0);
	FVoxelVoxelProxyData Data;
	FString Error;
	const FVoxelVoxelProxyBuilder Builder(Config, Cache);
	if (!TestTrue(TEXT("Proxy builds from immutable edits"),
		Builder.Build({ FIntVector(-1, 0, 0), 1 }, Captured, Data, Error))) return false;
	TestTrue(TEXT("Negative-coordinate edit comes from captured version"), Data.Cells[0].IsAir());
	TestTrue(TEXT("Neighbour halo uses the same edit snapshot"), Data.Halo[0][0].IsAir());

	FVoxelOverlaySnapshotSet Placed;
	FVoxelOverlaySnapshot& AboveGround = Placed.Sections.FindOrAdd(FIntVector(0, 0, 128));
	AboveGround.Section = FIntVector(0, 0, 128);
	AboveGround.Blocks.Add(0, FVoxelBlockState(1, 0));
	FVoxelVoxelProxyData Natural;
	FVoxelVoxelProxyData Built;
	const FVoxelViewKey ElevatedKey{ FIntVector(0, 0, 64), 1 };
	if (!TestTrue(TEXT("Elevated natural proxy builds"),
		Builder.BuildNatural(ElevatedKey, Natural, Error))) return false;
	if (!TestTrue(TEXT("Elevated edited proxy builds"),
		Builder.Build(ElevatedKey, Placed, Built, Error))) return false;
	TestTrue(TEXT("Elevated natural cell is air"), Natural.Cells[0].IsAir());
	TestEqual(TEXT("Off-center player block contributes to distant silhouette"),
		Built.Cells[0].Pack(), FVoxelBlockState(1, 0).Pack());
	FVoxelOverlaySnapshotSet CoarseBuilding;
	FVoxelOverlaySnapshot& BuildingSection =
		CoarseBuilding.Sections.FindOrAdd(FIntVector(0, 0, 8));
	BuildingSection.Section = FIntVector(0, 0, 8);
	BuildingSection.Blocks.Add(0, FVoxelBlockState(1, 0));
	BuildingSection.Blocks.Add(1, FVoxelBlockState(1, 0));
	FVoxelVoxelProxyData CoarseBuilt;
	if (!TestTrue(TEXT("Coarse player-built silhouette builds"),
		Builder.Build({ FIntVector(0, 0, 1), 3 }, CoarseBuilding,
			CoarseBuilt, Error))) return false;
	TestEqual(TEXT("Small wall fragment survives distant voxel coarsening"),
		CoarseBuilt.Cells[0].Pack(), FVoxelBlockState(1, 0).Pack());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelHeightfieldTransitionEdgeTest,
	"WHFramework.Voxel.Rendering.HeightfieldTransitionEdges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHeightfieldTransitionEdgeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TArray<int32> CoarseGround = {10, 10, 10, 10, 10, 10, 10, 10, 10};
	TArray<int32> FineGroundA = {5, 5, 5, 5, 5, 5, 5, 5, 5};
	TArray<int32> FineGroundB = {7, 7, 7, 7, 7, 7, 7, 7, 7};
	TArray<int32> Dry;
	Dry.Init(MIN_int32, 9);
	TArray<uint16> Materials;
	Materials.Init(1, 9);
	FVoxelHeightfieldTileView Coarse;
	Coarse.Key = {EVoxelHeightfieldRepresentation::Surface, FIntPoint(0, 0), 2};
	Coarse.Side = 3;
	Coarse.Step = 4;
	Coarse.Ground = CoarseGround;
	Coarse.Water = Dry;
	Coarse.Material = Materials;
	FVoxelHeightfieldTileView FineA;
	FineA.Key = {EVoxelHeightfieldRepresentation::Surface, FIntPoint(2, 0), 1};
	FineA.Origin = FIntPoint(8, 0);
	FineA.Side = 3;
	FineA.Step = 2;
	FineA.Ground = FineGroundA;
	FineA.Water = Dry;
	FineA.Material = Materials;
	FVoxelHeightfieldTileView FineB = FineA;
	FineB.Key.Coordinate = FIntPoint(2, 1);
	FineB.Origin = FIntPoint(8, 4);
	FineB.Ground = FineGroundB;
	TArray<FVoxelHeightfieldTileView> Views = {Coarse, FineA, FineB};
	TArray<FVoxelHeightfieldTransitionEdge> Edges;
	FString Error;
	if (!TestTrue(TEXT("Two-to-one heightfield edges build"),
		FVoxelHeightfieldTransitionBuilder::BuildEdges(Views, Edges, Error))) return false;
	TestEqual(TEXT("One coarse tile owns both fine boundary intervals"), Edges.Num(), 2);
	for (const FVoxelHeightfieldTransitionEdge& Edge : Edges)
	{
		TestTrue(TEXT("The coarser tile owns each interval"), Edge.Owner == Coarse.Key);
		TestEqual(TEXT("Boundary direction is +X"),
			static_cast<uint8>(Edge.Direction),
			static_cast<uint8>(EVoxelLodEdgeDirection::PositiveX));
	}
	TMap<FVoxelHeightfieldNodeKey, FVoxelHeightfieldTileView> ViewMap;
	for (const FVoxelHeightfieldTileView& View : Views) ViewMap.Add(View.Key, View);
	FVoxelSectionMeshResult Mesh;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Registry = VoxelTest::MakeRegistry();
	if (!TestTrue(TEXT("Coarse owner transition mesh builds"),
		FVoxelHeightfieldTransitionBuilder::BuildMesh(Coarse, Edges, ViewMap,
			*Config, *Registry, -64, 4.0, Mesh, Error))) return false;
	int32 GroundVertices = 0;
	for (const FVoxelRenderBatch& Batch : Mesh.Batches)
	{
		TestTrue(TEXT("Transition mesh attributes are complete"), Batch.Mesh.Validate());
		GroundVertices += Batch.Mesh.Vertices.Num();
	}
	TestEqual(TEXT("Both fine edges are closed at fine-cell resolution"),
		GroundVertices, 16);
	const uint64 Signature = FVoxelHeightfieldTransitionBuilder::BuildSignature(
		Coarse.Key, Coarse.Revision, Edges);
	Edges.Swap(0, 1);
	TestEqual(TEXT("Transition signature is independent of neighbor enumeration order"),
		FVoxelHeightfieldTransitionBuilder::BuildSignature(
			Coarse.Key, Coarse.Revision, Edges), Signature);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelHeightfieldTransitionInvariantTest,
	"WHFramework.Voxel.Rendering.HeightfieldTransitionInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHeightfieldTransitionInvariantTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TArray<int32> HighGround;
	HighGround.Init(10, 9);
	TArray<int32> LowGround;
	LowGround.Init(5, 9);
	TArray<int32> OwnerWater;
	OwnerWater.Init(12, 9);
	TArray<int32> Dry;
	Dry.Init(MIN_int32, 9);
	TArray<uint16> Material;
	Material.Init(1, 9);
	FVoxelHeightfieldTileView Owner;
	Owner.Key = {EVoxelHeightfieldRepresentation::Macro, FIntPoint(0, 0), 0};
	Owner.Side = 3;
	Owner.Step = 8;
	Owner.Ground = HighGround;
	Owner.Water = OwnerWater;
	Owner.Material = Material;
	FVoxelHeightfieldTileView Neighbor;
	Neighbor.Key = {EVoxelHeightfieldRepresentation::Surface, FIntPoint(4, 0), 0};
	Neighbor.Origin = FIntPoint(16, 0);
	Neighbor.Side = 3;
	Neighbor.Step = 2;
	Neighbor.Ground = LowGround;
	Neighbor.Water = Dry;
	Neighbor.Material = Material;
	TArray<FVoxelHeightfieldTileView> Views = {Owner, Neighbor};
	TArray<FVoxelHeightfieldTransitionEdge> Edges;
	FString Error;
	if (!TestTrue(TEXT("Temporary four-to-one edge is planned"),
		FVoxelHeightfieldTransitionBuilder::BuildEdges(Views, Edges, Error))) return false;
	TestEqual(TEXT("Only the touching interval needs a transition"), Edges.Num(), 1);
	if (Edges.Num() != 1) return false;
	TestTrue(TEXT("The coarser side owns the temporary transition"), Edges[0].Owner == Owner.Key);
	TestTrue(TEXT("Temporary ratio above two is recorded"), Edges[0].bUnbalanced);
	const uint64 Signature = FVoxelHeightfieldTransitionBuilder::BuildSignature(
		Owner.Key, Owner.Revision, Edges);
	Edges[0].NeighborRevision++;
	TestTrue(TEXT("Neighbor edit changes the transition signature"),
		FVoxelHeightfieldTransitionBuilder::BuildSignature(
			Owner.Key, Owner.Revision, Edges) != Signature);
	Edges[0].NeighborRevision--;
	TMap<FVoxelHeightfieldNodeKey, FVoxelHeightfieldTileView> ViewMap;
	ViewMap.Add(Owner.Key, Owner);
	ViewMap.Add(Neighbor.Key, Neighbor);
	FVoxelSectionMeshResult Mesh;
	const auto Config = VoxelTest::MakeGenerationConfig();
	FVoxelRegistrySnapshot Registry = *VoxelTest::MakeRegistry();
	Registry.Definitions[Config->Water.TypeId].RenderGroup = EVoxelRenderGroup::Water;
	if (!TestTrue(TEXT("Ground and water transition build from existing heightfields"),
		FVoxelHeightfieldTransitionBuilder::BuildMesh(Owner, Edges, ViewMap,
			*Config, Registry, -64, 4.0, Mesh, Error))) return false;
	int32 GroundVertices = 0;
	int32 WaterVertices = 0;
	for (const FVoxelRenderBatch& Batch : Mesh.Batches)
	{
		TestTrue(TEXT("Transition mesh is valid"), Batch.Mesh.Validate());
		for (const FVector& Vertex : Batch.Mesh.Vertices)
		{
			TestEqual(TEXT("Boundary vertex lies on the shared plane"), Vertex.X, 16.0);
		}
		if (Batch.Group == EVoxelRenderGroup::Water) WaterVertices += Batch.Mesh.Vertices.Num();
		else GroundVertices += Batch.Mesh.Vertices.Num();
	}
	TestEqual(TEXT("Ground closes both fine cells"), GroundVertices, 8);
	TestEqual(TEXT("Water closes both fine cells"), WaterVertices, 8);
	TArray<FVoxelHeightfieldTileView> OnlyOwner = {Owner};
	TArray<FVoxelHeightfieldTileFootprint> PendingNeighbor = {{
		Neighbor.Key, Neighbor.Origin, Neighbor.CellSide() * Neighbor.Step,
		Neighbor.Step}};
	TArray<FVoxelHeightfieldTransitionEdge> PendingEdges;
	FVoxelHeightfieldTransitionBuilder::AppendPendingEdges(
		OnlyOwner, PendingNeighbor, PendingEdges);
	TestEqual(TEXT("Only a wanted but unready neighbor receives a safety edge"),
		PendingEdges.Num(), 1);
	if (PendingEdges.Num() == 1)
	{
		TestTrue(TEXT("Loading edge is explicitly pending"), PendingEdges[0].bPending);
		TMap<FVoxelHeightfieldNodeKey, FVoxelHeightfieldTileView> OwnerOnlyMap;
		OwnerOnlyMap.Add(Owner.Key, Owner);
		FVoxelSectionMeshResult SafetyMesh;
		if (!TestTrue(TEXT("Pending edge builds a shallow ground and water wall"),
			FVoxelHeightfieldTransitionBuilder::BuildMesh(Owner, PendingEdges, OwnerOnlyMap,
				*Config, Registry, -64, 4.0, SafetyMesh, Error))) return false;
		for (const FVoxelRenderBatch& Batch : SafetyMesh.Batches)
		{
			for (const FVector& Vertex : Batch.Mesh.Vertices)
			{
				TestTrue(TEXT("Loading wall is at most two cells deep"), Vertex.Z >= 9.0);
			}
		}
		TestTrue(TEXT("Ready neighbor replaces the pending-edge signature"),
			FVoxelHeightfieldTransitionBuilder::BuildSignature(
				Owner.Key, Owner.Revision, PendingEdges) != Signature);
	}
	TArray<FBox> FineCoverage = {
		FBox(FVector(15, 1, -100), FVector(17, 3, 100))};
	FVoxelHeightfieldTransitionBuilder::ExcludeCoveredIntervals(
		Views, FineCoverage, TConstArrayView<FBox>(), Edges);
	TestEqual(TEXT("Coverage removes only its boundary interval"), Edges.Num(), 2);
	if (Edges.Num() == 2)
	{
		TestEqual(TEXT("First visible interval starts at zero"), Edges[0].RangeMin, 0);
		TestEqual(TEXT("First visible interval ends at coverage"), Edges[0].RangeMax, 1);
		TestEqual(TEXT("Second visible interval starts after coverage"), Edges[1].RangeMin, 3);
		TestEqual(TEXT("Second visible interval ends at neighbor edge"), Edges[1].RangeMax, 4);
		FVoxelSectionMeshResult ClippedMesh;
		if (!TestTrue(TEXT("Only uncovered edge segments are meshed"),
			FVoxelHeightfieldTransitionBuilder::BuildMesh(Owner, Edges, ViewMap,
				*Config, Registry, -64, 4.0, ClippedMesh, Error))) return false;
		for (const FVoxelRenderBatch& Batch : ClippedMesh.Batches)
		{
			for (const FVector& Vertex : Batch.Mesh.Vertices)
			{
				TestTrue(TEXT("Transition does not enter the Fine coverage interval"),
				Vertex.Y <= 1.0 || Vertex.Y >= 3.0);
			}
		}
	}
	Neighbor.Step = Owner.Step;
	Neighbor.Key.Coordinate = FIntPoint(2, 0);
	Views.Reset();
	Views.Add(Owner);
	Views.Add(Neighbor);
	if (!TestTrue(TEXT("Equal-resolution edge scan succeeds"),
		FVoxelHeightfieldTransitionBuilder::BuildEdges(Views, Edges, Error))) return false;
	TestTrue(TEXT("Equal-resolution neighbors add zero transition quads"), Edges.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelVolumeTransitionAdjacencyTest,
	"WHFramework.Voxel.Rendering.VolumeTransitionAdjacency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelVolumeTransitionAdjacencyTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const FVoxelViewKey Coarse{FIntVector(0, 0, 0), 1};
	TSet<FVoxelViewKey> Visible;
	Visible.Add(Coarse);
	for (int32 Y = 0; Y < 2; ++Y)
	{
		for (int32 Z = 0; Z < 2; ++Z)
		{
			Visible.Add({FIntVector(2, Y, Z), 0});
		}
	}
	TArray<FVoxelVolumeTransitionFace> Faces;
	FVoxelVolumeTransitionPlanner::Build(Visible, 3, Faces);
	TestEqual(TEXT("One coarse face meets four fine nodes"), Faces.Num(), 4);
	for (const FVoxelVolumeTransitionFace& Face : Faces)
	{
		TestTrue(TEXT("The coarse node owns every face"), Face.Owner == Coarse);
		TestEqual(TEXT("The owner face points toward +X"),
			static_cast<uint8>(Face.Direction),
			static_cast<uint8>(EVoxelVolumeFaceDirection::PositiveX));
		TestEqual(TEXT("Normal visible transition is two-to-one"), Face.Ratio, 2);
	}
	Visible.Reset();
	Visible.Add({FIntVector(0, 0, 0), 2});
	Visible.Add({FIntVector(4, 0, 0), 0});
	FVoxelVolumeTransitionPlanner::Build(Visible, 3, Faces);
	TestEqual(TEXT("Temporary four-to-one gap is still planned"), Faces.Num(), 1);
	if (Faces.Num() == 1) TestEqual(TEXT("Temporary ratio is four"), Faces[0].Ratio, 4);
	Visible.Reset();
	Visible.Add({FIntVector(-1, 0, 0), 1});
	Visible.Add({FIntVector(0, 0, 0), 0});
	FVoxelVolumeTransitionPlanner::Build(Visible, 3, Faces);
	TestEqual(TEXT("Negative-coordinate neighbors use floor division"), Faces.Num(), 1);
	Visible.Reset();
	Visible.Add({FIntVector(0, 0, 0), 1});
	Visible.Add({FIntVector(1, 0, 0), 1});
	FVoxelVolumeTransitionPlanner::Build(Visible, 3, Faces);
	TestTrue(TEXT("Equal-resolution volume neighbors need no transition"), Faces.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelVolumeTransitionMeshInvariantTest,
	"WHFramework.Voxel.Rendering.VolumeTransitionMeshInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelVolumeTransitionMeshInvariantTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelRegistrySnapshot Registry = *VoxelTest::MakeRegistry();
	Registry.Definitions[1].bSolid = true;
	Registry.Definitions[1].bOccludes = true;
	FVoxelShapeRegistry Shapes;
	Shapes.BuildDefaults();
	const FVoxelViewKey Owner{FIntVector(0, 0, 0), 1};
	const FVoxelViewKey Neighbor{FIntVector(2, 0, 0), 0};
	TSet<FVoxelViewKey> Visible;
	Visible.Add(Owner);
	Visible.Add(Neighbor);
	TArray<FVoxelVolumeTransitionFace> Faces;
	FVoxelVolumeTransitionPlanner::Build(Visible, 1, Faces);
	if (!TestEqual(TEXT("Visible coarse/fine boundary has one owner patch"), Faces.Num(), 1))
	{
		return false;
	}
	FVoxelSection Fine;
	Fine.Status = EVoxelSectionStatus::DataReady;
	Fine.Blocks.Init(FVoxelBlockState(0, 0), 4096);
	Fine.Blocks[0] = FVoxelBlockState(1, 0);
	Fine.CommittedRevision = 7;
	FVoxelBoundaryTransitionContext Context;
	Context.Owner = Owner;
	FVoxelBoundaryTransitionPatch& Patch = Context.Patches.AddDefaulted_GetRef();
	Patch.Face = Faces[0];
	if (!TestTrue(TEXT("Fine boundary is copied from the runtime section"),
		FVoxelBoundaryFaceSnapshot::CaptureFine(Neighbor, 1, Fine, Patch.Neighbor)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Captured transition footprint is valid"), Context.Validate()))
	{
		return false;
	}
	FVoxelSectionSnapshot Coarse;
	Coarse.Blocks.Init(FVoxelBlockState(0, 0).Pack(), 4096);
	Coarse.Blocks[15] = FVoxelBlockState(1, 0).Pack();
	Coarse.Known[0] = true;
	Coarse.Halo[0].Init(FVoxelBlockState(0, 0).Pack(), 256);
	auto CountBoundaryTriangles = [](const FVoxelSectionMeshResult& Mesh)
	{
		int32 Count = 0;
		for (const FVoxelRenderBatch& Batch : Mesh.Batches)
		{
			for (int32 Index = 0; Index < Batch.Mesh.Triangles.Num(); Index += 3)
			{
				const FVector& A = Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index]];
				const FVector& B = Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index + 1]];
				const FVector& C = Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index + 2]];
				if (FMath::IsNearlyEqual(A.X, 16.0) && FMath::IsNearlyEqual(B.X, 16.0) &&
					FMath::IsNearlyEqual(C.X, 16.0)) ++Count;
			}
		}
		return Count;
	};
	FVoxelSectionMeshResult Mesh;
	if (!TestTrue(TEXT("Coarse boundary with transition builds"),
		FVoxelSectionMesher::Build(Coarse, Registry, Shapes, Mesh, nullptr, 1.0, &Context)))
	{
		return false;
	}
	TestEqual(TEXT("One solid fine subface hides only its quadrant; no coarse duplicate"),
		CountBoundaryTriangles(Mesh), 6);
	const uint64 PreviousSignature = Context.Signature(3);
	Fine.Blocks[0] = FVoxelBlockState(0, 0);
	++Fine.CommittedRevision;
	TestTrue(TEXT("Boundary snapshot updates after a player edit"),
		FVoxelBoundaryFaceSnapshot::CaptureFine(Neighbor, 1, Fine, Patch.Neighbor));
	TestTrue(TEXT("Fine edit invalidates the coarse transition mesh"),
		Context.Signature(3) != PreviousSignature);
	if (!TestTrue(TEXT("Edited transition remesh builds"),
		FVoxelSectionMesher::Build(Coarse, Registry, Shapes, Mesh, nullptr, 1.0, &Context)))
	{
		return false;
	}
	TestEqual(TEXT("All four fine subfaces become visible after removal"),
		CountBoundaryTriangles(Mesh), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelProxyTransitionDirectionsTest,
	"WHFramework.Voxel.Rendering.ProxyTransitionDirections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelProxyTransitionDirectionsTest::RunTest(const FString& InParameters)
{
	FVoxelRegistrySnapshot Registry = *VoxelTest::MakeRegistry();
	Registry.Definitions[1].bSolid = true;
	Registry.Definitions[1].bOccludes = true;
	FVoxelShapeRegistry Shapes;
	Shapes.BuildDefaults();
	for (const FIntVector Origin : {FIntVector::ZeroValue, FIntVector(-2, -1, -3)})
	{
		for (uint8 Level = 2; Level <= 4; ++Level)
		{
			for (uint8 Face = 0; Face < 6; ++Face)
			{
				const FString Case = FString::Printf(TEXT("Origin=%s Level=%d Face=%d"),
					*Origin.ToString(), Level, Face);
				const int32 Axis = Face / 2;
				const FVoxelViewKey Owner{Origin, Level};
				FVoxelViewKey Neighbor{Origin * 2, static_cast<uint8>(Level - 1)};
				Neighbor.Coordinate[Axis] += (Face & 1) ? -1 : 2;
				TSet<FVoxelViewKey> Visible{Owner, Neighbor};
				TArray<FVoxelVolumeTransitionFace> Faces;
				FVoxelVolumeTransitionPlanner::Build(Visible, Level, Faces);
				if (!TestEqual(Case + TEXT(" has one shared face"), Faces.Num(), 1)) continue;
				FVoxelVoxelProxyData NeighborData;
				NeighborData.Key = Neighbor;
				NeighborData.Cells.Init(FVoxelBlockState(0, 0), 4096);
				FIntVector NeighborCell = FIntVector::ZeroValue;
				NeighborCell[Axis] = (Face & 1) ? 15 : 0;
				auto Linear = [](const FIntVector& Cell) { return Cell.X + 16 * Cell.Y + 256 * Cell.Z; };
				NeighborData.Cells[Linear(NeighborCell)] = FVoxelBlockState(1, 0);
				FVoxelBoundaryTransitionContext Context;
				Context.Owner = Owner;
				FVoxelBoundaryTransitionPatch& Patch = Context.Patches.AddDefaulted_GetRef();
				Patch.Face = Faces[0];
				if (!TestTrue(Case + TEXT(" captures proxy boundary"),
					FVoxelBoundaryFaceSnapshot::CaptureProxy(NeighborData, Face ^ 1, Patch.Neighbor))) continue;
				FVoxelSectionSnapshot Snapshot;
				Snapshot.Blocks.Init(FVoxelBlockState(0, 0).Pack(), 4096);
				FIntVector OwnerCell = FIntVector::ZeroValue;
				OwnerCell[Axis] = (Face & 1) ? 0 : 15;
				Snapshot.Blocks[Linear(OwnerCell)] = FVoxelBlockState(1, 0).Pack();
				Snapshot.Known[Face] = true;
				Snapshot.Halo[Face].Init(FVoxelBlockState(0, 0).Pack(), 256);
				FVoxelSectionMeshResult Mesh;
				if (!TestTrue(Case + TEXT(" meshes the shared boundary"),
					FVoxelSectionMesher::Build(Snapshot, Registry, Shapes, Mesh, nullptr, 1.0, &Context))) continue;
				int32 BoundaryTriangles = 0;
				const double Plane = (Face & 1) ? 0.0 : 16.0;
				for (const FVoxelRenderBatch& Batch : Mesh.Batches)
				{
					TestTrue(Case + TEXT(" has valid mesh attributes"), Batch.Mesh.Validate());
					for (int32 Index = 0; Index < Batch.Mesh.Triangles.Num(); Index += 3)
					{
						const FVector& A = Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index]];
						const FVector& B = Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index + 1]];
						const FVector& C = Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index + 2]];
						if (A[Axis] == Plane && B[Axis] == Plane && C[Axis] == Plane) ++BoundaryTriangles;
					}
				}
				TestEqual(Case + TEXT(" exposes three quadrants without a duplicate coarse face"), BoundaryTriangles, 6);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelProxyTreeSilhouetteTest,
	"WHFramework.Voxel.Rendering.ProxyTreeSilhouette",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelProxyTreeSilhouetteTest::RunTest(const FString& InParameters)
{
	FVoxelGenerationRuntimeConfig MutableConfig = *VoxelTest::MakeGenerationConfig();
	FVoxelGenerationRecipe Recipe = *MutableConfig.Recipe;
	Recipe.Settings.SeaLevel = -64;
	Recipe.Settings.RiverSourceAccumulation = 1000000;
	Recipe.Settings.Ecology.Tree.bEnabled = true;
	Recipe.Settings.Ecology.Tree.Spacing = 4;
	Recipe.Settings.Ecology.Tree.ChancePermille = 1000;
	Recipe.Settings.Ecology.Tree.MinHeight = 8;
	Recipe.Settings.Ecology.Tree.MaxHeight = 8;
	Recipe.Settings.Ecology.Tree.CrownRadius = 2;
	Recipe.Settings.Ecology.Tree.MaxSlopePermille = 10000;
	Recipe.Settings.Ecology.Tree.Temperature = {-32768, 32767};
	Recipe.Settings.Ecology.Tree.Moisture = {-32768, 32767};
	Recipe.Settings.Ecology.Tree.bAllowNearWater = true;
	Recipe.Ecology.TreeTrunk = 8;
	Recipe.Ecology.TreeLeaves = 9;
	MutableConfig.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(MutableConfig));
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelGenerationQuery Query;
	FString Error;
	if (!TestTrue(TEXT("Tree test query creates"),
		FVoxelGenerationQuery::Create(Config, Cache, Query, Error))) return false;
	const FVoxelEcologyGenerator Ecology(Config->Recipe.ToSharedRef());
	auto SampleColumn = [&Query, &Error](const FIntVector& Position,
		FVoxelColumnSample& Column)
	{
		return Query.SampleEnvironmentColumn(Position.X, Position.Y, Column, Error);
	};
	auto SampleBase = [&SampleColumn, &Config](const FIntVector& Position,
		uint32& Symbol)
	{
		FVoxelColumnSample Column;
		if (!SampleColumn(Position, Column)) return false;
		Symbol = Position.Z <= Column.SurfaceZ ?
			Config->Recipe->Palette.Stone : Config->Recipe->Palette.Air;
		return true;
	};
	int32 Candidates = 0;
	int32 Accepted = 0;
	FIntVector FirstAnchor = FIntVector::ZeroValue;
	Ecology.EnumerateTrees({FIntVector(0, 0, -64), FIntVector(32, 32, 128)},
		SampleColumn, SampleBase,
		[&FirstAnchor](const FIntVector& Anchor, const int32 Height,
			const FVoxelStableId)
		{
			if (FirstAnchor == FIntVector::ZeroValue) FirstAnchor = Anchor;
		}, Candidates, Accepted);
	if (!TestTrue(TEXT("Tree planner accepts candidates"), Accepted > 0)) return false;
	const FVoxelViewKey Key{
		FIntVector(VoxelGeneration::FloorDivide(FirstAnchor.X, 32),
			VoxelGeneration::FloorDivide(FirstAnchor.Y, 32),
			VoxelGeneration::FloorDivide(FirstAnchor.Z + 7, 32)), 1 };
	const FVoxelGenerationBounds Bounds = Key.GetBounds();
	const int32 Crown = Config->Recipe->Settings.Ecology.Tree.CrownRadius;
	const FVoxelGenerationBounds TreeBounds{
		FIntVector(Bounds.Min.X - Crown, Bounds.Min.Y - Crown, Config->Recipe->Settings.MinZ),
		FIntVector(Bounds.Max.X + Crown, Bounds.Max.Y + Crown, Config->Recipe->Settings.MaxZ)
	};
	FVoxelOverlaySnapshotSet Removed;
	Candidates = 0;
	Accepted = 0;
	Ecology.EnumerateTrees(TreeBounds, SampleColumn, SampleBase,
		[&Removed](const FIntVector& Anchor, const int32 Height, const FVoxelStableId)
		{
			const FIntVector Section(
				VoxelGeneration::FloorDivide(Anchor.X, 16),
				VoxelGeneration::FloorDivide(Anchor.Y, 16),
				VoxelGeneration::FloorDivide(Anchor.Z, 16));
			const FIntVector Local = Anchor - Section * 16;
			FVoxelOverlaySnapshot& Snapshot = Removed.Sections.FindOrAdd(Section);
			Snapshot.Section = Section;
			Snapshot.Blocks.Add(Local.X + Local.Y * 16 + Local.Z * 256,
				FVoxelBlockState());
		}, Candidates, Accepted);
	const FVoxelVoxelProxyBuilder Builder(Config, Cache);
	FVoxelVoxelProxyData Natural;
	FVoxelVoxelProxyData Destroyed;
	if (!TestTrue(TEXT("Tree proxy builds"),
		Builder.Build(Key, {}, Natural, Error)) ||
		!TestTrue(TEXT("Destroyed tree proxy builds"),
			Builder.Build(Key, Removed, Destroyed, Error))) return false;
	int32 NaturalTreeCells = 0;
	int32 DestroyedTreeCells = 0;
	for (int32 Index = 0; Index < Natural.Cells.Num(); ++Index)
	{
		NaturalTreeCells += Natural.Cells[Index].TypeId == 8 || Natural.Cells[Index].TypeId == 9;
		DestroyedTreeCells += Destroyed.Cells[Index].TypeId == 8 || Destroyed.Cells[Index].TypeId == 9;
	}
	TestTrue(TEXT("Distant proxy shows planned trees"), NaturalTreeCells > 0);
	TestEqual(TEXT("Removed tree anchors hide distant tree silhouettes"),
		DestroyedTreeCells, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelWaterViewTest,
	"WHFramework.Voxel.Rendering.WaterView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelWaterViewTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelSurfaceTileData Surface;
	Surface.Side = 2;
	Surface.Step = 4;
	Surface.Revision = 9;
	Surface.GroundZ = { 4, 8, 7, 0 };
	Surface.WaterZ = { 7, 8, 9, MIN_int32 };
	Surface.Flags = { VoxelSurface_River, VoxelSurface_Lake, VoxelSurface_Ocean, 0 };
	FVoxelWaterSurfaceTileData Water;
	FString Error;
	const FVoxelWaterViewBuilder Builder;
	TestTrue(TEXT("Water tile builds"), Builder.Build(Surface, Water, Error));
	TestEqual(TEXT("One-cell river is preserved"), Water.WaterZ[0], 7);
	TestEqual(TEXT("River kind"), Water.WaterKind[0], uint8(EVoxelWaterKind::River));
	TestEqual(TEXT("Lake kind"), Water.WaterKind[1], uint8(EVoxelWaterKind::Lake));
	TestEqual(TEXT("Ocean kind"), Water.WaterKind[2], uint8(EVoxelWaterKind::Ocean));
	TestEqual(TEXT("Dry cell kind"), Water.WaterKind[3], uint8(EVoxelWaterKind::None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSurfaceDistantEditsTest,
	"WHFramework.Voxel.Rendering.SurfaceDistantEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSurfaceDistantEditsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Generator = VoxelTest::MakeGenerator();
	FVoxelGenerationSettings Settings = Config->Recipe->Settings;
	Settings.MaxZ = 512;
	FVoxelTestOverlaySource Overlay;
	const FVoxelSurfaceProxyBuilder Builder(Generator, Config, Settings, Overlay, MakeSurfaceRegistry());
	const FVoxelSurfaceTileKey Key{FIntPoint::ZeroValue, 4};
	FString Error;
	FVoxelSurfaceTileData Natural;
	if (!TestTrue(TEXT("Coarse surface builds"), Builder.Build(Key, Natural, Error))) return false;
	const int32 BaseZ = (VoxelGeneration::FloorDivide(Natural.GroundZ[0] + 31, 16) + 1) * 16;
	Overlay.Set(FIntVector(0, 0, BaseZ), FVoxelBlockState(1, 0));
	FVoxelSurfaceTileData Tiny;
	if (!TestTrue(TEXT("Small player edit builds"), Builder.Build(Key, Tiny, Error))) return false;
	TestEqual(TEXT("Subcell edit does not create a distant block"), Tiny.DistantCells.Num(), Natural.DistantCells.Num());
	for (int32 Z = 1; Z < 16; ++Z)
	{
		Overlay.Set(FIntVector(0, 0, BaseZ + Z), FVoxelBlockState(1, 0));
	}
	FVoxelSurfaceTileData Built;
	if (!TestTrue(TEXT("Visible player construction builds"), Builder.Build(Key, Built, Error))) return false;
	bool bFound = false;
	for (const FVoxelDistantCell& Cell : Built.DistantCells)
	{
		bFound |= Cell.Min == FIntVector(0, 0, BaseZ) &&
			Cell.Max == FIntVector(1, 1, BaseZ + 16) && Cell.State == FVoxelBlockState(1, 0);
	}
	TestTrue(TEXT("Distant construction keeps its actual dimensions"), bFound);
	for (int32 X = 8; X < 32; ++X)
	{
		Overlay.Set(FIntVector(X, 4, BaseZ + 32), FVoxelBlockState(1, 0));
	}
	FVoxelSurfaceTileData CrossBoundary;
	if (!TestTrue(TEXT("Cross-boundary construction builds"),
		Builder.Build(Key, CrossBoundary, Error))) return false;
	bool bShowsShortEdge = false;
	for (const FVoxelDistantCell& Cell : CrossBoundary.DistantCells)
	{
		bShowsShortEdge |= Cell.Min == FIntVector(8, 4, BaseZ + 32) &&
			Cell.Max == FIntVector(16, 5, BaseZ + 33);
	}
	TestTrue(TEXT("A large connected construction retains the segment across a coarse-cell boundary"),
		bShowsShortEdge);
	for (int32 Z = 0; Z < 16; ++Z)
	{
		Overlay.Set(FIntVector(0, 0, BaseZ + Z), FVoxelBlockState());
	}
	FVoxelSurfaceTileData Removed;
	if (!TestTrue(TEXT("Destroyed construction builds"),
		Builder.Build(Key, Removed, Error))) return false;
	bool bRetainsDestroyedColumn = false;
	for (const FVoxelDistantCell& Cell : Removed.DistantCells)
	{
		bRetainsDestroyedColumn |= Cell.Min == FIntVector(0, 0, BaseZ) &&
			Cell.Max == FIntVector(1, 1, BaseZ + 16);
	}
	TestFalse(TEXT("Destroyed player construction is absent from distant representation"),
		bRetainsDestroyedColumn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSurfaceProxyOverlayTest,
	"WHFramework.Voxel.Rendering.SurfaceOverlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSurfaceProxyOverlayTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		VoxelTest::MakeGenerationConfig();
	const TSharedRef<FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator = VoxelTest::MakeGenerator();
	FVoxelGenerationSettings SurfaceSettings = Config->Recipe->Settings;
	SurfaceSettings.MaxZ = 512;
	FVoxelTestOverlaySource Overlay;
	FVoxelSurfaceProxyBuilder Builder(Generator, Config, SurfaceSettings, Overlay, MakeSurfaceRegistry());
	const FVoxelSurfaceTileKey Key{ FIntPoint::ZeroValue, 0 };
	FVoxelSurfaceTileData Natural;
	FString Error;
	TestTrue(TEXT("Natural surface builds without exact runtime"), Builder.Build(Key, Natural, Error));
	const int32 NaturalGround = Natural.GroundZ[0];
	Overlay.Set(FIntVector(-1, 0, NaturalGround + 1), FVoxelBlockState());
	FVoxelSurfaceTileData BorderEdit;
	TestTrue(TEXT("Border edit surface builds"), Builder.Build(Key, BorderEdit, Error));
	TestTrue(TEXT("Adjacent edit revision is captured for distant silhouettes"),
		BorderEdit.Revision > Natural.Revision);
	TestEqual(TEXT("Adjacent edit leaves sampled ground unchanged"),
		BorderEdit.GroundZ[0], NaturalGround);

	Overlay.Set(FIntVector(0, 0, NaturalGround - 20), FVoxelBlockState());
	FVoxelSurfaceTileData UndergroundEdit;
	TestTrue(TEXT("Underground overlay surface builds"), Builder.Build(Key, UndergroundEdit, Error));
	TestEqual(TEXT("Underground edit does not change surface"), UndergroundEdit.GroundZ[0], NaturalGround);

	Overlay.Set(FIntVector(0, 0, NaturalGround), FVoxelBlockState());
	FVoxelSurfaceTileData SurfaceEdit;
	TestTrue(TEXT("Surface overlay builds"), Builder.Build(Key, SurfaceEdit, Error));
	TestTrue(
		FString::Printf(
			TEXT("Surface removal lowers the visible ground (natural=%d, edited=%d)"),
			NaturalGround,
			SurfaceEdit.GroundZ[0]),
		SurfaceEdit.GroundZ[0] < NaturalGround);

	FVoxelGenerationRuntimeConfig RemappedConfig = *Config;
	RemappedConfig.RuntimeToSymbol[1] = 3;
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Remapped =
		MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(RemappedConfig));
	FVoxelTestOverlaySource MaterialOverlay;
	MaterialOverlay.Set(FIntVector(0, 0, NaturalGround + 1), FVoxelBlockState(1, 0));
	FVoxelSurfaceProxyBuilder RemappedBuilder(
		Generator,
		Remapped,
		SurfaceSettings,
		MaterialOverlay, MakeSurfaceRegistry());
	FVoxelSurfaceTileData MaterialEdit;
	TestTrue(TEXT("Remapped material overlay builds"), RemappedBuilder.Build(Key, MaterialEdit, Error));
	TestEqual(TEXT("Overlay material remains in recipe symbol domain"), MaterialEdit.SurfaceMaterial[0], uint16(3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMacroOverlayTest, "WHFramework.Voxel.Rendering.MacroOverlay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMacroOverlayTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Generator = VoxelTest::MakeGenerator();
	FVoxelTestOverlaySource Overlay;
	FVoxelGenerationSettings Settings = Config->Recipe->Settings;
	Settings.MaxZ = 512;
	Overlay.Set(FIntVector(0, 0, 511), FVoxelBlockState(1, 0));
	const FVoxelMacroTerrainBuilder MacroBuilder(Generator, Config, Settings, Overlay, MakeSurfaceRegistry());
	FVoxelMacroTileData Macro;
	FString Error;
	if (!TestTrue(TEXT("Macro consumes immutable edits"), MacroBuilder.Build({ FIntPoint::ZeroValue, 0 }, Macro, Error)))
	{
		return false;
	}
	TestEqual(TEXT("Edited top reaches distant representation"), Macro.Height[0], 511);
	TestEqual(TEXT("Edited material reaches distant representation"), Macro.SurfaceClass[0], uint16(1));
	TestEqual(TEXT("Macro preserves coarse sampling scale"), Macro.Step, FVoxelMacroTileData::BaseStep);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelMacroWaterMeshTest,
	"WHFramework.Voxel.Rendering.MacroWaterMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMacroWaterMeshTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	FVoxelRegistrySnapshot Registry = *VoxelTest::MakeRegistry();
	Registry.Definitions[6].RenderGroup = EVoxelRenderGroup::Water;
	FVoxelMacroTileData Macro;
	Macro.Side = 2;
	Macro.Step = 64;
	Macro.Height = { 10, 10, 10, 10 };
	Macro.WaterHeight = { 12, MIN_int32, MIN_int32, MIN_int32 };
	Macro.SurfaceClass = { 1, 1, 1, 1 };
	Macro.ForestCoverage = { 0, 0, 0, 0 };
	FVoxelSectionMeshResult Mesh;
	FString Error;
	if (!TestTrue(TEXT("Macro terrain and water build"),
		FVoxelHeightfieldMesher::BuildMacro(Macro, *Config, Registry, Mesh, Error)))
	{
		return false;
	}
	int32 WaterVertices = 0;
	bool bGroundHasMaterialTint = false;
	const FLinearColor GroundTint = Registry.Definitions[1].Face(0, 4).Tint;
	for (const FVoxelRenderBatch& Batch : Mesh.Batches)
	{
		if (Batch.Group == EVoxelRenderGroup::Water)
		{
			WaterVertices += Batch.Mesh.Vertices.Num();
		}
		else if (!Batch.Mesh.Colors.IsEmpty())
		{
			bGroundHasMaterialTint |= Batch.Mesh.Colors[0].Equals(GroundTint);
		}
	}
	TestEqual(TEXT("One wet macro corner produces only a clipped water triangle"), WaterVertices, 3);
	TestTrue(TEXT("Zero forest coverage does not darken macro ground"), bGroundHasMaterialTint);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelBlockyFlatTest,
	"WHFramework.Voxel.Rendering.Blocky.Flat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelBlockyFlatTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		VoxelTest::MakeGenerationConfig();
	const TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry =
		VoxelTest::MakeRegistry();
	const TArray<int32> Heights = { 10, 10, 10, 10, 10, 10, 10, 10, 10 };
	const TArray<uint16> Materials = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	FVoxelSectionMeshResult Mesh;
	FString Error;
	TestTrue(TEXT("Flat blocky terrain builds"), FVoxelHeightfieldMesher::BuildBlockyTerrain(
		3, 1, Heights, Materials, *Config, *Registry, Mesh, Error));
	int32 Vertices = 0;
	for (const FVoxelRenderBatch& Batch : Mesh.Batches)
	{
		Vertices += Batch.Mesh.Vertices.Num();
		for (const FVector& Normal : Batch.Mesh.Normals)
		{
			TestTrue(TEXT("Flat top normal points upward"), Normal.Z > 0.99);
		}
	}
	TestEqual(TEXT("Flat terrain has tops and no internal sides"), Vertices, 16);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelBlockyStepTest,
	"WHFramework.Voxel.Rendering.Blocky.Step",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelBlockyStepTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		VoxelTest::MakeGenerationConfig();
	const TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry =
		VoxelTest::MakeRegistry();
	const TArray<int32> Heights = { 10, 4, 4, 10, 4, 4, 10, 4, 4 };
	const TArray<uint16> Materials = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	FVoxelSectionMeshResult Mesh;
	FString Error;
	TestTrue(TEXT("Stepped blocky terrain builds"), FVoxelHeightfieldMesher::BuildBlockyTerrain(
		3, 1, Heights, Materials, *Config, *Registry, Mesh, Error));
	int32 VerticalQuads = 0;
	for (const FVoxelRenderBatch& Batch : Mesh.Batches)
	{
		for (int32 Vertex = 0; Vertex + 3 < Batch.Mesh.Vertices.Num(); Vertex += 4)
		{
			const TConstArrayView<FVector> Quad(&Batch.Mesh.Vertices[Vertex], 4);
			const double MinZ = FMath::Min(FMath::Min(Quad[0].Z, Quad[1].Z), FMath::Min(Quad[2].Z, Quad[3].Z));
			const double MaxZ = FMath::Max(FMath::Max(Quad[0].Z, Quad[1].Z), FMath::Max(Quad[2].Z, Quad[3].Z));
			if (FMath::IsNearlyEqual(MaxZ - MinZ, 6.0))
			{
				++VerticalQuads;
			}
		}
	}
	TestEqual(TEXT("Each stepped cell boundary produces one side quad"), VerticalQuads, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelBlockyNoSlopeTest,
	"WHFramework.Voxel.Rendering.Blocky.NoSlope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelBlockyNoSlopeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		VoxelTest::MakeGenerationConfig();
	const TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry =
		VoxelTest::MakeRegistry();
	const TArray<int32> Heights = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	const TArray<uint16> Materials = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	FVoxelSectionMeshResult Mesh;
	FString Error;
	TestTrue(TEXT("Irregular blocky terrain builds"), FVoxelHeightfieldMesher::BuildBlockyTerrain(
		3, 1, Heights, Materials, *Config, *Registry, Mesh, Error));
	for (const FVoxelRenderBatch& Batch : Mesh.Batches)
	{
		for (int32 Vertex = 0; Vertex + 3 < Batch.Mesh.Vertices.Num(); Vertex += 4)
		{
			const FVector& A = Batch.Mesh.Vertices[Vertex];
			const FVector& B = Batch.Mesh.Vertices[Vertex + 1];
			const FVector& C = Batch.Mesh.Vertices[Vertex + 2];
			const FVector& D = Batch.Mesh.Vertices[Vertex + 3];
			const bool bHorizontal = FMath::IsNearlyEqual(A.Z, B.Z) &&
				FMath::IsNearlyEqual(B.Z, C.Z) && FMath::IsNearlyEqual(C.Z, D.Z);
			const bool bVertical = FMath::IsNearlyEqual(A.X, B.X) &&
				FMath::IsNearlyEqual(B.X, C.X) && FMath::IsNearlyEqual(C.X, D.X) ||
				FMath::IsNearlyEqual(A.Y, B.Y) && FMath::IsNearlyEqual(B.Y, C.Y) &&
				FMath::IsNearlyEqual(C.Y, D.Y);
			TestTrue(TEXT("Every blocky quad is horizontal or vertical"), bHorizontal || bVertical);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelWaterContinuousTest,
	"WHFramework.Voxel.Rendering.Water.Continuous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelWaterContinuousTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWaterSurfaceTileData Water;
	Water.Side = 2;
	Water.Step = 1;
	Water.WaterZ = { 2, 3, 4, 5 };
	Water.GroundZ = { 0, 0, 0, 0 };
	Water.WaterKind.Init(static_cast<uint8>(EVoxelWaterKind::River), 4);
	FVoxelRegistrySnapshot Registry = *VoxelTest::MakeRegistry();
	Registry.Definitions[6].RenderGroup = EVoxelRenderGroup::Water;
	FVoxelSectionMeshResult Mesh;
	FString Error;
	TestTrue(TEXT("Continuous water builds"), FVoxelHeightfieldMesher::BuildWater(
		Water, Registry, Mesh, Error));
	TestFalse(TEXT("Wet cell produces a surface"), Mesh.Batches.IsEmpty());
	if (Mesh.Batches.IsEmpty())
	{
		return false;
	}
	const FVoxelMeshBuffers& Full = Mesh.Batches[0].Mesh;
	TestEqual(TEXT("Four wet corners retain four water heights"), Full.Vertices.Num(), 4);
	if (Full.Vertices.Num() == 4)
	{
		TestEqual(TEXT("Southwest water height"), Full.Vertices[0].Z, 3.0);
		TestEqual(TEXT("Southeast water height"), Full.Vertices[1].Z, 4.0);
		TestEqual(TEXT("Northeast water height"), Full.Vertices[2].Z, 6.0);
		TestEqual(TEXT("Northwest water height"), Full.Vertices[3].Z, 5.0);
	}
	auto WaterArea = [](const FVoxelSectionMeshResult& InMesh)
	{
		double Area = 0.0;
		for (const FVoxelRenderBatch& Batch : InMesh.Batches)
		{
			const FVoxelMeshBuffers& Buffers = Batch.Mesh;
			for (int32 Index = 0; Index + 2 < Buffers.Triangles.Num(); Index += 3)
			{
				const FVector& A = Buffers.Vertices[Buffers.Triangles[Index]];
				const FVector& B = Buffers.Vertices[Buffers.Triangles[Index + 1]];
				const FVector& C = Buffers.Vertices[Buffers.Triangles[Index + 2]];
				Area += FMath::Abs((B.X - A.X) * (C.Y - A.Y) -
					(B.Y - A.Y) * (C.X - A.X)) * 0.5;
			}
		}
		return Area;
	};
	TestEqual(TEXT("Full wet cell covers its full footprint"), WaterArea(Mesh), 1.0);
	Water.WaterKind[1] = static_cast<uint8>(EVoxelWaterKind::None);
	Water.WaterZ[1] = MIN_int32;
	TestTrue(TEXT("Shoreline water builds"), FVoxelHeightfieldMesher::BuildWater(Water, Registry, Mesh, Error));
	TestEqual(TEXT("Three wet corners omit the dry corner"), WaterArea(Mesh), 0.875);
	Water.WaterKind[0] = static_cast<uint8>(EVoxelWaterKind::None);
	TestTrue(TEXT("Dry owning cell builds"), FVoxelHeightfieldMesher::BuildWater(Water, Registry, Mesh, Error));
	TestEqual(TEXT("Two adjacent wet corners occupy half the cell"), WaterArea(Mesh), 0.5);
	Water.WaterKind[2] = static_cast<uint8>(EVoxelWaterKind::None);
	TestTrue(TEXT("Single wet corner builds"), FVoxelHeightfieldMesher::BuildWater(Water, Registry, Mesh, Error));
	TestEqual(TEXT("Single wet corner occupies an eighth of the cell"), WaterArea(Mesh), 0.125);
	Water.WaterKind[0] = static_cast<uint8>(EVoxelWaterKind::River);
	Water.WaterZ[0] = 2;
	TestTrue(TEXT("Opposite wet corners build separately"),
		FVoxelHeightfieldMesher::BuildWater(Water, Registry, Mesh, Error));
	TestEqual(TEXT("Diagonal wet corners do not bridge dry land"), WaterArea(Mesh), 0.25);
	Water.WaterKind[0] = static_cast<uint8>(EVoxelWaterKind::None);
	Water.WaterKind[3] = static_cast<uint8>(EVoxelWaterKind::None);
	TestTrue(TEXT("Dry cell builds"), FVoxelHeightfieldMesher::BuildWater(Water, Registry, Mesh, Error));
	TestEqual(TEXT("Dry cell has no water footprint"), WaterArea(Mesh), 0.0);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSurfaceStateTest,
	"WHFramework.Voxel.Rendering.SurfaceState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSurfaceStateTest::RunTest(const FString& InParameters)
{
	FVoxelGenerationRuntimeConfig MutableConfig = *VoxelTest::MakeGenerationConfig();
	FVoxelGenerationRecipe Recipe = *MutableConfig.Recipe;
	Recipe.Settings.MaxZ = 512;
	MutableConfig.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
	const auto Config = MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(MutableConfig));
	const auto Generator = MakeShared<FVoxelGenerationPipeline, ESPMode::ThreadSafe>(Config,
		MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>());
	FVoxelRegistrySnapshot Definitions = *MakeSurfaceRegistry();
	Definitions.Definitions[9].bSolid = false;
	const auto Registry = MakeShared<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe>(MoveTemp(Definitions));
	FVoxelTestOverlaySource Overlay;
	FVoxelSurfaceProxyBuilder Builder(Generator, Config, Config->Recipe->Settings, Overlay, Registry);
	const FVoxelSurfaceTileKey Key { FIntPoint::ZeroValue, 0 };
	FVoxelSurfaceTileData Data;
	FString Error;
	if (!TestTrue(TEXT("Natural reference builds"), Builder.Build(Key, Data, Error))) return false;
	const int32 Ground = Data.GroundZ[0];
	Overlay.Set(FIntVector(0, 0, 510), FVoxelBlockState(9, 0));
	TestTrue(TEXT("Decoration edit builds"), Builder.Build(Key, Data, Error));
	TestEqual(TEXT("Non-solid decoration cannot raise terrain"), Data.GroundZ[0], Ground);
	Overlay.Set(FIntVector(0, 0, 509), Config->Water);
	TestTrue(TEXT("Water edit builds"), Builder.Build(Key, Data, Error));
	TestEqual(TEXT("Water is separate from solid ground"), Data.GroundZ[0], Ground);
	TestEqual(TEXT("Edited water level is preserved"), Data.WaterZ[0], 509);
	FVoxelWaterSurfaceTileData Water;
	TestTrue(TEXT("Edited water view builds"), FVoxelWaterViewBuilder().Build(Data, Water, Error));
	TestTrue(TEXT("Water without natural hydrology flags remains visible"), Water.WaterKind[0] != uint8(EVoxelWaterKind::None));
	Overlay.Set(FIntVector(0, 0, 509), Config->Air);
	TestTrue(TEXT("Removed water builds"), Builder.Build(Key, Data, Error));
	TestTrue(TEXT("Removed water does not retain stale level"), Data.WaterZ[0] < 509);
	for (int32 Z = Config->Recipe->Settings.MinZ; Z < Config->Recipe->Settings.MaxZ; ++Z)
	{
		Overlay.Set(FIntVector(0, 0, Z), Config->Air);
	}
	TestTrue(TEXT("Fully cleared column builds"), Builder.Build(Key, Data, Error));
	TestEqual(TEXT("Cleared column has no phantom ground"), Data.GroundZ[0], MIN_int32);
	TestEqual(TEXT("Cleared column has no phantom water"), Data.WaterZ[0], MIN_int32);
	FVoxelSectionMeshResult Mesh;
	const TArray<int32> EmptyHeights { MIN_int32, MIN_int32, MIN_int32, MIN_int32 };
	const TArray<uint16> EmptyMaterials { 0, 0, 0, 0 };
	TestTrue(TEXT("Empty terrain mesh builds"), FVoxelHeightfieldMesher::BuildBlockyTerrain(
		2, 4, EmptyHeights, EmptyMaterials, *Config, *Registry, Mesh, Error));
	TestEqual(TEXT("Empty terrain has no artificial cap or wall"), Mesh.Batches.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHeightfieldHaloSeamTest, "WHFramework.Voxel.Rendering.Blocky.PositiveHaloSeam", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHeightfieldHaloSeamTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Registry = VoxelTest::MakeRegistry();
	const TArray<int32> Heights { 10, 4, 10, 4 };
	const TArray<uint16> Materials { 1, 1, 1, 1 };
	FVoxelSectionMeshResult Mesh;
	FString Error;
	TestTrue(TEXT("Single tile with lower positive neighbor builds"), FVoxelHeightfieldMesher::BuildBlockyTerrain(
		2, 4, Heights, Materials, *Config, *Registry, Mesh, Error));
	int32 Walls = 0;
	for (const auto& Batch : Mesh.Batches)
	{
		for (int32 Index = 0; Index < Batch.Mesh.Vertices.Num(); Index += 4)
		{
			if (FMath::Abs(Batch.Mesh.Normals[Index].X) < 0.5) continue;
			++Walls;
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const FVector& Vertex = Batch.Mesh.Vertices[Index + Corner];
				TestEqual(TEXT("Shared wall is on the exact tile boundary"), Vertex.X, 4.0);
				TestTrue(TEXT("Wall joins actual surfaces instead of a fixed depth skirt"), Vertex.Z == 5.0 || Vertex.Z == 11.0);
			}
		}
	}
	TestEqual(TEXT("The shared boundary has one owning wall"), Walls, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHeightfieldNoUnconditionalSkirtTest, "WHFramework.Voxel.Rendering.Blocky.NoUnconditionalSkirt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHeightfieldNoUnconditionalSkirtTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Registry = VoxelTest::MakeRegistry();
	const TArray<int32> Heights { 100, 0, 100, 0 };
	const TArray<uint16> Materials { 1, 1, 1, 1 };
	FVoxelSectionMeshResult Mesh;
	FString Error;
	if (!TestTrue(TEXT("Heightfield with a steep LOD boundary builds"),
		FVoxelHeightfieldMesher::BuildBlockyTerrain(2, 4, Heights, Materials,
			*Config, *Registry, Mesh, Error)))
	{
		return false;
	}
	bool bHasDeepPerimeterWall = false;
	for (const FVoxelRenderBatch& Batch : Mesh.Batches)
	{
		for (const FVector& Vertex : Batch.Mesh.Vertices)
		{
			bHasDeepPerimeterWall |= Vertex.X == 0.0 && Vertex.Z < 1.0;
		}
	}
	TestFalse(TEXT("Base terrain adds no unconditional deep perimeter wall"), bHasDeepPerimeterWall);
	return true;
}

#endif
