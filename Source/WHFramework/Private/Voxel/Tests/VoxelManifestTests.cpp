#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/VoxelModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelManifestV12IdentityTest,
	"WHFramework.Voxel.Generation.ManifestV12Identity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelManifestV12IdentityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.WorldId = FGuid(1, 2, 3, 4);
	Manifest.RegistryHash = 17;
	Manifest.RecipeHash = 23;
	Manifest.BaseSampleHash = 31;
	auto CheckLandform = [this, &Manifest](const TCHAR* Name,
		int32 FVoxelLandformGenerationSettings::* Member)
	{
		const uint64 Previous = FVoxelManifestCodec::RecipeFingerprint(Manifest);
		++(Manifest.Settings.Landform.*Member);
		TestNotEqual(FString::Printf(TEXT("%s changes generation identity"), Name),
			FVoxelManifestCodec::RecipeFingerprint(Manifest), Previous);
		TArray<uint8> Bytes;
		FVoxelWorldManifest Loaded;
		if (!TestTrue(TEXT("v12 manifest encodes"), FVoxelManifestCodec::Encode(Manifest, Bytes)) ||
			!TestTrue(TEXT("v12 manifest decodes"), FVoxelManifestCodec::Decode(Bytes, Loaded))) return;
		TestEqual(FString::Printf(TEXT("%s survives save/load"), Name),
			Loaded.Settings.Landform.*Member, Manifest.Settings.Landform.*Member);
		TestEqual(TEXT("Save/load preserves the complete generation identity"),
			FVoxelManifestCodec::RecipeFingerprint(Loaded), FVoxelManifestCodec::RecipeFingerprint(Manifest));
	};
	CheckLandform(TEXT("DomainPeriod"), &FVoxelLandformGenerationSettings::DomainPeriod);
	CheckLandform(TEXT("ReliefPeriod"), &FVoxelLandformGenerationSettings::ReliefPeriod);
	CheckLandform(TEXT("HillsPeriod"), &FVoxelLandformGenerationSettings::HillsPeriod);
	CheckLandform(TEXT("HillsDetailPeriod"), &FVoxelLandformGenerationSettings::HillsDetailPeriod);
	CheckLandform(TEXT("PlateauPeriod"), &FVoxelLandformGenerationSettings::PlateauPeriod);
	CheckLandform(TEXT("ValleyPeriod"), &FVoxelLandformGenerationSettings::ValleyPeriod);
	CheckLandform(TEXT("DomainWarpCells"), &FVoxelLandformGenerationSettings::DomainWarpCells);
	CheckLandform(TEXT("PlainRelief"), &FVoxelLandformGenerationSettings::PlainRelief);
	CheckLandform(TEXT("HillRelief"), &FVoxelLandformGenerationSettings::HillRelief);
	CheckLandform(TEXT("HillsDetailRelief"), &FVoxelLandformGenerationSettings::HillsDetailRelief);
	CheckLandform(TEXT("HighlandUplift"), &FVoxelLandformGenerationSettings::HighlandUplift);
	CheckLandform(TEXT("MountainBaseUplift"), &FVoxelLandformGenerationSettings::MountainBaseUplift);
	CheckLandform(TEXT("PlateauUplift"), &FVoxelLandformGenerationSettings::PlateauUplift);
	CheckLandform(TEXT("BasinDepth"), &FVoxelLandformGenerationSettings::BasinDepth);
	CheckLandform(TEXT("ValleyDepth"), &FVoxelLandformGenerationSettings::ValleyDepth);
	CheckLandform(TEXT("BlendSharpnessQ15"), &FVoxelLandformGenerationSettings::BlendSharpnessQ15);
	auto CheckCave = [this, &Manifest](int32 FVoxelGenerationSettings::* Member)
	{
		const uint64 Previous = FVoxelManifestCodec::RecipeFingerprint(Manifest);
		++(Manifest.Settings.*Member);
		TestNotEqual(TEXT("Cave entrance dimensions change generation identity"),
			FVoxelManifestCodec::RecipeFingerprint(Manifest), Previous);
		TArray<uint8> Bytes;
		FVoxelWorldManifest Loaded;
		if (!TestTrue(TEXT("Cave dimensions encode"), FVoxelManifestCodec::Encode(Manifest, Bytes)) ||
			!TestTrue(TEXT("Cave dimensions decode"), FVoxelManifestCodec::Decode(Bytes, Loaded))) return;
		TestEqual(TEXT("Cave entrance dimensions survive save/load"),
			Loaded.Settings.*Member, Manifest.Settings.*Member);
	};
	CheckCave(&FVoxelGenerationSettings::CaveEntranceMinWidth);
	CheckCave(&FVoxelGenerationSettings::CaveEntranceMinHeight);
	CheckCave(&FVoxelGenerationSettings::CaveEntranceClearance);
	TArray<uint8> Bytes;
	FVoxelWorldManifest Loaded;
	if (!TestTrue(TEXT("Current manifest encodes"), FVoxelManifestCodec::Encode(Manifest, Bytes))) return false;
	Bytes[4] = 5;
	TestFalse(TEXT("Previous generation identity protocol is rejected"), FVoxelManifestCodec::Decode(Bytes, Loaded));
	Manifest.GeneratorVersion = 11;
	FVoxelWorldSaveData PreviousWorld;
	TestTrue(TEXT("Old algorithm fixture encodes"), FVoxelManifestCodec::Encode(Manifest, PreviousWorld.ManifestBytes));
	FString Error;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	TestFalse(TEXT("Runtime refuses a previous terrain algorithm"),
		Module->ValidateWorldData(FParameter(MoveTemp(PreviousWorld)), Error));
	TestTrue(TEXT("Runtime explains the algorithm mismatch"),
		Error.Contains(TEXT("algorithm 11 differs from current algorithm 12")));
	return true;
}

#endif
