#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SaveGame/SaveDataSerializer.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Save/VoxelRegionStore.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/Save/VoxelWorldSaveAdapter.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/VoxelModuleTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelManifestCodecTest,
	"WHFramework.Voxel.Save.ManifestV4",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelManifestCodecTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Source;
	Source.WorldId = FGuid::NewGuid();
	Source.RecipeHash = 0x1122334455667788ull;
	Source.BaseSampleHash = 0x8877665544332211ull;
	Source.RegistryHash = 0x12345678ull;
	Source.Settings.Seed = 42;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Manifest encodes"), FVoxelManifestCodec::Encode(Source, Bytes));
	FVoxelWorldManifest Decoded;
	TestTrue(TEXT("Manifest decodes"), FVoxelManifestCodec::Decode(Bytes, Decoded));
	TestEqual(TEXT("Protocol V4 is fixed"), FVoxelWorldManifest::ProtocolVersion, uint32(4));
	TestEqual(TEXT("Recipe hash preserved"), Decoded.RecipeHash, Source.RecipeHash);
	TestEqual(TEXT("Base sample hash preserved"), Decoded.BaseSampleHash, Source.BaseSampleHash);
	TestEqual(TEXT("Manifest fingerprint stable"), FVoxelManifestCodec::RecipeFingerprint(Decoded), FVoxelManifestCodec::RecipeFingerprint(Source));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSceneColumnCodecTest,
	"WHFramework.Voxel.Save.SceneColumnCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSceneColumnCodecTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const FIntPoint Column(-12, 34);
	TestEqual(
		TEXT("Scene files use deterministic XY column paths"),
		FVoxelSceneColumnCodec::RelativePath(Column),
		FString(TEXT("voxel/actors/c_-12_34.bin")));

	FVoxelWorldBasicSaveData ActorData;
	ActorData.Seed = 19;
	FVoxelSavedSceneActor SourceActor;
	SourceActor.Id = FGuid::NewGuid();
	SourceActor.Data = FParameter(ActorData);
	FVoxelWorldBasicSaveData SourceProjectData;
	SourceProjectData.Seed = 23;
	TArray<FVoxelSavedSceneActor> SourceActors{ SourceActor };
	FParameter DirectSource(ActorData);
	TestTrue(TEXT("Direct payload marks saved"), FSaveDataSerializer::MarkSaveDataSaved(DirectSource));
	TArray<uint8> DirectBytes;
	TestTrue(TEXT("Direct payload serializes"), FSaveDataSerializer::SerializeParameter(DirectSource, DirectBytes));
	FParameter DirectDecoded;
	TestTrue(TEXT("Direct payload deserializes"), FSaveDataSerializer::DeserializeParameter(DirectBytes, DirectDecoded));
	TArray<uint8> ModuleBytes;
	TestTrue(
		TEXT("V2 module file builds"),
		FSaveDataSerializer::BuildModuleFile(TEXT("VoxelTest"), 4, DirectSource, ModuleBytes));
	FModuleSaveFileHeader ModuleHeader;
	FParameter ModuleDecoded;
	TestTrue(
		TEXT("V2 module file reads"),
		FSaveDataSerializer::ReadModuleFile(ModuleBytes, ModuleHeader, ModuleDecoded));
	TestEqual(
		TEXT("Module storage version is current"),
		ModuleHeader.StorageVersion,
		FModuleSaveFileHeader::CurrentStorageVersion);
	if (ModuleBytes.Num() >= 8)
	{
		ModuleBytes[4] = 1;
		ModuleBytes[5] = 0;
		ModuleBytes[6] = 0;
		ModuleBytes[7] = 0;
		TestFalse(
			TEXT("Legacy module storage is rejected"),
			FSaveDataSerializer::ReadModuleFile(ModuleBytes, ModuleHeader, ModuleDecoded));
	}
	TArray<uint8> Bytes;
	FString Error;
	TestTrue(
		TEXT("Scene column encodes"),
		FVoxelSceneColumnCodec::Encode(
			SourceActors,
			FParameter(SourceProjectData),
			Bytes,
			Error));
	TArray<FVoxelSavedSceneActor> Actors;
	FParameter ProjectData;
	const bool bDecoded = FVoxelSceneColumnCodec::Decode(Bytes, Actors, ProjectData, Error);
	TestTrue(
		TEXT("Scene column decodes"),
		bDecoded);
	if (!bDecoded)
	{
		AddError(FString::Printf(TEXT("Scene decode error: %s"), *Error));
	}
	TestEqual(TEXT("Actor count roundtrips"), Actors.Num(), 1);
	if (Actors.Num() == 1)
	{
		TestEqual(TEXT("Actor id roundtrips"), Actors[0].Id, SourceActor.Id);
		const FVoxelWorldBasicSaveData* DecodedActor = Actors[0].Data.GetPtr<FVoxelWorldBasicSaveData>();
		TestNotNull(TEXT("Actor payload type roundtrips"), DecodedActor);
		if (DecodedActor)
		{
			TestEqual(TEXT("Actor payload roundtrips"), DecodedActor->Seed, 19);
		}
	}
	const FVoxelWorldBasicSaveData* DecodedProject = ProjectData.GetPtr<FVoxelWorldBasicSaveData>();
	TestNotNull(TEXT("Project payload type roundtrips"), DecodedProject);
	if (DecodedProject)
	{
		TestEqual(TEXT("Project payload roundtrips"), DecodedProject->Seed, 23);
	}

	Bytes.Add(0);
	TestFalse(
		TEXT("Scene column rejects trailing bytes"),
		FVoxelSceneColumnCodec::Decode(Bytes, Actors, ProjectData, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelRegionStoreTransactionTest,
	"WHFramework.Voxel.Save.RegionStoreTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelRegionStoreTransactionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const FString Root = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation"),
		TEXT("VoxelRegionStore"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString Generation1 = FPaths::Combine(Root, TEXT("1"));
	const FString Generation2 = FPaths::Combine(Root, TEXT("2"));
	const FString Generation3 = FPaths::Combine(Root, TEXT("3"));
	const FIntVector SectionA(-8, -8, -8);
	const FIntVector SectionB(-1, -1, -1);
	FString Error;

	FVoxelRegionWritePlan First;
	First.TransactionId = FGuid::NewGuid();
	TArray<uint8> BytesA{ 1, 2, 3 };
	TArray<uint8> BytesB{ 4, 5, 6, 7 };
	TestTrue(TEXT("First section stages"), FVoxelRegionStore::StageSection(First, SectionA, MoveTemp(BytesA)));
	TestTrue(TEXT("Second section stages"), FVoxelRegionStore::StageSection(First, SectionB, MoveTemp(BytesB)));
	TestTrue(TEXT("First region transaction writes"), FVoxelRegionStore::WritePendingRegions(First, Generation1, Error));

	FVoxelRegionStore Store;
	Store.AdvanceSource(Generation1);
	TArray<uint8> ReadBytes;
	TestEqual(TEXT("First section reads"), Store.ReadSection(SectionA, ReadBytes, Error), EVoxelRegionRead::Loaded);
	TestTrue(TEXT("First section payload matches"), ReadBytes == TArray<uint8>({ 1, 2, 3 }));
	TestEqual(TEXT("Second section reads"), Store.ReadSection(SectionB, ReadBytes, Error), EVoxelRegionRead::Loaded);
	TestTrue(TEXT("Second section payload matches"), ReadBytes == TArray<uint8>({ 4, 5, 6, 7 }));

	uint64 Revision = 0;
	TArray<uint64> Mask;
	TestTrue(TEXT("Change header reads"), Store.ReadChangeHeader(FIntVector(-1), Revision, Mask, Error));
	TestEqual(TEXT("First region revision is one"), Revision, uint64(1));
	TestEqual(TEXT("Change mask has eight words"), Mask.Num(), 8);
	if (Mask.Num() == 8)
	{
		TestTrue(TEXT("Local zero is modified"), (Mask[0] & 1ull) != 0);
		TestTrue(TEXT("Local 511 is modified"), (Mask[7] & (1ull << 63)) != 0);
	}

	FVoxelRegionWritePlan Second;
	Second.TransactionId = FGuid::NewGuid();
	Second.SourceDirectory = Generation1;
	TArray<uint8> Replacement{ 8, 9 };
	TestTrue(TEXT("Replacement stages"), FVoxelRegionStore::StageSection(Second, SectionA, MoveTemp(Replacement)));
	FVoxelRegionStore::StageDelete(Second, SectionB);
	TestTrue(TEXT("Second region transaction writes"), FVoxelRegionStore::WritePendingRegions(Second, Generation2, Error));
	Store.AdvanceSource(Generation2);
	TestEqual(TEXT("Replacement reads"), Store.ReadSection(SectionA, ReadBytes, Error), EVoxelRegionRead::Loaded);
	TestTrue(TEXT("Replacement payload matches"), ReadBytes == TArray<uint8>({ 8, 9 }));
	TestEqual(TEXT("Deleted section is absent"), Store.ReadSection(SectionB, ReadBytes, Error), EVoxelRegionRead::Missing);
	TestTrue(TEXT("Updated change header reads"), Store.ReadChangeHeader(FIntVector(-1), Revision, Mask, Error));
	TestEqual(TEXT("Region revision increments"), Revision, uint64(2));
	if (Mask.Num() == 8)
	{
		TestTrue(TEXT("Remaining section stays modified"), (Mask[0] & 1ull) != 0);
		TestFalse(TEXT("Deleted section leaves mask"), (Mask[7] & (1ull << 63)) != 0);
	}

	FVoxelRegionWritePlan Third;
	Third.TransactionId = FGuid::NewGuid();
	Third.SourceDirectory = Generation2;
	FVoxelRegionStore::StageDelete(Third, SectionA);
	TestTrue(TEXT("Final delete transaction writes"), FVoxelRegionStore::WritePendingRegions(Third, Generation3, Error));
	Store.AdvanceSource(Generation3);
	TestEqual(TEXT("Empty region has no record"), Store.ReadSection(SectionA, ReadBytes, Error), EVoxelRegionRead::Missing);
	TestFalse(
		TEXT("Empty region file is removed"),
		IFileManager::Get().FileExists(*FPaths::Combine(Generation3, TEXT("voxel/regions/r_-1_-1_-1.bin"))));

	const FString RegionFile = FPaths::Combine(Generation2, TEXT("voxel/regions/r_-1_-1_-1.bin"));
	TArray<uint8> Corrupt;
	if (TestTrue(TEXT("Region file loads for corruption test"), FFileHelper::LoadFileToArray(Corrupt, *RegionFile)) &&
		Corrupt.Num() > 20)
	{
		Corrupt[20] ^= 1;
		TestTrue(TEXT("Corrupt region file saves"), FFileHelper::SaveArrayToFile(Corrupt, *RegionFile));
		Store.AdvanceSource(Generation2);
		TestEqual(TEXT("Header-mask mismatch is rejected"), Store.ReadSection(SectionA, ReadBytes, Error), EVoxelRegionRead::Failed);
	}

	IFileManager::Get().DeleteDirectory(*Root, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSaveFailureRetainsDirtyTest,
	"WHFramework.Voxel.Save.FailureRetainsDirty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSaveFailureRetainsDirtyTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = VoxelTest::MakeRegistry();
	FVoxelWorldRuntime Runtime(5, true, Registry, VoxelTest::MakeGenerator());
	const FIntVector SectionKey = FIntVector::ZeroValue;
	FVoxelSection* Section = Runtime.FindOrAllocate(SectionKey, 1);
	FString Error;
	TArray<FVoxelBlockState> Base;
	Base.Init(FVoxelBlockState(), VoxelBlock::Volume);
	TestTrue(TEXT("Save test base publishes"), Runtime.PublishBase(SectionKey, Section->Stamp, MoveTemp(Base), Error));
	TestTrue(TEXT("Save test final publishes"), Runtime.PublishFinal(SectionKey, 0, {}, {}, Error));
	FVoxelPreparedEdit Prepared;
	TestTrue(
		TEXT("Save test edit prepares"),
		Runtime.PrepareEdit(
			{ { FIntVector::ZeroValue, FVoxelBlockState(), FVoxelBlockState(1, 0) } },
			{},
			Prepared,
			Error));
	FVoxelEditBatch Batch;
	TestTrue(TEXT("Save test edit commits"), Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error));

	FVoxelRegionStore Store;
	FVoxelWorldSaveAdapter Adapter;
	FVoxelWorldManifest Manifest;
	Manifest.WorldId = FGuid::NewGuid();
	Manifest.RegistryHash = Registry->Hash;
	Manifest.RecipeHash = VoxelTest::MakeGenerationConfig()->Recipe->RecipeHash;
	TestTrue(TEXT("Dirty save captures"), Adapter.Capture(Runtime, Manifest, Registry, Store, Error));
	TestEqual(TEXT("Save capture pins dirty section"), Runtime.FindSection(SectionKey)->PinCount.Load(), 1);
	Adapter.Complete(Runtime, Store, false, FString());
	const FVoxelSection* Dirty = Runtime.FindSection(SectionKey);
	TestNotNull(TEXT("Failed save keeps dirty section"), Dirty);
	if (Dirty)
	{
		TestEqual(TEXT("Failed save releases pin"), Dirty->PinCount.Load(), 0);
		TestEqual(TEXT("Failed save keeps committed revision"), Dirty->CommittedRevision, uint64(1));
		TestEqual(TEXT("Failed save does not advance persisted revision"), Dirty->PersistedRevision, uint64(0));
	}
	TestTrue(TEXT("Failed save does not advance source directory"), Store.GetSourceDirectory().IsEmpty());
	return true;
}

#endif
