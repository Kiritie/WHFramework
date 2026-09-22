#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelHeightfieldMesher.h"
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
	for (const FVector& Vertex : Mesh.Batches[0].Mesh.Vertices)
	{
		TestEqual(TEXT("Water cell uses its own horizontal water level"), Vertex.Z, 3.0);
	}
	Water.WaterKind[1] = static_cast<uint8>(EVoxelWaterKind::None);
	Water.WaterZ[1] = MIN_int32;
	TestTrue(TEXT("Shoreline water builds"), FVoxelHeightfieldMesher::BuildWater(Water, Registry, Mesh, Error));
	TestTrue(TEXT("Dry neighbouring sample does not erase wet cell"),
		!Mesh.Batches.IsEmpty() && Mesh.Batches[0].Mesh.Triangles.Num() == 6);
	Water.WaterKind[0] = static_cast<uint8>(EVoxelWaterKind::None);
	TestTrue(TEXT("Dry owning cell builds"), FVoxelHeightfieldMesher::BuildWater(Water, Registry, Mesh, Error));
	TestTrue(TEXT("Wet neighbours do not create water over dry owning cell"),
		Mesh.Batches.IsEmpty() || Mesh.Batches[0].Mesh.Triangles.IsEmpty());
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

#endif
