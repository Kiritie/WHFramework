#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
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

#endif
