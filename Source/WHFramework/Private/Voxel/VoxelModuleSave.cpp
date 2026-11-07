#include "Voxel/VoxelModule.h"
#include "WHFrameworkCoreMacros.h"
#include "Common/LifecycleTypes.h"
#include "Ability/Inventory/InventorySnapshotTypes.h"
#include "Ability/Inventory/InventoryCommandTypes.h"
#include "Ability/Inventory/InventoryModule.h"
#include "Ability/Inventory/InventoryItemRegistry.h"
#include "Voxel/Interaction/VoxelInventoryDomainBridge.h"
#include "Voxel/Save/InventoryVoxelEntityCodec.h"
#include "Misc/SecureHash.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Rendering/VoxelViewLod.h"

#include "Async/Async.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
#include <limits>
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Asset/AssetModuleStatics.h"
#include "GameFramework/PlayerController.h"
#include "Main/MainModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Voxel/Authoring/VoxelWorldGenerationProfile.h"
#include "Voxel/Authoring/VoxelViewProfile.h"
#include "Voxel/Collision/VoxelCollisionPresenter.h"
#include "Voxel/Collision/VoxelPhysicsRetirement.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Voxel/Navigation/VoxelNavigationPresenter.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "SaveGame/SaveGameModule.h"
#include "SaveGame/SaveGameStorage.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelBuiltinFeatures.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Agent/VoxelAgentComponent.h"
#include "Voxel/Interaction/VoxelEditTransaction.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "Voxel/Network/VoxelOverlayReplica.h"
#include "Voxel/Map/VoxelMapTileCache.h"
#include "Voxel/Rendering/VoxelTerrainFrameBudget.h"
#include "Voxel/Map/VoxelMapSurfaceResolver.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Rendering/VoxelDetailView.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Rendering/VoxelFineSafety.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"
#include "Voxel/Scene/VoxelSceneRegion.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Voxels/VoxelItemBridge.h"

#include "Voxel/VoxelModuleInternal.h"

using namespace VoxelModuleInternal;

bool UVoxelModule::IsSaveEnabled() const
{
	return
		Super::IsSaveEnabled() &&
		!bWorldLoadRejected &&
		IsAuthority() &&
		bPersistenceEnabledForCurrentWorld;
}

const FVoxelWorldSaveData& UVoxelModule::GetWorldData() const
{
	check(WorldData);
	return *WorldData;
}

EVoxelWorldState UVoxelModule::GetWorldState() const
{
	return WorldState;
}

bool UVoxelModule::ApplyGenerationProfileFromSave(
	const FVoxelWorldSaveData& InData,
	FString& OutError)
{
	TSoftObjectPtr<
		UVoxelWorldGenerationProfile>
		ProfileAsset =
			InData.
				GenerationProfile;

	if (ProfileAsset.IsNull())
	{
		ProfileAsset =
			DefaultData.GenerationProfile;
	}

	UVoxelWorldGenerationProfile* Profile =
		ProfileAsset.
			LoadSynchronous();

	if (!Profile)
	{
		OutError =
			FString::Printf(
				TEXT(
					"Voxel world generation profile could not be loaded: %s"),
				*ProfileAsset.
					ToSoftObjectPath().
					ToString());

		return false;
	}

	ActiveGenerationProfileAsset =
		ProfileAsset;

	WorldGenerationProfile =
		Profile;

	OutError.Reset();
	return true;
}

TUniquePtr<FVoxelWorldSaveData>
UVoxelModule::NewWorldData(
	const FParameter& InBasic) const
{
	TUniquePtr<FVoxelModuleSaveData> Result =
		MakeUnique<
			FVoxelModuleSaveData>();

	static_cast<FVoxelWorldSaveData&>(*Result) = DefaultData;

	if (const FVoxelWorldSaveData* Source =
		InBasic.
			GetPtr<
				FVoxelWorldSaveData>())
	{
		static_cast<
			FVoxelWorldSaveData&>(
				*Result) =
					*Source;
	}

	return Result;
}

bool UVoxelModule::ValidateNewWorldData(const FParameter& InData, FString& OutError) const
{
	const FVoxelWorldSaveData* Data = InData.GetPtr<FVoxelWorldSaveData>();
	if (!Data || Data->IsSaved() || !Data->ManifestBytes.IsEmpty())
	{
		OutError = TEXT("New voxel world validation requires unsaved data without a manifest");
		return false;
	}
	if (Data->TerrainType != EVoxelTerrainType::Default && Data->TerrainType != EVoxelTerrainType::Superflat)
	{
		OutError = TEXT("New voxel world has an invalid terrain type");
		return false;
	}
	const TSoftObjectPtr<UVoxelWorldGenerationProfile> ProfileAsset =
		Data->GenerationProfile.IsNull() ? DefaultData.GenerationProfile : Data->GenerationProfile;
	const UVoxelWorldGenerationProfile* Profile = ProfileAsset.LoadSynchronous();
	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> RegistrySnapshot = Registry.GetSnapshot();
	if (!IsAuthority() || !RegistrySnapshot || !Profile)
	{
		OutError = TEXT("New voxel world requires an authoritative initialized registry and generation profile");
		return false;
	}
	FVoxelGenerationSettings Settings = Profile->Defaults;
	Settings.Seed = Data->Seed;
	if (!NormalizeVoxelWorldSettings(Settings, Data->TerrainType, OutError)) return false;
	FVoxelGenerationRuntimeConfig Config;
	if (!FVoxelGenerationBinding::Build(*Profile, *RegistrySnapshot, Settings,
		Profile->TargetCellCentimeters, Config, OutError))
	{
		return false;
	}
	if (!Config.Recipe)
	{
		OutError = TEXT("New voxel world generation binding has no frozen recipe");
		return false;
	}
	if (!ValidateSuperflatGround(Data->TerrainType, RegistrySnapshot.Get(), Shapes.Get(), Config, OutError)) return false;
	OutError.Reset();
	return true;
}

bool UVoxelModule::ValidateWorldData(const FParameter& InData, FString& OutError) const
{
	const FVoxelWorldSaveData* Data =
		InData.GetPtr<FVoxelWorldSaveData>();
	if (!Data)
	{
		OutError = TEXT("Expected typed voxel world save data");
		return false;
	}
	FVoxelWorldManifest SavedManifest;
	if (!FVoxelManifestCodec::Decode(Data->ManifestBytes, SavedManifest))
	{
		OutError = TEXT("Voxel save manifest is invalid or uses an unsupported generation identity protocol");
		return false;
	}
	if (Data->TerrainType != SavedManifest.TerrainType)
	{
		OutError = TEXT("Voxel terrain type differs from its saved world manifest");
		return false;
	}
	if (!FVoxelGenerationRecipe::IsSupportedAlgorithmVersion(SavedManifest.GeneratorVersion))
	{
		OutError = FString::Printf(TEXT("Saved voxel terrain algorithm %u differs from current algorithm %u"),
			SavedManifest.GeneratorVersion, FVoxelGenerationRecipe::CurrentAlgorithmVersion);
		return false;
	}
	if (!Registry.GetSnapshot() ||
		!WorldGenerationProfile ||
		SavedManifest.RegistryHash != Registry.GetSnapshot()->Hash)
	{
		OutError = TEXT("Voxel save manifest or registry identity is invalid");
		return false;
	}
	FVoxelGenerationRuntimeConfig Config;
	if (!FVoxelGenerationBinding::Build(
		*WorldGenerationProfile,
		*Registry.GetSnapshot(),
		SavedManifest.Settings,
		SavedManifest.BlockSizeCentimeters,
		Config,
		OutError))
	{
		return false;
	}
	if (!Config.Recipe || Config.Recipe->RecipeHash != SavedManifest.RecipeHash ||
		Config.Recipe->AlgorithmVersion != SavedManifest.GeneratorVersion)
	{
		OutError = TEXT("Saved voxel RecipeHash differs from the current generation profile");
		return false;
	}
	if (!ValidateSuperflatGround(SavedManifest.TerrainType, Registry.GetSnapshot().Get(), Shapes.Get(), Config, OutError)) return false;
	uint64 GenerationIdentitySalt = 0;
	if (!ResolveGenerationIdentitySalt(SavedManifest.TerrainType, GenerationIdentitySalt))
	{
		OutError = TEXT("Voxel generation identity requires the current registry and shapes");
		return false;
	}
	const uint64 ExpectedGenerationSignature =
		BuildGenerationSignature(SavedManifest, GenerationIdentitySalt);
	if (ExpectedGenerationSignature == 0 ||
		ExpectedGenerationSignature != SavedManifest.BaseSampleHash)
	{
		OutError = TEXT("Saved voxel base generation signature differs");
		return false;
	}
	OutError.Reset();
	return true;
}

void UVoxelModule::LoadData(
	const FParameter& InData,
	const EPhase InPhase)
{
	if (!PHASEC(InPhase, EPhase::Primary))
	{
		return;
	}

	if (!IsAuthority())
	{
		MarkLoadDataFailed();
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Voxel world load ignored on a non-authoritative world (phase=%d, netMode=%d)."),
			static_cast<int32>(InPhase),
			GetWorld()
				? static_cast<int32>(GetWorld()->GetNetMode())
				: -1);

		return;
	}

	FString Error;

	const FVoxelWorldSaveData* Data =
		InData.
			GetPtr<
				FVoxelWorldSaveData>();

	if (!Data)
	{
		MarkLoadDataFailed();
		if (!Runtime)
		{
			bWorldLoadRejected = true;
			WorldState = EVoxelWorldState::Failed;
			LastSaveError = TEXT("Voxel module expected FVoxelWorldSaveData");
		}

		UE_LOG(
			LogTemp,
			Error,
			TEXT("Voxel world load rejected: %s (parameter struct=%s)."),
			TEXT("Voxel module expected FVoxelWorldSaveData"),
			InData.GetStructType()
				? *InData.GetStructType()->GetPathName()
				: TEXT("None"));

		return;
	}

	if (Runtime &&
		!StopWorld(
			false,
			Error))
	{
		MarkLoadDataFailed();
		UE_LOG(LogTemp, Error, TEXT("Voxel load retained the original world: %s"), *Error);
		return;
	}
	bWorldLoadRejected = false;
	LastSaveError.Reset();

	if (!ApplyGenerationProfileFromSave(
			*Data,
			Error))
	{
		MarkLoadDataFailed();
		bWorldLoadRejected =
			true;

		WorldState =
			EVoxelWorldState::
				Failed;

		LastSaveError =
			Error;

		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Voxel profile load rejected: %s"),
			*Error);

		return;
	}

	WorldData =
		NewWorldData(
			InData);

	if (Data->ManifestBytes.IsEmpty())
	{
		RegionStore.Reset();
		UnloadedSceneFiles.Reset();
		NotifySceneCaptureChanged();

		if (!CreateWorldFromProfile(
				Data->Seed,
				Error,
				Data->TerrainType))
		{
			MarkLoadDataFailed();
			bWorldLoadRejected =
				true;

			WorldState =
				EVoxelWorldState::
					Failed;

			LastSaveError =
				Error;

			UE_LOG(
				LogTemp,
				Error,
				TEXT(
					"Voxel new world: %s"),
				*Error);
		}

		return;
	}

	if (!ValidateWorldData(
			InData,
			Error))
	{
		MarkLoadDataFailed();
		bWorldLoadRejected =
			true;

		WorldState =
			EVoxelWorldState::
				Failed;

		LastSaveError =
			Error;

		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Voxel load rejected: %s"),
			*Error);

		return;
	}

	FVoxelWorldManifest SavedManifest;

	if (!FVoxelManifestCodec::Decode(
			Data->ManifestBytes,
			SavedManifest) ||
		!StartWorld(
			SavedManifest,
			false,
			Error))
	{
		MarkLoadDataFailed();
		bWorldLoadRejected =
			true;

		WorldState =
			EVoxelWorldState::
				Failed;

		LastSaveError =
			Error.IsEmpty()
				? TEXT(
					"Voxel saved manifest could not start")
				: Error;

		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Voxel start: %s"),
			*LastSaveError);
	}
}

FParameter UVoxelModule::ToData()
{
	if (!IsReady() ||
		!LastSaveError.IsEmpty() ||
		!WorldData)
	{
		return FParameter();
	}

	FVoxelModuleSaveData Data;

	static_cast<
		FVoxelWorldSaveData&>(
			Data) =
				*WorldData;

	Data.GenerationProfile =
		ActiveGenerationProfileAsset;

	Data.Seed =
		Manifest.
			Settings.Seed;
	Data.TerrainType = Manifest.TerrainType;

	if (!FVoxelManifestCodec::Encode(
			Manifest,
			Data.ManifestBytes))
	{
		return FParameter();
	}

	return FParameter(
		MoveTemp(
			Data));
}

FParameter UVoxelModule::GetData()
{
	return ToData();
}

void UVoxelModule::UnloadData(const EPhase InPhase)
{
	if (PHASEC(InPhase, EPhase::Primary))
	{
		FString Error;
		if (!StopWorld(false, Error))
		{
			MarkUnloadDataFailed();
			UE_LOG(LogTemp, Error, TEXT("Voxel unload: %s"), *Error);
		}
	}
}

void UVoxelModule::SetActiveSaveSource(
	const FGuid& InSaveId,
	const int32 InGeneration,
	FSaveGameStorage* InStorage)
{
	if (SaveAdapter.IsBusy() || SceneSaveBuilder.IsSet())
	{
		UE_LOG(LogTemp, Error, TEXT("Scene save source cannot change during an owned save transaction"));
		return;
	}
	NotifySceneCaptureChanged();
	RegionStore.SetSource(InSaveId, InGeneration, InStorage, true);
	check(SceneSourceIncarnation < MAX_uint64);
	++SceneSourceIncarnation;
	if (InStorage &&
		InSaveId.IsValid() &&
		InGeneration > 0)
	{
		bPersistenceEnabledForCurrentWorld = true;
	}
	UnloadedSceneFiles.Reset();
}

void UVoxelModule::OnBeforeSaveData()
{
	check(IsInGameThread());
	if (SceneSaveBuilder.IsSet() || SaveAdapter.IsBusy())
	{
		LastSaveError = TEXT("Voxel save capture is already owned by an active transaction");
		return;
	}
	LastSaveError.Reset();
	CapturedSceneSave.Reset();
	if (!IsSaveEnabled())
	{
		return;
	}
	if (!Runtime || !Scheduler)
	{
		LastSaveError = TEXT("Create or load the voxel world before capturing it");
		return;
	}
	if (!FMath::IsFinite(MaximumSceneCaptureStepMilliseconds) || MaximumSceneCaptureStepMilliseconds <= 0.0)
	{
		LastSaveError = TEXT("Scene capture single-step capability limit is invalid");
		return;
	}
	SceneSaveBuilder.Emplace();
	SceneSaveBuilder->WorldEpoch = Epoch;
	SceneSaveBuilder->SceneRevision = SceneCaptureRevision;
	SceneSaveBuilder->ProjectRevision = GetSceneProjectCaptureRevision();
	SceneSaveBuilder->NextRegion = SceneRegionHead;
}

void UVoxelModule::NotifySceneCaptureChanged()
{
	check(IsInGameThread() && SceneCaptureRevision < MAX_uint64);
	++SceneCaptureRevision;
}

EVoxelSceneCaptureStep UVoxelModule::GetSaveCaptureState(FString& OutError) const
{
	check(IsInGameThread());
	OutError = LastSaveError;
	if (!OutError.IsEmpty()) return EVoxelSceneCaptureStep::Failed;
	if (SceneSaveBuilder.IsSet()) return (SceneSaveBuilder->bCapacityBlocked || SceneSaveBuilder->Column.bCapacityBlocked ||
		SaveAdapter.IsCaptureCapacityBlocked()) ?
		EVoxelSceneCaptureStep::CapacityBlocked : EVoxelSceneCaptureStep::Pending;
	if (SaveAdapter.GetCapture() && CapturedSceneSave) return EVoxelSceneCaptureStep::Complete;
	OutError = TEXT("Voxel save was not captured");
	return EVoxelSceneCaptureStep::Failed;
}

double UVoxelModule::GetMaximumSceneCaptureStepMilliseconds() const
{
	return MaximumSceneCaptureStepMilliseconds;
}

bool UVoxelModule::DidApplySaveCommit() const
{
	return bLatestSceneSaveSucceeded;
}

EVoxelSceneCaptureStep UVoxelModule::AdvanceSaveCaptureStep()
{
	check(SceneSaveBuilder.IsSet() && Runtime && Scheduler);
	auto& Builder = SceneSaveBuilder.GetValue();
	if (Builder.WorldEpoch != Epoch || Builder.SceneRevision != SceneCaptureRevision ||
		Builder.ProjectRevision != GetSceneProjectCaptureRevision())
	{
		LastSaveError = TEXT("Scene producer changed during its incremental fixed save cut");
		return EVoxelSceneCaptureStep::Failed;
	}
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources = Scheduler->GetRuntimeResources();
	uint64 BeforeCapacity = 0;
	const auto Blocked = [&]()
	{
		Builder.bCapacityBlocked = true;
		Builder.BlockedReleaseVersion = BeforeCapacity;
		LastSaveError.Reset();
		return EVoxelSceneCaptureStep::CapacityBlocked;
	};
	if (Builder.Phase == 0)
	{
		// 只有真实释放任务完成后才重用捕获槽；不在 GT join 旧任务。
		if (bMutating || SceneLoadingCount > 0 || !SaveAdapter.CanCapture() || !RegionStore.IsDirectoryReady())
			return EVoxelSceneCaptureStep::Pending;
		if (!SaveAdapter.Capture(*Runtime, Manifest, Registry.GetSnapshot(), RegionStore, LastSaveError))
		{
			if (!LastSaveError.StartsWith(TEXT("CapacityBlocked:"))) return EVoxelSceneCaptureStep::Failed;
			// Adapter保存原事务授予前的外部版本，外层仅转交等待状态。
			LastSaveError.Reset();
			return EVoxelSceneCaptureStep::CapacityBlocked;
		}
		Builder.Phase = 1;
		return EVoxelSceneCaptureStep::Pending;
	}
	const auto* VoxelCapture = SaveAdapter.GetCapture();
	if (!VoxelCapture || VoxelCapture->WorldEpoch != Builder.WorldEpoch)
	{
		LastSaveError = TEXT("Scene producer lost its authoritative voxel transaction");
		return EVoxelSceneCaptureStep::Failed;
	}
	if (!Builder.Consumer.IsValid()) Builder.Consumer = FVoxelGenerationConsumerKey::Section(
		Epoch, VoxelCapture->TransactionId, 1, {}, 1);
	if (!Resources->TryGetReleaseVersionExcludingOwner(Builder.Consumer, BeforeCapacity))
		return EVoxelSceneCaptureStep::CapacityBlocked;
	if (Builder.bCapacityBlocked && BeforeCapacity <= Builder.BlockedReleaseVersion)
		return EVoxelSceneCaptureStep::CapacityBlocked;
	Builder.bCapacityBlocked = false;
	const FVoxelGenerationRequiredSourceScope CaptureMetadataSource(Resources.Get(), VoxelCapture->MetadataLease.Get());
	const FVoxelGenerationRequiredSourceScope CaptureRetirementSource(Resources.Get(), SaveAdapter.GetRetirementResourceLease().Get());
	const FVoxelGenerationRequiredSourceScope DirectorySource(Resources.Get(),
		VoxelCapture->SourceDirectoryRoot ? VoxelCapture->SourceDirectoryRoot->ResourceLease.Get() : nullptr);
	const FVoxelGenerationRequiredSourceScope GenerationSource(Resources.Get(),
		VoxelCapture->SourceLease ? VoxelCapture->SourceLease->GetResourceLease().Get() : nullptr);
	const FVoxelGenerationRequiredSourceScope SceneSources(Resources.Get(), &Builder,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Source = *static_cast<const FVoxelSceneSaveCaptureBuilder*>(Context);
			if (Source.Scenes && Source.Scenes->Resources)
				Source.Scenes->VisitRequiredSources(*Source.Scenes->Resources, Visit);
		});
	const auto Grant = [&](const uint64 Bytes, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		const auto Admission = FVoxelSceneColumnCapture::TryGrantCapacity(*Resources, Builder.Consumer,
			EVoxelTerrainResourcePool::Input, Bytes, Lease, LastSaveError);
		return Admission == EVoxelTerrainResourceAdmission::Granted ? EVoxelSceneCaptureStep::Pending :
			Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? Blocked() : EVoxelSceneCaptureStep::Failed;
	};
	if (Builder.Phase == 1)
	{
		const int64 FileCount = int64(UnloadedSceneFiles.Num()) + SceneRegions.Num();
		if (FileCount > MAX_int32)
		{
			LastSaveError = TEXT("Scene save directory exceeded its protocol capacity");
			return EVoxelSceneCaptureStep::Failed;
		}
		const FVoxelSceneSaveCapture Shape;
		Builder.MetadataBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSceneSaveCapture>() +
			VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, FVoxelSceneSaveCut>>(Shape.Regions, SceneRegions.Num()) +
			VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FString, TArray<uint8>>>(Shape.Files, int32(FileCount)) +
			VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FString, TSharedPtr<const FVoxelSceneColumnCapture, ESPMode::ThreadSafe>>>(
				Shape.Columns, SceneRegions.Num());
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const auto Step = Grant(Builder.MetadataBytes, Lease);
		if (Step != EVoxelSceneCaptureStep::Pending) return Step;
		Builder.Scenes = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSceneSaveCapture>(Lease);
		auto& Scenes = *Builder.Scenes;
		Scenes.MetadataLease = Lease;
		Scenes.Resources = Resources;
		Scenes.TransactionId = VoxelCapture->TransactionId;
		Scenes.WorldEpoch = VoxelCapture->WorldEpoch;
		Scenes.Completion = VoxelCapture->Completion;
		// 失败的半成品也由原 SaveAdapter 的预授予释放任务接管。
		verify(SaveAdapter.AttachSceneCapture(Builder.Scenes));
		Scenes.Regions.Reserve(SceneRegions.Num());
		Scenes.Files.Reserve(int32(FileCount));
		Scenes.Columns.Reserve(SceneRegions.Num());
		const uint64 Actual = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSceneSaveCapture>() +
			VoxelGenerationAllocation::AllocatedBytes(Scenes.Regions) + VoxelGenerationAllocation::AllocatedBytes(Scenes.Files) +
			VoxelGenerationAllocation::AllocatedBytes(Scenes.Columns);
		check(Actual <= Builder.MetadataBytes);
		verify(Scenes.MetadataLease->TryResize(Actual) == EVoxelTerrainResourceAdmission::Granted);
		Builder.MetadataBytes = Actual;
		Builder.Phase = 2;
		return EVoxelSceneCaptureStep::Pending;
	}
	auto& Scenes = *Builder.Scenes;
	if (Builder.Phase == 2)
	{
		if (Builder.FileCursor >= UnloadedSceneFiles.GetMaxIndex())
		{
			Builder.Phase = 4;
			return EVoxelSceneCaptureStep::Pending;
		}
		const auto Id = FSetElementId::FromInteger(Builder.FileCursor);
		if (!UnloadedSceneFiles.IsValidId(Id))
		{
			++Builder.FileCursor;
			return EVoxelSceneCaptureStep::Pending;
		}
		const auto& File = UnloadedSceneFiles.Get(Id);
		if (File.Value.Num() > 8 * 1024 * 1024 || !File.Key.StartsWith(TEXT("voxel/actors/c_")) || File.Key.Contains(TEXT("..")))
		{
			LastSaveError = TEXT("Unloaded Scene file is outside its bounded protocol");
			return EVoxelSceneCaptureStep::Failed;
		}
		const uint64 RawBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(File.Value.Num());
		const uint64 PathBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<TCHAR, FDefaultAllocator>(File.Key.Len() + 1);
		const uint64 OldPathBytes = VoxelGenerationAllocation::AllocatedBytes(Builder.CurrentFilePath.GetCharArray());
		const bool bPathGrows = Builder.CurrentFilePath.GetCharArray().Max() < File.Key.Len() + 1;
		const uint64 PeakBytes = Builder.MetadataBytes + RawBytes + PathBytes + (bPathGrows ? PathBytes : 0);
		const auto Step = Grant(PeakBytes, Scenes.MetadataLease);
		if (Step != EVoxelSceneCaptureStep::Pending) return Step;
		Builder.CurrentFilePath.GetCharArray().Reserve(File.Key.Len() + 1);
		Builder.CurrentFilePath = File.Key;
		auto& SavedBytes = Scenes.Files.Add(File.Key);
		SavedBytes.Reserve(File.Value.Num());
		const uint64 ActualBytes = Builder.MetadataBytes - OldPathBytes +
			VoxelGenerationAllocation::AllocatedBytes(Builder.CurrentFilePath.GetCharArray()) +
			VoxelGenerationAllocation::AllocatedBytes(SavedBytes) + PathBytes;
		check(ActualBytes <= PeakBytes);
		verify(Scenes.MetadataLease->TryResize(ActualBytes) == EVoxelTerrainResourceAdmission::Granted);
		Builder.MetadataBytes = ActualBytes;
		Builder.FileOffset = 0;
		Builder.Phase = 3;
		return EVoxelSceneCaptureStep::Pending;
	}
	if (Builder.Phase == 3)
	{
		const auto& File = UnloadedSceneFiles.FindChecked(Builder.CurrentFilePath);
		auto& Bytes = Scenes.Files.FindChecked(Builder.CurrentFilePath);
		const int32 Count = FMath::Min(64 * 1024, File.Num() - Builder.FileOffset);
		if (Count > 0) Bytes.Append(File.GetData() + Builder.FileOffset, Count);
		Builder.FileOffset += Count;
		if (Builder.FileOffset == File.Num())
		{
			Builder.CurrentFilePath.Reset();
			++Builder.FileCursor;
			Builder.Phase = 2;
		}
		return EVoxelSceneCaptureStep::Pending;
	}
	if (Builder.Phase == 4)
	{
		auto* Region = Builder.CurrentRegion.Get();
		if (!Region)
		{
			Region = Builder.NextRegion.Get();
			if (!Region)
			{
				if (Builder.RegionCount != SceneRegions.Num())
				{
					LastSaveError = TEXT("Scene save cut lost a registered region owner");
					return EVoxelSceneCaptureStep::Failed;
				}
				Builder.Phase = 5;
				return EVoxelSceneCaptureStep::Pending;
			}
			Builder.CurrentRegion = Region;
			Builder.NextRegion = Region->NextRegion;
		}
		if (SceneRegions.FindRef(Region->GetRegionKey()) != Region)
		{
			LastSaveError = TEXT("Scene save cursor no longer owns its registered Region");
			return EVoxelSceneCaptureStep::Failed;
		}
		const FIntVector Key = Region->GetRegionKey();
		if (Builder.CurrentFilePath.IsEmpty())
		{
			TCHAR RelativePath[64];
			FCString::Snprintf(RelativePath, UE_ARRAY_COUNT(RelativePath), TEXT("voxel/actors/c_%d_%d.bin"), Key.X, Key.Y);
			const int32 PathLength = FCString::Strlen(RelativePath);
			const uint64 OldPathBytes = VoxelGenerationAllocation::AllocatedBytes(Builder.CurrentFilePath.GetCharArray());
			const bool bGrows = Builder.CurrentFilePath.GetCharArray().Max() < PathLength + 1;
			const uint64 NewPathBytes = bGrows ?
				VoxelGenerationAllocation::Private::ArrayReserveBytes<TCHAR, FDefaultAllocator>(PathLength + 1) : 0;
			const auto PathStep = Grant(Builder.MetadataBytes + NewPathBytes, Scenes.MetadataLease);
			if (PathStep != EVoxelSceneCaptureStep::Pending) return PathStep;
			Builder.CurrentFilePath.GetCharArray().Reserve(PathLength + 1);
			Builder.CurrentFilePath = RelativePath;
			const uint64 ActualBytes = Builder.MetadataBytes - OldPathBytes +
				VoxelGenerationAllocation::AllocatedBytes(Builder.CurrentFilePath.GetCharArray());
			verify(Scenes.MetadataLease->TryResize(ActualBytes) == EVoxelTerrainResourceAdmission::Granted);
			Builder.MetadataBytes = ActualBytes;
		}
		const FString& Path = Builder.CurrentFilePath;
		if (!Scenes.Columns.Contains(Path))
		{
			const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<TCHAR, FDefaultAllocator>(Path.Len() + 1);
			const auto KeyStep = Grant(Builder.MetadataBytes + Bytes, Scenes.MetadataLease);
			if (KeyStep != EVoxelSceneCaptureStep::Pending) return KeyStep;
			Builder.MetadataBytes += Bytes;
			Scenes.Columns.Add(Path);
			Scenes.Regions.Add(Key, {Region->SceneInstanceId, Region->SceneRevision});
		}
		const auto Step = Region->AdvanceSavePayloads(Builder.Column, LastSaveError);
		if (Builder.Column.Capture) Scenes.Columns.FindChecked(Path) = Builder.Column.Capture;
		if (Step == EVoxelSceneCaptureStep::CapacityBlocked)
		{
			// Column已保存grant前的原Consumer外部版本；外层不能改写为较晚的全局版本。
			LastSaveError.Reset();
			return Step;
		}
		if (Step == EVoxelSceneCaptureStep::Failed) return Step;
		if (Step == EVoxelSceneCaptureStep::Complete)
		{
			Scenes.Regions.Add(Key, {Builder.Column.RegionInstanceId, Builder.Column.Revision});
			if (const auto* OldFile = Scenes.Files.Find(Path))
			{
				const uint64 FreedBytes = VoxelGenerationAllocation::AllocatedBytes(*OldFile) +
					VoxelGenerationAllocation::Private::ArrayReserveBytes<TCHAR, FDefaultAllocator>(Path.Len() + 1);
				Scenes.Files.Remove(Path);
				check(FreedBytes <= Builder.MetadataBytes);
				Builder.MetadataBytes -= FreedBytes;
				verify(Scenes.MetadataLease->TryResize(Builder.MetadataBytes) == EVoxelTerrainResourceAdmission::Granted);
			}
			Builder.Column = {};
			Builder.CurrentRegion.Reset();
			Builder.CurrentFilePath.Reset();
			++Builder.RegionCount;
		}
		return EVoxelSceneCaptureStep::Pending;
	}
	const uint64 CursorPathBytes = VoxelGenerationAllocation::AllocatedBytes(Builder.CurrentFilePath.GetCharArray());
	Builder.CurrentFilePath.Empty();
	check(CursorPathBytes <= Builder.MetadataBytes);
	Builder.MetadataBytes -= CursorPathBytes;
	verify(Scenes.MetadataLease->TryResize(Builder.MetadataBytes) == EVoxelTerrainResourceAdmission::Granted);
	CapturedSceneSave = Builder.Scenes;
	bSceneSaveRequested = false;
	return EVoxelSceneCaptureStep::Complete;
}

void UVoxelModule::AdvanceSaveCapture()
{
	check(IsInGameThread());
	if (!SceneSaveBuilder.IsSet() || !LastSaveError.IsEmpty() || !Runtime || !Scheduler || !TerrainFrameContext.IsActive()) return;
	const double Deadline = TerrainFrameContext.Deadline(0.35);
	for (int32 Steps = 0; Steps < 32 && SceneSaveBuilder.IsSet(); ++Steps)
	{
		const double Started = FPlatformTime::Seconds();
		bool bOverdue = false;
		if (Started >= Deadline || !TerrainFrameContext.GetPublishBudget().TryStart(EVoxelPublishPhase::Prepare,
			Started, SceneSaveCaptureCostMilliseconds))
		{
			if (Steps != 0 || !TerrainFrameContext.IsOverdueOpportunityAvailable()) break;
			TerrainFrameContext.ConsumeOverdueOpportunity();
			bOverdue = true;
		}
		EVoxelSceneCaptureStep Step;
		{
			TGuardValue<bool> Transition(bSceneTransitionActive, true);
			Step = AdvanceSaveCaptureStep();
		}
		const double Finished = FPlatformTime::Seconds();
		const double Actual = (Finished - Started) * 1000.0;
		SceneSaveCaptureCostMilliseconds = FMath::Lerp(SceneSaveCaptureCostMilliseconds, Actual, 0.15);
		TerrainFrameContext.GetPublishBudget().Finish(EVoxelPublishPhase::Prepare, Started, Finished);
		if (Actual > MaximumSceneCaptureStepMilliseconds)
		{
			const FString TimingError = FString::Printf(TEXT("Scene capture capability error: indivisible step %.3fms exceeds configured %.3fms"),
				Actual, MaximumSceneCaptureStepMilliseconds);
			if (USaveGameModule::IsStepTimeLimitEnforced())
			{
				LastSaveError = TimingError;
				CapturedSceneSave.Reset();
				Step = EVoxelSceneCaptureStep::Failed;
			}
			else UE_LOG(LogTemp, Warning, TEXT("Save timing limit disabled: %s"), *TimingError);
		}
		if (Step == EVoxelSceneCaptureStep::Complete) SceneSaveBuilder.Reset();
		if (Step == EVoxelSceneCaptureStep::Complete || Step == EVoxelSceneCaptureStep::Failed ||
			Step == EVoxelSceneCaptureStep::CapacityBlocked || bOverdue) break;
	}
}

bool UVoxelModule::CopySaveCapture(FVoxelModuleSaveCapture& OutCapture, FString& OutError) const
{
	if (GetSaveCaptureState(OutError) != EVoxelSceneCaptureStep::Complete)
	{
		if (OutError.IsEmpty()) OutError = TEXT("Voxel save capture is still pending");
		return false;
	}
	OutCapture.Voxels = *SaveAdapter.GetCapture();
	OutCapture.Scenes = CapturedSceneSave;
	return true;
}

bool UVoxelModule::BindSaveCommitOwner(const FGuid& InSaveId, const FString& InStorageRoot, FString& OutError)
{
	check(IsInGameThread());
	const auto* Capture = SaveAdapter.GetCapture();
	if (!Capture || !Capture->Completion || !InSaveId.IsValid() || InStorageRoot.IsEmpty() ||
		Capture->Completion->ExpectedStorageRoot || !Capture->Journal.Resources)
	{
		OutError = TEXT("Cannot bind save slot/root ownership to its captured transaction");
		return false;
	}
	auto& Completion = *Capture->Completion;
	const uint64 Bytes = VoxelBoundedAllocation::SharedBytes<FString>() + VoxelBoundedAllocation::ArrayBytes<TCHAR>(InStorageRoot.Len() + 1);
	if (!Capture->Journal.Resources->Acquire(FVoxelGenerationConsumerKey::Section(Capture->WorldEpoch,
		Capture->TransactionId, 1, {}, 7), EVoxelTerrainResourcePool::Input, Bytes, Completion.OwnerLease))
	{
		OutError = TEXT("CapacityBlocked: save slot/root owner metadata was not granted");
		return false;
	}
	Completion.ExpectedStorageRoot = MakeShared<const FString, ESPMode::ThreadSafe>(InStorageRoot);
	Completion.ExpectedSaveId = InSaveId;
	OutError.Reset();
	return true;
}

bool UVoxelModule::BindSaveCommitTarget(const FGuid& InSaveId, const int32 InGeneration,
	const FString& InDirectory, FString& OutError)
{
	check(IsInGameThread());
	const auto* Capture = SaveAdapter.GetCapture();
	if (!Capture || !Capture->Completion || !InSaveId.IsValid() || InGeneration <= 0 || InDirectory.IsEmpty() ||
		Capture->Completion->TargetDirectory || !Capture->Journal.Resources)
	{
		OutError = TEXT("Cannot bind voxel save target to its captured transaction");
		return false;
	}
	auto& Completion = *Capture->Completion;
	const uint64 Bytes = VoxelBoundedAllocation::SharedBytes<FString>() +
		2 * VoxelBoundedAllocation::ArrayBytes<TCHAR>(InDirectory.Len() + 1);
	if (!Capture->Journal.Resources->Acquire(
		FVoxelGenerationConsumerKey::Section(Capture->WorldEpoch, Capture->TransactionId, 1, {}, 5),
		EVoxelTerrainResourcePool::Input, Bytes, Completion.TargetLease))
	{
		OutError = TEXT("CapacityBlocked: save commit target was not granted");
		return false;
	}
	Completion.TargetDirectory = MakeShared<const FString, ESPMode::ThreadSafe>(InDirectory);
	Completion.TargetSaveId = InSaveId;
	Completion.TargetGeneration = InGeneration;
	PendingCommitDirectory = InDirectory;
	OutError.Reset();
	return true;
}

bool UVoxelModule::WriteSaveCapture(
	const FVoxelModuleSaveCapture& InCapture,
	const FString& InTemporaryGeneration,
	FString& OutError)
{
	const auto& Completion = InCapture.Voxels.Completion;
	if (!InCapture.Scenes || InCapture.Scenes != InCapture.Voxels.SceneCapture ||
		!Completion || InCapture.Scenes->Completion != Completion ||
		InCapture.Scenes->TransactionId != InCapture.Voxels.TransactionId ||
		InCapture.Scenes->WorldEpoch != InCapture.Voxels.WorldEpoch)
	{
		OutError = TEXT("Scene save cut does not belong to the voxel save transaction");
		return false;
	}
	Completion->bSceneFilesWritten = false;
	Completion->bFilesFlushed = false;
	Completion->bGenerationCommitted = false;
	if (!FVoxelWorldSaveAdapter::WriteCapture(
		InCapture.Voxels,
		InTemporaryGeneration,
		OutError))
	{
		return false;
	}
	for (const TPair<FString, TArray<uint8>>& Pair : InCapture.Scenes->Files)
	{
		if (!Pair.Key.StartsWith(TEXT("voxel/actors/c_")) ||
			Pair.Key.Contains(TEXT("..")) ||
			Pair.Value.Num() > 8 * 1024 * 1024)
		{
			OutError = TEXT("Invalid voxel scene region capture");
			return false;
		}
		const FString Path = FPaths::Combine(InTemporaryGeneration, Pair.Key);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		if (!FSaveGameStorage::WriteBinaryDurable(Path, Pair.Value))
		{
			OutError = TEXT("Voxel scene region write failed");
			return false;
		}
	}
	for (const auto& Pair : InCapture.Scenes->Columns)
	{
		const auto EncodingConsumer = FVoxelGenerationConsumerKey::Section(InCapture.Voxels.WorldEpoch,
			InCapture.Voxels.TransactionId, 1, Pair.Value ? Pair.Value->Consumer.Min : FIntVector::ZeroValue, 9);
		if (!Pair.Value || !Pair.Key.StartsWith(TEXT("voxel/actors/c_")) || Pair.Key.Contains(TEXT("..")) ||
			!FVoxelSceneColumnCodec::EncodePayloads(*Pair.Value, OutError, &EncodingConsumer))
		{
			if (OutError.IsEmpty()) OutError = TEXT("Invalid voxel Scene payload column capture");
			return false;
		}
		if (!FSaveGameStorage::WriteBinaryDurable(FPaths::Combine(InTemporaryGeneration, Pair.Key), Pair.Value->Completion->Bytes))
		{
			OutError = TEXT("Voxel Scene encoded payload column write failed");
			return false;
		}
	}
	Completion->bSceneFilesWritten = true;
	return true;
}

void UVoxelModule::OnAfterSaveData(const bool bInSuccess)
{
	check(IsInGameThread());
	if (Runtime && SaveAdapter.IsBusy())
	{
		const TSharedPtr<const FVoxelSceneSaveCapture, ESPMode::ThreadSafe> Attempt = CapturedSceneSave ?
			CapturedSceneSave : SceneSaveBuilder.IsSet() ? SceneSaveBuilder->Scenes : nullptr;
		const auto Completion = Attempt ? Attempt->Completion : nullptr;
		const auto* VoxelCapture = SaveAdapter.GetCapture();
		if (bInSuccess && Completion && Completion->TargetDirectory && Completion->ExpectedStorageRoot &&
			Completion->TargetSaveId == Completion->ExpectedSaveId && PendingCommitDirectory.IsEmpty())
			PendingCommitDirectory = *Completion->TargetDirectory;
		const bool bActualCommit = bInSuccess && Attempt && Completion && VoxelCapture &&
			VoxelCapture->SceneCapture == Attempt && VoxelCapture->Completion == Completion &&
			VoxelCapture->TransactionId == Attempt->TransactionId &&
			Attempt->WorldEpoch == Runtime->Epoch() && Completion->WorldEpoch == Attempt->WorldEpoch &&
			Completion->TransactionId == Attempt->TransactionId && Completion->TargetSaveId.IsValid() &&
			Completion->TargetGeneration > 0 && Completion->TargetDirectory &&
			(!Completion->ExpectedSaveId.IsValid() || Completion->TargetSaveId == Completion->ExpectedSaveId) &&
			FPaths::IsSamePath(*Completion->TargetDirectory, PendingCommitDirectory) &&
			Completion->bSceneFilesWritten && Completion->bFilesFlushed && Completion->bGenerationCommitted;
		const bool bApplied = SaveAdapter.Complete(*Runtime, RegionStore, bActualCommit, PendingCommitDirectory,
			MoveTemp(LatestSceneSaveAttempt), bActualCommit ? DurableSceneSaveReceipt : nullptr);
		LatestSceneSaveAttempt = Attempt;
		bLatestSceneSaveSucceeded = bActualCommit && bApplied;
		if (bLatestSceneSaveSucceeded) DurableSceneSaveReceipt = Attempt;
		else bSceneSaveRequested = false;
	}
	CapturedSceneSave.Reset();
	SceneSaveBuilder.Reset();
	LastSaveError.Reset();
	PendingCommitDirectory.Empty();
}

void UVoxelModule::OnSaveCaptureAbandoned()
{
	// The worker owns a separate immutable capture. The failure path does not read
	// its shared completion fields and cannot adopt the target into this World.
	OnAfterSaveData(false);
}
