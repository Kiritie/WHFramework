#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Network/VoxelRepresentationSync.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNetworkSectionStateCodecTest,
	"WHFramework.Voxel.Network.SectionStateCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkSectionStateCodecTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelSectionState Source;
	Source.Section = FIntVector(-17, 3, -9);
	Source.State = EVoxelSectionWireState::Modified;
	Source.Revision = 42;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Encode section state"), FVoxelNetworkCodec::EncodeSectionState(Source, Bytes));
	FVoxelSectionState Decoded;
	TestTrue(TEXT("Decode section state"), FVoxelNetworkCodec::DecodeSectionState(Bytes, Decoded));
	TestEqual(TEXT("Section coordinate"), Decoded.Section, Source.Section);
	TestEqual(TEXT("Section state"), static_cast<uint8>(Decoded.State), static_cast<uint8>(Source.State));
	TestEqual(TEXT("Section revision"), Decoded.Revision, Source.Revision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNetworkPatchRejectsTrailingBytesTest,
	"WHFramework.Voxel.Network.PatchRejectsTrailingBytes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkPatchRejectsTrailingBytesTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelNetworkPatchBatch Source;
	Source.TransactionId = FGuid::NewGuid();
	FVoxelSectionPatch Patch;
	Patch.Section = FIntVector(-2, 3, 4);
	Patch.FromRevision = 4;
	Patch.ToRevision = 5;
	Source.Sections.Add(Patch);
	TArray<uint8> Bytes;
	FString Error;
	TestTrue(TEXT("Encode patch batch"), FVoxelNetworkCodec::EncodePatchBatch(Source, Bytes, Error));
	Bytes.Add(0xff);
	FVoxelNetworkPatchBatch Decoded;
	TestFalse(TEXT("Trailing bytes are rejected"), FVoxelNetworkCodec::DecodePatchBatch(Bytes, Decoded, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNetworkChangeSummaryCodecTest,
	"WHFramework.Voxel.Network.ChangeSummaryCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkChangeSummaryCodecTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelRegionChangeSummary Source;
	Source.Region = FIntVector(-3, 7, -1);
	Source.Revision = 91;
	Source.ModifiedMask.Init(0, 8);
	Source.ModifiedMask[0] = 1ull << 17;
	Source.ModifiedMask[7] = 1ull << 63;
	TArray<uint8> Bytes;
	FString Error;
	TestTrue(TEXT("Encode change summary"), FVoxelNetworkCodec::EncodeChangeSummary(Source, Bytes, Error));
	FVoxelRegionChangeSummary Decoded;
	TestTrue(TEXT("Decode change summary"), FVoxelNetworkCodec::DecodeChangeSummary(Bytes, Decoded, Error));
	TestEqual(TEXT("Summary region"), Decoded.Region, Source.Region);
	TestEqual(TEXT("Summary revision"), Decoded.Revision, Source.Revision);
	TestEqual(TEXT("Summary mask"), Decoded.ModifiedMask, Source.ModifiedMask);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNetworkRepresentationInvalidateCodecTest,
	"WHFramework.Voxel.Network.RepresentationInvalidateCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkRepresentationInvalidateCodecTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelRepresentationInvalidate Source;
	Source.Revision = 123;
	Source.Keys.Add({ FIntVector(-5, 4, 2), 1 });
	Source.Keys.Add({ FIntVector(8, -7, 0), 3 });
	TArray<uint8> Bytes;
	FString Error;
	TestTrue(TEXT("Encode representation invalidation"),
		FVoxelNetworkCodec::EncodeRepresentationInvalidate(Source, Bytes, Error));
	FVoxelRepresentationInvalidate Decoded;
	TestTrue(TEXT("Decode representation invalidation"),
		FVoxelNetworkCodec::DecodeRepresentationInvalidate(Bytes, Decoded, Error));
	TestEqual(TEXT("Invalidation revision"), Decoded.Revision, Source.Revision);
	TestEqual(TEXT("Invalidation key count"), Decoded.Keys.Num(), Source.Keys.Num());
	TestTrue(TEXT("First invalidation key"), Decoded.Keys[0] == Source.Keys[0]);
	TestTrue(TEXT("Second invalidation key"), Decoded.Keys[1] == Source.Keys[1]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNetworkRepresentationDataCodecTest,
	"WHFramework.Voxel.Network.RepresentationDataCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkRepresentationDataCodecTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FString Error;
	TArray<uint8> Bytes;

	FVoxelVoxelProxyData Proxy;
	Proxy.Key = { FIntVector(-3, 2, 1), 2 };
	Proxy.Revision = 17;
	Proxy.GridSide = 16;
	Proxy.Cells.Init(FVoxelBlockState { 3, 9 }, 4096);
	TestTrue(TEXT("Encode voxel proxy data"), FVoxelRepresentationSync::EncodeVoxelProxy(Proxy, Bytes, Error));
	FVoxelVoxelProxyData DecodedProxy;
	TestTrue(TEXT("Decode voxel proxy data"), FVoxelRepresentationSync::DecodeVoxelProxy(Bytes, DecodedProxy, Error));
	TestEqual(TEXT("Voxel proxy key"), DecodedProxy.Key.Coordinate, Proxy.Key.Coordinate);
	TestEqual(TEXT("Voxel proxy revision"), DecodedProxy.Revision, Proxy.Revision);
	TestEqual(TEXT("Voxel proxy cell"), DecodedProxy.Cells[2048].Pack(), Proxy.Cells[2048].Pack());

	FVoxelSurfaceTileData Surface;
	Surface.Key = { FIntPoint(4, -5), 1 };
	Surface.Side = 2; Surface.Step = 2; Surface.Revision = 18;
	Surface.GroundZ = { 1, 2, 3, 4 }; Surface.WaterZ = { 4, 3, 2, 1 };
	Surface.SurfaceMaterial = { 5, 6, 7, 8 }; Surface.Biome = { 9, 10, 11, 12 };
	Surface.Flags = { 0, 1, 2, 4 };
	Surface.DistantCells.Add({FIntVector(256, -320, 5), FIntVector(272, -304, 21), FVoxelBlockState(3, 9)});
	TestTrue(TEXT("Encode surface data"), FVoxelRepresentationSync::EncodeSurface(Surface, Bytes, Error));
	FVoxelSurfaceTileData DecodedSurface;
	TestTrue(TEXT("Decode surface data"), FVoxelRepresentationSync::DecodeSurface(Bytes, DecodedSurface, Error));
	TestEqual(TEXT("Surface heights"), DecodedSurface.GroundZ, Surface.GroundZ);
	TestEqual(TEXT("Surface flags"), DecodedSurface.Flags, Surface.Flags);
	TestEqual(TEXT("Surface distant cell count"), DecodedSurface.DistantCells.Num(), 1);
	if (DecodedSurface.DistantCells.Num() == 1)
	{
		TestEqual(TEXT("Surface distant cell bounds"), DecodedSurface.DistantCells[0].Max, Surface.DistantCells[0].Max);
		TestEqual(TEXT("Surface distant cell material"), DecodedSurface.DistantCells[0].State.Pack(), Surface.DistantCells[0].State.Pack());
	}

	FVoxelMacroTileData Macro;
	Macro.Key = { FIntPoint(-2, 8), 0 };
	Macro.Side = 2; Macro.Step = 64; Macro.Revision = 19;
	Macro.Height = { 11, 12, 13, 14 }; Macro.WaterHeight = { 5, 6, 7, 8 };
	Macro.SurfaceClass = { 1, 2, 3, 4 }; Macro.ForestCoverage = { 5, 6, 7, 8 };
	Macro.SnowCoverage = { 8, 7, 6, 5 };
	Macro.LargeStructures.Add({ FIntPoint(4, 5), 6, 7, 8 });
	Macro.DistantCells.Add({FIntVector(-4096, 16384, 12), FIntVector(-4048, 16432, 58), FVoxelBlockState(3, 9)});
	TestTrue(TEXT("Encode macro data"), FVoxelRepresentationSync::EncodeMacro(Macro, Bytes, Error));
	FVoxelMacroTileData DecodedMacro;
	TestTrue(TEXT("Decode macro data"), FVoxelRepresentationSync::DecodeMacro(Bytes, DecodedMacro, Error));
	TestEqual(TEXT("Macro height"), DecodedMacro.Height, Macro.Height);
	TestEqual(TEXT("Macro structures"), DecodedMacro.LargeStructures.Num(), 1);
	TestEqual(TEXT("Macro distant cell count"), DecodedMacro.DistantCells.Num(), 1);
	if (DecodedMacro.DistantCells.Num() == 1)
	{
		TestEqual(TEXT("Macro distant cell bounds"), DecodedMacro.DistantCells[0].Min, Macro.DistantCells[0].Min);
		TestEqual(TEXT("Macro distant cell material"), DecodedMacro.DistantCells[0].State.Pack(), Macro.DistantCells[0].State.Pack());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNetworkNaturalFarRepresentationTest,
	"WHFramework.Voxel.Network.NaturalFarRepresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkNaturalFarRepresentationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		VoxelTest::MakeGenerationConfig();
	const TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache =
		MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator =
		MakeShared<const FVoxelGenerationPipeline, ESPMode::ThreadSafe>(Config, Cache);
	FVoxelWorldRuntime Runtime(3, true, VoxelTest::MakeRegistry(), Generator);
	const int32 ResidentBefore = Runtime.NumSections();

	FVoxelRepresentationBuildInput Input;
	Input.Request.Type = EVoxelRepresentationWireType::VoxelProxy;
	Input.Request.Key.Coordinate = FIntVector(8, -4, 1);
	Input.Request.Key.Level = 1;
	Input.Settings = Config->Recipe->Settings;
	Input.Config = Config;
	Input.Cache = Cache;
	Input.Generator = Generator;
	TArray<uint8> Bytes;
	FString Error;
	TestTrue(TEXT("Natural far representation builds locally"), FVoxelRepresentationSync::BuildServerData(Input, Bytes, Error));
	TestEqual(TEXT("Natural far build does not allocate exact runtime"), Runtime.NumSections(), ResidentBefore);

	FVoxelVoxelProxyData Decoded;
	TestTrue(TEXT("Natural far representation decodes"), FVoxelRepresentationSync::DecodeVoxelProxy(Bytes, Decoded, Error));
	TestEqual(TEXT("Natural far representation has proxy cells"), Decoded.Cells.Num(), VoxelBlock::Volume);
	return true;
}

#endif
