#include "Voxel/VoxelModule.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
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
#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "Voxel/Network/VoxelOverlayReplica.h"
#include "Voxel/Map/VoxelMapTileCache.h"
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

namespace
{
	constexpr int32 InterestErrorCapacity = 1024;
	bool IsTerminalInterestBuildError(const FString& InError)
	{
		return InError.StartsWith(TEXT("Oversized")) || InError.StartsWith(TEXT("CapacityUnachievable")) ||
			InError.StartsWith(TEXT("TerminalFailure")) || InError.StartsWith(TEXT("InvalidConsumer")) || InError.StartsWith(TEXT("InvalidRequest"));
	}

	struct FVoxelInterestTaskPayload : FVoxelTaskCustomPayload, FVoxelResidencyDemandEvents
	{
		TSharedPtr<FVoxelInterestSet, ESPMode::ThreadSafe> Interest;
		TSharedPtr<FVoxelInterestRuntime, ESPMode::ThreadSafe> CandidateRuntime;
		FVoxelInterestDelta Delta;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ErrorLease;
		FString ErrorBuffer;
		TAtomic<bool> bCapacityWaiting{false};
		TAtomic<uint64> CapacityVersion{0};
		bool bChanged = false;
		bool bHasSnapshot = false;
		virtual const FVoxelInterestDelta& GetResidencyDelta() const override
		{
			return Delta;
		}

		virtual uint64 GetAllocatedBytes() const override
		{
			return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelInterestTaskPayload>() +
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelInterestRuntime>() +
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelInterestSet>() +
				(Interest && Interest->SafetyVersions ? VoxelBoundedAllocation::WorkerSharedBytes<FVoxelChangeHierarchySnapshot>() : 0) +
				(Interest ? Interest->GetOwnedAllocatedBytes() : 0) + Delta.GetAllocatedBytes() + ErrorBuffer.GetAllocatedSize();
		}
		virtual uint64 GetResourceAccountedBytes() const override
		{
			return (MetadataLease ? MetadataLease->Bytes() : 0) + (Interest ? Interest->GetResourceAccountedBytes() : 0) +
				(Delta.ResourceLease ? Delta.ResourceLease->Bytes() : 0) + (ErrorLease ? ErrorLease->Bytes() : 0);
		}
		virtual void LogResourceAccounting(const FVoxelTaskStamp& InStamp) const override
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Error, TEXT("Voxel interest accounting: epoch=%llu token=%llu actual=%llu accounted=%llu owned=%llu finalizationActual=%llu deltaActual=%llu metadata=%llu deltaLease=%llu errorBuffer=%llu errorLease=%llu"),
				InStamp.WorldEpoch, InStamp.Token, GetAllocatedBytes(), GetResourceAccountedBytes(),
				Interest ? Interest->GetOwnedAllocatedBytes() : 0, Interest ? Interest->GetFinalizationAllocatedBytes() : 0,
				Delta.GetAllocatedBytes(), MetadataLease ? MetadataLease->Bytes() : 0, Delta.ResourceLease ? Delta.ResourceLease->Bytes() : 0,
				ErrorBuffer.GetAllocatedSize(), ErrorLease ? ErrorLease->Bytes() : 0);
			if (!Interest) return;
			const FVoxelTerrainResourceLease* Leases[] = {MetadataLease.Get(), Interest->DemandLease.Get(), Interest->CompositionLease.Get(),
				Interest->CopyLease.Get(), Interest->FinalizationLease.Get(), Interest->CoarseCompositionLease.Get(),
				Interest->CoarsePlanLeases[0].Get(), Interest->CoarsePlanLeases[1].Get(), Interest->CoarsePlanLeases[2].Get(), Delta.ResourceLease.Get()};
			const TCHAR* Names[] = {TEXT("metadata"), TEXT("demand"), TEXT("composition"), TEXT("copy"), TEXT("finalization"),
				TEXT("coarseComposition"), TEXT("coarse0"), TEXT("coarse1"), TEXT("coarse2"), TEXT("delta")};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Leases); ++Index)
			{
				const auto* Lease = Leases[Index];
				if (Lease) UE_LOG(LogTemp, Error, TEXT("Voxel interest lease: epoch=%llu token=%llu name=%s serial=%llu pool=%d bytes=%llu"),
					InStamp.WorldEpoch, InStamp.Token, Names[Index], Lease->Serial(), int32(Lease->GetPool()), Lease->Bytes());
			}
#endif
		}
	};
	struct FVoxelInterestSourceCapture
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		TArray<FVoxelStreamingSource> Values;
	};

	FIntVector VoxelModuleToSection(const FIntVector& InPosition)
	{
		return FIntVector(
			VoxelGeneration::FloorDivide(InPosition.X, 16),
			VoxelGeneration::FloorDivide(InPosition.Y, 16),
			VoxelGeneration::FloorDivide(InPosition.Z, 16));
	}

	int32 VoxelModuleToCellIndex(const FIntVector& InPosition)
	{
		auto PositiveMod = [](const int32 InValue)
		{
			const int32 Result = InValue % 16;
			return Result < 0 ? Result + 16 : Result;
		};
		return PositiveMod(InPosition.X) +
			PositiveMod(InPosition.Y) * 16 +
			PositiveMod(InPosition.Z) * 256;
	}

	uint64 BuildGenerationSignature(
		const FVoxelWorldManifest& InManifest,
		const uint64 InContentSalt)
	{
		const uint64 Fingerprint =
			FVoxelManifestCodec::RecipeFingerprint(
				InManifest);

		if (Fingerprint == 0 ||
			InManifest.RegistryHash == 0 ||
			InManifest.RecipeHash == 0)
		{
			return 0;
		}

		const uint64 ContentIdentity = InContentSalt == 0
			? 0
			: VoxelBinary::Mix64(InContentSalt);
		const uint64 Signature =
			VoxelBinary::Mix64(
				Fingerprint ^
				VoxelBinary::Mix64(
					InManifest.RegistryHash) ^
				VoxelBinary::Mix64(
					InManifest.RecipeHash) ^
				ContentIdentity);

		return Signature == 0
			? 1
			: Signature;
	}

}

IMPLEMENTATION_MODULE(UVoxelModule)

UVoxelModule::UVoxelModule(FVTableHelper& Helper)
	: Super(Helper)
{
}

UVoxelModule::UVoxelModule()
{
	ModuleName =
		TEXT("VoxelModule");

	ModuleDisplayName =
		FText::FromString(
			TEXT(
				"Voxel Module"));

	SaveScope =
		ESaveScope::World;

	/**
	 * Save struct 已扁平化。
	 * 不做旧开发存档兼容。
	 */
	SaveDataVersion =
		5;

	ModuleDependencies = {
		FName(TEXT("AbilityModule")),
		FName(TEXT("SceneModule"))
	};

	WorldData =
		MakeUnique<
			FVoxelModuleSaveData>();
}

UVoxelModule::~UVoxelModule()
{
	FString Error;
	StopWorld(true, Error);
	TERMINATION_MODULE(UVoxelModule)
}

bool UVoxelModule::CreateGenerationOverlay(
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InNaturalGenerator,
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
	TSharedPtr<const IVoxelGenerationOverlay, ESPMode::ThreadSafe>& OutOverlay,
	FString& OutError) const
{
	(void)InNaturalGenerator;
	(void)InRegistry;
	OutOverlay.Reset();
	OutError.Reset();
	return true;
}

uint64 UVoxelModule::GetGenerationIdentitySalt() const
{
	return 0;
}

#if WITH_EDITOR
void UVoxelModule::OnDestroy()
{
	Super::OnDestroy();
	TERMINATION_MODULE(UVoxelModule)
}
#endif

UWorld* UVoxelModule::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	const AMainModule* MainModule = GetTypedOuter<AMainModule>();
	return MainModule ? MainModule->GetWorld() : nullptr;
}

void UVoxelModule::OnInitialize()
{
	Super::OnInitialize();
	FString Error;
	if (!VoxelBuiltinFeatures::Register(Error))
	{
		WorldState = EVoxelWorldState::Failed;
		UE_LOG(LogTemp, Error, TEXT("Voxel builtin feature registration: %s"), *Error);
		return;
	}
	const bool bRendering = GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer;
	const TArray<UVoxelData*> Assets = UAssetModuleStatics::LoadPrimaryAssets<UVoxelData>(FName(TEXT("Voxel")));
	if (!Registry.Build(Assets, bRendering, Error))
	{
		WorldState = EVoxelWorldState::Failed;
		UE_LOG(LogTemp, Error, TEXT("Voxel registry: %s"), *Error);
		return;
	}
	TSharedRef<FVoxelShapeRegistry, ESPMode::ThreadSafe> NewShapes = MakeShared<FVoxelShapeRegistry, ESPMode::ThreadSafe>();
	NewShapes->BuildDefaults();
	Shapes = NewShapes;
	if (bRendering)
	{
		MaterialSet = MaterialSetAsset.LoadSynchronous();
		if (!MaterialSet || !MaterialSet->Validate(Error))
		{
			WorldState = EVoxelWorldState::Failed;
			UE_LOG(LogTemp, Error, TEXT("Voxel material set: %s"), *Error);
			return;
		}
	}
	WorldGenerationProfile = WorldGenerationProfileAsset.LoadSynchronous();
	if (!WorldGenerationProfile)
	{
		WorldState = EVoxelWorldState::Failed;
		UE_LOG(LogTemp, Error, TEXT("Voxel world generation profile is missing"));
	}
}

void UVoxelModule::OnPreparatory(const EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
	if (bAutoGenerate && IsAuthority() && !Runtime && WorldState != EVoxelWorldState::Failed)
	{
		FString Error;
		if (!CreateWorldFromProfile(ResolveDefaultWorldSeed(), Error))
		{
			UE_LOG(LogTemp, Error, TEXT("Voxel auto world creation: %s"), *Error);
		}
	}
}

void UVoxelModule::OnRefresh(
	const float InDeltaSeconds,
	const bool bInEditor)
{
	Super::OnRefresh(
		InDeltaSeconds,
		bInEditor);

	if (bInEditor)
	{
		return;
	}
	USaveGameModule* SaveCleanupOwner = nullptr;
	const bool bTerrainReady = IsReady();
	if (!bTerrainReady)
	{
		SaveCleanupOwner = USaveGameModule::GetPtr(GetWorld());
		if (!SaveCleanupOwner || !SaveCleanupOwner->HasWorldSaveWork(*this)) return;
	}

	const double VoxelTickStart = FPlatformTime::Seconds();
	const bool bDedicated = GetWorld()->GetNetMode() == NM_DedicatedServer;
	const double MissingTiming = std::numeric_limits<double>::quiet_NaN();
	FVoxelTerrainFrameObservation Observation;
	for (const auto& Pair : Sources)
	{
		Observation.bInitialSpawn |= Pair.Value.Value.Purpose == EVoxelStreamingSourcePurpose::InitialSpawn;
	}
	Observation.GameThreadMilliseconds = GGameThreadTime > 0 ?
		FPlatformTime::ToMilliseconds(GGameThreadTime) : MissingTiming;
	Observation.TerrainGameThreadMilliseconds = TerrainFrameContext.GetLastTerrainMilliseconds();
	Observation.RenderThreadMilliseconds = bDedicated ? 0.0 :
		(GRenderThreadTime > 0 ? FPlatformTime::ToMilliseconds(GRenderThreadTime) : MissingTiming);
	const uint32 GPUCycles = bDedicated ? 0 : RHIGetGPUFrameCycles();
	Observation.GPUMilliseconds = bDedicated ? 0.0 :
		(GPUCycles > 0 ? FPlatformTime::ToMilliseconds(GPUCycles) : MissingTiming);
	if (!TerrainFrameContext.Begin(Epoch, GFrameCounter, VoxelTickStart, Observation))
	{
		return;
	}
	if (!bTerrainReady)
	{
		// 失败世界只结算其原保存 owner，不恢复 admission 或重新打开正常生成链。
		SaveCleanupOwner->AdvanceWorldSaveCapture(*this);
		TerrainFrameContext.End(FPlatformTime::Seconds());
		return;
	}
	if (const auto Statistics = Scheduler->GetTerrainStatistics())
	{
		Statistics->AddFrame(Observation.GameThreadMilliseconds, EVoxelTerrainFrameMetric::GameThread);
		Statistics->AddFrame(Observation.TerrainGameThreadMilliseconds, EVoxelTerrainFrameMetric::TerrainGameThread);
		if (!bDedicated)
		{
			Statistics->AddFrame(Observation.RenderThreadMilliseconds, EVoxelTerrainFrameMetric::RenderThread);
			Statistics->AddFrame(Observation.GPUMilliseconds, EVoxelTerrainFrameMetric::GPU);
		}
	}

	if (IsAuthority()) RegionStore.AdvanceDirectoryLoad(*Scheduler, Epoch,
		[this, ExpectedEpoch = Epoch](TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> InDirectory)
		{
			if (Runtime && Epoch == ExpectedEpoch)
			{
				Runtime->GetChangeIndex().SetBaseline(MoveTemp(InDirectory));
				bInterestDirty = true;
			}
		});
	Scheduler->Tick(
		[this](
			FVoxelTaskResult&& InResult)
		{
			ApplyTask(
				MoveTemp(InResult));
		}, ViewSettings.Budget.ResultApplyMilliseconds);

	if (IsAuthority() && !RegionStore.GetDirectoryError().IsEmpty())
	{
		WorldState = EVoxelWorldState::Failed;
		UE_LOG(LogTemp, Error, TEXT("Voxel启动目录读取失败: %s"), *RegionStore.GetDirectoryError());
		TerrainFrameContext.End(FPlatformTime::Seconds());
		return;
	}
	const double AfterScheduler = FPlatformTime::Seconds();
	const double NetworkDeadline = TerrainFrameContext.Deadline(0.1);
	for (int32 Count = 0; Count < TerrainNetworkComponents.Num() && FPlatformTime::Seconds() < NetworkDeadline; ++Count)
	{
		TerrainNetworkCursor %= TerrainNetworkComponents.Num();
		if (auto* Network = TerrainNetworkComponents[TerrainNetworkCursor++].Get()) Network->AdvanceTerrainNetwork(TerrainFrameContext);
	}
	if (OverlayReplica)
	{
		FString OverlayError;
		if (FPlatformTime::Seconds() < NetworkDeadline) OverlayReplica->Advance(1, OverlayError);
	}
	const double OverlayDeadline = TerrainFrameContext.Deadline(0.05);
	for (int32 Count = 0; Count < 16 && FPlatformTime::Seconds() < OverlayDeadline; ++Count)
	{
		if (!ApplyingOverlayChange)
		{
			if (!OverlayChangeQueue.Dequeue(ApplyingOverlayChange)) break;
			OverlayChangeCursor = 0;
			bWaitingOverlayChangeCapacity = false;
			bOverlayChangeInvalidated = false;
			bOverlayChangeWaitingEmerge = false;
		}
		if (!ApplyingOverlayChange->Sections.IsValidIndex(OverlayChangeCursor))
		{
			ApplyingOverlayChange.Reset();
			bWaitingOverlayChangeCapacity = false;
			bOverlayChangeInvalidated = false;
			bOverlayChangeWaitingEmerge = false;
			continue;
		}
		const auto ChangeResources = bOverlayChangeWaitingEmerge ? Scheduler->GetTerrainResources() : Scheduler->GetRuntimeResources();
		const auto ChangeOwner = bOverlayChangeWaitingEmerge ? EmergeManager->GetControlConsumer() :
			Runtime->GetChangeHierarchy().GetControlConsumer();
		const uint64 BeforeChange = ChangeResources->ReleaseVersionExcludingOwner(ChangeOwner);
		if (bWaitingOverlayChangeCapacity && BeforeChange <= OverlayChangeCapacityVersion) break;
		const FIntVector Key = ApplyingOverlayChange->Sections[OverlayChangeCursor];
		FString ChangeError;
		if (!bOverlayChangeInvalidated && !Runtime->InvalidateRemoteSection(Key, ChangeError))
		{
			if (ChangeError.StartsWith(TEXT("CapacityBlocked")))
			{
				bWaitingOverlayChangeCapacity = true;
				OverlayChangeCapacityVersion = BeforeChange;
			}
			else
			{
				WorldState = EVoxelWorldState::Failed;
				UE_LOG(LogTemp, Error, TEXT("Voxel remote change rejected: %s"), *ChangeError);
			}
			break;
		}
		bOverlayChangeInvalidated = true;
		uint64 Revision = 0;
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Overlay;
		if (OverlayReplica->LookupSection(Key, Revision, Overlay))
		{
			const uint64 BeforeEmerge = Scheduler->GetTerrainResources()->ReleaseVersionExcludingOwner(EmergeManager->GetControlConsumer());
			if (!EmergeManager->SetRemoteChangeState(Key,
				Overlay ? EVoxelSectionChangeState::Modified : EVoxelSectionChangeState::Natural, ChangeError))
			{
				if (ChangeError.StartsWith(TEXT("CapacityBlocked")))
				{
					bWaitingOverlayChangeCapacity = true;
					bOverlayChangeWaitingEmerge = true;
					OverlayChangeCapacityVersion = BeforeEmerge;
				}
				else
				{
					WorldState = EVoxelWorldState::Failed;
					UE_LOG(LogTemp, Error, TEXT("Voxel remote state rejected: %s"), *ChangeError);
				}
				break;
			}
		}
		bWaitingOverlayChangeCapacity = false;
		bOverlayChangeWaitingEmerge = false;
		bOverlayChangeInvalidated = false;
		++OverlayChangeCursor;
		FVoxelEditBatch Batch;
		auto& Patch = Batch.Sections.AddDefaulted_GetRef();
		Patch.Section = Key;
		Patch.ToRevision = Revision;
		PublishProjectEdit(Batch);
		bInterestDirty = true;
	}

	if (GenerationCache)
	{
		GenerationCache->TickMaintenance(256);
	}

	const double InterestStart = FPlatformTime::Seconds();
	const double Now = InterestStart;
	const auto RenderCoverage = ViewManager ? ViewManager->CapturePresentedSnapshot() : nullptr;
	const auto PhysicsCoverage = CollisionPresenter ? CollisionPresenter->CaptureSnapshot() : nullptr;
	if ((RenderCoverage ? RenderCoverage->GetSerial() : 0) != LastPlannedRenderCoverageSerial ||
		(PhysicsCoverage ? PhysicsCoverage->Serial : 0) != LastPlannedPhysicsCoverageSerial)
		bInterestDirty = true;

	if ((bInterestDirty || (InterestRuntime && InterestRuntime->HasPlanningRetry())) &&
		(LastInterestRefresh < 0.0 ||
			Now - LastInterestRefresh >= 0.1))
	{
		RefreshInterest(Now);
	}

	const double AfterInterest = FPlatformTime::Seconds();
	// 先接纳新需求，避免旧区域工作耗尽预算后，新出生点无法接管。
	if (ViewManager)
	{
		ViewManager->AdvanceInterestInputs(InterestRevision, CollectLocalViewObservers());
	}
	const double AfterInterestInputs = FPlatformTime::Seconds();
	double ViewMilliseconds = 0.0;
	const auto TickView = [this, &ViewMilliseconds]()
	{
		if (!ViewManager) return true;
		const double ViewStart = FPlatformTime::Seconds();
		ViewManager->Tick(InterestRevision, CollectLocalViewObservers());
		ViewMilliseconds += (FPlatformTime::Seconds() - ViewStart) * 1000.0;
		const auto Admission = ViewManager->GetLastServiceResourceAdmission();
		if (Admission != EVoxelTerrainResourceAdmission::CapacityUnachievable &&
			Admission != EVoxelTerrainResourceAdmission::InvalidRequest) return true;
		WorldState = EVoxelWorldState::Failed;
		UE_LOG(LogTemp, Error, TEXT("Voxel service admission failed: %s"),
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable") : TEXT("InvalidRequest"));
		UpdateReadiness();
		TerrainFrameContext.End(FPlatformTime::Seconds());
		return false;
	};
	// 数据准入和可见发布轮流先执行，单次数据操作超时不能持续饿死已完成网格的提交。
	const bool bViewFirst = (GFrameCounter & 1) == 0;
	if (bViewFirst && !TickView()) return;
	const double EmergeStart = FPlatformTime::Seconds();

	EmergeManager->Tick(*CurrentInterest, CurrentInterest->DemandRevision, Now,
		ViewSettings.Budget.DataAdmissionPerFrame, ViewSettings.Budget.AdmissionMilliseconds, &TerrainFrameContext);
	const auto EmergeAdmission = EmergeManager->GetLastResourceAdmission();
	if (EmergeAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ||
		EmergeAdmission == EVoxelTerrainResourceAdmission::InvalidRequest)
	{
		WorldState = EVoxelWorldState::Failed;
		UE_LOG(LogTemp, Error, TEXT("Voxel Emerge admission failed: %s"),
			EmergeAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable") : TEXT("InvalidRequest"));
		UpdateReadiness();
		TerrainFrameContext.End(FPlatformTime::Seconds());
		return;
	}

	const double AfterEmerge = FPlatformTime::Seconds();
	LastEmergeAdmissionMilliseconds = (AfterEmerge - EmergeStart) * 1000.0;

	ResidencyManager->Tick(CurrentInterest->Exact, InterestRevision, Now, &TerrainFrameContext);

	const double AfterResidency = FPlatformTime::Seconds();

	if (CollisionPresenter)
	{
		CollisionPresenter->Tick(CurrentInterest->Exact, InterestRevision);
	}
	if (PhysicsRetirement) PhysicsRetirement->Advance(TerrainFrameContext);
	ReconcileSimulationDemand();
	if (WorldState == EVoxelWorldState::Failed)
	{
		UpdateReadiness();
		TerrainFrameContext.End(FPlatformTime::Seconds());
		return;
	}

	const double AfterCollision = FPlatformTime::Seconds();
	if (NavigationPresenter)
	{
		NavigationPresenter->Tick();
	}
	const double AfterNavigation = FPlatformTime::Seconds();

	if (!bViewFirst && !TickView()) return;

	const double AfterView = FPlatformTime::Seconds();

	if (DetailView)
	{
		DetailView->Tick(
			CollectDetailObservers());
	}

	AdvanceSceneRegions();

	const double AfterScenes = FPlatformTime::Seconds();

	AdvancePlayerClaims();
	UpdateReadiness();

#if !UE_BUILD_SHIPPING
	static double LastSlowTickLog = 0.0;
	if (FPlatformTime::Seconds() - VoxelTickStart > 0.008 && Now - LastSlowTickLog > 2.0)
	{
		LastSlowTickLog = Now;
		UE_LOG(LogTemp, Display, TEXT("Voxel slow tick ms: scheduler=%.2f streaming=%.2f interest=%.2f interestInputs=%.2f emerge=%.2f residency=%.2f collision=%.2f navigation=%.2f view=%.2f scenes=%.2f readiness=%.2f"),
			(AfterScheduler - VoxelTickStart) * 1000, (InterestStart - AfterScheduler) * 1000,
			(AfterInterest - InterestStart) * 1000, (AfterInterestInputs - AfterInterest) * 1000,
			LastEmergeAdmissionMilliseconds, (AfterResidency - AfterEmerge) * 1000,
			(AfterCollision - AfterResidency) * 1000, (AfterNavigation - AfterCollision) * 1000, ViewMilliseconds,
			(AfterScenes - AfterView) * 1000, (FPlatformTime::Seconds() - AfterScenes) * 1000);
	}
	if (DiagnosticFrameTimes.Num() < 4096)
	{
		DiagnosticFrameTimes.Add(InDeltaSeconds * 1000.0);
		DiagnosticModuleTimes.Add((FPlatformTime::Seconds() - VoxelTickStart) * 1000.0);
	}
	if (Now - LastDiagnosticsLog >= 10.0)
	{
		LastDiagnosticsLog = Now;
		if (!DiagnosticFrameTimes.IsEmpty())
		{
			DiagnosticFrameTimes.Sort();
			DiagnosticModuleTimes.Sort();
			auto Percentile = [](const TArray<double>& Samples, const double Quantile)
			{
				return Samples[FMath::Clamp(FMath::CeilToInt(Quantile * Samples.Num()) - 1, 0, Samples.Num() - 1)];
			};
			UE_LOG(LogTemp, Display,
				TEXT("Voxel frame timing samples=%d frameMs p50=%.2f p95=%.2f p99=%.2f max=%.2f voxelGT p50=%.2f p95=%.2f p99=%.2f max=%.2f"),
				DiagnosticFrameTimes.Num(), Percentile(DiagnosticFrameTimes, 0.5), Percentile(DiagnosticFrameTimes, 0.95),
				Percentile(DiagnosticFrameTimes, 0.99), DiagnosticFrameTimes.Last(), Percentile(DiagnosticModuleTimes, 0.5),
				Percentile(DiagnosticModuleTimes, 0.95), Percentile(DiagnosticModuleTimes, 0.99), DiagnosticModuleTimes.Last());
			DiagnosticFrameTimes.Reset();
			DiagnosticModuleTimes.Reset();
		}

		if (GenerationCache)
		{
			const FVoxelGenerationCacheStats CacheStats = GenerationCache->GetStats();
			UE_LOG(LogTemp, Display,
				TEXT("Voxel cache: base=%d natural=%d hydro=%d cave=%d ecology=%d feature=%d structure=%d memory=%.2fMiB waits=%llu waitMs=%.2f"),
				CacheStats.BaseColumns,
				CacheStats.NaturalColumns,
				CacheStats.Hydrology,
				CacheStats.Caves,
				CacheStats.Ecology,
				CacheStats.Features,
				CacheStats.Structures,
				static_cast<double>(CacheStats.AllocatedBytes) / (1024.0 * 1024.0),
				CacheStats.GateWaitCount,
				static_cast<double>(CacheStats.GateWaitMicroseconds) / 1000.0);
		}

		for (const TPair<FGuid, FSource>& Pair : Sources)
		{
			const UObject* Owner = Pair.Value.Owner.Get();
			const FVoxelStreamingSource& Source = Pair.Value.Value;

			UE_LOG(
				LogTemp,
				Display,
				TEXT("Voxel source id=%s owner=%s center=(%d,%d,%d) render=%d exact=%d collision=%d"),
				*Pair.Key.ToString(EGuidFormats::DigitsWithHyphens),
				Owner ? *Owner->GetName() : TEXT("<invalid>"),
				Source.Center.X,
				Source.Center.Y,
				Source.Center.Z,
				static_cast<int32>(Source.Capabilities),
				Source.ExactRadius,
				Source.CollisionRadius);
		}
	}
#endif
	TerrainFrameContext.End(FPlatformTime::Seconds());
	// 控制消息可替换 Runtime/Scheduler；必须在所有旧帧使用者退出后处理。
	if (auto* Network = PendingTerrainWorldControl.Get())
	{
		PendingTerrainWorldControl.Reset();
		Network->AdvanceDeferredWorldControl();
	}
}

void UVoxelModule::OnTermination(const EPhase InPhase)
{
	FString Error;
	StopWorld(true, Error);
	Super::OnTermination(InPhase);
}

bool UVoxelModule::CreateWorldFromProfile(const int32 InSeed, FString& OutError)
{
	if (!WorldGenerationProfile)
	{
		OutError = TEXT("No baked world generation profile");
		return false;
	}
	FVoxelGenerationSettings Settings = WorldGenerationProfile->Defaults;
	Settings.Seed = InSeed;
	return CreateWorld(Settings, WorldGenerationProfile->TargetCellCentimeters, OutError);
}

bool UVoxelModule::CreateWorld(
	const FVoxelGenerationSettings& InSettings,
	const int32 InBlockSizeCentimeters,
	FString& OutError)
{
	if (!IsAuthority() ||
		!Registry.GetSnapshot() ||
		!WorldGenerationProfile ||
		Runtime)
	{
		OutError =
			TEXT("Creating a voxel world requires an idle authoritative initialized module");

		return false;
	}

	FVoxelGenerationRuntimeConfig Config;

	if (!FVoxelGenerationBinding::Build(
		*WorldGenerationProfile,
		*Registry.GetSnapshot(),
		InSettings,
		InBlockSizeCentimeters,
		Config,
		OutError))
	{
		return false;
	}

	if (!Config.Recipe)
	{
		OutError =
			TEXT("Voxel generation binding has no frozen recipe");

		return false;
	}

	FVoxelWorldManifest NewManifest;
	NewManifest.WorldId =
		FGuid::NewGuid();

	NewManifest.Settings =
		InSettings;

	NewManifest.BlockSizeCentimeters =
		InBlockSizeCentimeters;

	NewManifest.RegistryHash =
		Registry.GetSnapshot()->Hash;

	NewManifest.RecipeHash =
		Config.Recipe->RecipeHash;

	NewManifest.BaseSampleHash =
		BuildGenerationSignature(
			NewManifest,
			GetGenerationIdentitySalt());

	if (NewManifest.BaseSampleHash == 0)
	{
		OutError =
			TEXT("Voxel generation signature could not be built");

		return false;
	}

	RegionStore.Reset();

	return StartWorld(
		NewManifest,
		false,
		OutError);
}

bool UVoxelModule::StartWorld(
	const FVoxelWorldManifest& InManifest,
	const bool bInRemote,
	FString& OutError)
{
	if (Runtime || TerrainFrameContext.IsActive())
	{
		OutError = TEXT("Voxel world is already running");
		return false;
	}
	if (!GetWorld() || !Registry.GetSnapshot() || !Shapes || !WorldGenerationProfile ||
		((GetWorld()->GetNetMode() == NM_Client) != bInRemote))
	{
		OutError = TEXT("Voxel world start prerequisites or network mode are invalid");
		return false;
	}

	WorldState = EVoxelWorldState::Initializing;
	Manifest = InManifest;
	const auto FailInitialization = [this]()
	{
		if (PendingTerrainFactResources && PendingTerrainFactControl.WorldEpoch > Epoch)
			Epoch = PendingTerrainFactControl.WorldEpoch;
		for (const auto& Component : TerrainNetworkComponents)
		{
			if (auto* Network = Component.Get()) Network->OnTerrainWorldStopping();
		}
		PendingTerrainWorldControl.Reset();
		Scheduler.Reset();
		Generator.Reset();
		GenerationCache.Reset();
		GenerationConfig.Reset();
		TerrainFrameContext.Reset();
		PendingTerrainFactResources.Reset();
		PendingTerrainFactControl = {};
		WorldState = EVoxelWorldState::Failed;
	};
	if (Manifest.RegistryHash != Registry.GetSnapshot()->Hash)
	{
		OutError = TEXT("Voxel registry hash mismatch");
		FailInitialization();
		return false;
	}
	FVoxelGenerationRuntimeConfig Config;
	if (!FVoxelGenerationBinding::Build(
		*WorldGenerationProfile,
		*Registry.GetSnapshot(),
		Manifest.Settings,
		Manifest.BlockSizeCentimeters,
		Config,
		OutError) ||
		!Config.Recipe ||
		Config.Recipe->RecipeHash != Manifest.RecipeHash)
	{
		FailInitialization();
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Voxel frozen recipe hash mismatch");
		}
		return false;
	}

	if ((Config.Recipe->Settings.Ecology.Tree.bEnabled &&
			(Config.Recipe->Ecology.TreeTrunk == MAX_uint16 ||
			 Config.Recipe->Ecology.TreeLeaves == MAX_uint16)) ||
		(Config.Recipe->Settings.Ecology.Grass.bEnabled &&
		 Config.Recipe->Ecology.GrassPlant == MAX_uint16))
	{
		OutError = TEXT("Enabled voxel ecology symbols are not bound");
		FailInitialization();
		return false;
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Voxel ecology runtime: treeEnabled=%d trunk=%u leaves=%u grassEnabled=%d grass=%u"),
		Config.Recipe->Settings.Ecology.Tree.bEnabled ? 1 : 0,
		Config.Recipe->Ecology.TreeTrunk,
		Config.Recipe->Ecology.TreeLeaves,
		Config.Recipe->Settings.Ecology.Grass.bEnabled ? 1 : 0,
		Config.Recipe->Ecology.GrassPlant);
#endif

	ViewSettings = {};
	UVoxelViewProfile* ViewProfile = WorldGenerationProfile->View.LoadSynchronous();
	if (!ViewProfile)
	{
		FailInitialization();
		OutError = TEXT("世界生成资产必须绑定地形ViewProfile");
		return false;
	}
	{
		ViewSettings.DefaultSourceView = ViewProfile->DefaultSourceView;
		ViewSettings.Fine = ViewProfile->Fine;
		ViewSettings.FrameBudget = ViewProfile->FrameBudget;
		ViewSettings.Lod = ViewProfile->Lod;
		ViewSettings.Readiness = ViewProfile->Readiness;
		ViewSettings.Planning = ViewProfile->Planning;
		ViewSettings.Budget = ViewProfile->Budget;
		ViewSettings.MaximumTextureStretchCells = ViewProfile->MaximumTextureStretchCells;
		if (!ViewSettings.Fine.Validate(OutError) || !VoxelViewLod::ValidateTopology(ViewSettings.Lod, OutError) ||
			!TerrainFrameContext.Configure(ViewSettings.FrameBudget, OutError) ||
			!VoxelViewLod::ValidatePerformanceBudget(ViewSettings.Budget, OutError) ||
			!FMath::IsFinite(ViewSettings.MaximumTextureStretchCells) ||
			ViewSettings.MaximumTextureStretchCells < 1.0 || ViewSettings.MaximumTextureStretchCells > 16.0)
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("地形纹理重复范围无效");
			}
			FailInitialization();
			return false;
		}

	}
	const uint64 ExpectedGenerationSignature =
		BuildGenerationSignature(
			Manifest,
			GetGenerationIdentitySalt());
	if (ExpectedGenerationSignature == 0 ||
		Manifest.BaseSampleHash != ExpectedGenerationSignature ||
		Epoch == MAX_uint64)
	{
		FailInitialization();
		OutError = TEXT("Voxel generation signature mismatch");
		return false;
	}

	if (IsAuthority() && !RegionStore.GetDirectoryError().IsEmpty())
	{
		OutError = RegionStore.GetDirectoryError();
		FailInitialization();
		return false;
	}
	const uint64 NewWorldEpoch = Epoch + 1;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> RuntimeResources;
	FVoxelGenerationConsumerKey SchedulerControlConsumer;
	if (!EnsureTerrainFactBootstrap(RuntimeResources, SchedulerControlConsumer, OutError))
	{
		FailInitialization();
		return false;
	}
	FVoxelTaskBudget TaskBudget;
	TaskBudget.MaxConcurrentTasks = FMath::Clamp(FPlatformMisc::NumberOfCores() * 2, 1, 16);
	TaskBudget.CriticalReservedTasks = ViewSettings.Budget.CriticalReservedTasks;
	TaskBudget.MaxConcurrentSurfaceTasks = ViewSettings.Budget.MaxConcurrentSurfaceTasks;
	TaskBudget.MaxConcurrentMacroTasks = ViewSettings.Budget.MaxConcurrentMacroTasks;
	TaskBudget.MaxConcurrentCoarseTerrainTasks = ViewSettings.Budget.MaxConcurrentCoarseTerrainTasks;
	TaskBudget.MaxReservedBytes = ViewSettings.Fine.GeometryLimits().WorkingBytes;
	// Resumable pages retain their output while queued between shards. Their exact
	// worker estimate participates in the same bounded input admission as other tasks.
	TaskBudget.MaxInputBytes = ViewSettings.Fine.GeometryLimits().InputBytes;
	TaskBudget.MaxCompletedResultsPerFrame = ViewSettings.Budget.CompletedResultsPerFrame;
	TaskBudget.MaxHeavyCompletedResultsPerFrame = FMath::Min(ViewSettings.Budget.HeavyResultsPerFrame, ViewSettings.Budget.CompletedResultsPerFrame);
	TaskBudget.MaxFineApplyPerFrame = ViewSettings.Budget.FineApplyPerFrame;
	TaskBudget.MaxVoxelLODApplyPerFrame = ViewSettings.Budget.VoxelProxyApplyPerFrame;
	TaskBudget.MaxSurfaceApplyPerFrame = ViewSettings.Budget.SurfaceApplyPerFrame;
	TaskBudget.MaxMacroApplyPerFrame = ViewSettings.Budget.MacroApplyPerFrame;
	const auto& MemoryPolicy = ViewSettings.Budget;
	const FVoxelTerrainResourceLimits GeometryLimits = ViewSettings.Fine.GeometryLimits();
	if (MemoryPolicy.MaximumRuntimeFactsInputMegabytes < 16 || MemoryPolicy.MaximumRuntimeFactsWorkingMegabytes < 32 ||
		MemoryPolicy.MaximumRuntimeFactsStagedMegabytes < 16 || MemoryPolicy.MaximumRuntimeFactsResidentMegabytes < 128 ||
		MemoryPolicy.RuntimeFactsOverlapMegabytes < 16)
	{
		OutError = TEXT("ViewProfile 的几何或 Runtime/Facts 独立资源池上限不合法");
		FailInitialization();
		return false;
	}
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> TerrainResources;
	if (!FVoxelTerrainResources::Create(GeometryLimits, SchedulerControlConsumer, TerrainResources, OutError) ||
		!FVoxelTaskScheduler::Create(TerrainResources.ToSharedRef(), RuntimeResources.ToSharedRef(),
			SchedulerControlConsumer, Scheduler, OutError))
	{
		FailInitialization();
		return false;
	}
	Scheduler->SetBudget(TaskBudget);
	if (!Scheduler->InitializeStatistics(NewWorldEpoch))
	{
		OutError = TEXT("地形统计固定窗口容量无法预授予");
		FailInitialization();
		return false;
	}
	const FVoxelGenerationConsumerKey CacheControlConsumer = {
		EVoxelGenerationConsumerKind::WorldFact, NewWorldEpoch, FGuid::NewGuid(), 1,
		EVoxelGenerationSpatialKind::Column, FIntVector::ZeroValue, FIntVector::ZeroValue, 1
	};
	if (!FVoxelGenerationPlanCache::Create(Scheduler->GetRuntimeResources(),
		CacheControlConsumer, GenerationCache, OutError))
	{
		FailInitialization();
		return false;
	}
	if (!FVoxelGenerationBinding::Freeze(Config, Scheduler->GetRuntimeResources(), CacheControlConsumer,
		GenerationConfig, OutError, GenerationCache->GetRootInputBytes(), 1))
	{
		FailInitialization();
		return false;
	}
	Config = FVoxelGenerationRuntimeConfig{};
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> NaturalPipeline;
	if (!FVoxelGenerationPipeline::Create(GenerationConfig.ToSharedRef(), GenerationCache.ToSharedRef(),
		NaturalPipeline, OutError, nullptr, true))
	{
		FailInitialization();
		return false;
	}
	const TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> NaturalGenerator = NaturalPipeline.ToSharedRef();
	TSharedPtr<const IVoxelGenerationOverlay, ESPMode::ThreadSafe> Overlay;
	if (!CreateGenerationOverlay(NaturalGenerator, Registry.GetSnapshot().ToSharedRef(),
		Overlay, OutError))
	{
		FailInitialization();
		return false;
	}
	if (Overlay)
	{
		FVoxelGenerationRequiredSourceScope NaturalSources(&Scheduler->GetRuntimeResources().Get(), NaturalPipeline.Get(),
			[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
			{
				static_cast<const FVoxelGenerationPipeline*>(InContext)->VisitRequiredSources(InVisit);
			});
		if (!FVoxelGenerationPipeline::Create(GenerationConfig.ToSharedRef(), GenerationCache.ToSharedRef(),
			Generator, OutError, Overlay, true))
		{
			FailInitialization();
			return false;
		}
	}
	else
	{
		Generator = NaturalGenerator;
	}

	Epoch = NewWorldEpoch;
	PendingTerrainFactResources.Reset();
	PendingTerrainFactControl = {};
	Runtime = MakeUnique<FVoxelWorldRuntime>(
		Epoch,
		IsAuthority(),
		Registry.GetSnapshot().ToSharedRef(),
		Generator.ToSharedRef(), Scheduler->GetRuntimeResources());
	GenerationConfig->InputLease->MarkPublished();
	GenerationConfig->RecipeInputLease->MarkPublished();
	GenerationCache->MarkPublished();
	NaturalGenerator->MarkPublished();
	Generator->MarkPublished();
	Runtime->GetChangeHierarchy().SetTopology(ViewSettings.Lod);
	if (GenerationConfig->Recipe->Settings.Ecology.Tree.bEnabled)
	{
		const FVoxelTreeGenerationSettings& Tree =
			GenerationConfig->Recipe->Settings.Ecology.Tree;
		Runtime->GetChangeHierarchy().SetNaturalInfluence(
			Tree.CrownRadius, Tree.MaxHeight + Tree.CrownRadius);
	}
	if (IsAuthority() && RegionStore.IsDirectoryReady()) Runtime->GetChangeIndex().SetBaseline(RegionStore.GetDirectorySnapshot());
	if (!IsAuthority()) OverlayReplica = MakeUnique<FVoxelOverlayReplica>(Epoch, Scheduler->GetRuntimeResources());
	Scheduler->BindFrameContext(TerrainFrameContext);
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		MapTileCache = MakeUnique<FVoxelMapTileCache>(
			*Scheduler, Generator.ToSharedRef(), GenerationConfig.ToSharedRef(),
			Epoch, BlockSize());
	}
	UE_LOG(LogTemp, Display, TEXT("Voxel worker budget: cores=%d workers=%d reservedMiB=%llu"),
		FPlatformMisc::NumberOfCores(), TaskBudget.MaxConcurrentTasks, TaskBudget.MaxReservedBytes / (1024ull * 1024ull));
	UE_LOG(LogTemp, Display, TEXT("Voxel frame budget: Fine=%d Proxy=%d Surface=%d Macro=%d Data=%d Results=%d Heavy=%d AdmissionMs=%.2f"),
		ViewSettings.Budget.FineAdmissionPerFrame, ViewSettings.Budget.VoxelProxyAdmissionPerFrame,
		ViewSettings.Budget.SurfaceAdmissionPerFrame, ViewSettings.Budget.MacroAdmissionPerFrame, ViewSettings.Budget.DataAdmissionPerFrame,
		TaskBudget.MaxCompletedResultsPerFrame, TaskBudget.MaxHeavyCompletedResultsPerFrame, ViewSettings.Budget.AdmissionMilliseconds);
	InterestRuntime = MakeShared<FVoxelInterestRuntime, ESPMode::ThreadSafe>();
	DesiredInterestRuntime = InterestRuntime;
	EmergeManager = MakeUnique<FVoxelEmergeManager>(
		*Runtime,
		*Scheduler,
		Generator.ToSharedRef(),
		GenerationConfig.ToSharedRef(),
		GenerationCache.ToSharedRef(),
		RegionStore,
		Manifest,
		Registry.GetSnapshot().ToSharedRef());
	if (OverlayReplica)
	{
		OverlayReplica->SetTaskBindingDomain(Scheduler->GetTerrainResources(), Scheduler->GetControlConsumer());
		OverlayReplica->SetTaskQueue([this](FVoxelTaskRequest&& InTask)
		{
			return Scheduler && Scheduler->Enqueue(MoveTemp(InTask));
		});
		OverlayReplica->SetTaskCanceler([this](const FVoxelTaskStamp& InStamp) { CancelTerrainNetworkTask(InStamp); });
		OverlayReplica->OnCommitted = [this](TSharedPtr<const FVoxelOverlayCommitNotification, ESPMode::ThreadSafe> InChange)
		{
			OverlayChangeQueue.Enqueue(MoveTemp(InChange));
			bInterestDirty = true;
		};
		EmergeManager->SetRemoteOverlayResolver([this](const FIntVector& Key, uint64& Revision,
			TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>& Overlay)
		{
			return OverlayReplica && OverlayReplica->LookupSection(Key, Revision, Overlay);
		});
		Runtime->SetRemoteSectionValidator([this](const FIntVector& Key, const uint64 Revision)
		{
			uint64 CurrentRevision = 0;
			TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Overlay;
			return OverlayReplica && OverlayReplica->LookupSection(Key, CurrentRevision, Overlay) && CurrentRevision == Revision;
		});
	}
	ResidencyManager = MakeUnique<FVoxelResidencyManager>(
		*Runtime,
		*Scheduler);
	if (!ResidencyManager->Initialize(
		[this](const FIntVector& InSection)
		{
			OnSectionEvicting(InSection);
		},
		[this](const FIntVector& InSection)
		{
			return ViewManager && ViewManager->RequiresSectionData(InSection);
		},
		[this](const FIntVector& InSection, const FVoxelSectionStamp& InStamp, const EVoxelRuntimeSectionEvent InEvent)
		{
			OnSimulationSectionEvent(InSection, InStamp, InEvent);
		}, OutError))
	{
		FString StopError;
		StopWorld(true, StopError);
		WorldState = EVoxelWorldState::Failed;
		return false;
	}
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		ViewManager = MakeUnique<FVoxelViewManager>(*this, *Scheduler, Epoch);
		EmergeManager->SetExternalConsumerWakeCallback([this](const FVoxelGenerationConsumerKey& Consumer)
		{
			if (ViewManager) ViewManager->OnGenerationDependencyReady(Consumer);
		});
	}
	// 服务端同样需要权威碰撞，碰撞发布不依赖视觉地形模块。
	CollisionPresenter = MakeUnique<FVoxelCollisionPresenter>(*this, *Scheduler, Epoch);
	if (IsAuthority() && bEnableProjectSceneSimulation)
	{
		NavigationPresenter = MakeUnique<FVoxelNavigationPresenter>(*this, *Scheduler);
	}
	DetailView = MakeUnique<FVoxelDetailView>(*this, *Scheduler, Epoch);
	if (!DetailView->Initialize(OutError))
	{
		FString StopError;
		StopWorld(true, StopError);
		WorldState = EVoxelWorldState::Failed;
		return false;
	}

	SessionId = IsAuthority() ? FGuid::NewGuid() : FGuid();
	Sources.Reset();
	CurrentInterest = MakeShared<FVoxelInterestSet, ESPMode::ThreadSafe>();
	DesiredInterest = CurrentInterest;
	InterestRevision = 0;
	LastPlannedRenderCoverageSerial = LastPlannedPhysicsCoverageSerial = 0;
	LastInterestRefresh = -1.0;
	bInterestBuildPending = false;
	LastDiagnosticsLog = -1.0;
	DiagnosticFrameTimes.Reset();
	DiagnosticModuleTimes.Reset();
	bInterestDirty = true;
	ReadyStage = EVoxelWorldReadyStage::AssetsValidated;
	ReadinessSnapshot = {};
	bWorldLoadRejected = false;
	WorldState = EVoxelWorldState::Running;
	if (!WorldData)
	{
		WorldData = NewWorldData();
	}
	WorldData->GenerationProfile =
		WorldGenerationProfileAsset;
	WorldData->Seed =
		Manifest.Settings.Seed;
	FVoxelManifestCodec::Encode(Manifest, WorldData->ManifestBytes);
	// 远端客户端没有 GameMode 的模块启动阶段；握手建立运行时后启用自身刷新。
	if (bInRemote && GetModuleState() == EModuleState::None && IsModuleAutoRun()) Run();
	OnWorldInitialized.Broadcast();
	OutError.Reset();
	return true;
}

void UVoxelModule::AddReferencedObjects(UObject* InThis, FReferenceCollector& InCollector)
{
	auto* Module = CastChecked<UVoxelModule>(InThis);
	if (Module->SceneSaveBuilder.IsSet())
		InCollector.AddPropertyReferences(FParameter::StaticStruct(), &Module->SceneSaveBuilder->Column.ProjectData, Module);
	Super::AddReferencedObjects(InThis, InCollector);
}

uint64 UVoxelModule::GetSceneProjectCaptureRevision() const
{
	return 0;
}

void UVoxelModule::OnWorldStopping()
{
}

void UVoxelModule::OnSimulationResidencyChanged(const FVoxelSimulationResidencyDelta& InDelta)
{
}

bool UVoxelModule::StopWorld(const bool bDiscardDirty, FString& OutError)
{
	if (bSceneTransitionActive)
	{
		OutError = TEXT("Voxel world control must wait for the active scene transaction to finish");
		return false;
	}
	if (TerrainFrameContext.IsActive())
	{
		OutError = TEXT("Voxel world control must wait for the active terrain frame to finish");
		return false;
	}
	if (!Runtime)
	{
		if (PendingTerrainFactResources)
		{
			WorldState = EVoxelWorldState::Closing;
			Epoch = PendingTerrainFactControl.WorldEpoch;
			for (const auto& Component : TerrainNetworkComponents)
			{
				if (auto* Network = Component.Get()) Network->OnTerrainWorldStopping();
			}
			PendingTerrainWorldControl.Reset();
			PendingTerrainFactResources.Reset();
			PendingTerrainFactControl = {};
		}
		WorldState = EVoxelWorldState::None;
		OutError.Reset();
		return true;
	}
	if (SaveAdapter.IsBusy() || SceneSaveBuilder.IsSet())
	{
		OutError = TEXT("Finish the SaveGame transaction before closing the voxel world");
		return false;
	}
	if (!bDiscardDirty && IsAuthority() && Runtime->HasUnpersistedChanges())
	{
		OutError = TEXT("World contains unpersisted voxel journal versions");
		return false;
	}
	if (!bDiscardDirty && IsAuthority() && HasUnpersistedSceneState())
	{
		OutError = TEXT("World contains scene owners awaiting an actual committed SaveGame cut");
		return false;
	}
	if (NavigationPresenter && !NavigationPresenter->TryCloseForWorldStop())
	{
		OutError = TEXT("Voxel world control must wait for actual navigation readers to release their inputs");
		return false;
	}

	TGuardValue<bool> SceneTransition(bSceneTransitionActive, true);
	WorldState = EVoxelWorldState::Closing;
	check(Epoch < MAX_uint64);
	++Epoch;
	PendingTerrainWorldControl.Reset();
	if (Scheduler)
	{
		Scheduler->StopAndJoin();
	}
	for (const auto& Component : TerrainNetworkComponents)
	{
		if (auto* Network = Component.Get()) Network->OnTerrainWorldStopping();
	}
	OnWorldStopping();
	MapTileCache.Reset();
	if (ViewManager)
	{
		ViewManager->Reset();
	}
	if (CollisionPresenter)
	{
		CollisionPresenter->Reset();
	}
	ViewManager.Reset();
	NavigationPresenter.Reset();
	CollisionPresenter.Reset();
	DetailView.Reset();
	if (PhysicsRetirement) PhysicsRetirement->Close();
	PhysicsRetirement.Reset();
	for (const TPair<FIntVector, TObjectPtr<UVoxelSceneRegion>>& Pair : SceneRegions)
	{
		if (Pair.Value)
		{
			while (Pair.Value->SimulationFailureHead)
			{
				ReleaseSimulationSectionFromRegion(*Pair.Value->SimulationFailureHead);
			}
			while (Pair.Value->SimulationSaveHead)
			{
				ReleaseSimulationSectionFromSave(*Pair.Value->SimulationSaveHead);
			}
			Pair.Value->Shutdown();
		}
	}
	SceneRegionHead = SceneRegionTail = SceneRegionCursor = nullptr;
	SimulationReadyHead = SimulationReadyTail = nullptr;
	SimulationDemandEvents.Reset();
	SimulationDemandEventsTail = nullptr;
	SimulationDemandCursor = 0;
	bReadingRemovedSimulation = false;
	// Empty 先释放全部 backing，再归还控制表租约。
	SceneRegions.Empty();
	check(SceneLoadingCount == 0);
	SimulationSections.Empty();
	SimulationStorageLease.Reset();
	SimulationSectionCapacity = SceneRegionCapacity = 0;
	SimulationResidencyRevision = 0;
	CapturedSceneSave.Reset();
	LatestSceneSaveAttempt.Reset();
	DurableSceneSaveReceipt.Reset();
	PendingCommitDirectory.Empty();
	bSceneSaveRequested = false;
	bLatestSceneSaveSucceeded = false;
	check(SimulationSaveWaitCount == 0);
	EmergeManager.Reset();
	ResidencyManager.Reset();
	InterestRuntime.Reset();
	DesiredInterestRuntime.Reset();
	DesiredInterestTaskPayload.Reset();
	OverlayReplica.Reset();
	ApplyingOverlayChange.Reset();
	OverlayChangeQueue.Empty();
	OverlayChangeCursor = 0;
	bWaitingOverlayChangeCapacity = false;
	bOverlayChangeInvalidated = false;
	bOverlayChangeWaitingEmerge = false;
	OverlayChangeCapacityVersion = 0;
	if(InventoryBridge) InventoryBridge->DetachRuntime();
	InventoryBridge.Reset();
	InventoryContribution = nullptr;
	Runtime.Reset();
	Generator.Reset();

	/**
	 * Generation caches can contain hundreds of thousands of immutable entries.
	 * Their destruction is independent after all scheduler work has joined, so
	 * release the memory off the game thread instead of blocking PIE teardown.
	 */
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> RetiredGenerationCache =
		MoveTemp(GenerationCache);
	if (RetiredGenerationCache)
	{
		AsyncTask(
			ENamedThreads::AnyBackgroundThreadNormalTask,
			[RetiredGenerationCache = MoveTemp(RetiredGenerationCache)]() mutable
			{
				RetiredGenerationCache.Reset();
			});
	}
	GenerationConfig.Reset();
	Scheduler.Reset();
	PendingTerrainFactResources.Reset();
	PendingTerrainFactControl = {};
	TerrainFrameContext.Reset();
	Sources.Reset();
	CurrentInterest = MakeShared<FVoxelInterestSet, ESPMode::ThreadSafe>();
	DesiredInterest = CurrentInterest;
	InterestRevision = 0;
	LastInterestRefresh = -1.0;
	bInterestBuildPending = false;
	LastDiagnosticsLog = -1.0;
	DiagnosticFrameTimes.Reset();
	DiagnosticModuleTimes.Reset();
	bInterestDirty = true;
	Breaking.Reset();
	SessionId.Invalidate();
	ReadyStage = EVoxelWorldReadyStage::None;
	ReadinessSnapshot = {};
	WorldState = EVoxelWorldState::None;
	OutError.Reset();
	return true;
}

bool UVoxelModule::IsAuthority() const
{
	return GetWorld() && GetWorld()->GetNetMode() != NM_Client;
}

bool UVoxelModule::IsReady() const
{
	return Runtime && WorldState == EVoxelWorldState::Running;
}

bool UVoxelModule::IsPlayable() const
{
	return ReadyStage == EVoxelWorldReadyStage::Playable;
}

bool UVoxelModule::IsCollisionReady(const FIntVector& InSection) const
{
	if (!IsAuthority())
	{
		const auto* Section = Runtime ? Runtime->FindSection(InSection) : nullptr;
		uint64 Revision = 0;
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Overlay;
		if (!OverlayReplica || !Section || Section->Status != EVoxelSectionStatus::DataReady ||
			!OverlayReplica->LookupSection(InSection, Revision, Overlay) || Section->CommittedRevision != Revision) return false;
	}
	return CollisionPresenter && CollisionPresenter->IsReady(InSection);
}

bool UVoxelModule::CaptureCollisionReceipt(const FIntVector& InSection, FVoxelPhysicsReceipt& OutReceipt) const
{
	return IsCollisionReady(InSection) && CollisionPresenter->CaptureCurrentReceipt(InSection, OutReceipt);
}

void UVoxelModule::NotifyCollisionChanged(const FIntVector& InSection)
{
	check(IsInGameThread());
	if (NavigationPresenter) NavigationPresenter->InvalidateSection(InSection);
	WakeSimulationSection(InSection);
}

bool UVoxelModule::IsNavigationReady(const FIntVector& InSection) const
{
	return NavigationPresenter && IsSimulationResident(InSection) &&
		(NavigationPresenter->IsReady(InSection) || NavigationPresenter->IsReady(InSection - FIntVector(0, 0, 1)));
}

bool UVoxelModule::IsSimulationResident(const FIntVector& InSection) const
{
	const auto* Section = Runtime ? Runtime->FindSection(InSection) : nullptr;
	return IsAuthority() && SimulationSections.Contains(InSection) && Section && !Section->SimulationSaveRegion;
}

bool UVoxelModule::IsNavigationInputCurrent(const FVoxelNavigationInputVersion& InVersion) const
{
	return NavigationPresenter && NavigationPresenter->IsInputCurrent(InVersion);
}

bool UVoxelModule::IsNavigationResident(const FIntVector& InSection) const
{
	return IsAuthority() && NavigationPresenter && NavigationPresenter->IsResident(InSection);
}

bool UVoxelModule::EnsureNavigationCollisionSection(const FIntVector& InSection)
{
	return IsAuthority() && CollisionPresenter && CollisionPresenter->EnsureNavigationDemand(InSection);
}

bool UVoxelModule::AcquireNavigationDataSection(const FIntVector& InSection, FVoxelRuntimeSectionPin& OutPin)
{
	check(IsInGameThread());
	if (!Runtime || !IsAuthority()) return false;
	if (EmergeManager) return EmergeManager->AcquireDataDemand(InSection, OutPin);
	// 手工装配的 Runtime 夹具也必须取得真实 pin；数据仍由夹具明确发布。
	auto* Section = Runtime->FindSection(InSection);
	if (!Section || !Runtime->TryPinSection(InSection, Section->GetResourceIdentity())) return false;
	OutPin = {Section->GetResourceIdentity(), FGuid::NewGuid()};
	return true;
}

void UVoxelModule::ReleaseNavigationDataSection(const FIntVector& InSection, const FVoxelRuntimeSectionPin& InPin)
{
	check(IsInGameThread());
	if (EmergeManager)
	{
		verify(EmergeManager->ReleaseDataDemand(InSection, InPin));
	}
	else if (Runtime)
	{
		verify(Runtime->ReleaseSectionPin(InSection, InPin.Resource));
	}
}

TSet<FIntVector> UVoxelModule::GetSimulationSections() const
{
	TSet<FIntVector> Active;
	Active.Reserve(SimulationSections.Num());
	for (const auto& Key : SimulationSections) if (IsSimulationResident(Key)) Active.Add(Key);
	return Active;
}

uint64 UVoxelModule::GetSimulationResidencyRevision() const
{
	return SimulationResidencyRevision;
}

float UVoxelModule::GetWarmupProgress() const
{
	if (!IsReady())
	{
		return 0.0f;
	}
	auto Ratio = [](const int32 InReady, const int32 InRequired)
	{
		return InRequired > 0 ?
			FMath::Clamp(static_cast<float>(InReady) / InRequired, 0.0f, 1.0f) : 1.0f;
	};
	const float Recipe = ReadinessSnapshot.bAssetsValidated && ReadinessSnapshot.bRecipeFrozen ? 1.0f : 0.0f;
	const float SpawnPlan = ReadinessSnapshot.bSpawnPlanReady ? 1.0f : 0.0f;
	const float SpawnData = Ratio(
		ReadinessSnapshot.ReadySpawnSections,
		ReadinessSnapshot.RequiredSpawnSections);
	const float Collision = Ratio(
		ReadinessSnapshot.ReadyCollisionSections,
		ReadinessSnapshot.RequiredCollisionSections);
	const float PrimaryView = Ratio(
		ReadinessSnapshot.ReadyPrimaryRepresentations,
		ReadinessSnapshot.RequiredPrimaryRepresentations);
	return Recipe * 0.1f +
		SpawnPlan * 0.1f +
		SpawnData * 0.3f +
		Collision * 0.3f +
		PrimaryView * 0.2f;
}

const FVoxelWorldReadinessSnapshot& UVoxelModule::GetReadiness() const
{
	return ReadinessSnapshot;
}

double UVoxelModule::BlockSize() const
{
	return Manifest.BlockSizeCentimeters;
}

const FVoxelWorldManifest& UVoxelModule::GetManifest() const
{
	return Manifest;
}

FGuid UVoxelModule::GetSessionId() const
{
	return SessionId;
}

uint64 UVoxelModule::GetWorldEpoch() const
{
	return Epoch;
}

uint64 UVoxelModule::GetInterestRevision() const
{
	return InterestRevision;
}

bool UVoxelModule::HasInterestBuildFailed() const
{
	return !InterestBuildError.IsEmpty() && FailedInterestEpoch == Epoch && FailedInterestSourceRevision == SourceRevision;
}

const FString& UVoxelModule::GetInterestBuildError() const
{
	static const FString Empty;
	return HasInterestBuildFailed() ? InterestBuildError : Empty;
}

FVoxelWorldRuntime* UVoxelModule::GetRuntime()
{
	return Runtime.Get();
}

const FVoxelWorldRuntime* UVoxelModule::GetRuntime() const
{
	return Runtime.Get();
}

bool UVoxelModule::CaptureFineOverlays(const FVoxelGenerationBounds& InBounds,
	FVoxelFineOverlayCapture& OutCapture, FString& OutError, const FVoxelGenerationConsumerKey* InConsumer) const
{
	check(IsInGameThread());
	if (!Runtime || !GetRegistry() || !InBounds.IsValid())
	{
		OutError = TEXT("Overlay描述捕获缺少运行时、注册表或有效范围");
		return false;
	}
	if (!IsAuthority())
	{
		if (OverlayReplica && OverlayReplica->Capture(InBounds, Manifest, GetRegistry(), OutCapture, OutError)) return true;
		if (OutError.IsEmpty()) OutError = TEXT("WaitingOverlay: 客户端完整目录或一致切点尚未准入");
		return false;
	}
	const auto Resources = GetTerrainFactResources();
	if (!Resources) { OutError = TEXT("Overlay 捕获尚未绑定事实资源池"); return false; }
	int32 MaximumSections = Runtime->GetChangeIndex().Count(InBounds);
	Runtime->VisitCommittedOverlaySections(InBounds, [&MaximumSections](const FIntVector&)
	{
		check(MaximumSections < MAX_int32);
		++MaximumSections;
	});
	check(NextOverlayCaptureSerial < MAX_uint64);
	const auto Consumer = InConsumer && InConsumer->IsValid() ? *InConsumer :
		FVoxelGenerationConsumerKey::Section(Epoch, Manifest.WorldId, 1, InBounds.Min, ++NextOverlayCaptureSerial);
	FVoxelGenerationWorkspace EnumerationWorkspace(Resources, Consumer);
	TArray<FIntVector> Modified;
	if (!Runtime->GetChangeIndex().Enumerate(InBounds, Modified, EnumerationWorkspace, OutError) ||
		!EnumerationWorkspace.ReserveArray(Modified, MaximumSections, OutError)) return false;
	Runtime->GatherCommittedOverlaySections(InBounds, Modified);
	Modified.Sort([](const FIntVector& Left, const FIntVector& Right)
	{
		if (Left.X != Right.X) return Left.X < Right.X;
		if (Left.Y != Right.Y) return Left.Y < Right.Y;
		return Left.Z < Right.Z;
	});
	int32 PinnedCount = 0;
	for (const auto& Key : Modified) PinnedCount += Runtime->FindCommittedOverlay(Key).IsValid() ? 1 : 0;
	const int32 ReadCount = Modified.Num() - PinnedCount;
	const uint64 PathBytes = VoxelBoundedAllocation::ArrayBytes<TCHAR>(RegionStore.GetSourceDirectory().Len() + 1) * ReadCount;
	const uint64 MetadataBytes = VoxelBoundedAllocation::SharedBytes<FVoxelOverlaySnapshotSet>() +
		VoxelBoundedAllocation::MapBytes<FIntVector, TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>>(PinnedCount) +
		VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelOverlaySnapshot>(ReadCount) +
		VoxelBoundedAllocation::MapBytes<FIntVector, uint64>(Modified.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelRegionReadView>(ReadCount) + PathBytes +
		FVoxelRegionStore::OverlayReadLeaseBytes(PinnedCount, ReadCount, PathBytes, Modified.Num());
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease;
	const auto CaptureAdmission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
		MetadataBytes, CaptureLease, 0, EnumerationWorkspace.GetRequiredInputBytes(), EnumerationWorkspace.GetRequiredLeaseCount());
	if (CaptureAdmission != EVoxelTerrainResourceAdmission::Granted)
	{
		OutError = CaptureAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked ?
			TEXT("CapacityBlocked: Overlay 固定切点元数据容量未授予") :
			CaptureAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
			TEXT("Oversized: Overlay 固定切点元数据容量不可达") : TEXT("InvalidRequest: Overlay capture resource domain");
		return false;
	}
	FVoxelFineOverlayCapture Capture;
	Capture.WorldEpoch = Epoch;
	Capture.Bounds = InBounds;
	Capture.Manifest = Manifest;
	Capture.Registry = GetRegistry();
	Capture.Resources = Resources;
	Capture.Consumer = Consumer;
	Capture.Resident.ResourceLease = MoveTemp(CaptureLease);
	Capture.Resident.PinnedSections.Reserve(PinnedCount);
	Capture.ContentRevisions.Reserve(Modified.Num());
	Capture.Reads.Reserve(ReadCount);
	for (const auto& Key : Modified)
	{
		Capture.ContentRevisions.Add(Key, Runtime->GetChangeHierarchy().GetSectionRevision(Key));
		if (const auto Journal = Runtime->FindCommittedOverlay(Key)) Capture.Resident.PinnedSections.Add(Key, Journal);
		else
		{
			FVoxelRegionReadView Read;
			if (!RegionStore.CapturePinnedRead(Key, Runtime->GetCommittedOverlayRevision(Key), Read, OutError)) return false;
			Capture.Reads.Add(MoveTemp(Read));
		}
	}
	Capture.ReadLeaseSet = FVoxelRegionStore::CreateOverlayReadLeaseSet(Capture);
	OutCapture = MoveTemp(Capture);
	OutError.Reset();
	return true;
}
FVoxelTerrainFrameContext& UVoxelModule::GetTerrainFrameContext()
{
	return TerrainFrameContext;
}

FVoxelGenerationDependencyStatus UVoxelModule::EnsureGenerationFacts(const FVoxelGenerationConsumerKey& InConsumer,
	const FVoxelGenerationBounds& InBounds, const FVoxelTerrainServicePriority& InPriority)
{
	return EmergeManager ? EmergeManager->EnsureGenerationFacts(InConsumer, InBounds, InPriority) :
		FVoxelGenerationDependencyStatus{false, true, TEXT("地形事实Owner尚未初始化")};
}

void UVoxelModule::ReleaseGenerationFacts(const FVoxelGenerationConsumerKey& InConsumer)
{
	if (EmergeManager) EmergeManager->ReleaseGenerationFacts(InConsumer);
}

bool UVoxelModule::IsOverlayReadCurrent(const FVoxelFineOverlayReadResult& InResult) const
{
	check(IsInGameThread());
	if (!Runtime || InResult.WorldEpoch != Epoch || !InResult.Snapshot || !InResult.ReadLeaseSet ||
		InResult.ReadLeaseSet->GetWorldEpoch() != Epoch)
	{
		return false;
	}
	if (InResult.ReadLeaseSet->IsRemoteDirectory()) return OverlayReplica && OverlayReplica->IsCurrent(InResult);
	if (!RegionStore.IsDirectoryReady()) return false;
	if (InResult.ContentRevisions.Num() != InResult.Snapshot->Num()) return false;
	bool bCurrent = true;
	const auto CheckSection = [&](const FIntVector& Key)
	{
		const uint64* CapturedRevision = InResult.ContentRevisions.Find(Key);
		if (!CapturedRevision || *CapturedRevision != Runtime->GetChangeHierarchy().GetSectionRevision(Key) ||
			!InResult.Snapshot->Find(Key)) bCurrent = false;
	};
	if (!Runtime->GetChangeIndex().Visit(InResult.Bounds, CheckSection)) return false;
	Runtime->VisitCommittedOverlaySections(InResult.Bounds, CheckSection);
	if (!bCurrent) return false;
	for (const auto& Pair : InResult.ContentRevisions)
	{
		if (!Runtime->GetChangeIndex().IsModified(Pair.Key) && !Runtime->FindCommittedOverlay(Pair.Key)) return false;
		CheckSection(Pair.Key);
	}
	return bCurrent;
}

FVoxelOverlayReplica* UVoxelModule::GetOverlayReplica() const
{
	return OverlayReplica.Get();
}

void UVoxelModule::RegisterTerrainNetwork(UVoxelModuleNetworkComponent* InComponent)
{
	if (InComponent) TerrainNetworkComponents.AddUnique(InComponent);
}

void UVoxelModule::UnregisterTerrainNetwork(UVoxelModuleNetworkComponent* InComponent)
{
	TerrainNetworkComponents.Remove(InComponent);
	if (PendingTerrainWorldControl == InComponent) PendingTerrainWorldControl.Reset();
}

void UVoxelModule::QueueTerrainWorldControl(UVoxelModuleNetworkComponent* InComponent)
{
	// 完成事件只占一个固定弱引用槽；其余连接保留自己的完成槽，帧外 Tick 接管。
	if (!PendingTerrainWorldControl.IsValid()) PendingTerrainWorldControl = InComponent;
}

bool UVoxelModule::IsOverlayVersionCurrent(
	const TSharedPtr<const FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe>& InProof) const
{
	return IsAuthority() || (OverlayReplica && InProof && OverlayReplica->IsCurrent(*InProof));
}

TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> UVoxelModule::GetTerrainFactResources() const
{
	return Scheduler ? TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>(Scheduler->GetRuntimeResources()) : PendingTerrainFactResources;
}

TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> UVoxelModule::GetTerrainResources() const
{
	return Scheduler ? TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>(Scheduler->GetTerrainResources()) : nullptr;
}

FVoxelGenerationConsumerKey UVoxelModule::GetTerrainTaskControlConsumer() const
{
	return Scheduler ? Scheduler->GetControlConsumer() : FVoxelGenerationConsumerKey();
}

TSharedPtr<FVoxelPhysicsRetirement, ESPMode::ThreadSafe> UVoxelModule::GetPhysicsRetirement()
{
	check(IsInGameThread());
	if (WorldState == EVoxelWorldState::Closing || !Runtime || !Scheduler) return nullptr;
	if (!PhysicsRetirement)
	{
		auto* Scene = GetWorld() ? GetWorld()->GetPhysicsScene() : nullptr;
		if (!Scene) return nullptr;
		PhysicsRetirement = FVoxelPhysicsRetirement::Create(*Scene, Scheduler->GetTerrainResources(),
			FVoxelGenerationConsumerKey::Section(Epoch, TerrainPlanningOwnerId, 1, {}, 1));
	}
	return PhysicsRetirement;
}

TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> UVoxelModule::GetRegistry() const
{
	return Registry.GetSnapshot();
}

TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> UVoxelModule::GetShapes() const
{
	return Shapes;
}

UVoxelMaterialSet* UVoxelModule::GetMaterialSet() const
{
	return MaterialSet;
}

UVoxelWorldGenerationProfile* UVoxelModule::GetWorldGenerationProfile() const
{
	return WorldGenerationProfile;
}

TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> UVoxelModule::GetGenerator() const
{
	return Generator;
}

bool UVoxelModule::ResolveMapSurface(
	const FVector2D& InMapPosition,
	FVector& OutLocation,
	FString& OutError) const
{
	if (!IsReady() || !Generator)
	{
		OutError = TEXT("Voxel world is not ready for a map surface query");
		return false;
	}
	return FVoxelMapSurfaceResolver::Resolve(
		*Generator, BlockSize(), InMapPosition, OutLocation, OutError);
}

TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> UVoxelModule::GetGenerationConfig() const
{
	return GenerationConfig;
}

TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> UVoxelModule::GetGenerationCache() const
{
	return GenerationCache;
}

FVoxelMapTileCache* UVoxelModule::GetMapTileCache() const
{
	return MapTileCache.Get();
}

const FVoxelRegionStore& UVoxelModule::GetRegionStore() const
{
	return RegionStore;
}

const FVoxelInterestSet& UVoxelModule::GetCurrentInterest() const
{
	return *CurrentInterest;
}

FVoxelRepresentationReadiness UVoxelModule::GetRepresentationReadiness() const
{
	return ViewManager ? ViewManager->GetRepresentationReadiness() : FVoxelRepresentationReadiness{};
}

FVoxelTerrainReadinessSnapshot UVoxelModule::CaptureTerrainReadiness() const
{
	return ViewManager ? ViewManager->CaptureTerrainReadiness() : FVoxelTerrainReadinessSnapshot{};
}

TMap<FName, FVoxelPlanDiagnostics> UVoxelModule::GetPlanDiagnostics() const
{
	TMap<FName, FVoxelPlanDiagnostics> Result;
	const FName Names[]{TEXT("Hydrology"), TEXT("Cave"), TEXT("Structure"), TEXT("Feature"), TEXT("Ecology")};
	for (uint8 Index = 0; Index < 5; ++Index)
	{
		const auto Kind = static_cast<EVoxelGenerationPlanKind>(Index);
		FVoxelPlanDiagnostics Stats = GenerationCache ? GenerationCache->GetPlanDiagnostics(Kind) : FVoxelPlanDiagnostics{};
		Stats.WaitingConsumers = EmergeManager ? EmergeManager->GetWaitingPlanConsumers(Kind) : 0;
		Result.Add(Names[Index], Stats);
	}
	return Result;
}

FVoxelTaskDiagnostics UVoxelModule::GetTaskDiagnostics() const
{
	return Scheduler ? Scheduler->GetDiagnostics() : FVoxelTaskDiagnostics{};
}

TSharedPtr<const FVoxelTerrainStatistics, ESPMode::ThreadSafe> UVoxelModule::GetTerrainStatistics() const
{
	return Scheduler ? Scheduler->GetTerrainStatistics() : nullptr;
}

FVoxelFrameTimings UVoxelModule::GetFrameTimings() const
{
	FVoxelFrameTimings Result = ViewManager ? ViewManager->GetFrameTimings() : FVoxelFrameTimings{};
	Result.AdmissionMilliseconds += LastEmergeAdmissionMilliseconds;
	if (Scheduler)
	{
		const auto Tasks = Scheduler->GetDiagnostics();
		Result.AdmissionMilliseconds += Tasks.LastAdmissionMilliseconds;
		Result.ResultApplyMilliseconds = Tasks.LastResultApplyMilliseconds;
	}
	return Result;
}

FVoxelPrimaryFineReadiness UVoxelModule::QueryFineReadiness(TConstArrayView<FIntVector> InKeys) const
{
	return ViewManager ? ViewManager->GetFineRadiusReadiness(InKeys) : FVoxelPrimaryFineReadiness{};
}

FVoxelGenerationDependencyStatus UVoxelModule::QueryEnvironment(const FIntPoint& InCell, FVoxelEnvironmentSample& OutSample)
{
	return EmergeManager ? EmergeManager->QueryEnvironment(InCell, OutSample)
		: FVoxelGenerationDependencyStatus{ false, true, TEXT("Voxel world is unavailable") };
}

FVoxelGenerationDependencyStatus UVoxelModule::EnsureEnvironmentBounds(
	const FVoxelGenerationBounds& InBounds, const EVoxelWorkClass InWorkClass,
	const int32 InSourcePriority, const double InDistanceScore, const double InForwardScore)
{
	return EmergeManager ? EmergeManager->EnsureEnvironmentBounds(InBounds, InWorkClass,
		InSourcePriority, InDistanceScore, InForwardScore)
		: FVoxelGenerationDependencyStatus{ false, true, TEXT("Voxel world is unavailable") };
}

bool UVoxelModule::EnqueueProjectBackgroundTask(FVoxelTaskRequest&& InRequest)
{
	check(IsInGameThread());
	if (!Scheduler || InRequest.Kind != EVoxelTaskKind::ProjectBackground)
	{
		return false;
	}
	return Scheduler->Enqueue(MoveTemp(InRequest));
}

void UVoxelModule::DonateProjectTaskPriority(const FVoxelTaskStamp& InStamp, const EVoxelWorkClass InClass,
	const int32 InSourcePriority, const double InDistanceScore, const double InForwardScore)
{
	check(IsInGameThread());
	if (!Scheduler) return;
	Scheduler->UpdatePriorities([&](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
		EVoxelWorkClass& WorkClass, int32& SourcePriority, double& DistanceScore, double& ForwardScore)
	{
		if (Kind != EVoxelTaskKind::ProjectBackground || !(Stamp == InStamp)) return;
		FVoxelTaskRequest Previous, Candidate;
		Previous.Kind = Candidate.Kind = Kind;
		Previous.WorkClass = WorkClass;
		Previous.SourcePriority = SourcePriority;
		Previous.DistanceScore = DistanceScore;
		Previous.ForwardScore = ForwardScore;
		Candidate.WorkClass = InClass;
		Candidate.SourcePriority = InSourcePriority;
		Candidate.DistanceScore = InDistanceScore;
		Candidate.ForwardScore = InForwardScore;
		if (!FVoxelTaskScheduler::IsHigherPriority(Candidate, Previous)) return;
		WorkClass = InClass;
		SourcePriority = InSourcePriority;
		DistanceScore = InDistanceScore;
		ForwardScore = InForwardScore;
	});
}

void UVoxelModule::CancelProjectTasksByOwner(const FGuid& InOwnerId, const uint64 InOwnerIncarnation)
{
	check(IsInGameThread());
	if (!Scheduler || !InOwnerId.IsValid() || InOwnerIncarnation == 0) return;
	Scheduler->CancelMatching([InOwnerId, InOwnerIncarnation](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::ProjectBackground && Stamp.OwnerId == InOwnerId &&
			Stamp.OwnerIncarnation == InOwnerIncarnation;
	});
}

void UVoxelModule::SetPersistenceEnabled(const bool bInEnabled)
{
	// 已接受的 SaveUncommitted 义务不能通过改变世界策略撤销。
	if (!bInEnabled && (SimulationSaveWaitCount > 0 || SaveAdapter.IsBusy() || SceneSaveBuilder.IsSet() || HasUnpersistedSceneState())) return;
	bPersistenceEnabledForCurrentWorld = bInEnabled;
}

bool UVoxelModule::IsPersistenceEnabled() const
{
	return bPersistenceEnabledForCurrentWorld;
}

TSoftObjectPtr<UVoxelWorldGenerationProfile> UVoxelModule::GetWorldGenerationProfileAsset() const
{
	return WorldGenerationProfileAsset;
}

int32 UVoxelModule::GetDefaultWorldSeed() const
{
	return DefaultWorldSeed;
}

int32 UVoxelModule::ResolveDefaultWorldSeed() const
{
	return DefaultWorldSeed == 0 ?
		FMath::Max(1, static_cast<int32>(FGuid::NewGuid().A & 0x7fffffffu)) : DefaultWorldSeed;
}

FGuid UVoxelModule::RegisterSource(UObject* InOwner, const FVoxelStreamingSource& InSource)
{
	if (!InOwner || !IsReady())
	{
		return {};
	}
	FSource Source;
	Source.Owner = InOwner;
	Source.Value = InSource;
	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		Source.Value.Capabilities &= ~(EVoxelStreamingCapability::FineVisual |
			EVoxelStreamingCapability::WorldVisual | EVoxelStreamingCapability::LocalRefinement);
	}
	Source.Value.Normalize();
	if (Source.Value.bInheritWorldView)
	{
		Source.Value.View = ViewSettings.DefaultSourceView;
	}
	FString Error;
	if (!VoxelStreamingGeometry::ValidateInitialSide(Source.Value.View.InitialChunkSideCells,
		ViewSettings.Lod.MaximumFineLevel, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Voxel source registration rejected: %s"), *Error);
		return {};
	}
	const FGuid Id = FGuid::NewGuid();
	Source.Value.Id = Id;
	check(NextSourceRegistrationOrder < MAX_uint64 && SourceRevision < MAX_uint64);
	Source.Value.RegistrationOrder = ++NextSourceRegistrationOrder;
	Source.PlannedValue = Source.Value;
	Source.LastAcceptedPosition = Source.Value.Center;
	Sources.Add(Id, MoveTemp(Source));
	++SourceRevision;
	bInterestDirty = true;
	LastInterestRefresh = -1.0;
	return Id;
}

bool UVoxelModule::UpdateSource(const FGuid& InId, const FVoxelStreamingSource& InSource)
{
	FSource* Source = Sources.Find(InId);
	if (!Source || !Source->Owner.IsValid())
	{
		return false;
	}
	FVoxelStreamingSource Updated = InSource;
	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		Updated.Capabilities &= ~(EVoxelStreamingCapability::FineVisual |
			EVoxelStreamingCapability::WorldVisual | EVoxelStreamingCapability::LocalRefinement);
	}
	Updated.Normalize();
	if (Updated.bInheritWorldView)
	{
		Updated.View = ViewSettings.DefaultSourceView;
	}
	FString Error;
	if (!VoxelStreamingGeometry::ValidateInitialSide(Updated.View.InitialChunkSideCells,
		ViewSettings.Lod.MaximumFineLevel, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Voxel source update rejected: id=%s error=%s"), *InId.ToString(), *Error);
		return false;
	}
	Updated.Id = InId;
	Updated.RegistrationOrder = Source->Value.RegistrationOrder;
	const EVoxelSourceChange ContractChange = FVoxelInterestRuntime::ClassifySourceChange(Source->Value,
		Updated, ViewSettings, CurrentInterest->Sources.Find(InId));
	if (EnumHasAnyFlags(ContractChange, EVoxelSourceChange::RuntimeDemand | EVoxelSourceChange::FineResidency |
		EVoxelSourceChange::PageLayout | EVoxelSourceChange::CoarseTopology))
	{
		check(Source->ContractRevision < MAX_uint64);
		++Source->ContractRevision;
	}
	if (Updated.Center != Source->Value.Center ||
		(Updated.Purpose == EVoxelStreamingSourcePurpose::InitialSpawn &&
		 Source->Value.Purpose != EVoxelStreamingSourcePurpose::InitialSpawn))
	{
		check(Source->CandidateRevision < MAX_uint64);
		++Source->CandidateRevision;
		Source->ClaimVerification.Reset();
		if (!Source->PlayerClaim.bPlayerOwned || Updated.Purpose == EVoxelStreamingSourcePurpose::InitialSpawn)
			Source->PlayerClaim = {};
	}
	const bool bValueChanged = FVoxelInterestRuntime::NeedsUpdate(Source->Value, Updated, ViewSettings, CurrentInterest->Sources.Find(Updated.Id));
	if (bValueChanged ||
		FVoxelInterestRuntime::NeedsUpdate(Source->PlannedValue, Updated, ViewSettings, CurrentInterest->Sources.Find(Updated.Id)))
	{
		bInterestDirty = true;
	}
	if (bValueChanged)
	{
		check(SourceRevision < MAX_uint64);
		++SourceRevision;
	}
	Source->Value = MoveTemp(Updated);
	return true;
}

void UVoxelModule::UnregisterSource(const FGuid& InId)
{
	if (Sources.Remove(InId) > 0)
	{
		if (DesiredInterest != CurrentInterest && DesiredInterest->Sources.Contains(InId))
		{
			DesiredInterest = CurrentInterest;
			DesiredInterestRuntime = InterestRuntime;
		}
		check(SourceRevision < MAX_uint64);
		++SourceRevision;
		bInterestDirty = true;
		LastInterestRefresh = -1.0;
	}
}

void UVoxelModule::ForceVoxelStreamingRefresh()
{
	bInterestDirty = true;
	LastInterestRefresh = -1.0;
}

bool UVoxelModule::IsSourceAdmitted(const FGuid& InId) const
{
	const FSource* Source = Sources.Find(InId);
	const FVoxelSourceInterest* Admitted = CurrentInterest->Sources.Find(InId);
	if (!Source || !Source->Owner.IsValid() || !Admitted) return false;
	FVoxelStreamingSource Input = Admitted->Source;
	// 排序生成的调度优先级不是来源输入，不能据此否认已准入的来源。
	Input.SchedulingPriority = Source->Value.SchedulingPriority;
	const EVoxelSourceChange Changed = FVoxelInterestRuntime::ClassifySourceChange(Input, Source->Value,
		ViewSettings, CurrentInterest->Sources.Find(Input.Id));
	return !EnumHasAnyFlags(Changed, EVoxelSourceChange::RuntimeDemand | EVoxelSourceChange::FineResidency |
		EVoxelSourceChange::PageLayout | EVoxelSourceChange::CoarseTopology);
}

void UVoxelModule::CollectStreamingSourcesForOwner(const AActor* InOwner, TArray<FVoxelStreamingSource>& OutSources) const
{
	for (const auto& Pair : Sources)
	{
		const UObject* Owner = Pair.Value.Owner.Get();
		if (Owner && InOwner && (Owner == InOwner || Owner->GetTypedOuter<AActor>() == InOwner))
		{
			OutSources.Add(Pair.Value.Value);
		}
	}
}

FVoxelStreamingReadiness UVoxelModule::QueryStreamingReadiness(const FGuid& InId) const
{
	FVoxelStreamingReadiness Result;
	Result.bAdmitted = IsReady() && IsSourceAdmitted(InId);
	if (!Result.bAdmitted)
	{
		return Result;
	}
	const FVoxelSourceInterest& Interest = CurrentInterest->Sources.FindChecked(InId);
	Result.RequiredDataSections = Interest.DataSections.Num();
	Result.RequiredCollisionSections = Interest.CollisionSections.Num();
	for (const FIntVector& Key : Interest.DataSections)
	{
		const FVoxelSection* Section = Runtime->FindSection(Key);
		Result.ReadyDataSections += Section && Section->Status == EVoxelSectionStatus::DataReady ? 1 : 0;
	}
	for (const FIntVector& Key : Interest.CollisionSections)
	{
		Result.ReadyCollisionSections += IsCollisionReady(Key) ? 1 : 0;
	}
	return Result;
}

FVoxelStreamingReadiness UVoxelModule::QueryBoundsReadiness(const FVoxelGenerationBounds& InBounds, const bool bInRequireCollision) const
{
	FVoxelStreamingReadiness Result;
	if (!IsReady() || InBounds.Min.X >= InBounds.Max.X || InBounds.Min.Y >= InBounds.Max.Y || InBounds.Min.Z >= InBounds.Max.Z)
	{
		return Result;
	}
	const FIntVector Min = VoxelModuleToSection(InBounds.Min);
	const FIntVector Max = VoxelModuleToSection(InBounds.Max - FIntVector(1));
	const FIntVector Size = Max - Min + FIntVector(1);
	if (Size.X > 4096 || Size.Y > 4096 || Size.Z > 4096 || static_cast<int64>(Size.X) * Size.Y * Size.Z > 4096)
	{
		return Result;
	}
	Result.bAdmitted = true;
	for (int32 Z = Min.Z; Z <= Max.Z; ++Z)
	{
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
		{
			for (int32 X = Min.X; X <= Max.X; ++X)
			{
				const FIntVector Key(X, Y, Z);
				const FVoxelSection* Section = Runtime->FindSection(Key);
				++Result.RequiredDataSections;
				Result.ReadyDataSections += Section && Section->Status == EVoxelSectionStatus::DataReady ? 1 : 0;
				if (bInRequireCollision)
				{
					++Result.RequiredCollisionSections;
					Result.ReadyCollisionSections += IsCollisionReady(Key) ? 1 : 0;
				}
			}
		}
	}
	return Result;
}

bool UVoxelModule::SetRemoteChangeState(
	const FIntVector& InSection,
	const EVoxelSectionChangeState InState, FString& OutError)
{
	if (EmergeManager)
	{
		return EmergeManager->SetRemoteChangeState(InSection, InState, OutError);
	}
	OutError = TEXT("InvalidRequest: Voxel Emerge manager is unavailable");
	return false;
}

bool UVoxelModule::ApplyRemoteRepresentation(
	const FVoxelRepresentationReply& InReply,
	FString& OutError)
{
	if (!ViewManager)
	{
		OutError = TEXT("Voxel view manager is unavailable");
		return false;
	}
	return ViewManager->ApplyRemoteRepresentation(InReply, OutError);
}

bool UVoxelModule::InvalidateRemoteRepresentations(const FVoxelRepresentationInvalidate& InInvalidate,
	int32& InOutCursor, FString& OutError, const int32 InMaximumEntries)
{
	if (!ViewManager)
	{
		OutError = TEXT("InvalidResource: voxel view manager is unavailable");
		return false;
	}
	return ViewManager->InvalidateRemoteRepresentations(InInvalidate, InOutCursor, OutError, InMaximumEntries);
}

bool UVoxelModule::RequestRemoteRepresentation(
	const EVoxelRepresentationWireType InType,
	const FVoxelRepresentationWireKey& InKey,
	const uint64 InKnownRevision)
{
	if (IsAuthority() || !GetWorld())
	{
		return false;
	}
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	UVoxelModuleNetworkComponent* Network = PlayerController ?
		PlayerController->FindComponentByClass<UVoxelModuleNetworkComponent>() : nullptr;
	return Network && Network->RequestRepresentation(InType, InKey, InKnownRevision);
}

bool UVoxelModule::EnqueueNetworkRepresentationTask(FVoxelTaskRequest&& InRequest)
{
	check(IsInGameThread());
	if (!Scheduler || InRequest.Kind != EVoxelTaskKind::NetworkRepresentation)
	{
		return false;
	}
	return Scheduler->Enqueue(MoveTemp(InRequest));
}

bool UVoxelModule::BeginProjectMutation()
{
	if (bMutating || !IsAuthority() || !IsReady())
	{
		return false;
	}
	bMutating = true;
	return true;
}

void UVoxelModule::EndProjectMutation()
{
	bMutating = false;
}

void UVoxelModule::PublishProjectEdit(const FVoxelEditBatch& InBatch)
{
	for (const FVoxelSectionPatch& Patch : InBatch.Sections)
	{
		if (ViewManager)
		{
			ViewManager->InvalidateSection(Patch.Section);
		}
		if (CollisionPresenter)
		{
			CollisionPresenter->InvalidateSection(Patch.Section);
		}
		if (NavigationPresenter)
		{
			NavigationPresenter->InvalidateSection(Patch.Section);
		}
	}
	OnBlocksCommitted.Broadcast(InBatch);
}

bool UVoxelModule::IsProjectSceneSimulationEnabled() const
{
	return bEnableProjectSceneSimulation;
}

void UVoxelModule::RecordInterestBuildFailure(const FString& InError, const uint64 InSourceRevision, const uint64 InToken)
{
	if (!IsTerminalInterestBuildError(InError)) return;
	FailedInterestEpoch = Epoch;
	FailedInterestSourceRevision = InSourceRevision;
	InterestBuildError = InError;
	UE_LOG(LogTemp, Error, TEXT("Voxel interest build stopped: epoch=%llu sourceRevision=%llu token=%llu currentInterest=%llu error=%s"),
		FailedInterestEpoch, FailedInterestSourceRevision, InToken, InterestRevision, *InterestBuildError);
}

void UVoxelModule::RefreshInterest(
	const double InNow)
{
	if (bInterestBuildPending || DesiredInterest != CurrentInterest) return;
	for (auto Iterator = Sources.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator.Value().Owner.IsValid()) continue;
		Iterator.RemoveCurrent();
		check(SourceRevision < MAX_uint64);
		++SourceRevision;
		bInterestDirty = true;
	}
	if (HasInterestBuildFailed()) return;
	InterestBuildError.Reset();
	FailedInterestSourceRevision = MAX_uint64;
	FailedInterestEpoch = 0;
	LastInterestRefresh = InNow;
	check(InterestAttemptSerial < MAX_uint64);
	const auto PlanningResources = Scheduler->GetTerrainResources();
	const uint64 CapturedSourceRevision = SourceRevision;
	FVoxelTaskStamp AttemptStamp;
	AttemptStamp.OwnerId = TerrainPlanningOwnerId;
	AttemptStamp.OwnerIncarnation = 1;
	AttemptStamp.WorldEpoch = Epoch;
	AttemptStamp.Token = InterestAttemptSerial + 1;
	const auto AcceptAllocation = [this, InNow, PlanningResources, AttemptStamp, CapturedSourceRevision](
		const EVoxelTerrainResourceAdmission Admission, const TCHAR* InStage, const uint64 InBytes, const uint64 InRequiredInput)
	{
		if (Admission == EVoxelTerrainResourceAdmission::Granted) return true;
		const auto State = PlanningResources->Snapshot();
		const bool bTerminal = Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable || Admission == EVoxelTerrainResourceAdmission::InvalidRequest;
		Scheduler->ObserveDemand(EVoxelTaskKind::BuildInterest, AttemptStamp,
			bTerminal ? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory, InBytes, 1);
		if (bTerminal)
		{
			RecordInterestBuildFailure(FString::Printf(TEXT("%s: interest %s request=%llu requiredInput=%llu used=%llu limit=%llu leases=%d releaseVersion=%llu"),
				Admission == EVoxelTerrainResourceAdmission::InvalidRequest ? TEXT("InvalidRequest") : TEXT("CapacityUnachievable"),
				InStage, InBytes, InRequiredInput, State.Used[int32(EVoxelTerrainResourcePool::Input)], State.Limits.InputBytes, State.LeaseCount, State.ReleaseVersion),
				CapturedSourceRevision, AttemptStamp.Token);
		}
		else if (InNow - LastInterestCapacityDiagnostic >= 5.0)
		{
			LastInterestCapacityDiagnostic = InNow;
			UE_LOG(LogTemp, Warning, TEXT("Voxel interest admission waiting: epoch=%llu sourceRevision=%llu token=%llu stage=%s admission=%d request=%llu requiredInput=%llu input=%llu/%llu working=%llu/%llu leases=%d releaseVersion=%llu"),
				Epoch, CapturedSourceRevision, AttemptStamp.Token, InStage, int32(Admission), InBytes, InRequiredInput,
				State.Used[int32(EVoxelTerrainResourcePool::Input)], State.Limits.InputBytes,
				State.Used[int32(EVoxelTerrainResourcePool::Working)], State.Limits.WorkingBytes, State.LeaseCount, State.ReleaseVersion);
		}
		return false;
	};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SourcesLease;
	const auto SourcesConsumer = FVoxelGenerationConsumerKey::Section(Epoch, TerrainPlanningOwnerId, 1, {}, AttemptStamp.Token);
	const uint64 SourcesBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelInterestSourceCapture>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelStreamingSource>(Sources.Num());
	if (!AcceptAllocation(PlanningResources->TryAcquire(SourcesConsumer, EVoxelTerrainResourcePool::Input, SourcesBytes, SourcesLease),
		TEXT("Source capture"), SourcesBytes, 0)) return;
	const auto SourcesCapture = VoxelBoundedAllocation::MakeWorkerShared<FVoxelInterestSourceCapture>(SourcesLease);
	SourcesCapture->Lease = MoveTemp(SourcesLease);
	auto& ActiveSources = SourcesCapture->Values;
	ActiveSources.Reserve(Sources.Num());

	for (const auto& Pair : Sources) ActiveSources.Add(Pair.Value.Value);

	ActiveSources.Sort([](const FVoxelStreamingSource& A, const FVoxelStreamingSource& B)
	{
		if (A.Priority != B.Priority) return A.Priority < B.Priority;
		if (A.bLocalView != B.bLocalView) return A.bLocalView;
		return A.RegistrationOrder < B.RegistrationOrder;
	});
	for (int32 Index = 0; Index < ActiveSources.Num(); ++Index)
	{
		FVoxelStreamingSource& Source = ActiveSources[Index];
		Source.SchedulingPriority = INDEX_NONE;
	}

	// 一个观察者快照最多有一个规划任务；移动期间合并后续请求，旧分区保留到新结果提交。
	if (bInterestBuildPending || DesiredInterest != CurrentInterest)
	{
		return;
	}
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildInterest;
	Request.WorkClass = EVoxelWorkClass::Interactive;
	Request.ServiceBand = EVoxelTerrainServiceBand::P1;
	Request.Stamp = AttemptStamp;
	const auto RenderRoot = ViewManager ? ViewManager->CapturePresentedSnapshot() : nullptr;
	const auto PhysicsRoot = CollisionPresenter ? CollisionPresenter->CaptureSnapshot() : nullptr;
	const int32 ShardCount = CurrentInterest->PublicationInputs ? CurrentInterest->PublicationInputs->FineShards.Num() : 0;
	const int32 MaximumRows = CurrentInterest->PublicationInputs ? CurrentInterest->PublicationInputs->MaximumFeedbackSections : 0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> FeedbackLease;
	const uint64 FeedbackBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineResidencyFeedback>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineFeedbackCapture>() +
		VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelFineSectionCoverage>(MaximumRows) +
		VoxelBoundedAllocation::MapBytes<FVoxelFineShardKey, FVoxelFineShardReadiness>(ShardCount) +
		VoxelBoundedAllocation::MapBytes<FGuid, FIntVector>(Sources.Num());
	if (!AcceptAllocation(PlanningResources->TryAcquire(SourcesConsumer, EVoxelTerrainResourcePool::Input,
		FeedbackBytes, FeedbackLease, 0, SourcesCapture->Lease->Bytes(), 1), TEXT("feedback capture"), FeedbackBytes, SourcesCapture->Lease->Bytes())) return;
	auto Feedback = VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineResidencyFeedback>(FeedbackLease);
	Feedback->Sections.Reserve(MaximumRows);
	Feedback->Shards.Reserve(ShardCount);
	Feedback->LastAcceptedPositions.Reserve(Sources.Num());
	Feedback->ResourceLease = MoveTemp(FeedbackLease);
	Feedback->PreparationSeconds = ViewSettings.Fine.MinimumPreparationSeconds;
	for (const auto& Pair : Sources) Feedback->LastAcceptedPositions.Add(Pair.Key, Pair.Value.LastAcceptedPosition);
	auto Capture = VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineFeedbackCapture>(Feedback->ResourceLease, CurrentInterest.ToSharedRef(),
		RenderRoot, PhysicsRoot, Runtime->GetChangeHierarchy(), Manifest, Shapes->GetContentHash(), ++SafetyCaptureSerial, Feedback.ToSharedRef());
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkLease;
	const uint64 WorkBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelInterestTaskPayload>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelInterestRuntime>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelInterestSet>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelChangeHierarchySnapshot>();
	const uint64 CapturedInputBytes = SourcesCapture->Lease->Bytes() + Feedback->ResourceLease->Bytes();
	if (!AcceptAllocation(PlanningResources->TryAcquire(SourcesConsumer, EVoxelTerrainResourcePool::Input,
		WorkBytes, WorkLease, 0, CapturedInputBytes, 2), TEXT("task metadata"), WorkBytes, CapturedInputBytes)) return;
	const auto Payload = VoxelBoundedAllocation::MakeWorkerShared<FVoxelInterestTaskPayload>(WorkLease);
	Payload->MetadataLease = MoveTemp(WorkLease);
	const uint64 ErrorBytes = VoxelBoundedAllocation::ArrayBytes<TCHAR>(InterestErrorCapacity);
	const uint64 RequiredErrorInput = CapturedInputBytes + Payload->MetadataLease->Bytes();
	if (!AcceptAllocation(PlanningResources->TryAcquire(SourcesConsumer, EVoxelTerrainResourcePool::Input,
		ErrorBytes, Payload->ErrorLease, 0, RequiredErrorInput, 3), TEXT("error buffer"), ErrorBytes, RequiredErrorInput)) return;
	Payload->ErrorBuffer.GetCharArray().Reserve(InterestErrorCapacity);
	check(Payload->ErrorBuffer.GetAllocatedSize() <= Payload->ErrorLease->Bytes());
	Request.ReservedBytes = 1;
	Request.InputBytes = 0;
	Request.FirstRunnableSeconds = InNow;
	Request.bResumable = true;
	Request.SetRequiredBindingInput(RequiredErrorInput + Payload->ErrorLease->Bytes(), 4);
	const auto RejectBinding = [this, &Request, CapturedSourceRevision](const FString& InError)
	{
		Scheduler->ObserveDemand(Request.Kind, Request.Stamp,
			IsTerminalInterestBuildError(InError) ? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory,
			Request.GetBindingBytes() + Request.GetRequiredBindingInputBytes(), Request.ReservedBytes);
		RecordInterestBuildFailure(InError, CapturedSourceRevision, Request.Stamp.Token);
	};
	{
		FString CallbackBindingError;
		if (!Request.BindGetBlockReason((*Scheduler).GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : (*Scheduler).GetControlConsumer()),
			[Payload, PlanningResources]
	{
		return Payload->bCapacityWaiting.Load() && PlanningResources->Snapshot().ReleaseVersion <= Payload->CapacityVersion.Load() ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	}, CallbackBindingError))
		{
			RejectBinding(CallbackBindingError);
			return;
		}
	}
	{
		FString CallbackBindingError;
		if (!Request.BindExecute((*Scheduler).GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : (*Scheduler).GetControlConsumer()),
			[SourcesCapture, WorldManifest = Manifest, Settings = ViewSettings,
		Runtime = InterestRuntime, Capture, Payload, ShapeRevision = Shapes->GetContentHash(),
		Resources = Scheduler->GetTerrainResources(), PlanningStamp = Request.Stamp, PlanningAge = Request.FirstRunnableSeconds](const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		if (Cancel.Load())
		{
			return Result;
		}
		if (!Capture->Advance(128, Cancel))
		{
			Result.bSuccess = Result.bContinue = !Cancel.Load();
			return Result;
		}
		Payload->bCapacityWaiting.Store(false);
		if (!Payload->CandidateRuntime)
			Payload->CandidateRuntime = VoxelBoundedAllocation::MakeWorkerShared<FVoxelInterestRuntime>(Payload->MetadataLease, *Runtime);
		Payload->CandidateRuntime->BindPlanningResources(Resources, {EVoxelGenerationConsumerKind::WorldFact,
			PlanningStamp.WorldEpoch, PlanningStamp.OwnerId, PlanningStamp.OwnerIncarnation,
			EVoxelGenerationSpatialKind::Bounds, FIntVector(MIN_int32), FIntVector(MAX_int32), PlanningStamp.Token}, PlanningAge);
		Payload->bChanged = Payload->CandidateRuntime->UpdateSnapshot(SourcesCapture->Values, WorldManifest, Settings,
			Payload->Interest, Payload->Delta, Payload->bHasSnapshot, &Capture->GetFeedback().Get());
		if (!Payload->Interest) Payload->Interest = VoxelBoundedAllocation::MakeWorkerShared<FVoxelInterestSet>(Payload->MetadataLease);
		Payload->Interest->SafetyContentRevision = Capture->GetFeedback()->ContentRevision;
		Payload->Interest->SafetyWorldEpoch = PlanningStamp.WorldEpoch;
		Payload->Interest->SafetyShapeRevision = ShapeRevision;
		Payload->Interest->SafetyRegistryRevision = WorldManifest.RegistryHash;
		if (Payload->bHasSnapshot)
			Payload->Interest->SafetyVersions = VoxelBoundedAllocation::MakeWorkerShared<FVoxelChangeHierarchySnapshot>(Payload->MetadataLease, Capture->GetVersions());
		Payload->Interest->RenderCoverageSerial = Capture->GetRenderSerial();
		Payload->Interest->PhysicsCoverageSerial = Capture->GetPhysicsSerial();
		Result.bSuccess = !Cancel.Load();
		const FString& PlanningError = Payload->CandidateRuntime->GetPlanningError();
		if (PlanningError.StartsWith(TEXT("CapacityBlocked")))
		{
			// 同一个规划 continuation 保留根、Token 和首次排队年龄；只在真正释放容量后重试。
			Payload->CapacityVersion.Store(Resources->Snapshot().ReleaseVersion);
			Payload->bCapacityWaiting.Store(true);
			Result.bSuccess = Result.bContinue = !Cancel.Load();
			return Result;
		}
		Result.bSuccess &= PlanningError.IsEmpty();
		if (Result.bSuccess && !Payload->bHasSnapshot && Payload->CandidateRuntime->GetCoarsePlanningPending())
		{
			// 有界粗域推进尚未形成完整快照；原任务保留候选根、游标、Token 与排队年龄继续执行。
			Result.bContinue = true;
			return Result;
		}
		if (!PlanningError.IsEmpty())
		{
			// 接纳前固定错误容量；结果与其授额一起转移，不在失败路径重新申请内存。
			auto& Buffer = Payload->ErrorBuffer.GetCharArray();
			Buffer.Reset();
			const bool bUnclassifiedFailure = !IsTerminalInterestBuildError(PlanningError) &&
				!PlanningError.StartsWith(TEXT("DependencyNotReady")) && !PlanningError.StartsWith(TEXT("PlanningPending"));
			constexpr TCHAR TerminalPrefix[] = TEXT("TerminalFailure: ");
			if (bUnclassifiedFailure) Buffer.Append(TerminalPrefix, UE_ARRAY_COUNT(TerminalPrefix) - 1);
			const int32 Count = FMath::Min(PlanningError.Len(), InterestErrorCapacity - Buffer.Num() - 1);
			Buffer.Append(*PlanningError, Count);
			Buffer.Add(TCHAR(0));
			if (Count < PlanningError.Len())
				UE_LOG(LogTemp, Error, TEXT("Voxel interest full failure: epoch=%llu token=%llu error=%s"), PlanningStamp.WorldEpoch, PlanningStamp.Token, *PlanningError);
			Result.Error = MoveTemp(Payload->ErrorBuffer);
			Result.OutputResourceLease = MoveTemp(Payload->ErrorLease);
		}
		else
		{
			Payload->ErrorBuffer.Empty();
			Payload->ErrorLease.Reset();
		}
		// Failed planning only needs its error and lease. The unfinished snapshot and delta are not publication results.
		if (Result.bSuccess) Result.CustomPayload = Payload;
		return Result;
	}, CallbackBindingError))
		{
			RejectBinding(CallbackBindingError);
			return;
		}
	}
	{
		FString CallbackBindingError;
		if (!Request.BindApply((*Scheduler).GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : (*Scheduler).GetControlConsumer()),
			[this, CapturedSourceRevision](FVoxelTaskResult&& Result)
	{
		if (Result.Stamp.WorldEpoch != Epoch)
		{
			return;
		}
		bInterestBuildPending = false;
		bool bOwnersValid = true;
		for (const auto& Pair : Sources)
		{
			bOwnersValid &= Pair.Value.Owner.IsValid();
		}
		if (!Result.bSuccess || Result.bCanceled || !bOwnersValid)
		{
			if (!Result.bCanceled && bOwnersValid && IsTerminalInterestBuildError(Result.Error))
			{
				// 同一输入的确定性错误只报告一次；新的 Source 版本或世界允许重新规划。
				const FString Error = Result.RejectedResultBytes != 0 ? FString::Printf(
					TEXT("%s; actual=%llu accounted=%llu payload=%llu payloadAccounted=%llu"), *Result.Error,
					Result.RejectedResultBytes, Result.RejectedAccountedBytes, Result.RejectedPayloadBytes, Result.RejectedPayloadAccountedBytes) : Result.Error;
				RecordInterestBuildFailure(Error, CapturedSourceRevision, Result.Stamp.Token);
			}
			bInterestDirty = true;
			return;
		}
		const auto Payload = StaticCastSharedPtr<const FVoxelInterestTaskPayload>(Result.CustomPayload);
		if (!Payload)
		{
			RecordInterestBuildFailure(TEXT("TerminalFailure: interest result payload is missing"), CapturedSourceRevision, Result.Stamp.Token);
			bInterestDirty = true;
			return;
		}
		InterestBuildError.Reset();
		FailedInterestSourceRevision = MAX_uint64;
		FailedInterestEpoch = 0;
		bInterestDirty |= SourceRevision != CapturedSourceRevision;
		LastPlannedRenderCoverageSerial = Payload->Interest->RenderCoverageSerial;
		LastPlannedPhysicsCoverageSerial = Payload->Interest->PhysicsCoverageSerial;
		if (Payload->bHasSnapshot)
		{
			Payload->Interest->PlanningWorkReceipt = Result.WorkReceipt;
			if (Payload->Interest->CoarseFailedRootCount != CurrentInterest->CoarseFailedRootCount)
				UE_LOG(LogTemp, Warning, TEXT("Voxel coarse planning: blocked local roots=%d"), Payload->Interest->CoarseFailedRootCount);
			DesiredInterestRuntime = Payload->CandidateRuntime;
			DesiredInterestTaskPayload = Payload;
			DesiredInterest = Payload->Interest;
			if (!CanCommitTerrainInterest(*DesiredInterest))
			{
				AbandonTerrainInterest(*DesiredInterest);
				return;
			}
			if (!ViewManager)
			{
				CommitTerrainInterest(DesiredInterest);
			}
			else if (Payload->bChanged || CurrentInterest->PublicationRevision != DesiredInterest->PublicationRevision)
			{
				ViewManager->ApplyInterestDelta(Payload->Delta);
			}
			else
			{
				CommitTerrainInterest(DesiredInterest);
				ViewManager->RefreshTaskPriorities();
			}
		}
		// 不清除执行期间新产生的移动请求，避免连续行走丢失末次位置。
	}, CallbackBindingError))
		{
			RejectBinding(CallbackBindingError);
			return;
		}
	}
	if (!Scheduler->Enqueue(MoveTemp(Request)))
	{
		if (Request.Blocked == EVoxelTaskBlockReason::Oversized || Request.Blocked == EVoxelTaskBlockReason::TerminalFailure)
		{
			RecordInterestBuildFailure(FString::Printf(TEXT("TerminalFailure: interest enqueue blocked=%u input=%llu reserved=%llu requiredInput=%llu"),
				uint32(Request.Blocked), Request.InputBytes, Request.ReservedBytes, Request.GetRequiredBindingInputBytes()),
				CapturedSourceRevision, AttemptStamp.Token);
		}
		return;
	}
	// 未接纳的容量重试复用原尝试身份，避免诊断等待表随重试无界增长。
	InterestAttemptSerial = AttemptStamp.Token;
	bInterestBuildPending = true;
	LastInterestRefresh = InNow;
	bInterestDirty = false;
	if (GenerationCache)
	{
		FVoxelGenerationCacheRetention Retention;
		Retention.HydrologyRegionSide = FMath::Max(8, Manifest.Settings.HydrologyRegionSide);
		Retention.HydrologyCellSize = FMath::Max(1, Manifest.Settings.HydrologyCellSize);
		Retention.Revision = InterestRevision + 1;
		const int64 HydrologySide = static_cast<int64>(Retention.HydrologyRegionSide) * Retention.HydrologyCellSize;
		const TConstArrayView<FVoxelStreamingSource> RetentionSources = MakeArrayView(SourcesCapture->Values);
		int32 PointCount = 0;
		for (const FVoxelStreamingSource& Source : RetentionSources)
		{
			PointCount += Source.Has(EVoxelStreamingCapability::RetainGenerationCache) ? 1 : 0;
		}
		const float NaturalMarginFraction = ViewSettings.Planning.GenerationCacheMarginFraction;
		const float PlanMarginFraction = ViewSettings.Planning.PlanCacheMarginFraction;
		FString Error;
		if (!GenerationCache->UpdateRetention(Retention, PointCount,
			[RetentionSources, NaturalMarginFraction, PlanMarginFraction, HydrologySide](
				TArrayView<FVoxelGenerationCacheRetentionPoint> OutPoints, FString& OutError)
			{
				int32 PointIndex = 0;
				for (const FVoxelStreamingSource& Source : RetentionSources)
				{
					if (!Source.Has(EVoxelStreamingCapability::RetainGenerationCache))
					{
						continue;
					}
					if (PointIndex >= OutPoints.Num())
					{
						OutError = TEXT("Generation cache retention source changed during snapshot fill");
						return false;
					}
					FVoxelGenerationCacheRetentionPoint& Point = OutPoints[PointIndex++];
					Point.Center = FIntPoint(Source.Center.X, Source.Center.Y);
					const int32 PlanningRadius = Source.Has(EVoxelStreamingCapability::WorldVisual)
						? VoxelViewLod::ResolveViewRanges(Source).Surface
						: FMath::Max3(Source.ExactRadius, Source.CollisionRadius, Source.View.FineRadiusCells);
					Point.NaturalRadiusCells = PlanningRadius + FMath::CeilToInt(PlanningRadius * NaturalMarginFraction);
					Point.PlanRadiusCells = Point.NaturalRadiusCells + FMath::CeilToInt(PlanningRadius * PlanMarginFraction);
					Point.HydrologyRadiusCells = static_cast<int32>(FMath::Min<int64>(MAX_int32, Point.PlanRadiusCells + HydrologySide));
				}
				if (PointIndex != OutPoints.Num())
				{
					OutError = TEXT("Generation cache retention source changed during snapshot fill");
					return false;
				}
				return true;
			}, Error))
		{
			bInterestDirty = true;
			if (!Error.StartsWith(TEXT("CapacityBlocked")))
			{
				UE_LOG(LogTemp, Warning, TEXT("Generation cache retention snapshot rejected: %s"), *Error);
			}
		}
	}
}

void UVoxelModule::ApplyTask(FVoxelTaskResult&& InResult)
{
	switch (InResult.Kind)
	{
	case EVoxelTaskKind::GenerateExactBase:
	case EVoxelTaskKind::DecodeOverlay:
	{
		const FIntVector Section = InResult.Stamp.Section;
		if (EmergeManager->OnTask(MoveTemp(InResult)))
		{
			if (const FVoxelSection* Data = Runtime->FindSection(Section);
				Data && Data->Status == EVoxelSectionStatus::DataReady)
			{
				if (ViewManager)
				{
					ViewManager->InvalidateNeighbors(Section);
				}
				WakeSimulationSection(Section);
				if (NavigationPresenter)
				{
					NavigationPresenter->InvalidateSection(Section);
				}
			}
		}
		return;
	}
	case EVoxelTaskKind::BuildCollision:
		if (CollisionPresenter)
		{
			const FIntVector Section = InResult.Stamp.Section;
			CollisionPresenter->OnTask(MoveTemp(InResult));
			WakeSimulationSection(Section);
		}
		return;
	case EVoxelTaskKind::BuildFineMesh:
	case EVoxelTaskKind::BuildVoxelProxy:
	case EVoxelTaskKind::BuildSurface:
	case EVoxelTaskKind::BuildWater:
	case EVoxelTaskKind::BuildMacro:
		if (ViewManager)
		{
			ViewManager->OnTask(MoveTemp(InResult));
		}
		return;
	case EVoxelTaskKind::BuildDetails:
		if (DetailView)
		{
			DetailView->OnTask(MoveTemp(InResult));
		}
		return;
	default:
		return;
	}
}

void UVoxelModule::UpdateReadiness()
{
#if !UE_BUILD_SHIPPING
	LogTerrainGeometryHistogram();
#endif
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_Readiness);

	const EVoxelWorldReadyStage PreviousReadyStage = ReadyStage;
	FVoxelWorldReadinessSnapshot Snapshot;
	Snapshot.WorldEpoch = static_cast<int64>(Epoch);
	Snapshot.InterestRevision = static_cast<int64>(InterestRevision);
	Snapshot.bAssetsValidated = Registry.GetSnapshot().IsValid() && Shapes.IsValid();
	Snapshot.bRecipeFrozen = GenerationConfig && GenerationConfig->Recipe;
	Snapshot.bSpawnPlanReady = false;
	auto CheckRequiredSection = [this](const FIntVector& Key)
	{
		if (WorldState == EVoxelWorldState::Failed)
		{
			return;
		}
		const FVoxelSection* Section = Runtime->FindSection(Key);
		if (Section && Section->Status == EVoxelSectionStatus::Failed)
		{
			WorldState = EVoxelWorldState::Failed;
			UE_LOG(LogTemp, Error,
				TEXT("Voxel loading failed: required section %s failed generation: %s"),
				*Key.ToString(), *Section->Error);
		}
	};
	for (const TPair<FIntVector, FVoxelExactDemand>& Pair : CurrentInterest->Warmup)
	{
		if (!Pair.Value.bWarmupData)
		{
			continue;
		}
		Snapshot.bSpawnPlanReady = true;
		++Snapshot.RequiredSpawnSections;
		CheckRequiredSection(Pair.Key);
		const FVoxelSection* Section = Runtime->FindSection(Pair.Key);
		if (Section && Section->Status == EVoxelSectionStatus::DataReady)
		{
			++Snapshot.ReadySpawnSections;
		}
		if (Pair.Value.bWarmupCollision)
		{
			++Snapshot.RequiredCollisionSections;
			if (CollisionPresenter && CollisionPresenter->IsReady(Pair.Key))
			{
				++Snapshot.ReadyCollisionSections;
			}
		}
	}
	for (const FIntVector& Key : CurrentInterest->PlayableFineKeys)
	{
		CheckRequiredSection(Key);
		for (int32 Face = 0; Face < 6; ++Face)
		{
			FIntVector Neighbor = Key;
			Neighbor[Face / 2] += (Face & 1) ? -1 : 1;
			if (CurrentInterest->Exact.Contains(Neighbor))
			{
				CheckRequiredSection(Neighbor);
			}
		}
		if (WorldState == EVoxelWorldState::Failed)
		{
			break;
		}
	}
	FVoxelPrimaryFineReadiness PrimaryFine;
	FVoxelPrimaryFineReadiness PlayableFine;
	if (ViewManager)
	{
		PrimaryFine = ViewManager->GetPrimaryFineReadiness(CurrentInterest->Warmup);
		PlayableFine = ViewManager->GetFineRadiusReadiness(CurrentInterest->PlayableFineKeys);
	}
	Snapshot.RequiredPrimaryFineSections = PrimaryFine.Required;
	Snapshot.ReadyPrimaryFineSections = PrimaryFine.Ready;
	Snapshot.RenderablePrimaryFineSections = PrimaryFine.Renderable;
	Snapshot.RequiredPlayableFineSections = PlayableFine.Required;
	Snapshot.ReadyPlayableFineSections = PlayableFine.Ready;
	Snapshot.PresentedPlayableFineSections = PlayableFine.Presented;
	Snapshot.RequiredPrimaryRepresentations = PrimaryFine.Required;
	Snapshot.ReadyPrimaryRepresentations = PrimaryFine.Ready;
	Snapshot.PendingCriticalDependencies =
		FMath::Max(0, Snapshot.RequiredSpawnSections - Snapshot.ReadySpawnSections) +
		FMath::Max(0, Snapshot.RequiredCollisionSections - Snapshot.ReadyCollisionSections);
	const bool bSpawnDataReady = Snapshot.bSpawnPlanReady &&
		Snapshot.ReadySpawnSections >= Snapshot.RequiredSpawnSections;
	const bool bSpawnCollisionReady = bSpawnDataReady &&
		Snapshot.ReadyCollisionSections >= Snapshot.RequiredCollisionSections;
	// A preview source may request a playable fine radius without marking any
	// warmup sections for fine rendering. In that case the warmup fine gate is
	// vacuously satisfied, while the playable radius still must be presented.
	const bool bPrimaryViewReady = bSpawnCollisionReady &&
		(PrimaryFine.Required == 0 || PrimaryFine.IsComplete()) && PlayableFine.IsComplete();
	if (WorldState == EVoxelWorldState::Failed || PrimaryFine.bFailed || PlayableFine.bFailed)
	{
		Snapshot.bFailed = true;
		ReadyStage = EVoxelWorldReadyStage::Failed;
	}
	else if (!Snapshot.bAssetsValidated)
	{
		ReadyStage = EVoxelWorldReadyStage::None;
	}
	else if (!Snapshot.bRecipeFrozen)
	{
		ReadyStage = EVoxelWorldReadyStage::AssetsValidated;
	}
	else if (!Snapshot.bSpawnPlanReady)
	{
		ReadyStage = EVoxelWorldReadyStage::RecipeFrozen;
	}
	else if (!bSpawnDataReady)
	{
		ReadyStage = EVoxelWorldReadyStage::SpawnPlanReady;
	}
	else if (!bSpawnCollisionReady)
	{
		ReadyStage = EVoxelWorldReadyStage::SpawnDataReady;
	}
	else if (!bPrimaryViewReady)
	{
		ReadyStage = EVoxelWorldReadyStage::SpawnCollisionReady;
	}
	else
	{
		ReadyStage = EVoxelWorldReadyStage::Playable;
	}
	if (ReadyStage != PreviousReadyStage)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Voxel readiness stage %d -> %d; warmup=%d/%d collision=%d/%d primary=%d/%d playableFine=%d/%d presented=%d critical=%d"),
			static_cast<int32>(PreviousReadyStage),
			static_cast<int32>(ReadyStage),
			Snapshot.ReadySpawnSections,
			Snapshot.RequiredSpawnSections,
			Snapshot.ReadyCollisionSections,
			Snapshot.RequiredCollisionSections,
			Snapshot.ReadyPrimaryRepresentations,
			Snapshot.RequiredPrimaryRepresentations,
			Snapshot.ReadyPlayableFineSections,
			Snapshot.RequiredPlayableFineSections,
			Snapshot.PresentedPlayableFineSections,
			Snapshot.PendingCriticalDependencies);
	}
	ReadinessSnapshot = Snapshot;
}

TArray<FVector> UVoxelModule::CollectLocalViewObservers() const
{
	TArray<FVector> Observers;
	if (!GetWorld())
	{
		return Observers;
	}
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* Controller = Iterator->Get();
		if (Controller && Controller->IsLocalController() && Controller->GetPawn())
		{
			Observers.Add(Controller->GetPawn()->GetActorLocation());
		}
	}
	return Observers;
}

TArray<FVector> UVoxelModule::CollectDetailObservers() const
{
	TArray<FVector> Observers;
	if (!CurrentInterest) return Observers;
	// Detail/近场物理由独立 Source 能力决定；专服的 Simulation Source 不依赖本地视觉玩家。
	for (const auto& Pair : CurrentInterest->Sources)
	{
		const auto& Source = Pair.Value.Source;
		if (Source.Has(EVoxelStreamingCapability::Collision | EVoxelStreamingCapability::Simulation) ||
			(Source.bLocalView && Source.Has(EVoxelStreamingCapability::FineVisual)))
			Observers.AddUnique(FVector(Source.Center) * BlockSize());
	}
	return Observers;
}

FVoxelTraceResult UVoxelModule::Trace(
	const FVector& InStart,
	const FVector& InDirection,
	const double InDistance) const
{
	if (!IsReady())
	{
		return {};
	}
	return FVoxelRaycast::Trace(
		*Runtime,
		*Registry.GetSnapshot(),
		*Shapes,
		InStart,
		InDirection,
		InDistance,
		BlockSize());
}

bool UVoxelModule::IsActorRayClear(
	const FVector& InStart,
	const FVector& InEnd,
	AActor* InIgnore) const
{
	FCollisionQueryParams Parameters(SCENE_QUERY_STAT(VoxelActorLineOfSight), false);
	if (InIgnore)
	{
		Parameters.AddIgnoredActor(InIgnore);
	}
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(
		Hit,
		InStart,
		InEnd,
		ECC_Visibility,
		Parameters);
}

bool UVoxelModule::VerifyView(
	APlayerController* InController,
	AActor* InAuthorizedObserver,
	const FVoxelEditIntent& InIntent,
	FVoxelTraceResult& OutTrace,
	FString& OutError) const
{
	if (!IsReady() ||
		!IsAuthority() ||
		!InAuthorizedObserver ||
		InAuthorizedObserver->GetWorld() != GetWorld() ||
		InIntent.Origin.ContainsNaN() ||
		InIntent.Direction.ContainsNaN() ||
		InIntent.Direction.IsNearlyZero())
	{
		OutError = TEXT("Invalid voxel edit view");
		return false;
	}
	FVector Eye;
	FRotator Rotation;
	InAuthorizedObserver->GetActorEyesViewPoint(Eye, Rotation);
	if (InController && InController->GetPawn() == InAuthorizedObserver)
	{
		Rotation = InController->GetControlRotation();
	}
	if (GetWorld()->GetNetMode() != NM_Standalone)
	{
		if (!InController ||
			!InController->HasAuthority() ||
			FVector::DistSquared(InIntent.Origin, Eye) > FMath::Square(500.0) ||
			FVector::DotProduct(Rotation.Vector(), InIntent.Direction.GetSafeNormal()) < 0.984807753)
		{
			OutError = TEXT("View is outside the authorized observer");
			return false;
		}
	}
	OutTrace = Trace(InIntent.Origin, InIntent.Direction, 600.0);
	if (OutTrace.Status != EVoxelTraceStatus::Hit ||
		OutTrace.Index != InIntent.ExpectedTarget ||
		FVector::Distance(OutTrace.Point, Eye) > 600.0 ||
		!IsActorRayClear(
			InIntent.Origin,
			OutTrace.Point - InIntent.Direction.GetSafeNormal() * 0.1,
			InAuthorizedObserver))
	{
		OutError = OutTrace.Status == EVoxelTraceStatus::NeedsData ?
			TEXT("Target data is not ready") :
			TEXT("Target is not visible or is too far");
		return false;
	}
	OutError.Reset();
	return true;
}

bool UVoxelModule::PlacementOverlapsActors(const FVoxelInteractionPlan& InPlan) const
{
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	for (const FVoxelCellEdit& Edit : InPlan.Cells)
	{
		if (Edit.Value.IsAir() || Edit.Value == Edit.Expected)
		{
			continue;
		}
		const FVoxelRuntimeDefinition* Definition = Registry.GetSnapshot()->Find(Edit.Value.TypeId);
		if (!Definition || !Definition->bSolid)
		{
			continue;
		}
		const FVoxelResolvedShape& Shape = Shapes->Get(Definition->Shape, Edit.Value.State);
		for (const FBox& Box : Shape.CollisionBoxes)
		{
			const FVector Center = (FVector(Edit.Position) + Box.GetCenter()) * BlockSize();
			const FVector HalfExtent = Box.GetExtent() * BlockSize() - FVector(0.1);
			if (HalfExtent.GetMin() > 0.0 && GetWorld()->OverlapAnyTestByObjectType(
				Center,
				FQuat::Identity,
				Objects,
				FCollisionShape::MakeBox(HalfExtent)))
			{
				return true;
			}
		}
	}
	return false;
}

UAbilityInventoryBase* UVoxelModule::ResolveInventory(
	APlayerController* InController,
	AActor* InSource) const
{
	AActor* Owner = InController && InController->GetPawn() ? InController->GetPawn() : InSource;
	IAbilityInventoryAgentInterface* Agent = Cast<IAbilityInventoryAgentInterface>(Owner);
	return Agent ? Agent->GetInventory() : nullptr;
}

FVoxelEditReply UVoxelModule::ExecuteIntent(
	APlayerController* InController,
	AActor* InSource,
	const FVoxelEditIntent& InIntent,
	bool bTrustedStandaloneCreative)
{
	FVoxelEditReply Reply;
	Reply.RequestId = InIntent.RequestId;
	Reply.Code = EVoxelEditCode::Rejected;
	if (!InIntent.RequestId || !InSource || bMutating || !IsReady() || !IsAuthority())
	{
		Reply.Code = EVoxelEditCode::Busy;
		return Reply;
	}
	UInventoryWorldSubsystem* System = GetWorld()->GetSubsystem<UInventoryWorldSubsystem>();
	if (!System || !System->IsReady() || System->IsArchiveFrozen())
	{
		Reply.Code = EVoxelEditCode::Busy;
		Reply.Reason = TEXT("Inventory.WorldNotReadyOrSaving");
		return Reply;
	}
	if (!InIntent.InventoryStoreId.IsValid() || InIntent.InventoryStoreId != System->GetStoreId())
	{
		Reply.Code = EVoxelEditCode::Stale;
		Reply.Reason = TEXT("Inventory.StoreChanged");
		return Reply;
	}
	if (InIntent.Action == EVoxelEditAction::BreakCancel)
	{
		Breaking.Remove(InSource);
		Reply.Code = EVoxelEditCode::Accepted;
		return Reply;
	}

	FVoxelTraceResult Hit;
	if (!VerifyView(InController, InSource, InIntent, Hit, Reply.Reason))
	{
		Reply.Code = Hit.Status == EVoxelTraceStatus::NeedsData ?
			EVoxelEditCode::NeedsData :
			EVoxelEditCode::Rejected;
		Breaking.Remove(InSource);
		return Reply;
	}
	const FVoxelSection* TargetSection = Runtime->FindSection(VoxelModuleToSection(Hit.Index));
	if (!TargetSection || TargetSection->CommittedRevision != InIntent.ExpectedRevision)
	{
		Reply.Code = EVoxelEditCode::Stale;
		return Reply;
	}

	const double Now = FPlatformTime::Seconds();
	if (InIntent.Action == EVoxelEditAction::BreakBegin)
	{
		FBreak Break;
		Break.InventoryStoreId = InIntent.InventoryStoreId;
		Break.Target = Hit.Index;
		Break.Expected = Hit.State;
		Break.Began = Now;
		Break.LastPulse = Now;
		Breaking.Add(InSource, Break);
		Reply.Code = EVoxelEditCode::Pending;
		return Reply;
	}
	if (InIntent.Action == EVoxelEditAction::BreakPulse)
	{
		FBreak* Break = Breaking.Find(InSource);
		if (!Break ||
			Break->InventoryStoreId != InIntent.InventoryStoreId ||
			Break->Target != Hit.Index ||
			Break->Expected != Hit.State ||
			Now - Break->LastPulse > 0.35)
		{
			Breaking.Remove(InSource);
			Reply.Code = EVoxelEditCode::Stale;
			return Reply;
		}
		Break->LastPulse = Now;
		const FVoxelRuntimeDefinition* Definition = Registry.GetSnapshot()->Find(Hit.State.TypeId);
		if (!Definition || (Now - Break->Began) * 1000.0 < Definition->BreakMilliseconds)
		{
			Reply.Code = EVoxelEditCode::Pending;
			return Reply;
		}
	}

    UAbilityInventoryBase* Inventory = ResolveInventory(InController, InSource);
    if(!System || !System->IsReady() || !Inventory || !InventoryBridge || !InventoryContribution)
    {
        Reply.Reason = TEXT("Inventory.WorldNotReady");
        return Reply;
    }
    const FInventorySnapshot Snapshot = Inventory->MakeSnapshot();
    const UVoxelAgentComponent* Agent = InSource->FindComponentByClass<UVoxelAgentComponent>();
    bTrustedStandaloneCreative = bTrustedStandaloneCreative && GetWorld()->GetNetMode() == NM_Standalone && Agent && Agent->bCreativeInStandalone;
    uint16 PlaceType = 0;
    const FInventorySlotState* Slot = nullptr;
    if(InIntent.Action == EVoxelEditAction::Place)
    {
        FPrimaryAssetId VoxelId = InIntent.ExpectedItemID;
        if(!bTrustedStandaloneCreative)
        {
            const IAbilityInventoryAgentInterface* InventoryAgent = Cast<IAbilityInventoryAgentInterface>(InSource);
            Slot = Snapshot.FindSlot(InIntent.InventorySlot);
            if(!InventoryAgent || InIntent.InventorySlot != InventoryAgent->GetSelectedInventorySlot() ||
                Snapshot.InventoryId != InIntent.ExpectedInventoryId || Snapshot.Revision != InIntent.ExpectedInventoryRevision ||
                Snapshot.Generation != InIntent.ExpectedInventoryGeneration || !Slot || Slot->bLocked || Slot->Item.IsEmpty() ||
                Slot->Item.ItemId != InIntent.ExpectedItemID)
            {
                Reply.Code = EVoxelEditCode::Stale;
                Reply.Reason = TEXT("Inventory.SelectedSlotChanged");
                return Reply;
            }
            const FInventoryItemDefinition* ItemDefinition = System->GetRegistry().Find(Slot->Item.ItemId);
            if(!ItemDefinition || !ItemDefinition->PlaceableVoxelId.IsValid())
            {
                Reply.Reason = TEXT("Inventory.ItemNotPlaceable");
                return Reply;
            }
            VoxelId = ItemDefinition->PlaceableVoxelId;
        }
        const FVoxelRuntimeDefinition* Definition = Registry.GetSnapshot()->Find(VoxelId);
        if(!Definition)
        {
            Reply.Reason = TEXT("Inventory.PlaceableVoxelUnknown");
            return Reply;
        }
        PlaceType = Definition->TypeId;
    }
    FVoxelInteractionPlan Plan;
    if(!BuildInteractionPlan(Hit, InIntent.Action, PlaceType, InIntent.Direction, InSource, Plan, Reply.Reason) ||
        !ValidateInteractionPlan(Plan, Reply.Reason))
    {
        return Reply;
    }
    Plan.Action = InIntent.Action;
    Plan.RequestId = InIntent.RequestId;
    const FString EventText = FString::Printf(TEXT("Voxel/%s/%s/%s/%llu/%d"),
        *System->GetWorldEpoch().ToString(EGuidFormats::Digits), *System->GetStoreId().ToString(EGuidFormats::Digits), *Snapshot.OwnerId.ToString(EGuidFormats::Digits),
        InIntent.RequestId, static_cast<int32>(InIntent.Action));
    const FTCHARToUTF8 EventUtf8(*EventText);
    FMD5 EventHash;
    EventHash.Update(reinterpret_cast<const uint8*>(EventUtf8.Get()), EventUtf8.Length());
    uint8 EventDigest[16];
    EventHash.Final(EventDigest);
    FVoxelByteReader EventReader(MakeArrayView(EventDigest));
    Plan.EventId = EventReader.Guid();
    FInventoryBatchRequest Request;
    if(Slot && Plan.Cost > 0)
    {
        FInventoryOperation Remove;
        Remove.Type = EInventoryOperationType::Remove;
        Remove.SourceInventoryId = Snapshot.InventoryId;
        Remove.SourceSlot = Slot->Key;
        Remove.Filter.bMatchSignature = true;
        Remove.Filter.Signature = Slot->Item.Signature();
        Remove.Requested = Plan.Cost;
        Remove.AmountMode = EInventoryAmountMode::Exact;
        Request.Operations.Add(MoveTemp(Remove));
        Request.ExpectedRevisions.Add({Snapshot.InventoryId, Snapshot.Revision, Snapshot.Generation});
    }
    FInventoryCommandResult Result;
    if(!ExecuteInventoryWorldEdit(Plan, InSource, Request, System->MakeServerAuthority(Snapshot.OwnerId, TEXT("Voxel.Interaction")), Result, Reply.Reason))
    {
        Reply.Code = Result.Code == EInventoryResultCode::ConfirmationPending ? EVoxelEditCode::ConfirmationPending :
            Result.Code == EInventoryResultCode::RevisionStale ? EVoxelEditCode::Stale : EVoxelEditCode::Rejected;
        return Reply;
    }
    Breaking.Remove(InSource);
    OnInteractionCommitted(Plan, InSource);
    Reply.Code = EVoxelEditCode::Accepted;
    return Reply;
}


bool UVoxelModule::BuildInteractionPlan(const FVoxelTraceResult& InHit, EVoxelEditAction InAction, uint16 InPlaceType, const FVector& InViewDirection, AActor* InSource, FVoxelInteractionPlan& OutPlan, FString& OutError)
{
	return FVoxelEditTransaction::Build(*Runtime, *Registry.GetSnapshot(), *Shapes, InHit, InAction, InPlaceType, InViewDirection, BlockSize(), OutPlan, OutError);
}

void UVoxelModule::OnInteractionCommitted(const FVoxelInteractionPlan& InPlan, AActor* InSource)
{
}

bool UVoxelModule::ValidateInteractionPlan(FVoxelInteractionPlan& InOutPlan, FString& OutError)
{
	return FVoxelEditTransaction::ValidateBatch(*Runtime, *Registry.GetSnapshot(), *Shapes, InOutPlan.Cells, OutError);
}





bool UVoxelModule::ApplyPrefab(
	const FVoxelPrefabSaveData& InPrefab,
	const FIntVector& InOrigin,
	FString& OutError)
{
	if (!IsAuthority() || !IsReady() || bMutating || InPrefab.Cells.IsEmpty())
	{
		OutError = TEXT("Invalid prefab operation");
		return false;
	}
	if (!UVoxelPrefabData::ValidateCells(InPrefab, *Registry.GetSnapshot(), OutError))
	{
		return false;
	}
	TArray<FVoxelCellEdit> Cells;
	TSet<FIntVector> Seen;
	for (const FVoxelPrefabCell& Cell : InPrefab.Cells)
	{
		const int64 X = static_cast<int64>(InOrigin.X) + Cell.Offset.X;
		const int64 Y = static_cast<int64>(InOrigin.Y) + Cell.Offset.Y;
		const int64 Z = static_cast<int64>(InOrigin.Z) + Cell.Offset.Z;
		if (FMath::Abs(X) >= VoxelBlock::MaxAbsCoordinate ||
			FMath::Abs(Y) >= VoxelBlock::MaxAbsCoordinate ||
			FMath::Abs(Z) >= VoxelBlock::MaxAbsCoordinate)
		{
			OutError = TEXT("Prefab target exceeds voxel coordinate bounds");
			return false;
		}
		FVoxelCellEdit Edit;
		Edit.Position = FIntVector(static_cast<int32>(X), static_cast<int32>(Y), static_cast<int32>(Z));
		if (Seen.Contains(Edit.Position) ||
			!Runtime->TryGetBlock(Edit.Position, Edit.Expected) ||
			(!Cell.bClear && !FVoxelItemBridge::ToBlock(*Registry.GetSnapshot(), Cell.Item, Edit.Value)))
		{
			OutError = TEXT("Prefab contains duplicate, unloaded, or invalid cells");
			return false;
		}
		if (Cell.bClear)
		{
			Edit.Value = {};
		}
		Seen.Add(Edit.Position);
		Cells.Add(Edit);
	}
	FVoxelInteractionPlan Plan;
	Plan.Cells = Cells;
	if (!ValidateInteractionPlan(Plan, OutError))
	{
		return false;
	}
	if (PlacementOverlapsActors(Plan))
	{
		OutError = TEXT("Prefab overlaps live actors");
		return false;
	}

    Plan.EventId = FGuid::NewGuid();
    Plan.ObjectAction = TEXT("World.ApplyPrefab");
    UInventoryWorldSubsystem* System = GetWorld()->GetSubsystem<UInventoryWorldSubsystem>();
    if(!System)
    {
        OutError = TEXT("Inventory.WorldNotReady");
        return false;
    }
    FInventoryBatchRequest Request;
    FInventoryCommandResult Result;
    return ExecuteInventoryWorldEdit(Plan, nullptr, Request, System->MakeServerAuthority(System->GetWorldId(), TEXT("World.ApplyPrefab")), Result, OutError);
}


bool UVoxelModule::ExportPrefab(
	const FIntVector& InMin,
	const FIntVector& InMax,
	FVoxelPrefabSaveData& OutPrefab,
	FString& OutError) const
{
	if (!IsReady())
	{
		OutError = TEXT("Voxel world is not running");
		return false;
	}
	const int64 SizeX = static_cast<int64>(InMax.X) - InMin.X;
	const int64 SizeY = static_cast<int64>(InMax.Y) - InMin.Y;
	const int64 SizeZ = static_cast<int64>(InMax.Z) - InMin.Z;
	if (SizeX <= 0 || SizeY <= 0 || SizeZ <= 0)
	{
		OutError = TEXT("Prefab bounds are empty");
		return false;
	}
	const FIntVector Center(
		static_cast<int32>(static_cast<int64>(InMin.X) + SizeX / 2),
		static_cast<int32>(static_cast<int64>(InMin.Y) + SizeY / 2),
		static_cast<int32>(static_cast<int64>(InMin.Z) + SizeZ / 2));
	FVoxelPrefabSaveData Prefab;
	for (int32 Z = InMin.Z; Z < InMax.Z; ++Z)
	{
		for (int32 Y = InMin.Y; Y < InMax.Y; ++Y)
		{
			for (int32 X = InMin.X; X < InMax.X; ++X)
			{
				const FIntVector Position(X, Y, Z);
				FVoxelBlockState State;
				if (!Runtime->TryGetBlock(Position, State))
				{
					OutError = TEXT("Prefab selection is not fully loaded");
					return false;
				}
				if (State.IsAir())
				{
					continue;
				}
				const FVoxelRuntimeDefinition* Definition = Registry.GetSnapshot()->Find(State.TypeId);
				if (Definition && Definition->EntityKind)
				{
					const FVoxelSection* Section = Runtime->FindSection(VoxelModuleToSection(Position));
					const FVoxelBlockEntityState* Entity = Section ? Section->Entities.Find(VoxelModuleToCellIndex(Position)) : nullptr;
					FVoxelBlockEntityState Default;
					if (!Entity ||
						!FVoxelBlockEntityCodec::MakeDefault(Definition->EntityKind, Default, Definition->EntityVariant) ||
						!(*Entity == Default))
					{
						OutError = TEXT("Prefab export requires default block entity state");
						return false;
					}
				}
				FVoxelPrefabCell Cell;
				Cell.Offset = Position - Center;
				if (!FVoxelItemBridge::ToItem(*Registry.GetSnapshot(), State, 1, Cell.Item))
				{
					return false;
				}
				Prefab.Cells.Add(MoveTemp(Cell));
			}
		}
	}
	OutPrefab = MoveTemp(Prefab);
	OutError.Reset();
	return true;
}

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
			WorldGenerationProfileAsset;
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

	WorldGenerationProfileAsset =
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

	Result->GenerationProfile =
		WorldGenerationProfileAsset;

	Result->Seed =
		DefaultWorldSeed;

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
	if (SavedManifest.GeneratorVersion != FVoxelGenerationRecipe::CurrentAlgorithmVersion)
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
	if (!Config.Recipe || Config.Recipe->RecipeHash != SavedManifest.RecipeHash)
	{
		OutError = TEXT("Saved voxel RecipeHash differs from the current generation profile");
		return false;
	}
	const uint64 ExpectedGenerationSignature =
		BuildGenerationSignature(SavedManifest, GetGenerationIdentitySalt());
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
	if (!IsAuthority())
	{
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

	if (!PHASEC(
		InPhase,
		EPhase::Primary))
	{
		return;
	}

	FString Error;

	const FVoxelWorldSaveData* Data =
		InData.
			GetPtr<
				FVoxelWorldSaveData>();

	if (!Data)
	{
		bWorldLoadRejected =
			true;

		WorldState =
			EVoxelWorldState::
				Failed;

		LastSaveError =
			TEXT(
				"Voxel module expected FVoxelWorldSaveData");

		UE_LOG(
			LogTemp,
			Error,
			TEXT("Voxel world load rejected: %s (parameter struct=%s)."),
			*LastSaveError,
			InData.GetStructType()
				? *InData.GetStructType()->GetPathName()
				: TEXT("None"));

		return;
	}

	if (Runtime &&
		!StopWorld(
			true,
			Error))
	{
		bWorldLoadRejected =
			true;

		WorldState =
			EVoxelWorldState::
				Failed;

		LastSaveError =
			Error;

		return;
	}

	if (!ApplyGenerationProfileFromSave(
			*Data,
			Error))
	{
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
				Error))
		{
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
		WorldGenerationProfileAsset;

	Data.Seed =
		Manifest.
			Settings.Seed;

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
		if (!StopWorld(true, Error))
		{
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
			LastSaveError = FString::Printf(TEXT("Scene capture capability error: indivisible step %.3fms exceeds configured %.3fms"),
				Actual, MaximumSceneCaptureStepMilliseconds);
			CapturedSceneSave.Reset();
			Step = EVoxelSceneCaptureStep::Failed;
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

bool UVoxelModule::IsSourceAdmittedAt(const FGuid& InId, const FIntVector& InCell, const uint64 InEpoch) const
{
	const FVoxelSourceInterest* Accepted = CurrentInterest->Sources.Find(InId);
	return Epoch == InEpoch && Accepted && Accepted->Source.Center == InCell && IsSourceAdmitted(InId);
}

void UVoxelModule::CancelProjectTask(const FVoxelTaskStamp& InStamp)
{
	if (Scheduler)
	{
		Scheduler->CancelMatching(
			[&InStamp](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
			{
				return Kind == EVoxelTaskKind::ProjectBackground && Stamp == InStamp;
			});
	}
}

TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> UVoxelModule::GetCurrentInterestSnapshot() const
{
	return CurrentInterest;
}

void UVoxelModule::CancelTerrainNetworkTask(const FVoxelTaskStamp& InStamp)
{
	if (Scheduler)
		Scheduler->CancelMatching([&InStamp](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::NetworkRepresentation && Stamp == InStamp;
		});
}

TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> UVoxelModule::GetDesiredInterestSnapshot() const
{
	return DesiredInterest;
}

bool UVoxelModule::CanCommitTerrainInterest(const FVoxelInterestSet& InSnapshot) const
{
	const auto CurrentRegistry = GetRegistry();
	if (!Runtime || &InSnapshot != DesiredInterest.Get() ||
		InSnapshot.PublicationRevision < CurrentInterest->PublicationRevision ||
		InSnapshot.SafetyWorldEpoch != Epoch || !InSnapshot.SafetyVersions || !Shapes || !CurrentRegistry ||
		InSnapshot.SafetyShapeRevision != Shapes->GetContentHash() || InSnapshot.SafetyRegistryRevision != CurrentRegistry->Hash) return false;
	for (const auto& Pair : InSnapshot.Sources)
	{
		const FSource* Source = Sources.Find(Pair.Key);
		if (!Source || !Source->Owner.IsValid()) return false;
		if (Pair.Value.Source.Has(EVoxelStreamingCapability::FineVisual) &&
			(!Pair.Value.FineBounds.IsValid() || !Runtime->GetChangeHierarchy().IsVersionSnapshotCurrent(*InSnapshot.SafetyVersions, Pair.Value.FineBounds))) return false;
	}
	return true;
}

void UVoxelModule::CommitTerrainInterest(TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> InSnapshot)
{
	check(IsInGameThread());
	check(InSnapshot && CanCommitTerrainInterest(*InSnapshot));
	const auto PreviousInterest = CurrentInterest;
	CurrentInterest = MoveTemp(InSnapshot);
	if (CurrentInterest->PlanningWorkReceipt) CurrentInterest->PlanningWorkReceipt->Resolve(true);
	InterestRuntime = DesiredInterestRuntime;
	check(InterestRevision < MAX_uint64);
	++InterestRevision;
	for (auto& Pair : Sources)
	{
		if (const auto* Accepted = CurrentInterest->Sources.Find(Pair.Key))
		{
			const auto* Previous = PreviousInterest->Sources.Find(Pair.Key);
			if (!Previous || Previous->GetCurrentWindow().Serial != Accepted->GetCurrentWindow().Serial)
			{
				check(Pair.Value.ContractRevision < MAX_uint64);
				++Pair.Value.ContractRevision;
			}
			Pair.Value.PlannedValue = Accepted->Source;
			if (!Accepted->bRequiresTravelBarrier) Pair.Value.LastAcceptedPosition = Accepted->Source.Center;
		}
	}
	if (DesiredInterestTaskPayload)
	{
		const auto Payload = StaticCastSharedPtr<const FVoxelInterestTaskPayload>(DesiredInterestTaskPayload);
		check(Payload->Interest == CurrentInterest);
		QueueSimulationDemandEvents(Payload);
		if (ResidencyManager) ResidencyManager->QueueDemandEvents(Payload);
		DesiredInterestTaskPayload.Reset();
	}
}

void UVoxelModule::AbandonTerrainInterest(const FVoxelInterestSet& InSnapshot)
{
	if (&InSnapshot != DesiredInterest.Get()) return;
	DesiredInterest = CurrentInterest;
	DesiredInterestRuntime = InterestRuntime;
	DesiredInterestTaskPayload.Reset();
	bInterestDirty = true;
}

void UVoxelModule::NotifyTerrainSectionReplacement(const FIntVector& InSection, const uint64 InWorldEpoch)
{
	check(IsInGameThread());
	if (!Runtime || !ResidencyManager || InWorldEpoch != Runtime->Epoch()) return;
	if (const auto* Section = Runtime->FindSection(InSection))
		ResidencyManager->WakeRetirement(InSection, Section->Stamp, Section->Retirement.Serial);
}

bool UVoxelModule::IsTerrainSafeAt(const FGuid& InSource, const FBox& InWorldBounds) const
{
	const auto* Source = CurrentInterest->Sources.Find(InSource);
	if (!IsReady() || !Source || !IsSourceAdmitted(InSource) || !InWorldBounds.IsValid ||
		InWorldBounds.Min.ContainsNaN() || InWorldBounds.Max.ContainsNaN()) return false;
	const bool bRequiresVisual = Source->Source.Has(EVoxelStreamingCapability::FineVisual);
	// 角色安全依据本次实际占用范围；驻留窗口的预测屏障不能阻止在已就绪地形上落地。
	if (bRequiresVisual && !ViewManager) return false;
	const FVector Minimum = InWorldBounds.Min / BlockSize();
	const FVector Maximum = InWorldBounds.Max / BlockSize();
	const FIntVector First(FMath::FloorToInt(Minimum.X / 16.0), FMath::FloorToInt(Minimum.Y / 16.0), FMath::FloorToInt(Minimum.Z / 16.0));
	const FIntVector Last(FMath::CeilToInt(Maximum.X / 16.0) - 1, FMath::CeilToInt(Maximum.Y / 16.0) - 1, FMath::CeilToInt(Maximum.Z / 16.0) - 1);
	const FIntVector Count = Last - First + FIntVector(1);
	if (Count.GetMin() <= 0 || Count.GetMax() > 64 || int64(Count.X) * Count.Y * Count.Z > 64) return false;
	for (int32 Z = First.Z; Z <= Last.Z; ++Z)
		for (int32 Y = First.Y; Y <= Last.Y; ++Y)
			for (int32 X = First.X; X <= Last.X; ++X)
			{
				const FIntVector Key(X, Y, Z);
				if (!IsCollisionReady(Key) || (bRequiresVisual && !ViewManager->IsLatestFinePresented(Key))) return false;
			}
	return true;
}

void UVoxelModule::LogTerrainGeometryHistogram() const
{
	const double Now = FPlatformTime::Seconds();
	if (Now < NextTerrainGeometryLogSeconds) return;
	NextTerrainGeometryLogSeconds = Now + 5.0;
	// 采样由 ViewManager 在同一帧期限内推进；日志只读取固定数量的桶，不遍历世界或网格。
	if (const auto Statistics = GetTerrainStatistics())
		UE_LOG(LogTemp, Display, TEXT("Voxel target readiness histogram (incremental owner samples):\n%s"), *Statistics->ToCsv());
}



void UVoxelModule::BindInventoryBridge(TSharedRef<FVoxelInventoryDomainBridge> InBridge, FInventoryVoxelContribution InContribution)
{
    InventoryBridge = InBridge;
    InventoryContribution = MoveTemp(InContribution);
    const TWeakObjectPtr<UVoxelModule> Weak(this);
    InBridge->SetPublisher([Weak](const FVoxelEditBatch& Batch)
    {
        if(Weak.IsValid())
        {
            Weak->PublishProjectEdit(Batch);
        }
    });
}

bool UVoxelModule::ExecuteInventoryWorldEdit(FVoxelInteractionPlan& InOutPlan, AActor* InSource,
    FInventoryBatchRequest& InOutRequest, const FInventoryAuthorityContext& InAuthority,
    FInventoryCommandResult& OutResult, FString& OutError)
{
    UInventoryWorldSubsystem* System = GetWorld()->GetSubsystem<UInventoryWorldSubsystem>();
    if(!System || !System->IsReady() || !IsAuthority() || !IsReady() || bMutating || !InventoryBridge || !InventoryContribution)
    {
        OutError = TEXT("Inventory.WorldEditUnavailable");
        return false;
    }
    TGuardValue<bool> Guard(bMutating, true);
    if(!InventoryContribution(InOutPlan, InSource, InOutRequest, OutError) ||
        !ValidateInteractionPlan(InOutPlan, OutError) || PlacementOverlapsActors(InOutPlan) ||
        !InventoryBridge->AppendEdit(InOutPlan.Cells, InOutPlan.Entities, InOutRequest.DomainChanges, OutError))
    {
        if(OutError.IsEmpty()) OutError = TEXT("Inventory.WorldEditOverlapsActor");
        return false;
    }
    if(InOutRequest.Operations.IsEmpty() && InOutRequest.CreateInventories.IsEmpty() && InOutRequest.DeleteInventories.IsEmpty() && InOutRequest.DomainChanges.IsEmpty())
    {
        OutResult.Code = EInventoryResultCode::Success;
        // 已通过交互规划与权限校验的非经济 Use 可直接触发其表现事件。
        // 游戏可以注册开窗、休息等动作；没有状态改动时不伪造 WAL 写入。
        return InOutPlan.Action == EVoxelEditAction::Use && ::IsValid(InSource) &&
            InOutPlan.ObjectId.IsValid() && !InOutPlan.ObjectAction.IsNone();
    }
    OutResult = System->ExecuteAuthorizedBatch(InOutRequest, InAuthority);
    OutError = OutResult.IsSuccess() ? FString() : StaticEnum<EInventoryResultCode>()->GetNameStringByValue(static_cast<int64>(OutResult.Code));
    return OutResult.IsSuccess();
}
