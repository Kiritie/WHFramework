#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Main/MainModule.h"
#include "Misc/ScopeExit.h"
#include "Tasks/Task.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SaveGame/SaveDataSerializer.h"
#include "SaveGame/SaveGameStorage.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Save/VoxelRegionStore.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/Save/VoxelSceneColumnCapture.h"
#include "Voxel/Save/VoxelSceneColumnRead.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/Save/VoxelWorldSaveAdapter.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/Tests/VoxelSaveTestTypes.h"
#include "Voxel/VoxelModuleTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSaveStorageAtomicCommitTest,
	"WHFramework.Voxel.Save.StorageAtomicCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSaveStorageAtomicCommitTest::RunTest(const FString& InParameters)
{
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("VoxelAtomicStorage"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FSaveGameStorage Storage(0, Root);
	FSaveManifest Manifest;
	Manifest.SaveId = FGuid::NewGuid();
	Manifest.DisplayName = TEXT("Original committed slot");
	const FString ManifestPath = Storage.GetManifestPath(Manifest.SaveId);
	ON_SCOPE_EXIT
	{
		FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ManifestPath, false);
		IFileManager::Get().DeleteDirectory(*Root, false, true);
	};
	if (!TestTrue(TEXT("Atomic storage fixture creates its original slot"), Storage.EnsureRoot() &&
		Storage.CreateWorldDirectory(Manifest.SaveId) && Storage.WriteManifestAtomic(Manifest.SaveId, Manifest))) return false;
	const FName ModuleName(TEXT("AtomicModule"));
	const TArray<uint8> Original = {1, 7, 11};
	const TArray<uint8> Replacement = {2, 9, 13};
	TestTrue(TEXT("Original generation writes and flushes before publication"),
		Storage.PrepareTempGeneration(Manifest.SaveId, 0, 1) &&
		Storage.WriteBinary(Storage.GetTempModuleFilePath(Manifest.SaveId, 1, ModuleName), Original) &&
		Storage.FlushTempGeneration(Manifest.SaveId, 1));
	if (!TestTrue(TEXT("Original generation publishes"), Storage.CommitGeneration(Manifest.SaveId, 1))) return false;
	Manifest.CurrentGeneration = 1;
	Manifest.ModuleVersions.Add(ModuleName, 1);
	TestTrue(TEXT("Committed manifest atomically replaces initial manifest"), Storage.WriteManifestAtomic(Manifest.SaveId, Manifest));
	TestTrue(TEXT("Conflicting generation prepares different bytes"),
		Storage.PrepareTempGeneration(Manifest.SaveId, 0, 1) &&
		Storage.WriteBinary(Storage.GetTempModuleFilePath(Manifest.SaveId, 1, ModuleName), Replacement) &&
		Storage.FlushTempGeneration(Manifest.SaveId, 1));
	TestFalse(TEXT("Generation publication cannot overwrite an existing immutable directory"), Storage.CommitGeneration(Manifest.SaveId, 1));
	TArray<uint8> ReadBack;
	TestTrue(TEXT("Failed publication preserves the original committed bytes"),
		Storage.ReadBinary(Storage.GetModuleFilePath(Manifest.SaveId, 1, ModuleName), ReadBack) && ReadBack == Original);
#if PLATFORM_WINDOWS
	auto& Platform = FPlatformFileManager::Get().GetPlatformFile();
	if (!TestTrue(TEXT("Fixture makes the existing manifest read-only"), Platform.SetReadOnly(*ManifestPath, true))) return false;
	Manifest.DisplayName = TEXT("Rejected replacement");
	TestFalse(TEXT("Rejected replacement cannot delete its existing manifest"), Storage.WriteManifestAtomic(Manifest.SaveId, Manifest));
	FSaveManifest Preserved;
	TestTrue(TEXT("Rejected replacement retains a readable complete old manifest"), Storage.ReadManifest(Manifest.SaveId, Preserved) &&
		Preserved.CurrentGeneration == 1 && Preserved.DisplayName == TEXT("Original committed slot"));
	TArray<FString> Temps;
	IFileManager::Get().FindFiles(Temps, *(Storage.GetWorldDir(Manifest.SaveId) / TEXT("manifest.json.*.tmp")), true, false);
	TestEqual(TEXT("Failed replacement removes only its own temporary file"), Temps.Num(), 0);
	TestTrue(TEXT("Fixture restores write permission"), Platform.SetReadOnly(*ManifestPath, false));
	TestTrue(TEXT("Replacement succeeds after permission recovery"), Storage.WriteManifestAtomic(Manifest.SaveId, Manifest));
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelManifestCodecTest,
	"WHFramework.Voxel.Save.ManifestV6",
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
	TestEqual(TEXT("Current manifest protocol is V6"), FVoxelWorldManifest::ProtocolVersion, uint32(6));
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

	FVoxelModuleSaveData ActorData;
	ActorData.Seed = 19;
	FVoxelSavedSceneActor SourceActor;
	SourceActor.Id = FGuid::NewGuid();
	SourceActor.Data = FParameter(ActorData);
	FVoxelModuleSaveData SourceProjectData;
	SourceProjectData.Seed = 23;
	TArray<FVoxelSavedSceneActor> SourceActors{ SourceActor };
	FParameter DirectSource(ActorData);
	TestTrue(TEXT("Direct payload marks saved"), FSaveDataSerializer::MarkSaveDataSaved(DirectSource));
	TArray<uint8> DirectBytes;
	TestTrue(TEXT("Direct payload serializes"), FSaveDataSerializer::SerializeParameter(DirectSource, DirectBytes));
	TArray<uint8> BoundedBytes;
	TestTrue(TEXT("Bounded payload serialization matches the real reflected data"), FSaveDataSerializer::SerializeParameterBounded(
		DirectSource, 256 * 1024, BoundedBytes, [](TArray<uint8>& Buffer, const int32 Required)
		{
			Buffer.Reserve(Required);
			return true;
		}));
	TestTrue(TEXT("Bounded serializer preserves the original wire bytes"), BoundedBytes == DirectBytes);
	TArray<uint8> LimitedBytes;
	int32 MaximumRequest = 0;
	const int32 Limit = FMath::Max(0, DirectBytes.Num() - 1);
	TestFalse(TEXT("Payload limit failure happens before requesting backing beyond the limit"),
		FSaveDataSerializer::SerializeParameterBounded(DirectSource, Limit, LimitedBytes,
			[&](TArray<uint8>& Buffer, const int32 Required)
			{
				MaximumRequest = FMath::Max(MaximumRequest, Required);
				Buffer.Reserve(Required);
				return true;
			}));
	TestTrue(TEXT("Rejected payload has not requested or written excess protocol bytes"), MaximumRequest <= Limit && LimitedBytes.Num() <= Limit);
	TArray<uint8> DeniedBytes;
	TestFalse(TEXT("A denied backing grant stops real reflection serialization"), FSaveDataSerializer::SerializeParameterBounded(
		DirectSource, 256 * 1024, DeniedBytes, [](TArray<uint8>&, const int32) { return false; }));
	TestEqual(TEXT("Denied serialization allocates no output backing"), DeniedBytes.GetAllocatedSize(), SIZE_T(0));
	FParameter DirectDecoded;
	TestTrue(TEXT("Direct payload deserializes"), FSaveDataSerializer::DeserializeParameter(DirectBytes, DirectDecoded));
	FParameter ViewDecoded;
	TestTrue(TEXT("Granted byte view deserializes without duplicating its payload array"),
		FSaveDataSerializer::DeserializeParameterView(DirectBytes, ViewDecoded));
	TArray<uint8> TrailingParameter = DirectBytes;
	TrailingParameter.Add(0);
	TestFalse(TEXT("Scene parameter views reject trailing bytes"),
		FSaveDataSerializer::DeserializeParameterView(TrailingParameter, ViewDecoded));
	TArray<uint8> ModuleBytes;
	TestTrue(
		TEXT("V2 module file builds"),
		FSaveDataSerializer::BuildModuleFile(TEXT("VoxelTest"), 4, DirectSource, ModuleBytes));
	TArray<uint8> BoundedModuleBytes;
	TestTrue(TEXT("Granted module header writes the original FName wire representation"),
		FSaveDataSerializer::WriteModuleHeaderBounded(TEXT("VoxelTest"), 4, DirectBytes.Num(),
			FCrc::MemCrc32(DirectBytes.GetData(), DirectBytes.Num()), BoundedModuleBytes,
			[](TArray<uint8>& Buffer, const int32 Required) { Buffer.Reserve(Required); return true; }));
	BoundedModuleBytes.Append(DirectBytes);
	TestTrue(TEXT("Separate bounded header/body assembly preserves the complete existing module file"), BoundedModuleBytes == ModuleBytes);
	TArray<uint8> DeniedHeader;
	TestFalse(TEXT("Denied module header grant is a real serialization failure"),
		FSaveDataSerializer::WriteModuleHeaderBounded(TEXT("VoxelTest"), 4, DirectBytes.Num(), 0, DeniedHeader,
			[](TArray<uint8>&, const int32) { return false; }));
	TestEqual(TEXT("Denied module header allocates no file backing"), DeniedHeader.GetAllocatedSize(), SIZE_T(0));
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
	const auto PayloadResources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	if (!PayloadResources->Configure({})) return false;
	const auto PayloadConsumer = FVoxelGenerationConsumerKey::Section(42, FGuid::NewGuid(), 1, FIntVector::ZeroValue, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PayloadMetadata;
	if (!PayloadResources->Acquire(PayloadConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSceneColumnCapture>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSceneColumnCompletion>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelSceneActorPayload>(1), PayloadMetadata)) return false;
	auto ColumnPayload = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSceneColumnCapture>(PayloadMetadata);
	ColumnPayload->MetadataLease = PayloadMetadata;
	ColumnPayload->Resources = PayloadResources;
	ColumnPayload->Consumer = PayloadConsumer;
	ColumnPayload->Completion = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSceneColumnCompletion>(PayloadMetadata);
	ColumnPayload->Actors.Reserve(1);
	auto& PayloadActor = ColumnPayload->Actors.AddDefaulted_GetRef();
	PayloadActor.Id = SourceActor.Id;
	if (!PayloadResources->Acquire(PayloadConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::ArrayBytes<uint8>(DirectBytes.Num()), PayloadActor.ResourceLease)) return false;
	PayloadActor.Bytes.Reserve(DirectBytes.Num());
	PayloadActor.Bytes.Append(DirectBytes);
	FParameter FrozenProject(SourceProjectData);
	if (!FSaveDataSerializer::MarkSaveDataSaved(FrozenProject)) return false;
	const auto ProjectReserve = [&](TArray<uint8>& Buffer, const int32 Required)
	{
		const uint64 Grant = Buffer.GetAllocatedSize() + VoxelBoundedAllocation::ArrayBytes<uint8>(Required);
		if ((!ColumnPayload->ProjectLease && !PayloadResources->Acquire(PayloadConsumer,
			EVoxelTerrainResourcePool::Input, Grant, ColumnPayload->ProjectLease)) || !ColumnPayload->ProjectLease->Resize(Grant)) return false;
		Buffer.Reserve(Required);
		return ColumnPayload->ProjectLease->Resize(Buffer.GetAllocatedSize());
	};
	if (!FSaveDataSerializer::SerializeParameterBounded(FrozenProject, 1024 * 1024, ColumnPayload->ProjectBytes, ProjectReserve)) return false;
	bool bWorkerEncoded = false;
	FString WorkerError;
	auto EncodeWorker = UE::Tasks::Launch(UE_SOURCE_LOCATION, [ColumnPayload, &bWorkerEncoded, &WorkerError]()
	{
		bWorkerEncoded = FVoxelSceneColumnCodec::EncodePayloads(*ColumnPayload, WorkerError);
	});
	EncodeWorker.Wait();
	TestTrue(TEXT("Actual Worker assembles Scene file from frozen plain payloads"), bWorkerEncoded);
	if (!bWorkerEncoded) AddError(WorkerError);
	TestTrue(TEXT("Worker column preserves original deterministic wire format"), ColumnPayload->Completion->Bytes == Bytes);
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
		const FVoxelModuleSaveData* DecodedActor = Actors[0].Data.GetPtr<FVoxelModuleSaveData>();
		TestNotNull(TEXT("Actor payload type roundtrips"), DecodedActor);
		if (DecodedActor)
		{
			TestEqual(TEXT("Actor payload roundtrips"), DecodedActor->Seed, 19);
		}
	}
	const FVoxelModuleSaveData* DecodedProject = ProjectData.GetPtr<FVoxelModuleSaveData>();
	TestNotNull(TEXT("Project payload type roundtrips"), DecodedProject);
	if (DecodedProject)
	{
		TestEqual(TEXT("Project payload roundtrips"), DecodedProject->Seed, 23);
	}

	Bytes.Add(0);
	TestFalse(TEXT("Native Scene framing rejects trailing bytes without reflection"), FVoxelSceneColumnRead::Validate(Bytes, Error));
	TestFalse(
		TEXT("Scene column rejects trailing bytes"),
		FVoxelSceneColumnCodec::Decode(Bytes, Actors, ProjectData, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSceneColumnPinnedReadTest,
	"WHFramework.Voxel.Save.SceneColumnPinnedRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSceneColumnPinnedReadTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
		TEXT("VoxelSceneRead"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FSaveGameStorage Storage(0, Root);
	const FGuid SaveId = FGuid::NewGuid();
	if (!Storage.CreateWorldDirectory(SaveId)) return false;
	const FString SourceDirectory = Storage.GetGenerationDir(SaveId, 1);
	const FIntPoint Column(-2, 3);
	const FString SourcePath = FPaths::Combine(SourceDirectory, FVoxelSceneColumnCodec::RelativePath(Column));
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(SourcePath), true)) return false;
	FVoxelModuleSaveData SavedActor;
	SavedActor.Seed = 73;
	const FGuid ActorId = FGuid::NewGuid();
	TArray<FVoxelSavedSceneActor> Actors{{ActorId, FParameter(SavedActor)}};
	TArray<uint8> Bytes;
	FString Error;
	if (!FVoxelSceneColumnCodec::Encode(Actors, {}, Bytes, Error) ||
		!FSaveGameStorage::WriteBinaryDurable(SourcePath, Bytes)) return false;
	TestTrue(TEXT("Pure framing validation accepts the actual reflected wire file"), FVoxelSceneColumnRead::Validate(Bytes, Error));
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	if (!Resources->Configure({})) return false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(86, FGuid::NewGuid(), 1, FIntVector(Column.X, Column.Y, 0), 1);
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> Source;
	bool bCapacityBlocked = false;
	if (!FSaveGameGenerationReadLease::AcquireGranted(SourceDirectory, Resources, Consumer, Source, bCapacityBlocked)) return false;
	TSharedPtr<const FVoxelSceneColumnRead, ESPMode::ThreadSafe> Read;
	TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> Directory;
	bool bLoaded = false;
	bool bDirectoryLoaded = false;
	auto Worker = UE::Tasks::Launch(UE_SOURCE_LOCATION, [&]()
	{
		bDirectoryLoaded = FVoxelRegionStore::BuildDirectory(SourceDirectory, Resources, Consumer, Directory, Error);
		if (bDirectoryLoaded) bLoaded = FVoxelSceneColumnRead::Load(Source, Column, Bytes.Num(), Resources, Consumer, Read, Error);
	});
	Worker.Wait();
	TestTrue(TEXT("Actual Worker builds the column descriptor and reads its pinned bytes"), bDirectoryLoaded && bLoaded);
	if (!Read || !Directory)
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Directory stores the exact Scene file size"), Directory->SceneColumns.FindRef(Column), uint32(Bytes.Num()));
	TestTrue(TEXT("Read body preserves the complete file and its original owner"), Read->Body.Bytes == Bytes &&
		Read->SourceLease == Source && Read->Consumer == Consumer && Read->Body.ResourceLease.IsValid());
	TestEqual(TEXT("Worker indexes one actor without deserializing UObject data"), Read->Actors.Num(), 1);
	if (Read->Actors.Num() != 1) return false;
	FParameter Data;
	TestTrue(TEXT("GT restores one actor directly from the granted body slice"),
		FSaveDataSerializer::DeserializeParameterView(Read->ActorPayload(0), Data));
	TestEqual(TEXT("Native indexed actor identity is exact"), Read->Actors[0].Id, ActorId);
	const auto* Restored = Data.GetPtr<FVoxelModuleSaveData>();
	TestTrue(TEXT("Native indexed actor payload preserves its typed state"), Restored && Restored->Seed == 73);
	TestFalse(TEXT("A live Scene read prevents retiring its source generation"), FSaveGameGenerationReadLease::TryRetireDirectory(SourceDirectory));
	TestTrue(TEXT("Save preparation carries unread columns to the next file generation"),
		Storage.PrepareTempGeneration(SaveId, 1, 2, &SourceDirectory, &Source));
	TArray<uint8> Copied;
	TestTrue(TEXT("The untouched Scene column is copied byte for byte"),
		FFileHelper::LoadFileToArray(Copied, *FPaths::Combine(Storage.GetTempGenerationDir(SaveId, 2),
			FVoxelSceneColumnCodec::RelativePath(Column))) && Copied == Bytes);
	const auto Snapshot = Resources->Snapshot();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const auto BlockerConsumer = FVoxelGenerationConsumerKey::Section(86, FGuid::NewGuid(), 1, {}, 1);
	if (!Resources->Acquire(BlockerConsumer, EVoxelTerrainResourcePool::Working,
		Snapshot.Limits.WorkingBytes - Snapshot.Used[static_cast<int32>(EVoxelTerrainResourcePool::Working)], Blocker)) return false;
	TSharedPtr<const FVoxelSceneColumnRead, ESPMode::ThreadSafe> Denied;
	bool bDenied = false;
	auto DeniedWorker = UE::Tasks::Launch(UE_SOURCE_LOCATION, [&]()
	{
		bDenied = !FVoxelSceneColumnRead::Load(Source, Column, Bytes.Num(), Resources, Consumer, Denied, Error);
	});
	DeniedWorker.Wait();
	TestTrue(TEXT("Native body admission failure is explicit and publishes no read root"),
		bDenied && !Denied && Error.StartsWith(TEXT("CapacityBlocked:")));
	Blocker.Reset();
	bool bRecovered = false;
	auto RetryWorker = UE::Tasks::Launch(UE_SOURCE_LOCATION, [&]()
	{
		bRecovered = FVoxelSceneColumnRead::Load(Source, Column, Bytes.Num(), Resources, Consumer, Denied, Error);
	});
	RetryWorker.Wait();
	TestTrue(TEXT("An external release admits the same source/consumer after capacity denial"),
		bRecovered && Denied && Denied->SourceLease == Source && Denied->Consumer == Consumer);
	Denied.Reset();
	TAtomic<bool> Cancel{true};
	TestFalse(TEXT("Canceled Scene reads publish no body"),
		FVoxelSceneColumnRead::Load(Source, Column, Bytes.Num(), Resources, Consumer, Denied, Error, &Cancel));
	TestTrue(TEXT("Cancellation is distinguished from a natural/missing column"), !Denied && Error == TEXT("Canceled"));
	Read.Reset();
	Directory.Reset();
	Source.Reset();
	const double Deadline = FPlatformTime::Seconds() + 5.0;
	while (FSaveGameGenerationReadLease::GetPinCount(SourceDirectory) > 0 && FPlatformTime::Seconds() < Deadline)
		FPlatformProcess::SleepNoStats(0.001f);
	TestTrue(TEXT("Scene source pin eventually releases after actual background destruction"),
		FSaveGameGenerationReadLease::GetPinCount(SourceDirectory) == 0);
	TestTrue(TEXT("Fixture world retires after its last actual reader"), Storage.DeleteWorldDirectory(SaveId));
	IFileManager::Get().DeleteDirectory(*Root, false, true);
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

	Store.Reset();
	TestTrue(TEXT("Released generations can retire"), FSaveGameGenerationReadLease::TryRetireDirectory(Root));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSaveGenerationGrantedLeaseTest,
	"WHFramework.Voxel.Save.GenerationGrantedLease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSaveGenerationGrantedLeaseTest::RunTest(const FString& InParameters)
{
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("VoxelGrantedGeneration"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString Directory = FPaths::Combine(Root, TEXT("generation"));
	const FString Sibling = Directory + TEXT("_sibling");
	if (!IFileManager::Get().MakeDirectory(*Directory, true) || !IFileManager::Get().MakeDirectory(*Sibling, true)) return false;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 0;
	if (!Resources->Configure(Limits)) return false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(85, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> First;
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> Second;
	bool bCapacityBlocked = false;
	TestFalse(TEXT("未授予时不登记文件代读者"), FSaveGameGenerationReadLease::AcquireGranted(Directory, Resources,
		Consumer, First, bCapacityBlocked));
	TestTrue(TEXT("容量拒绝返回原生等待原因且不产生 pin"), bCapacityBlocked && !First &&
		FSaveGameGenerationReadLease::GetPinCount(Root) == 0);
	if (!Resources->Configure(FVoxelTerrainResourceLimits()) ||
		!FSaveGameGenerationReadLease::AcquireGranted(Directory, Resources, Consumer, First, bCapacityBlocked)) return false;
	TestTrue(TEXT("实际租约绑定原资源所有者和 World"), First->IsResourceAccountedFor(&Resources.Get(), 85));
	TestFalse(TEXT("另一个 World 不能借用原资源归属"), First->IsResourceAccountedFor(&Resources.Get(), 86));
	TestTrue(TEXT("实际节点和路径计入 Input"), Resources->Snapshot().Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)] > 0);
	const FString Alias = FPaths::Combine(Directory, TEXT(".."), TEXT("generation"));
	if (!FSaveGameGenerationReadLease::AcquireGranted(Alias, Resources, Consumer, Second, bCapacityBlocked)) return false;
	TestEqual(TEXT("同一规范路径的存活读者保持文件代身份"), First->GenerationIdentity(), Second->GenerationIdentity());
	TestEqual(TEXT("真实读者节点形成两个独立 pin"), FSaveGameGenerationReadLease::GetPinCount(Directory), uint64(2));
	auto SiblingLease = FSaveGameGenerationReadLease::Acquire(Sibling);
	TestEqual(TEXT("相同文字前缀的兄弟目录不增加目标 pin"), FSaveGameGenerationReadLease::GetPinCount(Directory), uint64(2));
	TestFalse(TEXT("读者存在时不能退休父目录"), FSaveGameGenerationReadLease::TryRetireDirectory(Root));
	auto WaitForPins = [&](const uint64 InExpected)
	{
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (FSaveGameGenerationReadLease::GetPinCount(Directory) != InExpected && FPlatformTime::Seconds() < Deadline)
			FPlatformProcess::SleepNoStats(0.001f);
		return FSaveGameGenerationReadLease::GetPinCount(Directory) == InExpected;
	};
	First.Reset();
	if (!TestTrue(TEXT("实际后台退休一个读者仍保留另一个"), WaitForPins(1))) return false;
	TestFalse(TEXT("最后一个读者仍保护实际目录"), FSaveGameGenerationReadLease::TryRetireDirectory(Directory));
	Second.Reset();
	if (!TestTrue(TEXT("实际后台退休最后一个读者后没有残留登记"), WaitForPins(0))) return false;
	TestTrue(TEXT("只退休目标目录且保留兄弟 pin"), FSaveGameGenerationReadLease::TryRetireDirectory(Directory) &&
		IFileManager::Get().DirectoryExists(*Sibling));
	SiblingLease.Reset();
	TestTrue(TEXT("全部实际读者退出后回收测试根"), FSaveGameGenerationReadLease::TryRetireDirectory(Root));
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
	TestEqual(TEXT("Save capture retains immutable journal without pinning Runtime data"), Runtime.FindSection(SectionKey)->GetPinCount(), 0);
	TestTrue(TEXT("Save capture includes the dirty committed journal"), Adapter.GetCapture()->Journal.FindDirty(SectionKey).IsValid());
	Adapter.Complete(Runtime, Store, false, FString());
	const FVoxelSection* Dirty = Runtime.FindSection(SectionKey);
	TestNotNull(TEXT("Failed save keeps dirty section"), Dirty);
	if (Dirty)
	{
		TestEqual(TEXT("Failed save releases pin"), Dirty->GetPinCount(), 0);
		TestEqual(TEXT("Failed save keeps committed revision"), Dirty->CommittedRevision, uint64(1));
		TestEqual(TEXT("Failed save does not advance persisted revision"), Dirty->PersistedRevision, uint64(0));
	}
	TestTrue(TEXT("Failed save does not advance source directory"), Store.GetSourceDirectory().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPinnedOverlayGenerationTest,
	"WHFramework.Voxel.Save.PinnedOverlayGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPinnedOverlayGenerationTest::RunTest(const FString& InParameters)
{
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("VoxelOverlayLease"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString FirstDirectory = FPaths::Combine(Root, TEXT("1"));
	const FString SecondDirectory = FPaths::Combine(Root, TEXT("2"));
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelWorldManifest Manifest;
	Manifest.WorldId = FGuid::NewGuid();
	Manifest.RegistryHash = Registry->Hash;
	Manifest.RecipeHash = VoxelTest::MakeGenerationConfig()->Recipe->RecipeHash;
	FVoxelPersistentSection First;
	First.Section = FIntVector(-1, 0, 0);
	First.Revision = 7;
	First.Blocks.Add(0, FVoxelBlockState());
	FString Error;
	TArray<uint8> Encoded;
	if (!TestTrue(TEXT("Air override is encoded as an authoritative edit"), FVoxelDeltaCodec::Encode(Manifest, *Registry, First, Encoded)))
	{
		return false;
	}
	FVoxelRegionWritePlan Plan;
	Plan.TransactionId = FGuid::NewGuid();
	FVoxelRegionStore::StageSection(Plan, First.Section, MoveTemp(Encoded));
	if (!TestTrue(TEXT("Pinned source generation is written"), FVoxelRegionStore::WritePendingRegions(Plan, FirstDirectory, Error)))
	{
		return false;
	}
	FVoxelRegionStore Store;
	Store.AdvanceSource(FirstDirectory);
	FVoxelFineOverlayCapture Capture;
	Capture.WorldEpoch = 11;
	Capture.Bounds = {First.Section * 16, (First.Section + FIntVector(1)) * 16};
	Capture.Manifest = Manifest;
	Capture.Registry = Registry;
	Capture.ContentRevisions.Add(First.Section, 7);
	FVoxelRegionReadView Read;
	if (!TestTrue(TEXT("GT captures a pinned descriptor"), Store.CapturePinnedRead(First.Section, 7, Read, Error)))
	{
		return false;
	}
	Capture.Reads.Add(Read);
	Capture.ReadLeaseSet = FVoxelRegionStore::CreateOverlayReadLeaseSet(Capture);
	FVoxelFineOverlayRead Reader;
	if (!TestTrue(TEXT("Frozen overlay input is accepted"), Reader.Initialize(MoveTemp(Capture), Error)))
	{
		return false;
	}
	IFileManager::Get().MakeDirectory(*SecondDirectory, true);
	Store.AdvanceSource(SecondDirectory);
	TestFalse(TEXT("A newer source cannot retire the active old read"), Store.TryRetireFileGeneration(FirstDirectory));
	TestFalse(TEXT("Parent cleanup cannot bypass a child generation pin"), Store.TryRetireFileGeneration(Root));
	TAtomic<bool> Cancel(false);
	bool bComplete = false;
	if (!TestTrue(TEXT("Worker reads and validates the original file generation"), Reader.Advance(1, Cancel, bComplete, Error)))
	{
		return false;
	}
	TestTrue(TEXT("One pinned read completes"), bComplete);
	FVoxelFineOverlayReadResult Result;
	TestTrue(TEXT("Decoded result retains its read generation"), Reader.TakeResult(Result));
	FVoxelBlockState Value(1, 0);
	Result.Snapshot->ApplyAt(First.Section * 16, Value);
	TestTrue(TEXT("Air remains authoritative after decode"), Value.IsAir());
	TArray<uint8> CheckBytes;
	TestEqual(TEXT("Pinned record still reads"), FVoxelRegionStore::Read(Read, CheckBytes, Error), EVoxelRegionRead::Loaded);
	TestFalse(TEXT("Decoded section revision must match its pinned request"),
		Result.ReadLeaseSet->ValidateRead(Read, CheckBytes, 8, Error));
	FVoxelRegionReadView Missing;
	TestFalse(TEXT("Missing modified record cannot become natural"), Store.CapturePinnedRead(First.Section, 0, Missing, Error));
	FVoxelRegionStore::ReleaseReadLease(Read);
	Result = {};
	Store.Reset();
	TestTrue(TEXT("Final consumer release permits old generation retirement"), Store.TryRetireFileGeneration(FirstDirectory));
	TestTrue(TEXT("All released test generations retire"), Store.TryRetireFileGeneration(Root));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelJournalFixedCutSaveTest,
	"WHFramework.Voxel.Save.JournalFixedCutNonresidentTombstone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelJournalFixedCutSaveTest::RunTest(const FString& InParameters)
{
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("VoxelJournalSave"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString FirstDirectory = FPaths::Combine(Root, TEXT("1"));
	const FString SecondDirectory = FPaths::Combine(Root, TEXT("2"));
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelWorldRuntime Runtime(51, true, Registry, VoxelTest::MakeGenerator());
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	TestTrue(TEXT("Journal test resource limits configure"), Resources->Configure({}));
	Runtime.BindTerrainResources(Resources);
	const FIntVector SectionA(-1, 0, 0);
	const FIntVector SectionB(-2, 0, 0);
	FString Error;
	auto Materialize = [&](FVoxelWorldRuntime& InRuntime, const FIntVector& InSection,
		const uint64 InRevision, const TMap<int32, FVoxelBlockState>& InBlocks)
	{
		FVoxelSection* Section = InRuntime.FindOrAllocate(InSection, 1);
		TArray<FVoxelBlockState> Base;
		Base.Init(FVoxelBlockState(1, 0), VoxelBlock::Volume);
		return Section && InRuntime.PublishBase(InSection, Section->Stamp, MoveTemp(Base), Error) &&
			InRuntime.PublishFinal(InSection, InRevision, InBlocks, {}, Error);
	};
	auto Edit = [&](const FIntVector& InSection, const FVoxelBlockState& InExpected, const FVoxelBlockState& InValue)
	{
		FVoxelPreparedEdit Prepared;
		FVoxelEditBatch Batch;
		return Runtime.PrepareEdit({{InSection * 16, InExpected, InValue}}, {}, Prepared, Error) &&
			Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error);
	};
	if (!TestTrue(TEXT("Both natural sections materialize"), Materialize(Runtime, SectionA, 0, {}) && Materialize(Runtime, SectionB, 0, {}))) return false;
	TestTrue(TEXT("Air is an explicit override of natural stone"), Edit(SectionA, FVoxelBlockState(1, 0), FVoxelBlockState()));
	TestTrue(TEXT("Second section edits"), Edit(SectionB, FVoxelBlockState(1, 0), FVoxelBlockState()));
	TestTrue(TEXT("Restoration creates an empty revision two journal"), Edit(SectionB, FVoxelBlockState(), FVoxelBlockState(1, 0)));
	TestTrue(TEXT("Dirty Runtime data may unload while its immutable journal is retained"), Runtime.RemoveSection(SectionA) && Runtime.RemoveSection(SectionB));
	TestEqual(TEXT("Unloaded data leaves no Runtime sections"), Runtime.NumSections(), 0);
	TestTrue(TEXT("Nonresident dirty journals still prevent a clean world close"), Runtime.HasUnpersistedChanges());
	FVoxelWorldManifest Manifest;
	Manifest.WorldId = FGuid::NewGuid();
	Manifest.RegistryHash = Registry->Hash;
	Manifest.RecipeHash = VoxelTest::MakeGenerationConfig()->Recipe->RecipeHash;
	FVoxelRegionStore Store;
	FVoxelWorldSaveAdapter Adapter;
	if (!TestTrue(TEXT("Save captures nonresident authority journals"), Adapter.Capture(Runtime, Manifest, Registry, Store, Error))) return false;
	FVoxelWorldSaveCapture Frozen = *Adapter.GetCapture();
	TestEqual(TEXT("Frozen Air revision is one"), Frozen.Journal.FindDirty(SectionA)->Revision, uint64(1));
	TestEqual(TEXT("Frozen Tombstone revision is two"), Frozen.Journal.FindDirty(SectionB)->Revision, uint64(2));
	const auto Existing = Runtime.FindCommittedOverlay(SectionA);
	TestTrue(TEXT("Unsaved journal rematerializes"), Materialize(Runtime, SectionA, Existing->Revision, Existing->Blocks));
	TestEqual(TEXT("Rematerialization does not falsely mark the journal persisted"), Runtime.GetPersistedRevision(SectionA), uint64(0));
	TestTrue(TEXT("An edit after capture creates a newer Tombstone"), Edit(SectionA, FVoxelBlockState(), FVoxelBlockState(1, 0)));
	TestEqual(TEXT("Captured journal keeps its original revision"), Frozen.Journal.FindDirty(SectionA)->Revision, uint64(1));
	if (!TestTrue(TEXT("Fixed cut streams multiple records in the same Region"), FVoxelWorldSaveAdapter::WriteCapture(Frozen, FirstDirectory, Error))) return false;
	Adapter.Complete(Runtime, Store, true, FirstDirectory);
	FVoxelChangeIndex BaselineIndex;
	BaselineIndex.SetBaseline(Store.GetDirectorySnapshot());
	TestTrue(TEXT("Immutable directory baseline includes both saved records"), BaselineIndex.IsModified(SectionA) && BaselineIndex.IsModified(SectionB));
	const FVoxelGenerationBounds NarrowBounds{SectionA * 16, (SectionA + FIntVector(1)) * 16};
	TestEqual(TEXT("Directory spatial query visits only intersecting section"), BaselineIndex.Count(NarrowBounds), 1);
	BaselineIndex.SetModified(SectionB, false);
	TestFalse(TEXT("Local restoration shadows saved baseline bit"), BaselineIndex.IsModified(SectionB));
	TestTrue(TEXT("Local restoration preserves neighboring baseline edit"), BaselineIndex.IsModified(SectionA));
	BaselineIndex.SetModified(SectionA, false);
	TestFalse(TEXT("An empty local region cannot reveal inherited baseline edits"), BaselineIndex.HasAnyModified());
	TestEqual(TEXT("Empty shadow removes baseline from spatial enumeration"), BaselineIndex.Count(NarrowBounds), 0);
	TestEqual(TEXT("Save proves only its captured revision"), Runtime.GetPersistedRevision(SectionA), uint64(1));
	TestEqual(TEXT("Newer edit remains committed"), Runtime.FindSection(SectionA)->CommittedRevision, uint64(2));
	TestTrue(TEXT("Saving an older cut leaves the world dirty"), Runtime.HasUnpersistedChanges());
	TArray<uint8> Bytes;
	FVoxelPersistentSection Decoded;
	TestEqual(TEXT("Nonresident Air record reads"), Store.ReadSection(SectionA, Bytes, Error), EVoxelRegionRead::Loaded);
	TestTrue(TEXT("Nonresident Air record decodes"), FVoxelDeltaCodec::Decode(Bytes, Manifest, *Registry, Decoded));
	TestEqual(TEXT("Disk stores the captured Air revision"), Decoded.Revision, uint64(1));
	TestTrue(TEXT("Disk stores explicit Air"), Decoded.Blocks.Contains(0) && Decoded.Blocks.FindChecked(0).IsAir());
	TestEqual(TEXT("Empty Tombstone record reads"), Store.ReadSection(SectionB, Bytes, Error), EVoxelRegionRead::Loaded);
	TestTrue(TEXT("Empty Tombstone record decodes"), FVoxelDeltaCodec::Decode(Bytes, Manifest, *Registry, Decoded));
	TestEqual(TEXT("Tombstone keeps revision two"), Decoded.Revision, uint64(2));
	TestTrue(TEXT("Tombstone carries empty edits"), Decoded.IsEmpty());
	FVoxelRegionReadView OldRead = Store.CaptureRead(SectionA);
	const double FirstRetirementDeadline = FPlatformTime::Seconds() + 5.0;
	while (!Adapter.RetireCaptureTask.IsCompleted() && FPlatformTime::Seconds() < FirstRetirementDeadline)
		FPlatformProcess::SleepNoStats(0.001f);
	if (!TestTrue(TEXT("Captured old roots release on Worker"), Adapter.RetireCaptureTask.IsCompleted())) return false;
	if (!TestTrue(TEXT("Second capture sees the new edit"), Adapter.Capture(Runtime, Manifest, Registry, Store, Error))) return false;
	TestTrue(TEXT("Second capture excludes already persisted Tombstone"), !Adapter.GetCapture()->Journal.FindDirty(SectionB));
	if (!TestTrue(TEXT("Second save streams replacement and copies unchanged record"), FVoxelWorldSaveAdapter::WriteCapture(*Adapter.GetCapture(), SecondDirectory, Error))) return false;
	Adapter.Complete(Runtime, Store, true, SecondDirectory);
	const double SecondRetirementDeadline = FPlatformTime::Seconds() + 5.0;
	while (!Adapter.RetireCaptureTask.IsCompleted() && FPlatformTime::Seconds() < SecondRetirementDeadline)
		FPlatformProcess::SleepNoStats(0.001f);
	if (!TestTrue(TEXT("Second capture releases old source generation on Worker"), Adapter.RetireCaptureTask.IsCompleted())) return false;
	TestEqual(TEXT("Second save proves the newer edit"), Runtime.GetPersistedRevision(SectionA), uint64(2));
	TestFalse(TEXT("Fully persisted edits permit a clean world close"), Runtime.HasUnpersistedChanges());
	TestEqual(TEXT("Unchanged Tombstone survives region rewrite"), Store.ReadSection(SectionB, Bytes, Error), EVoxelRegionRead::Loaded);
	TestTrue(TEXT("Unchanged Tombstone decodes after rewrite"), FVoxelDeltaCodec::Decode(Bytes, Manifest, *Registry, Decoded) && Decoded.Revision == 2 && Decoded.IsEmpty());
	TestFalse(TEXT("Outstanding old file read prevents generation retirement"), Store.TryRetireFileGeneration(FirstDirectory));
	FVoxelWorldRuntime Reloaded(52, true, Registry, VoxelTest::MakeGenerator());
	TestTrue(TEXT("Reloaded Tombstone materializes with persistent version"), Materialize(Reloaded, SectionB, Decoded.Revision, Decoded.Blocks));
	TestEqual(TEXT("Reloaded Tombstone does not restart revision numbering"), Reloaded.GetCommittedOverlayRevision(SectionB), uint64(2));
	TestTrue(TEXT("Persisted Runtime data retires"), Runtime.RemoveSection(SectionA));
	for (int32 Step = 0; Step < 32; ++Step) Runtime.AdvanceJournalRetirement(64, FPlatformTime::Seconds() + 0.01);
	TestTrue(TEXT("Journal retirement preserves Tombstone metadata"), Runtime.FindCommittedOverlay(SectionB).IsValid());
	FVoxelRegionStore::ReleaseReadLease(OldRead);
	Frozen = {};
	Store.Reset();
	TestTrue(TEXT("Released save generations retire"), Store.TryRetireFileGeneration(Root));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelJournalCaptureIsolationTest,
	"WHFramework.Voxel.Save.JournalCaptureIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelJournalCaptureIsolationTest::RunTest(const FString& InParameters)
{
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelWorldRuntime Runtime(61, true, Registry, VoxelTest::MakeGenerator());
	FVoxelWorldRuntime Other(61, true, Registry, VoxelTest::MakeGenerator());
	const FIntVector Key(3, -2, 8);
	const FIntVector OtherKey(-7, 4, -3);
	int32 KeyNotifications = 0;
	int32 OtherNotifications = 0;
	Runtime.SetSectionEventCallback([&](const FIntVector& InKey, const FVoxelSectionStamp&, const EVoxelRuntimeSectionEvent InEvent)
	{
		if (InEvent != EVoxelRuntimeSectionEvent::Persisted) return;
		if (InKey == Key) ++KeyNotifications;
		if (InKey == OtherKey) ++OtherNotifications;
	});
	FString Error;
	FVoxelSection* Section = Runtime.FindOrAllocate(Key, 0);
	TArray<FVoxelBlockState> Base;
	Base.Init(FVoxelBlockState(), VoxelBlock::Volume);
	TestTrue(TEXT("Isolation natural base publishes"), Runtime.PublishBase(Key, Section->Stamp, MoveTemp(Base), Error));
	TestTrue(TEXT("Isolation final publishes"), Runtime.PublishFinal(Key, 0, {}, {}, Error));
	FVoxelPreparedEdit Prepared;
	FVoxelEditBatch Batch;
	TestTrue(TEXT("Isolation edit prepares"), Runtime.PrepareEdit({{Key * 16, FVoxelBlockState(), FVoxelBlockState(1, 0)}}, {}, Prepared, Error));
	TestTrue(TEXT("Isolation edit commits"), Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error));
	Section = Runtime.FindOrAllocate(OtherKey, 0);
	Base.Init(FVoxelBlockState(), VoxelBlock::Volume);
	TestTrue(TEXT("Other notification natural base publishes"), Runtime.PublishBase(OtherKey, Section->Stamp, MoveTemp(Base), Error));
	TestTrue(TEXT("Other notification final publishes"), Runtime.PublishFinal(OtherKey, 0, {}, {}, Error));
	TestTrue(TEXT("Other notification edit prepares"), Runtime.PrepareEdit({{OtherKey * 16, FVoxelBlockState(), FVoxelBlockState(1, 0)}}, {}, Prepared, Error));
	TestTrue(TEXT("Other notification edit commits"), Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error));
	auto First = Runtime.CaptureJournal();
	TestTrue(TEXT("Saved cut notification cursor grants before acceptance"), First.PrepareSaveNotifications(FGuid::NewGuid()));
	TestFalse(TEXT("Matching epoch cannot accept another journal owner"), Other.CompleteJournalSave(First));
	TestTrue(TEXT("Matching full journal identity accepts saved cut"), Runtime.CompleteJournalSave(First));
	TestEqual(TEXT("Saved proof reaches committed revision"), Runtime.GetPersistedRevision(Key), uint64(1));
	TestTrue(TEXT("Later edit prepares"), Runtime.PrepareEdit({{Key * 16, FVoxelBlockState(1, 0), FVoxelBlockState()}}, {}, Prepared, Error));
	TestTrue(TEXT("Later edit commits"), Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error));
	auto Second = Runtime.CaptureJournal();
	TestTrue(TEXT("Later cut notification cursor grants before acceptance"), Second.PrepareSaveNotifications(FGuid::NewGuid()));
	TestTrue(TEXT("Later saved cut accepts"), Runtime.CompleteJournalSave(Second));
	TestTrue(TEXT("Back-to-back receipts retain their independent event cursors"), Runtime.HasPendingJournalSaveNotifications());
	TestEqual(TEXT("Accepting receipts does not batch notification callbacks"), KeyNotifications + OtherNotifications, 0);
	Runtime.AdvanceJournalRetirement(1, FPlatformTime::Seconds() + 0.01);
	TestEqual(TEXT("One journal microstep cannot traverse a whole dirty tree"), KeyNotifications + OtherNotifications, 0);
	bool bAtMostOneNotificationPerStep = true;
	for (int32 Step = 0; Step < 4096 && Runtime.HasPendingJournalSaveNotifications(); ++Step)
	{
		const int32 Before = KeyNotifications + OtherNotifications;
		Runtime.AdvanceJournalRetirement(1, FPlatformTime::Seconds() + 0.01);
		bAtMostOneNotificationPerStep &= KeyNotifications + OtherNotifications - Before <= 1;
	}
	TestTrue(TEXT("One journal microstep notifies at most one section"), bAtMostOneNotificationPerStep);
	TestFalse(TEXT("Original receipt event cursors finish"), Runtime.HasPendingJournalSaveNotifications());
	TestEqual(TEXT("Old cut cannot notify a newer unpersisted section revision"), KeyNotifications, 1);
	TestEqual(TEXT("Later capture cannot discard the first receipt's other section"), OtherNotifications, 1);
	TestTrue(TEXT("Duplicate current cut is idempotent"), Runtime.CompleteJournalSave(Second));
	TestFalse(TEXT("Duplicate receipt cannot enqueue the same notification cursor again"), Runtime.HasPendingJournalSaveNotifications());
	TestFalse(TEXT("Old saved cut cannot roll back persisted proof"), Runtime.CompleteJournalSave(First));
	TestEqual(TEXT("Persisted proof stays at revision two"), Runtime.GetPersistedRevision(Key), uint64(2));
	const uint64 BeforeNoOp = Runtime.GetAuthorityCommitCut();
	TestTrue(TEXT("No-op edit prepares"), Runtime.PrepareEdit({}, {}, Prepared, Error));
	TestTrue(TEXT("No-op edit commits"), Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error));
	TestEqual(TEXT("No-op edit does not invent an authority commit cut"), Runtime.GetAuthorityCommitCut(), BeforeNoOp);
	TestFalse(TEXT("No-op edit does not dirty a saved world"), Runtime.HasUnpersistedChanges());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelJournalSaveCapacityRecoveryTest,
	"WHFramework.Voxel.Save.JournalSaveCapacityRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelJournalSaveCapacityRecoveryTest::RunTest(const FString& InParameters)
{
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("VoxelJournalCapacity"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const auto Registry = VoxelTest::MakeRegistry();
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	TestTrue(TEXT("Capacity test resource limits configure"), Resources->Configure({}));
	FVoxelWorldRuntime Runtime(71, true, Registry, VoxelTest::MakeGenerator());
	Runtime.BindTerrainResources(Resources);
	const FIntVector Key(4, -3, 2);
	FString Error;
	FVoxelSection* Section = Runtime.FindOrAllocate(Key, 0);
	TArray<FVoxelBlockState> Base;
	Base.Init(FVoxelBlockState(), VoxelBlock::Volume);
	TestTrue(TEXT("Capacity test base publishes"), Runtime.PublishBase(Key, Section->Stamp, MoveTemp(Base), Error));
	TestTrue(TEXT("Capacity test final publishes"), Runtime.PublishFinal(Key, 0, {}, {}, Error));
	FVoxelPreparedEdit Prepared;
	FVoxelEditBatch Batch;
	TestTrue(TEXT("Capacity test edit prepares"), Runtime.PrepareEdit({{Key * 16, FVoxelBlockState(), FVoxelBlockState(1, 0)}}, {}, Prepared, Error));
	TestTrue(TEXT("Capacity test edit commits"), Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error));
	FVoxelWorldManifest Manifest;
	Manifest.WorldId = FGuid::NewGuid();
	Manifest.RegistryHash = Registry->Hash;
	Manifest.RecipeHash = VoxelTest::MakeGenerationConfig()->Recipe->RecipeHash;
	FVoxelRegionStore Store;
	FVoxelWorldSaveAdapter Adapter;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(71, FGuid::NewGuid(), 1, Key, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!TestTrue(TEXT("Withholding handle first accounts its ledger metadata"),
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, 1, Blocker))) return false;
	const auto Capacity = Resources->Snapshot();
	TestTrue(TEXT("All remaining Input capacity can be withheld"), Blocker->Resize(1 + Capacity.Limits.InputBytes -
		Capacity.Used[static_cast<int32>(EVoxelTerrainResourcePool::Input)]));
	TestFalse(TEXT("Capture waits before allocating metadata without Input grant"), Adapter.Capture(Runtime, Manifest, Registry, Store, Error));
	TestTrue(TEXT("Blocked capture retains its original pending save"), Adapter.IsBusy());
	TestNull(TEXT("Pending save has not published its capture"), Adapter.GetCapture());
	const FGuid PendingTransaction = Adapter.Pending->TransactionId;
	TestFalse(TEXT("No external release keeps the same capture waiting"), Adapter.Capture(Runtime, Manifest, Registry, Store, Error));
	TestEqual(TEXT("Waiting does not create a new transaction"), Adapter.Pending->TransactionId, PendingTransaction);
	TestEqual(TEXT("Blocked capture cannot advance persisted proof"), Runtime.GetPersistedRevision(Key), uint64(0));
	Blocker.Reset();
	TestTrue(TEXT("Capture succeeds after Input release"), Adapter.Capture(Runtime, Manifest, Registry, Store, Error));
	TestEqual(TEXT("Successful continuation retains its transaction"), Adapter.GetCapture()->TransactionId, PendingTransaction);
	TestTrue(TEXT("All Working capacity can be withheld"), Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		Resources->Snapshot().Limits.WorkingBytes, Blocker));
	TestFalse(TEXT("Write waits before encoding without Working peak grant"), FVoxelWorldSaveAdapter::WriteCapture(*Adapter.GetCapture(), Directory, Error));
	TestTrue(TEXT("Failed write retains committed dirty journal"), Runtime.FindCommittedOverlay(Key).IsValid());
	Adapter.Complete(Runtime, Store, false, FString());
	TestEqual(TEXT("Failed write does not advance persisted proof"), Runtime.GetPersistedRevision(Key), uint64(0));
	const double FailedRetirementDeadline = FPlatformTime::Seconds() + 5.0;
	while (!Adapter.RetireCaptureTask.IsCompleted() && FPlatformTime::Seconds() < FailedRetirementDeadline)
		FPlatformProcess::SleepNoStats(0.001f);
	if (!TestTrue(TEXT("Failed capture releases old roots on Worker"), Adapter.RetireCaptureTask.IsCompleted())) return false;
	Blocker.Reset();
	TestTrue(TEXT("Retry captures after Working release"), Adapter.Capture(Runtime, Manifest, Registry, Store, Error));
	if (!TestTrue(TEXT("Retry writes after Working release"), FVoxelWorldSaveAdapter::WriteCapture(*Adapter.GetCapture(), Directory, Error))) return false;
	TestTrue(TEXT("Worker prepares directory before success receipt"), Adapter.GetCapture()->Completion->Directory.IsValid());
	Adapter.Complete(Runtime, Store, true, Directory);
	const double SuccessfulRetirementDeadline = FPlatformTime::Seconds() + 5.0;
	while (!Adapter.RetireCaptureTask.IsCompleted() && FPlatformTime::Seconds() < SuccessfulRetirementDeadline)
		FPlatformProcess::SleepNoStats(0.001f);
	if (!TestTrue(TEXT("Successful capture releases old roots on Worker"), Adapter.RetireCaptureTask.IsCompleted())) return false;
	TestEqual(TEXT("Successful retry advances persisted proof"), Runtime.GetPersistedRevision(Key), uint64(1));
	TestTrue(TEXT("Saved Runtime data retires"), Runtime.RemoveSection(Key));
	const uint64 BeforeRetirement = Resources->Snapshot().Used[static_cast<int32>(EVoxelTerrainResourcePool::Resident)];
	for (int32 Step = 0; Step < 32; ++Step) Runtime.AdvanceJournalRetirement(64, FPlatformTime::Seconds() + 0.01);
	const double JournalRetirementDeadline = FPlatformTime::Seconds() + 5.0;
	while (!Runtime.IsJournalRetirementIdle() && FPlatformTime::Seconds() < JournalRetirementDeadline)
		FPlatformProcess::SleepNoStats(0.001f);
	if (!TestTrue(TEXT("Retired payload releases on Worker"), Runtime.IsJournalRetirementIdle())) return false;
	TestFalse(TEXT("Saved nonresident payload releases from journal cache"), Runtime.FindCommittedOverlay(Key).IsValid());
	TestEqual(TEXT("Payload retirement preserves full content revision"), Runtime.GetCommittedOverlayRevision(Key), uint64(1));
	TestTrue(TEXT("Payload retirement returns Resident capacity"), Resources->Snapshot().Used[static_cast<int32>(EVoxelTerrainResourcePool::Resident)] < BeforeRetirement);
	TArray<uint8> Bytes;
	FVoxelPersistentSection Decoded;
	TestEqual(TEXT("Retired payload remains readable from pinned disk generation"), Store.ReadSection(Key, Bytes, Error), EVoxelRegionRead::Loaded);
	TestTrue(TEXT("Retired payload decodes with original version"), FVoxelDeltaCodec::Decode(Bytes, Manifest, *Registry, Decoded) && Decoded.Revision == 1);
	Store.Reset();
	TestTrue(TEXT("Capacity test generation retires"), Store.TryRetireFileGeneration(Directory));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelDirectoryBootstrapIsolationTest,
	"WHFramework.Voxel.Save.DirectoryBootstrapIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelDirectoryBootstrapIsolationTest::RunTest(const FString& InParameters)
{
	FSaveGameStorage Storage(7301);
	const FGuid SaveId = FGuid::NewGuid();
	const FString FirstDirectory = Storage.GetGenerationDir(SaveId, 1);
	const FString SecondDirectory = Storage.GetGenerationDir(SaveId, 2);
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelWorldManifest Manifest;
	Manifest.WorldId = FGuid::NewGuid();
	Manifest.RegistryHash = Registry->Hash;
	Manifest.RecipeHash = VoxelTest::MakeGenerationConfig()->Recipe->RecipeHash;
	FVoxelPersistentSection Original;
	Original.Section = FIntVector(-7, 2, -3);
	Original.Revision = 7;
	Original.Blocks.Add(0, FVoxelBlockState());
	FString Error;
	TArray<uint8> Bytes;
	FVoxelRegionWritePlan First;
	First.TransactionId = FGuid::NewGuid();
	TestTrue(TEXT("Bootstrap original record encodes"), FVoxelDeltaCodec::Encode(Manifest, *Registry, Original, Bytes));
	TestTrue(TEXT("Bootstrap original record stages"), FVoxelRegionStore::StageSection(First, Original.Section, MoveTemp(Bytes)));
	if (!TestTrue(TEXT("Bootstrap original generation writes"), FVoxelRegionStore::WritePendingRegions(First, FirstDirectory, Error))) return false;
	Original.Revision = 8;
	Original.Blocks.Reset();
	FVoxelRegionWritePlan Second;
	Second.TransactionId = FGuid::NewGuid();
	Second.SourceDirectory = FirstDirectory;
	TestTrue(TEXT("Bootstrap replacement Tombstone encodes"), FVoxelDeltaCodec::Encode(Manifest, *Registry, Original, Bytes));
	TestTrue(TEXT("Bootstrap replacement Tombstone stages"), FVoxelRegionStore::StageSection(Second, Original.Section, MoveTemp(Bytes)));
	if (!TestTrue(TEXT("Bootstrap replacement generation writes"), FVoxelRegionStore::WritePendingRegions(Second, SecondDirectory, Error))) return false;
	FVoxelTaskScheduler Scheduler(true);
	const auto Resources = Scheduler.GetRuntimeResources();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(83, FGuid::NewGuid(), 1, {}, 1);
	TestTrue(TEXT("Bootstrap Working pool pressure is installed"), Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		Resources->Snapshot().Limits.WorkingBytes, Blocker));
	FVoxelRegionStore Store;
	FVoxelChangeIndex Index;
	int32 FirstApplied = 0;
	int32 SecondApplied = 0;
	Store.SetSource(SaveId, 1, &Storage, true);
	TestFalse(TEXT("Asynchronous source remains Unknown before directory receipt"), Store.IsDirectoryReady());
	FVoxelRegionReadView Read;
	TestFalse(TEXT("Pending directory cannot fabricate a modified record"), Store.CapturePinnedRead(Original.Section, 7, Read, Error));
	Store.AdvanceDirectoryLoad(Scheduler, 83, [&](auto) { ++FirstApplied; });
	Scheduler.Tick([](FVoxelTaskResult&&) {});
	Store.SetSource(SaveId, 2, &Storage, true);
	Store.AdvanceDirectoryLoad(Scheduler, 83, [&](auto Directory)
	{
		++SecondApplied;
		Index.SetBaseline(MoveTemp(Directory));
	});
	TestFalse(TEXT("New source does not inherit old pending readiness"), Store.IsDirectoryReady());
	Blocker.Reset();
	const double Deadline = FPlatformTime::Seconds() + 10.0;
	while (!Store.IsDirectoryReady() && Store.GetDirectoryError().IsEmpty() && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {});
		FPlatformProcess::SleepNoStats(0.001f);
	}
	if (!TestTrue(TEXT("Current directory completes after capacity release"), Store.IsDirectoryReady()))
	{
		AddError(Error + Store.GetDirectoryError());
		Scheduler.StopAndJoin();
		return false;
	}
	for (int32 Step = 0; Step < 8; ++Step)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {});
		FPlatformProcess::SleepNoStats(0.001f);
	}
	TestEqual(TEXT("Old directory receipt cannot publish into new source"), FirstApplied, 0);
	TestEqual(TEXT("Current directory publishes once"), SecondApplied, 1);
	TestTrue(TEXT("Current baseline includes committed Tombstone record"), Index.IsModified(Original.Section));
	TestTrue(TEXT("Current source provides full pinned descriptor"), Store.CapturePinnedRead(Original.Section, 8, Read, Error));
	TestEqual(TEXT("Current source reads replacement generation"), FVoxelRegionStore::Read(Read, Bytes, Error), EVoxelRegionRead::Loaded);
	FVoxelPersistentSection Decoded;
	TestTrue(TEXT("Source switch keeps replacement Tombstone revision"), FVoxelDeltaCodec::Decode(Bytes, Manifest, *Registry, Decoded) && Decoded.Revision == 8 && Decoded.IsEmpty());
	FVoxelRegionStore::ReleaseReadLease(Read);
	Scheduler.StopAndJoin();
	Store.Reset();
	TestTrue(TEXT("Released bootstrap generations retire"), Store.TryRetireFileGeneration(Storage.GetWorldDir(SaveId)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelModuleFileWindowTest,
	"WHFramework.Voxel.Save.ModuleFileWindows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelModuleFileWindowTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelModuleSaveData Data;
	Data.ManifestBytes.SetNumUninitialized(3 * 64 * 1024 + 19);
	for (int32 Index = 0; Index < Data.ManifestBytes.Num(); ++Index) Data.ManifestBytes[Index] = uint8(Index * 17 + 3);
	const FParameter Captured(MoveTemp(Data));
	const FName Name(TEXT("VoxelModule_保存_1729"));
	TArray<uint8> Original;
	TArray<uint8> Payload;
	if (!TestTrue(TEXT("Existing module writer captures large real reflected payload"),
		FSaveDataSerializer::BuildModuleFile(Name, 7, Captured, Original) &&
		FSaveDataSerializer::SerializeParameter(Captured, Payload))) return false;
	uint32 Crc = 0;
	int32 Windows = 0;
	for (int32 Offset = 0; Offset < Payload.Num(); Offset += 64 * 1024)
	{
		Crc = FCrc::MemCrc32(Payload.GetData() + Offset, FMath::Min(64 * 1024, Payload.Num() - Offset), Crc);
		++Windows;
	}
	TestTrue(TEXT("CRC window fixture spans multiple bounded steps"), Windows >= 4);
	TestEqual(TEXT("Persistent CRC seed produces the original whole-payload CRC"), Crc, FCrc::MemCrc32(Payload.GetData(), Payload.Num()));
	TArray<uint8> Windowed;
	if (!FSaveDataSerializer::WriteModuleHeaderBounded(Name, 7, Payload.Num(), Crc, Windowed,
		[](TArray<uint8>& Buffer, const int32 Required) { Buffer.Reserve(Required); return true; })) return false;
	Windowed.Reserve(Windowed.Num() + Payload.Num());
	for (int32 Offset = 0; Offset < Payload.Num(); Offset += 64 * 1024)
		Windowed.Append(Payload.GetData() + Offset, FMath::Min(64 * 1024, Payload.Num() - Offset));
	TestTrue(TEXT("Unicode/numbered FName and chunked body preserve the original complete wire file"), Windowed == Original);
	FModuleSaveFileHeader Header;
	FParameter Decoded;
	TestTrue(TEXT("Original reader accepts the real windowed module file"), FSaveDataSerializer::ReadModuleFile(Windowed, Header, Decoded));
	TestEqual(TEXT("Windowed header preserves actual module identity"), Header.ModuleName, Name);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSaveCoordinatorTest,
	"WHFramework.Voxel.Save.WorldSaveCoordinator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSaveCoordinatorTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Coordinator fixture creates a real world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AMainModule* Owner = World->SpawnActor<AMainModule>();
	USaveGameModule* Save = NewObject<USaveGameModule>(Owner);
	UVoxelSaveTestModule* Producer = NewObject<UVoxelSaveTestModule>(Owner);
	UVoxelSaveOtherTestModule* OtherProducer = NewObject<UVoxelSaveOtherTestModule>(Owner);
	Producer->Coordinator = Save;
	OtherProducer->Coordinator = Save;
	Producer->bRefreshDuringBefore = true;
	Owner->GetModules().Add(Producer);
	Owner->GetModuleMap().Add(Producer->GetModuleName(), Producer);
	Owner->GetModules().Add(OtherProducer);
	Owner->GetModuleMap().Add(OtherProducer->GetModuleName(), OtherProducer);
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("VoxelSaveCoordinator"));
	FSaveGameStorage Storage(0, Root);
	TArray<FGuid> Created;
	ON_SCOPE_EXIT
	{
		Save->OnTermination(EPhase::All);
		World->DestroyWorld(false);
		Owner->GetModuleMap().Reset();
		Owner->GetModules().Reset();
		GEngine->DestroyWorldContext(World);
		for (const FGuid& Id : Created) Storage.DeleteWorldDirectory(Id);
	};
	if (!TestEqual(TEXT("Coordinator uses its world MainModule singleton"), AMainModule::GetPtr(World, true), Owner)) return false;
	auto Create = [&]()
	{
		const FGuid Id = FGuid::NewGuid();
		FSaveManifest Manifest;
		Manifest.SaveId = Id;
		if (!Storage.CreateWorldDirectory(Id) || !Storage.WriteManifestAtomic(Id, Manifest)) return FGuid();
		Created.Add(Id);
		Save->Storage = MakeUnique<FSaveGameStorage>(0, Root);
		return Id;
	};
	const FGuid First = Create();
	const FSaveOperationResult Accepted = Save->SaveSlotAsync(First);
	if (!TestTrue(TEXT("World save returns a distinct accepted operation"),
		Accepted.Code == ESaveResultCode::Accepted && Accepted.OperationId.IsValid())) return false;
	FSaveManifest Manifest;
	TestTrue(TEXT("Accepted has not pretended to publish a generation"), Storage.ReadManifest(First, Manifest) && Manifest.CurrentGeneration == 0);
	TestTrue(TEXT("Accepting a save does not run the entire batch of module callbacks"),
		Producer->BeforeCount == 0 && OtherProducer->BeforeCount == 0);
	FVoxelTerrainFrameContext BlockedFrame;
	FString FrameError;
	if (!TestTrue(TEXT("Coordinator test uses the actual shared terrain frame"),
		BlockedFrame.Configure(FVoxelTerrainFrameBudgetSettings(), FrameError) &&
		BlockedFrame.Begin(1, 1, FPlatformTime::Seconds(), FVoxelTerrainFrameObservation()))) return false;
	auto ExhaustOperationBudget = [&]()
	{
		const double Started = FPlatformTime::Seconds();
		auto& Budget = BlockedFrame.GetPublishBudget();
		Budget.Begin(Started, 1000.0, 1);
		const bool bStarted = Budget.TryStart(EVoxelPublishPhase::Plan, Started, 0.0);
		if (bStarted) Budget.Finish(EVoxelPublishPhase::Plan, Started, FPlatformTime::Seconds());
		return bStarted;
	};
	if (!TestTrue(TEXT("Another actual phase consumes the available frame operation"), ExhaustOperationBudget())) return false;
	BlockedFrame.ConsumeOverdueOpportunity();
	Save->AdvanceAsyncSaveCapture(&BlockedFrame);
	TestTrue(TEXT("Exhausted shared frame and overdue token preserve the original callback cursor"),
		Producer->BeforeCount == 0 && OtherProducer->BeforeCount == 0 && Save->AsyncPrepareCount == 0);
	BlockedFrame.End(FPlatformTime::Seconds());
	if (!TestTrue(TEXT("A new frame provides one actual overdue opportunity"),
		BlockedFrame.Begin(1, 2, FPlatformTime::Seconds(), FVoxelTerrainFrameObservation()))) return false;
	if (!TestTrue(TEXT("The next frame operation is also owned by another phase"), ExhaustOperationBudget())) return false;
	Save->AdvanceAsyncSaveCapture(&BlockedFrame);
	TestTrue(TEXT("One coordinator step prepares one real module; callback Tick reentry cannot prepare the next module"),
		Producer->BeforeCount == 1 && OtherProducer->BeforeCount == 0 && Producer->CaptureCount == 0);
	Save->AdvanceAsyncSaveCapture(&BlockedFrame);
	TestTrue(TEXT("The same exhausted frame cannot reuse its original overdue token for another callback"),
		OtherProducer->BeforeCount == 0 && Save->AsyncPrepareCount == 1);
	BlockedFrame.End(FPlatformTime::Seconds());
	Producer->bRefreshDuringBefore = false;
	Save->OnRefresh(0.01f, false);
	TestTrue(TEXT("The next coordinator step advances the persistent preparation cursor"),
		Producer->BeforeCount == 1 && OtherProducer->BeforeCount == 1 && Producer->CaptureCount == 0);
	Save->OnRefresh(0.01f, false);
	TestEqual(TEXT("A later coordinator step freezes the module exactly once"), Producer->CaptureCount, 1);
	Producer->Seed = 99;
	int32 MaximumCopied = 0;
	int32 MaximumAfterCallbacks = 0;
	bool bObservedDurableFilesBeforeRuntimeCompletion = false;
	const double Deadline = FPlatformTime::Seconds() + 10.0;
	while (Save->bSaveOperationRunning && FPlatformTime::Seconds() < Deadline)
	{
		const int32 Previous = Save->AsyncModuleCapturePhase == 4 ? Save->AsyncModulePayloadCursor : 0;
		const int32 PreviousAfter = Producer->AfterCount + OtherProducer->AfterCount;
		Save->OnRefresh(0.01f, false);
		MaximumAfterCallbacks = FMath::Max(MaximumAfterCallbacks, Producer->AfterCount + OtherProducer->AfterCount - PreviousAfter);
		if (Save->AsyncCompletionResult.IsSet() && Save->bAsyncCompletionFilesCommitted)
		{
			FGuid LastActive;
			bObservedDurableFilesBeforeRuntimeCompletion |=
				Save->GetWorldSaveResult(Accepted.OperationId).Code == ESaveResultCode::Accepted &&
				Storage.ReadManifest(First, Manifest) && Manifest.CurrentGeneration > 0 &&
				Storage.ReadLastActiveSave(LastActive) && LastActive == First;
		}
		if (Save->AsyncModuleCapturePhase == 4 || Save->AsyncModuleCapturePhase == 5)
			MaximumCopied = FMath::Max(MaximumCopied, Save->AsyncModulePayloadCursor - Previous);
		FPlatformProcess::SleepNoStats(0.001f);
	}
	const auto Completed = Save->GetWorldSaveResult(Accepted.OperationId);
	if (!TestTrue(TEXT("Actual original worker completion changes Accepted to Success"), Completed.Code == ESaveResultCode::Success)) return false;
	TestEqual(TEXT("Capacity/file phases do not recapture the original module"), Producer->CaptureCount, 1);
	TestTrue(TEXT("Actual file copy advances in bounded windows"), MaximumCopied > 0 && MaximumCopied <= 64 * 1024);
	TestEqual(TEXT("Actual completion dispatches the module callback exactly once"), Producer->AfterCount, 1);
	TestEqual(TEXT("The second prepared module receives its own real completion callback"), OtherProducer->AfterCount, 1);
	TestTrue(TEXT("Durable worker files and last-active metadata still wait for the original GT completion cursor"),
		bObservedDurableFilesBeforeRuntimeCompletion);
	TestEqual(TEXT("Normal GT completion dispatches at most one original module callback per step"), MaximumAfterCallbacks, 1);
	TArray<uint8> File;
	FModuleSaveFileHeader Header;
	FParameter Decoded;
	if (!Storage.ReadManifest(First, Manifest) || !Storage.ReadBinary(Storage.GetModuleFilePath(First, Manifest.CurrentGeneration, Producer->GetModuleName()), File) ||
		!FSaveDataSerializer::ReadModuleFile(File, Header, Decoded)) return false;
	TestTrue(TEXT("Real persisted file preserves the originally frozen module data"), Decoded.GetPtr<FVoxelModuleSaveData>() && Decoded.GetPtr<FVoxelModuleSaveData>()->Seed == 71);
	Producer->bTerminateAfter = true;
	const FGuid AfterSlot = Create();
	const auto AfterAccepted = Save->SaveSlotAsync(AfterSlot);
	const double AfterDeadline = FPlatformTime::Seconds() + 10.0;
	while (Save->bSaveOperationRunning && FPlatformTime::Seconds() < AfterDeadline)
	{
		Save->OnRefresh(0.01f, false);
		FPlatformProcess::SleepNoStats(0.001f);
	}
	TestTrue(TEXT("OnAfter termination applies after the original commit callback without recursive cleanup"),
		Save->GetWorldSaveResult(AfterAccepted.OperationId).Code == ESaveResultCode::Success &&
		Producer->AfterCount == 2 && !Save->Storage && !Save->bSaveOperationRunning);
	Producer->bTerminateAfter = false;
	Producer->bTerminateBefore = true;
	const FGuid BeforeSlot = Create();
	const auto BeforeResult = Save->SaveSlotAsync(BeforeSlot);
	Save->OnRefresh(0.01f, false);
	TestTrue(TEXT("OnBefore termination cancels the accepted capture once before file work"),
		BeforeResult.Code == ESaveResultCode::Accepted && Save->GetWorldSaveResult(BeforeResult.OperationId).Code != ESaveResultCode::Accepted &&
		Producer->AfterCount == 3 && OtherProducer->AfterCount == 2 && !Producer->bAfterSucceeded && !Save->Storage);
	Producer->bTerminateBefore = false;
	Producer->bTerminateCapture = true;
	const FGuid CaptureSlot = Create();
	const auto CaptureAccepted = Save->SaveSlotAsync(CaptureSlot);
	Save->OnRefresh(0.01f, false);
	Save->OnRefresh(0.01f, false);
	Save->OnRefresh(0.01f, false);
	TestTrue(TEXT("ToData termination preserves the callback frame until original capture cleanup finishes"),
		Save->GetWorldSaveResult(CaptureAccepted.OperationId).Code != ESaveResultCode::Accepted &&
		Producer->AfterCount == 4 && !Producer->bAfterSucceeded && !Save->Storage && !Save->bSaveOperationRunning);
	return true;
}

#endif
