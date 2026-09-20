#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelHeightfieldMesher.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/VoxelWaterView.h"
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
	FVoxelSurfaceProxyBuilder Builder(Generator, Config, SurfaceSettings, Overlay);
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
		MaterialOverlay);
	FVoxelSurfaceTileData MaterialEdit;
	TestTrue(TEXT("Remapped material overlay builds"), RemappedBuilder.Build(Key, MaterialEdit, Error));
	TestEqual(TEXT("Overlay material remains in recipe symbol domain"), MaterialEdit.SurfaceMaterial[0], uint16(3));
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
	TestTrue(TEXT("Continuous water retains differing corner heights"),
		!Mesh.Batches.IsEmpty() && Mesh.Batches[0].Mesh.Vertices[0].Z != Mesh.Batches[0].Mesh.Vertices[1].Z);
	return true;
}

#endif
