#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Network/VoxelRepresentationSync.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Rendering/VoxelRemotePageBuild.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
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
	Source.Entries.Add({{{EVoxelTerrainRepresentation::VoxelProxy, FIntVector(-5, 4, 2), 1}, 2, -128, 128, 17, 23}, 123});
	Source.Entries.Add({{{EVoxelTerrainRepresentation::Surface, FIntVector(8, -7, 0), 3}, 4, -128, 128, 17, 23}, 91});
	TArray<uint8> Bytes;
	FString Error;
	TestTrue(TEXT("Encode representation invalidation"),
		FVoxelNetworkCodec::EncodeRepresentationInvalidate(Source, Bytes, Error));
	FVoxelRepresentationInvalidate Decoded;
	TestTrue(TEXT("Decode representation invalidation"),
		FVoxelNetworkCodec::DecodeRepresentationInvalidate(Bytes, Decoded, Error));
	TestEqual(TEXT("Invalidation key count"), Decoded.Entries.Num(), Source.Entries.Num());
	TestTrue(TEXT("First complete geometry"), Decoded.Entries[0].Geometry == Source.Entries[0].Geometry);
	TestTrue(TEXT("Second complete geometry"), Decoded.Entries[1].Geometry == Source.Entries[1].Geometry);
	TestEqual(TEXT("First local revision"), Decoded.Entries[0].Revision, uint64(123));
	TestEqual(TEXT("Second local revision"), Decoded.Entries[1].Revision, uint64(91));
	Bytes.Add(0xff);
	TestFalse(TEXT("Trailing invalidation bytes rejected"), FVoxelNetworkCodec::DecodeRepresentationInvalidate(Bytes, Decoded, Error));
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
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(3, FGuid::NewGuid(), 1, {}, 1);
	FVoxelBinaryBuffer Bytes;

	FVoxelVoxelProxyData Proxy;
	Proxy.Key = { FIntVector(-3, 2, 1), 2 };
	Proxy.Revision = 17;
	Proxy.RequestId = 1;
	Proxy.Geometry = {{EVoxelTerrainRepresentation::VoxelProxy, Proxy.Key.Coordinate, Proxy.Key.Level},
		2, -128, 128, 17, 23};
	Proxy.GridSide = int32(Proxy.Geometry.Key.SideCells() / Proxy.Geometry.SampleStepCells);
	Proxy.Cells.Init(FVoxelBlockState { 3, 9 }, Proxy.GridSide * Proxy.GridSide * Proxy.GridSide);
	if (!TestTrue(TEXT("Encode voxel proxy data"), FVoxelRepresentationSync::EncodeVoxelProxy(Proxy, Bytes, Error, Resources, Consumer))) return false;
	FVoxelVoxelProxyData DecodedProxy;
	if (!TestTrue(TEXT("Decode voxel proxy data"), FVoxelRepresentationSync::DecodeVoxelProxy(Bytes.Bytes, DecodedProxy, Error, Resources, Consumer)) ||
		!TestEqual(TEXT("Voxel proxy complete cell count"), DecodedProxy.Cells.Num(), Proxy.Cells.Num())) return false;
	TestEqual(TEXT("Voxel proxy key"), DecodedProxy.Key.Coordinate, Proxy.Key.Coordinate);
	TestEqual(TEXT("Voxel proxy revision"), DecodedProxy.Revision, Proxy.Revision);
	TestEqual(TEXT("Voxel proxy cell"), DecodedProxy.Cells[2048].Pack(), Proxy.Cells[2048].Pack());
	const FVoxelTerrainSpatialKey SpatialKey = Proxy.Geometry.Key;
	for (const int32 Step : {4, 8})
	{
		Proxy.Geometry.SampleStepCells = Step;
		Proxy.GridSide = int32(Proxy.Geometry.Key.SideCells() / Step);
		Proxy.Cells.Init(FVoxelBlockState{3, 9}, Proxy.GridSide * Proxy.GridSide * Proxy.GridSide);
		if (!TestTrue(TEXT("Same spatial key encodes a different valid sampling identity"),
			FVoxelRepresentationSync::EncodeVoxelProxy(Proxy, Bytes, Error, Resources, Consumer)) ||
			!TestTrue(TEXT("Actual sampling identity decodes"), FVoxelRepresentationSync::DecodeVoxelProxy(Bytes.Bytes, DecodedProxy, Error, Resources, Consumer))) return false;
		TestTrue(TEXT("Spatial identity is preserved independently of sampling"), DecodedProxy.Geometry.Key == SpatialKey);
		TestTrue(TEXT("Complete sampling identity survives network roundtrip"), DecodedProxy.Geometry == Proxy.Geometry);
		TestEqual(TEXT("Decoded grid follows actual geometry"), DecodedProxy.GridSide, Proxy.GridSide);
	}
	Proxy.Geometry.SampleStepCells = 1;
	TestFalse(TEXT("Fine step cannot enter a Proxy response"), FVoxelRepresentationSync::EncodeVoxelProxy(Proxy, Bytes, Error, Resources, Consumer));
	Proxy.Geometry.SampleStepCells = 8;
	Proxy.GridSide = 16;
	Proxy.Cells.SetNum(4096);
	TestFalse(TEXT("Wrong proxy sampling resolution cannot encode"), FVoxelRepresentationSync::EncodeVoxelProxy(Proxy, Bytes, Error, Resources, Consumer));

	FVoxelSurfaceTileData Surface;
	Surface.Key = { FIntPoint(4, -5), 1 };
	Surface.Side = 2; Surface.Step = 32; Surface.Revision = 18;
	Surface.RequestId = 2;
	Surface.Geometry = {{EVoxelTerrainRepresentation::Surface, FIntVector(Surface.Key.Coordinate.X, Surface.Key.Coordinate.Y, 0), Surface.Key.Level},
		Surface.Step, -128, 128, 17, 23};
	Surface.GroundZ = { 1, 2, 3, 4 }; Surface.WaterZ = { 4, 3, 2, 1 };
	Surface.SurfaceMaterial = { 5, 6, 7, 8 }; Surface.Biome = { 9, 10, 11, 12 };
	Surface.Flags = { 0, 1, 2, 4 };
	Surface.DistantCells.Add({FIntVector(128, -160, 5), FIntVector(144, -144, 21),
		FVoxelBlockState(3, 9), {17, 23}});
	if (!TestTrue(TEXT("Encode surface data"), FVoxelRepresentationSync::EncodeSurface(Surface, Bytes, Error, Resources, Consumer))) return false;
	FVoxelSurfaceTileData DecodedSurface;
	if (!TestTrue(TEXT("Decode surface data"), FVoxelRepresentationSync::DecodeSurface(Bytes.Bytes, DecodedSurface, Error, Resources, Consumer))) return false;
	TestEqual(TEXT("Surface heights"), DecodedSurface.GroundZ, Surface.GroundZ);
	TestEqual(TEXT("Surface flags"), DecodedSurface.Flags, Surface.Flags);
	TestEqual(TEXT("Surface distant cell count"), DecodedSurface.DistantCells.Num(), 1);
	if (DecodedSurface.DistantCells.Num() == 1)
	{
		TestEqual(TEXT("Surface distant cell bounds"), DecodedSurface.DistantCells[0].Max, Surface.DistantCells[0].Max);
		TestEqual(TEXT("Surface distant cell material"), DecodedSurface.DistantCells[0].State.Pack(), Surface.DistantCells[0].State.Pack());
		TestTrue(TEXT("Surface distant tree identity"),
			DecodedSurface.DistantCells[0].SourceTreeId == Surface.DistantCells[0].SourceTreeId);
	}

	FVoxelMacroTileData Macro;
	Macro.Key = { FIntPoint(-2, 8), 2 };
	Macro.Side = 2; Macro.Step = 64; Macro.Revision = 19;
	Macro.RequestId = 3;
	Macro.Geometry = {{EVoxelTerrainRepresentation::Macro, FIntVector(Macro.Key.Coordinate.X, Macro.Key.Coordinate.Y, 0), Macro.Key.Level},
		Macro.Step, -128, 128, 17, 23};
	Macro.Height = { 11, 12, 13, 14 }; Macro.WaterHeight = { 5, 6, 7, 8 };
	Macro.WaterKind = {uint8(EVoxelWaterKind::River), uint8(EVoxelWaterKind::Lake),
		uint8(EVoxelWaterKind::Ocean), uint8(EVoxelWaterKind::None)};
	Macro.SurfaceClass = { 1, 2, 3, 4 }; Macro.ForestCoverage = { 5, 6, 7, 8 };
	Macro.SnowCoverage = { 8, 7, 6, 5 };
	Macro.LargeStructures.Add({ FIntPoint(4, 5), 6, 7, 8 });
	Macro.DistantCells.Add({FIntVector(-128, 512, 12), FIntVector(-80, 560, 58),
		FVoxelBlockState(3, 9), {31, 37}});
	if (!TestTrue(TEXT("Encode macro data"), FVoxelRepresentationSync::EncodeMacro(Macro, Bytes, Error, Resources, Consumer))) return false;
	FVoxelMacroTileData DecodedMacro;
	if (!TestTrue(TEXT("Decode macro data"), FVoxelRepresentationSync::DecodeMacro(Bytes.Bytes, DecodedMacro, Error, Resources, Consumer))) return false;
	TestEqual(TEXT("Macro height"), DecodedMacro.Height, Macro.Height);
	TestEqual(TEXT("Macro water semantics"), DecodedMacro.WaterKind, Macro.WaterKind);
	TestEqual(TEXT("Macro structures"), DecodedMacro.LargeStructures.Num(), 1);
	TestEqual(TEXT("Macro distant cell count"), DecodedMacro.DistantCells.Num(), 1);
	if (DecodedMacro.DistantCells.Num() == 1)
	{
		TestEqual(TEXT("Macro distant cell bounds"), DecodedMacro.DistantCells[0].Min, Macro.DistantCells[0].Min);
		TestEqual(TEXT("Macro distant cell material"), DecodedMacro.DistantCells[0].State.Pack(), Macro.DistantCells[0].State.Pack());
		TestTrue(TEXT("Macro distant tree identity"),
			DecodedMacro.DistantCells[0].SourceTreeId == Macro.DistantCells[0].SourceTreeId);
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
	Input.Registry = VoxelTest::MakeRegistry();
	Input.Request.Serial = 1;
	Input.Request.Geometry = {{EVoxelTerrainRepresentation::VoxelProxy, Input.Request.Key.Coordinate, Input.Request.Key.Level},
		2, Input.Settings.MinZ, Input.Settings.MaxZ, 17, 23};
	Input.OverlayCapture.WorldEpoch = Runtime.Epoch();
	Input.OverlayCapture.Bounds = FVoxelViewKey{Input.Request.Key.Coordinate, Input.Request.Key.Level}.GetBounds();
	Input.OverlayCapture.Manifest.Settings = Input.Settings;
	Input.OverlayCapture.Registry = Input.Registry;
	Input.OverlayCapture.ReadLeaseSet = FVoxelRegionStore::CreateOverlayReadLeaseSet(Input.OverlayCapture);
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(3, FGuid::NewGuid(), 1, {}, 1);
	FVoxelBinaryBuffer Bytes;
	FString Error;
	const FVoxelTerrainGeometry ExpectedGeometry = Input.Request.Geometry;
	FVoxelRepresentationPageBuild Build(Resources.ToSharedRef(), Consumer);
	if (!TestTrue(TEXT("Natural far representation initializes"), Build.Initialize(MoveTemp(Input), Error))) return false;
	bool bComplete = false;
	for (int32 StepIndex = 0; StepIndex < 4096 && !bComplete; ++StepIndex)
	{
		if (!TestTrue(TEXT("Natural far representation advances"), Build.Advance(Bytes, bComplete, Error))) return false;
	}
	if (!TestTrue(TEXT("Natural far representation completes bounded page work"), bComplete)) return false;
	TestEqual(TEXT("Natural far build does not allocate exact runtime"), Runtime.NumSections(), ResidentBefore);

	FVoxelVoxelProxyData Decoded;
	if (!TestTrue(TEXT("Natural far representation decodes"), FVoxelRepresentationSync::DecodeVoxelProxy(Bytes.Bytes, Decoded, Error, Resources, Consumer))) return false;
	const FVoxelViewKey ExpectedKey{ExpectedGeometry.Key.Coordinate, ExpectedGeometry.Key.Level};
	const int32 Side = ExpectedKey.GetSide() / ExpectedGeometry.SampleStepCells;
	TestTrue(TEXT("Network response retains requested sampling identity"), Decoded.Geometry == ExpectedGeometry);
	TestEqual(TEXT("Natural far representation has complete proxy cells"), Decoded.Cells.Num(), Side * Side * Side);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNetworkGrantedCompressionLimitTest,
	"WHFramework.Voxel.Network.GrantedCompressionLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkGrantedCompressionLimitTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(71, FGuid::NewGuid(), 1, {}, 1);
	TArray<uint8> Raw;
	Raw.Init(0, 2 * 1024 * 1024);
	FVoxelBinaryBuffer Encoded;
	bool bCapacityBlocked = false;
	if (!TestTrue(TEXT("Compressible raw body may exceed its wire limit"),
		VoxelBinary::CompressGranted(Raw, Encoded, Resources, Consumer, Raw.Num(), 4096, bCapacityBlocked))) return false;
	TestFalse(TEXT("Successful compression is not capacity blocked"), bCapacityBlocked);
	TestTrue(TEXT("Compression bound exceeds the allowed stored bytes"), VoxelBinary::CompressionBound(Raw.Num()) > 4096);
	TestTrue(TEXT("Actual stored bytes fit the wire limit"), Encoded.Bytes.Num() <= 4096);
	TestTrue(TEXT("Output backing remains charged"), Encoded.ResourceLease &&
		Encoded.Bytes.GetAllocatedSize() <= Encoded.ResourceLease->Bytes());
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RawLease;
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
			VoxelBoundedAllocation::ArrayBytes<uint8>(Raw.Num()), RawLease)) return false;
		TArray<uint8> Decoded;
		if (!TestTrue(TEXT("Granted native inflate roundtrip"), VoxelBinary::DecompressGranted(
			Encoded.Bytes, Decoded, Resources, Consumer, Raw.Num(), bCapacityBlocked))) return false;
		TestEqual(TEXT("Complete raw body survived the smaller wire envelope"), Decoded, Raw);
	}
	const TArray<uint8> Original = Encoded.Bytes;
	const uint64 OriginalLease = Encoded.ResourceLease->Serial();
	FVoxelTerrainResourceLimits Limits;
	Limits.WorkingBytes = 64 * 1024;
	if (!TestTrue(TEXT("Constrain native scratch without invalidating live output"), Resources->Configure(Limits))) return false;
	TestFalse(TEXT("Native allocation denial suspends compression"), VoxelBinary::CompressGranted(
		Raw, Encoded, Resources, Consumer, Raw.Num(), 4096, bCapacityBlocked));
	TestTrue(TEXT("Native allocation denial reports capacity"), bCapacityBlocked);
	TestEqual(TEXT("Denied attempt preserves old bytes"), Encoded.Bytes, Original);
	TestEqual(TEXT("Denied attempt preserves old ownership"), Encoded.ResourceLease->Serial(), OriginalLease);
	Limits.WorkingBytes = 256ull * 1024 * 1024;
	if (!Resources->Configure(Limits)) return false;
	if (!TestTrue(TEXT("Same frozen input resumes after capacity restoration"), VoxelBinary::CompressGranted(
		Raw, Encoded, Resources, Consumer, Raw.Num(), 4096, bCapacityBlocked))) return false;
	TestEqual(TEXT("Capacity restoration keeps deterministic wire content"), Encoded.Bytes, Original);
	TestFalse(TEXT("An actually insufficient wire limit is rejected"), VoxelBinary::CompressGranted(
		Raw, Encoded, Resources, Consumer, Raw.Num(), 17, bCapacityBlocked));
	TestFalse(TEXT("Wire size failure is distinct from capacity waiting"), bCapacityBlocked);
	TestEqual(TEXT("Wire failure preserves accepted output"), Encoded.Bytes, Original);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNetworkFrozenPageCodecRetryTest,
	"WHFramework.Voxel.Network.FrozenPageCodecRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkFrozenPageCodecRetryTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const auto Generator = MakeShared<const FVoxelGenerationPipeline, ESPMode::ThreadSafe>(Config, Cache);
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(72, FGuid::NewGuid(), 1, {}, 1);
	FVoxelTerrainResourceLimits Limits;
	Limits.WorkingBytes = 64 * 1024;
	if (!Resources->Configure(Limits)) return false;
	FVoxelRepresentationBuildInput Input;
	Input.Request.Type = EVoxelRepresentationWireType::VoxelProxy;
	Input.Request.Key = {FIntVector(8, -4, 1), 1};
	Input.Request.Serial = 37;
	Input.Settings = Config->Recipe->Settings;
	Input.Config = Config; Input.Cache = Cache; Input.Generator = Generator; Input.Registry = VoxelTest::MakeRegistry();
	Input.Request.Geometry = {{EVoxelTerrainRepresentation::VoxelProxy, Input.Request.Key.Coordinate, 1},
		2, Input.Settings.MinZ, Input.Settings.MaxZ, 17, 23};
	Input.Revision = 19;
	Input.OverlayCapture.WorldEpoch = 72;
	Input.OverlayCapture.Bounds = FVoxelViewKey{Input.Request.Key.Coordinate, 1}.GetBounds();
	Input.OverlayCapture.Manifest.Settings = Input.Settings;
	Input.OverlayCapture.Registry = Input.Registry;
	Input.OverlayCapture.ReadLeaseSet = FVoxelRegionStore::CreateOverlayReadLeaseSet(Input.OverlayCapture);
	const FVoxelTerrainGeometry ExpectedGeometry = Input.Request.Geometry;
	FVoxelRepresentationPageBuild Build(Resources.ToSharedRef(), Consumer);
	FString Error;
	if (!TestTrue(TEXT("Frozen page initializes"), Build.Initialize(MoveTemp(Input), Error))) return false;
	FVoxelBinaryBuffer Encoded;
	bool bComplete = false;
	bool bBlocked = false;
	for (int32 StepIndex = 0; StepIndex < 4096 && !bBlocked; ++StepIndex)
	{
		if (!Build.Advance(Encoded, bComplete, Error))
		{
			bBlocked = Error.StartsWith(TEXT("CapacityBlocked"));
			break;
		}
	}
	if (!TestTrue(TEXT("Real page generation reaches a codec capacity wait"), bBlocked)) return false;
	TestFalse(TEXT("Capacity failure does not publish completion"), bComplete);
	TestTrue(TEXT("No partial encoded body is accepted"), Encoded.Bytes.IsEmpty());
	const auto OriginalSnapshot = Build.GetOverlayReadResult().Snapshot;
	Limits.WorkingBytes = 256ull * 1024 * 1024;
	if (!Resources->Configure(Limits)) return false;
	if (!TestTrue(TEXT("One retry encodes the retained generated body"), Build.Advance(Encoded, bComplete, Error)) ||
		!TestTrue(TEXT("Retry completes without rebuilding/taking page twice"), bComplete)) return false;
	TestTrue(TEXT("Retry retains original overlay proof"), Build.GetOverlayReadResult().Snapshot == OriginalSnapshot);
	FVoxelVoxelProxyData Decoded;
	if (!TestTrue(TEXT("Retained encoded page decodes"), FVoxelRepresentationSync::DecodeVoxelProxy(
		Encoded.Bytes, Decoded, Error, Resources, Consumer))) return false;
	TestTrue(TEXT("Retry preserves complete geometry"), Decoded.Geometry == ExpectedGeometry);
	TestEqual(TEXT("Retry preserves request serial"), Decoded.RequestId, uint64(37));
	TestEqual(TEXT("Retry preserves content revision"), Decoded.Revision, uint64(19));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNetworkRemoteAdmissionCapacityTest,
	"WHFramework.Voxel.Network.RemoteAdmissionCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNetworkRemoteAdmissionCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelTerrainGeometry Geometry{{EVoxelTerrainRepresentation::VoxelProxy, {}, 1}, 2, -128, 128, 17, 23};
	TestTrue(TEXT("Actual Proxy geometry is admitted"), FVoxelRemotePageBuild::RawCapacity(Geometry) > 0);
	Geometry.SampleStepCells = 0;
	TestEqual(TEXT("Zero sampling rejected before division"), FVoxelRemotePageBuild::RawCapacity(Geometry), uint64(0));
	Geometry.SampleStepCells = 2; Geometry.Key.Level = 0;
	TestEqual(TEXT("Proxy L0 cannot enter the remote coarse chain"), FVoxelRemotePageBuild::RawCapacity(Geometry), uint64(0));
	Geometry.Key.Representation = EVoxelTerrainRepresentation::Fine; Geometry.SampleStepCells = 1;
	TestEqual(TEXT("Fine is not a remote coarse representation"), FVoxelRemotePageBuild::RawCapacity(Geometry), uint64(0));
	Geometry.Key = {EVoxelTerrainRepresentation::Surface, {}, 5}; Geometry.SampleStepCells = 2;
	TestTrue(TEXT("Actual 256-sample column is admitted"), FVoxelRemotePageBuild::RawCapacity(Geometry) > 0);
	Geometry.Key.Level = 6;
	TestEqual(TEXT("512-sample column cannot overrun wire/mesh limits"), FVoxelRemotePageBuild::RawCapacity(Geometry), uint64(0));

	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(73, FGuid::NewGuid(), 1, {}, 1);
	auto OtherAttempt = Consumer;
	++OtherAttempt.RequestSerial;
	FVoxelTerrainResourceLimits Limits;
	Limits.WorkingBytes = 8 * 1024 * 1024;
	if (!Resources->Configure(Limits)) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OtherLease, OwnLease;
	if (!Resources->Acquire(OtherAttempt, EVoxelTerrainResourcePool::Working, 6 * 1024 * 1024, OtherLease) ||
		!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, 32768, OwnLease)) return false;
	FVoxelCoarseBuildStorage Storage(Resources.ToSharedRef(), Consumer, nullptr);
	Storage.WorkingCapacityBytes = 4 * 1024 * 1024;
	FVoxelTaskResult Result;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
	TestFalse(TEXT("Real coarse capacity acquisition waits"), Storage.BeginCount(Result, Working));
	TestEqual(TEXT("Waiting is memory blocked"), uint8(Storage.BlockReason()), uint8(EVoxelTaskBlockReason::Memory));
	OwnLease.Reset();
	TestEqual(TEXT("Own cleanup cannot trigger a retry loop"), uint8(Storage.BlockReason()), uint8(EVoxelTaskBlockReason::Memory));
	OtherLease.Reset();
	TestEqual(TEXT("Another attempt on the same module wakes waiting work"), uint8(Storage.BlockReason()), uint8(EVoxelTaskBlockReason::None));
	TestTrue(TEXT("Original consumer resumes successfully"), Storage.BeginCount(Result, Working));
	return true;
}

#endif
