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
#include "GameFramework/Pawn.h"
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

	DefaultData.GenerationProfile = TSoftObjectPtr<UVoxelWorldGenerationProfile>(
		FSoftObjectPath(TEXT("/WHFramework/Voxel/Generation/DA_VoxelWorldGenerationProfile_Default.DA_VoxelWorldGenerationProfile_Default")));

	WorldData =
		MakeUnique<
			FVoxelModuleSaveData>();
}

UVoxelModule::~UVoxelModule()
{
	if (Runtime || PendingTerrainFactResources || bWorldRetirementStarted)
		UE_LOG(LogTemp, Error, TEXT("体素模块在未完成安全退役时析构，未能证明状态已持久化。"));
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

TSoftObjectPtr<UVoxelMaterialSet> UVoxelModule::ResolveMaterialSetAsset() const
{
	return MaterialSetAsset;
}

uint64 UVoxelModule::GetGenerationIdentitySalt(const EVoxelTerrainType InTerrainType) const
{
	(void)InTerrainType;
	return 0;
}

bool UVoxelModule::ResolveGenerationIdentitySalt(const EVoxelTerrainType InTerrainType, uint64& OutSalt) const
{
	OutSalt = GetGenerationIdentitySalt(InTerrainType);
	if (InTerrainType != EVoxelTerrainType::Superflat) return true;
	const auto Snapshot = Registry.GetSnapshot();
	if (!Snapshot || !Shapes) return false;
	const uint64 SupportIdentity = VoxelBinary::Mix64(Snapshot->SuperflatSupportHash ^ Shapes->GetCollisionTopHash() ^ 0x53464C4F4F523031ull);
	OutSalt = VoxelBinary::Mix64(VoxelBinary::Mix64(OutSalt) ^ SupportIdentity);
	if (OutSalt == 0) OutSalt = 1;
	return true;
}

#if WITH_EDITOR
void UVoxelModule::OnDestroy()
{
	Super::OnDestroy();
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
		MaterialSet = ResolveMaterialSetAsset().LoadSynchronous();
		if (!MaterialSet || !MaterialSet->Validate(Error))
		{
			WorldState = EVoxelWorldState::Failed;
			UE_LOG(LogTemp, Error, TEXT("Voxel material set: %s"), *Error);
			return;
		}
	}
	ActiveGenerationProfileAsset = DefaultData.GenerationProfile;
	WorldGenerationProfile = ActiveGenerationProfileAsset.LoadSynchronous();
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
		if (!CreateWorldFromProfile(ResolveDefaultWorldSeed(), Error, DefaultData.TerrainType))
		{
			UE_LOG(LogTemp, Error, TEXT("Voxel auto world creation: %s"), *Error);
		}
	}
}

void UVoxelModule::OnRefresh(
	const float InDeltaSeconds,
	const bool bInEditor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ModuleRefresh);
	if (IsWorldSaveSuspended()) return;
	Super::OnRefresh(
		InDeltaSeconds,
		bInEditor);

	if (bInEditor)
	{
		return;
	}
	USaveGameModule* SaveCleanupOwner = nullptr;
	const bool bTerrainReady = Runtime && (WorldState == EVoxelWorldState::Running || WorldState == EVoxelWorldState::Initializing);
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

	const bool bMapFirst = (GFrameCounter & 3) == 0;
	if (MapTileCache && bMapFirst)
	{
		MapTileCache->Tick(TerrainFrameContext.Deadline(0.25));
	}

	if (IsAuthority()) RegionStore.AdvanceDirectoryLoad(*Scheduler, Epoch,
		[this, ExpectedEpoch = Epoch](TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> InDirectory)
		{
			if (Runtime && Epoch == ExpectedEpoch)
			{
				Runtime->GetChangeIndex().SetBaseline(MoveTemp(InDirectory));
				bInterestDirty = true;
				if (WorldState == EVoxelWorldState::Initializing)
				{
					WorldState = EVoxelWorldState::Running;
					OnWorldInitialized.Broadcast();
				}
			}
		});
	if (WorldState != EVoxelWorldState::Running && WorldState != EVoxelWorldState::Initializing)
	{
		TerrainFrameContext.End(FPlatformTime::Seconds());
		return;
	}
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
	if (WorldState != EVoxelWorldState::Running)
	{
		TerrainFrameContext.End(FPlatformTime::Seconds());
		return;
	}
	const double AfterScheduler = FPlatformTime::Seconds();
	const double NetworkDeadline = TerrainFrameContext.Deadline(0.1);
	const int32 NetworkServiceCount = TerrainNetworkComponents.Num() + (OverlayReplica ? 1 : 0);
	for (int32 Count = 0; Count < NetworkServiceCount && FPlatformTime::Seconds() < NetworkDeadline; ++Count)
	{
		TerrainNetworkCursor %= NetworkServiceCount;
		const int32 Service = TerrainNetworkCursor++;
		if (Service < TerrainNetworkComponents.Num())
		{
			if (auto* Network = TerrainNetworkComponents[Service].Get()) Network->AdvanceTerrainNetwork(TerrainFrameContext);
		}
		else
		{
			FString OverlayError;
			OverlayReplica->Advance(1, OverlayError);
		}
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
	// 只有等待安全覆盖或窗口交接时，远处新回执才能改变驻留决策。
	// 来源移动、配置和编辑仍通过各自的变更入口立即触发规划。
	if (InterestRuntime && InterestRuntime->NeedsCoverageFeedback() &&
		((RenderCoverage ? RenderCoverage->GetSerial() : 0) != LastPlannedRenderCoverageSerial ||
		 (PhysicsCoverage ? PhysicsCoverage->Serial : 0) != LastPlannedPhysicsCoverageSerial))
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
	// 碰撞、数据准入和可见发布轮流获得帧预算。
	const bool bCollisionFirst = GFrameCounter % 3 == 0;
	double EarlyCollisionMilliseconds = 0.0;
	if (bCollisionFirst && CollisionPresenter)
	{
		const double CollisionStart = FPlatformTime::Seconds();
		CollisionPresenter->Tick(CurrentInterest->Exact, InterestRevision);
		EarlyCollisionMilliseconds = (FPlatformTime::Seconds() - CollisionStart) * 1000.0;
	}
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

	if (!bCollisionFirst && CollisionPresenter)
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

	if (MapTileCache && !bMapFirst)
	{
		MapTileCache->Tick(TerrainFrameContext.Deadline(0.25));
	}

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
			EarlyCollisionMilliseconds + (AfterCollision - AfterResidency) * 1000, (AfterNavigation - AfterCollision) * 1000, ViewMilliseconds,
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
			const FVoxelGenerationCacheStats CacheStats = GenerationCache->GetStats(false);
			UE_LOG(LogTemp, Display,
				TEXT("Voxel cache: base=%d natural=%d hydro=%d cave=%d ecology=%d feature=%d structure=%d waits=%llu waitMs=%.2f"),
				CacheStats.BaseColumns,
				CacheStats.NaturalColumns,
				CacheStats.Hydrology,
				CacheStats.Caves,
				CacheStats.Ecology,
				CacheStats.Features,
				CacheStats.Structures,
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

void UVoxelModule::OnPause()
{
	Super::OnPause();
	if (Scheduler) Scheduler->SetPaused(true);
}

void UVoxelModule::OnUnPause()
{
	Super::OnUnPause();
	if (Scheduler) Scheduler->SetPaused(false);
}

void UVoxelModule::OnTermination(const EPhase InPhase)
{
	if (PHASEC(InPhase, EPhase::Final))
	{
		if (Scheduler) Scheduler->StopAndJoin();
		FString Error;
		if (!StopWorld(false, Error))
		{
			RecordRetirementFailure(Error);
			UE_LOG(LogTemp, Error, TEXT("Voxel world could not be retired during host termination: %s"), *Error);
			// 持久化失败不能宣称世界已退役；世界正在销毁时仍须在 GC 前释放已排空的导航呈现。
			if (GetWorld() && GetWorld()->bIsTearingDown && NavigationPresenter &&
				(bWorldRetirementStarted || NavigationPresenter->TryCloseForWorldStop()))
			{
				NavigationPresenter->Reset();
			}
		}
	}
	Super::OnTermination(InPhase);
}

bool UVoxelModule::HasUnpersistedRetirementState() const
{
	return Runtime && Runtime->IsServer() &&
		(Runtime->HasUnpersistedChanges() || HasUnpersistedSceneState() || (!bPersistenceEnabledForCurrentWorld && SceneRegionHead));
}

void UVoxelModule::RecordRetirementFailure(const FString& InError)
{
	RetirementError = InError;
	RetirementStatus = HasUnpersistedRetirementState() ? EVoxelRetirementStatus::AwaitingPersistence : EVoxelRetirementStatus::AwaitingSettlement;
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

bool UVoxelModule::CanRetireWorld(FString& OutError) const
{
	OutError.Reset();
	return true;
}

bool UVoxelModule::OnWorldStopping(FString& OutError)
{
	OutError.Reset();
	return true;
}

void UVoxelModule::OnSimulationResidencyChanged(const FVoxelSimulationResidencyDelta& InDelta)
{
}

bool UVoxelModule::StopWorld(const bool bDiscardDirty, FString& OutError)
{
	return StopWorldInternal(bDiscardDirty, OutError);
}

bool UVoxelModule::CanStopWorld(FString& OutError) const
{
	if (bSceneTransitionActive || TerrainFrameContext.IsActive() || IsWorldSaveSuspended() ||
		bWorldStartInProgress || bPartialWorldRetirementInProgress)
	{
		OutError = TEXT("Voxel world has an active scene, frame, save or startup operation");
		return false;
	}
	if (!Runtime) { OutError.Reset(); return true; }
	if (!CanRetireWorld(OutError)) return false;
	if (!bWorldRetirementStarted && (SaveAdapter.IsBusy() || SceneSaveBuilder.IsSet() ||
		(Runtime->IsServer() && (Runtime->HasUnpersistedChanges() || HasUnpersistedSceneState() ||
			(!bPersistenceEnabledForCurrentWorld && SceneRegionHead)))))
	{
		OutError = TEXT("Voxel world still owns unsaved or in-flight state");
		return false;
	}
	if (!bWorldRetirementStarted && NavigationPresenter && NavigationPresenter->HasReaders())
	{
		OutError = TEXT("Voxel navigation still has active readers");
		return false;
	}
	OutError.Reset();
	return true;
}

bool UVoxelModule::RetirePartialWorldStart(FString& OutError)
{
	UWorld* const World = GetWorld();
	if (Runtime || bWorldStartInProgress || bPartialWorldRetirementInProgress || !World)
	{
		OutError = TEXT("Partial voxel startup cannot yet be retired or its prior callback result is unknown");
		return false;
	}
	if (!bPartialWorldStartNeedsRetirement && !PendingTerrainFactResources && !PendingTerrainWorldControl.IsValid() &&
		!Scheduler && !GenerationCache && !GenerationConfig && !Generator)
	{
		WorldState = EVoxelWorldState::None;
		OutError.Reset();
		return true;
	}
	TGuardValue<bool> Retiring(bPartialWorldRetirementInProgress, true);
	bPartialWorldStartNeedsRetirement = true;
	if (Scheduler) Scheduler->StopAndJoin();
	if (PartialWorldNetworks.IsEmpty() && PartialWorldNetworkCursor == 0) PartialWorldNetworks = TerrainNetworkComponents;
	while (PartialWorldNetworkCursor < PartialWorldNetworks.Num())
	{
		if (UVoxelModuleNetworkComponent* Network = PartialWorldNetworks[PartialWorldNetworkCursor].Get())
		{
			Network->OnTerrainWorldStopping();
			++PartialWorldNetworkCursor;
			continue;
		}
		++PartialWorldNetworkCursor;
	}
	PartialWorldNetworks.Reset();
	PartialWorldNetworkCursor = 0;
	PendingTerrainWorldControl.Reset();
	Scheduler.Reset();
	Generator.Reset();
	GenerationCache.Reset();
	GenerationConfig.Reset();
	TerrainFrameContext.Reset();
	PendingTerrainFactResources.Reset();
	PendingTerrainFactControl = {};
	bPartialWorldStartNeedsRetirement = false;
	WorldState = EVoxelWorldState::None;
	OutError.Reset();
	return true;
}

bool UVoxelModule::StopWorldInternal(const bool bDiscardDirty, FString& OutError)
{
	if (bWorldStartInProgress)
	{
		OutError = TEXT("Voxel startup must unwind before world retirement can begin");
		return false;
	}
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
	if (IsWorldSaveSuspended())
	{
		OutError = TEXT("Voxel world retirement is waiting for the original save suspension owner to release its token");
		return false;
	}
	if (!Runtime)
	{
		if (PendingTerrainFactResources && PendingTerrainFactControl.WorldEpoch > Epoch) Epoch = PendingTerrainFactControl.WorldEpoch;
		return RetirePartialWorldStart(OutError);
	}
	if (!CanRetireWorld(OutError)) return false;
	if (!bWorldRetirementStarted && (SaveAdapter.IsBusy() || SceneSaveBuilder.IsSet()))
	{
		OutError = TEXT("Finish the SaveGame transaction before closing the voxel world");
		return false;
	}
	if (!bWorldRetirementStarted && !bDiscardDirty && Runtime->IsServer() && Runtime->HasUnpersistedChanges())
	{
		OutError = TEXT("World contains unpersisted voxel journal versions");
		return false;
	}
	if (!bWorldRetirementStarted && !bDiscardDirty && Runtime->IsServer() && (HasUnpersistedSceneState() || (!bPersistenceEnabledForCurrentWorld && SceneRegionHead)))
	{
		OutError = TEXT("World contains scene owners awaiting an actual committed SaveGame cut");
		return false;
	}
	if (!bWorldRetirementStarted && NavigationPresenter && !NavigationPresenter->TryCloseForWorldStop())
	{
		OutError = TEXT("Voxel world control must wait for actual navigation readers to release their inputs");
		return false;
	}


	TGuardValue<bool> SceneTransition(bSceneTransitionActive, true);
	if (!bWorldRetirementStarted)
	{
		bWorldRetirementDiscarded = bDiscardDirty && HasUnpersistedRetirementState();
		bWorldRetirementStarted = true;
		WorldState = EVoxelWorldState::Closing;
		WorldSaveSuspension.Reset();
		PendingTerrainWorldControl.Reset();
		if (Scheduler) Scheduler->StopAndJoin();
		const auto Networks = TerrainNetworkComponents;
		for (const auto& Component : Networks)
		{
			if (auto* Network = Component.Get()) Network->OnTerrainWorldStopping();

		}
		WorldRetirementStage = 1;
	}
	if (WorldRetirementStage == 1)
	{
		if (!OnWorldStopping(OutError)) return false;
		WorldRetirementStage = 2;
	}
	if (WorldRetirementStage == 2)
	{
		TArray<FIntVector> RegionKeys;
		SceneRegions.GetKeys(RegionKeys);
		for (const FIntVector& Key : RegionKeys)
		{
			if (RetiredSceneRegions.Contains(Key)) continue;
			UVoxelSceneRegion* Region = SceneRegions.FindRef(Key);
			if (Region)
			{
				while (Region->SimulationFailureHead) ReleaseSimulationSectionFromRegion(*Region->SimulationFailureHead);
				while (Region->SimulationSaveHead) ReleaseSimulationSectionFromSave(*Region->SimulationSaveHead);
				Region->Shutdown();
			}
			RetiredSceneRegions.Add(Key);

		}
		WorldRetirementStage = 3;
	}
	MapTileCache.Reset();
	if (WorldRetirementStage == 3)
	{
		if (ViewManager && !ViewManager->TryResetForWorldStop(OutError)) return false;

		WorldRetirementStage = 4;
	}
	if (WorldRetirementStage == 4)
	{
		if (DetailView && !DetailView->TryResetForWorldStop(OutError)) return false;

		WorldRetirementStage = 5;
	}
	if (WorldRetirementStage == 5)
	{
		if (CollisionPresenter) CollisionPresenter->Reset();

		WorldRetirementStage = 6;
	}
	check(Epoch < MAX_uint64);
	++Epoch;
	ViewManager.Reset();
	NavigationPresenter.Reset();
	CollisionPresenter.Reset();
	DetailView.Reset();
	if (PhysicsRetirement) PhysicsRetirement->Close();
	PhysicsRetirement.Reset();
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
	RetirementStatus = bWorldRetirementDiscarded ? EVoxelRetirementStatus::ExplicitlyDiscarded : EVoxelRetirementStatus::RetiredWithoutUnpersistedChanges;
	bWorldRetirementStarted = false;
	bWorldRetirementDiscarded = false;
	WorldRetirementStage = 0;
	RetiredSceneRegions.Reset();
	if (RetirementStatus == EVoxelRetirementStatus::ExplicitlyDiscarded)
		UE_LOG(LogTemp, Warning, TEXT("调用方明确请求放弃原体素世界的未持久化状态。"));
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

bool UVoxelModule::HasCurrentCollisionFailure(const FIntVector& InSection) const
{
	return IsReady() && CollisionPresenter && CollisionPresenter->HasCurrentCollisionFailure(InSection);
}

bool UVoxelModule::CaptureCollisionReceipt(const FIntVector& InSection, FVoxelPhysicsReceipt& OutReceipt) const
{
	return IsCollisionReady(InSection) && CollisionPresenter->CaptureCurrentReceipt(InSection, OutReceipt);
}

void UVoxelModule::NotifyCollisionChanged(const FIntVector& InSection)
{
	check(IsInGameThread());
	if (ViewManager) ViewManager->OnCollisionChanged(InSection);
	if (NavigationPresenter) NavigationPresenter->InvalidateSection(InSection);
	WakeSimulationSection(InSection);
}

void UVoxelModule::NotifyFineCollisionGeometryReady(const FVoxelFineShardKey& InKey)
{
	check(IsInGameThread());
	if (CollisionPresenter) CollisionPresenter->NotifyFineGeometryReady(InKey);
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
	const FVoxelGenerationBounds& InBounds, const FVoxelTerrainServicePriority& InPriority, const bool bInAllowContinuation)
{
	return EmergeManager ? EmergeManager->EnsureGenerationFacts(InConsumer, InBounds, InPriority, bInAllowContinuation) :
		FVoxelGenerationDependencyStatus{false, true, TEXT("地形事实Owner尚未初始化")};
}

bool UVoxelModule::HasGenerationFactConsumer(const FVoxelGenerationConsumerKey& InConsumer) const
{
	return EmergeManager && EmergeManager->HasGenerationFactConsumer(InConsumer);
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

#if !UE_BUILD_SHIPPING
bool UVoxelModule::TryCountPublishedTerrainMeshes(int32& OutCount, uint64& OutSerial, uint64& OutScratchBytes,
	double& OutMilliseconds, FString& OutError) const
{
	if (ViewManager) return ViewManager->TryCountPublishedTerrainMeshes(OutCount, OutSerial, OutScratchBytes, OutMilliseconds, OutError);
	OutCount = 0;
	OutSerial = OutScratchBytes = 0;
	OutMilliseconds = 0.0;
	OutError = TEXT("Terrain view manager unavailable");
	return false;
}
#endif

FVoxelProxyPublicationState UVoxelModule::CaptureProxyPublicationState(const FVoxelViewKey& InKey, const uint64 InExpectedEpoch) const
{
	if (!IsInGameThread())
	{
		FVoxelProxyPublicationState State;
		State.Error = TEXT("Proxy probe must be queried on the game thread");
		return State;
	}
	if (ViewManager) return ViewManager->CaptureProxyPublicationState(InKey, InExpectedEpoch);
	FVoxelProxyPublicationState State;
	State.Error = TEXT("Proxy view manager is unavailable");
	return State;
}

TMap<FName, FVoxelPlanDiagnostics> UVoxelModule::GetPlanDiagnostics() const
{
	TMap<FName, FVoxelPlanDiagnostics> Result;
	const FName Names[]{TEXT("Hydrology"), TEXT("Cave"), TEXT("Structure"), TEXT("Feature"), TEXT("Ecology")};
	for (uint8 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
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

FVoxelGenerationDependencyStatus UVoxelModule::EnsureEnvironmentSamples(
	const FVoxelGenerationConsumerKey& InConsumer, const FIntPoint& InOrigin,
	const int32 InWidth, const int32 InHeight, const int32 InStep,
	const double InDistanceScore, bool& bOutNeedsAdvance)
{
	bOutNeedsAdvance = false;
	return EmergeManager ? EmergeManager->EnsureEnvironmentSamples(InConsumer, InOrigin,
		InWidth, InHeight, InStep, InDistanceScore, bOutNeedsAdvance)
		: FVoxelGenerationDependencyStatus{false, true, TEXT("Voxel world is unavailable")};
}

FVoxelGenerationDependencyStatus UVoxelModule::EnsureEnvironmentGeometry(
	const FVoxelGenerationConsumerKey& InConsumer, const FVoxelTerrainGeometry& InGeometry,
	const EVoxelWorkClass InWorkClass, const int32 InSourcePriority,
	const double InDistanceScore, const double InForwardScore)
{
	return EmergeManager ? EmergeManager->EnsureEnvironmentGeometry(InConsumer, InGeometry,
		InWorkClass, InSourcePriority, InDistanceScore, InForwardScore)
		: FVoxelGenerationDependencyStatus{false, true, TEXT("Voxel world is unavailable")};
}

void UVoxelModule::VisitGenerationFactSources(const FVoxelGenerationConsumerKey& InConsumer, FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
{
	if (EmergeManager) EmergeManager->VisitGenerationFactSources(InConsumer, InVisit);
}

TSharedPtr<const FVoxelGenerationEnvironmentInputs, ESPMode::ThreadSafe> UVoxelModule::GetEnvironmentGeometryInputs(
	const FVoxelGenerationConsumerKey& InConsumer, const FVoxelTerrainGeometry& InGeometry) const
{
	return EmergeManager ? EmergeManager->GetEnvironmentGeometryInputs(InConsumer, InGeometry) : nullptr;
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
	return ActiveGenerationProfileAsset.IsNull() ? DefaultData.GenerationProfile : ActiveGenerationProfileAsset;
}

int32 UVoxelModule::GetDefaultWorldSeed() const
{
	return DefaultData.Seed;
}

int32 UVoxelModule::ResolveDefaultWorldSeed() const
{
	return DefaultData.Seed == 0 ?
		FMath::Max(1, static_cast<int32>(FGuid::NewGuid().A & 0x7fffffffu)) : DefaultData.Seed;
}

FIntPoint UVoxelModule::GetLocalFineRadiusLimits() const
{
	const FVoxelStreamingSourceView& SourceView = ViewSettings.DefaultSourceView;
	const int32 Minimum = ViewSettings.Fine.MinimumRadiusCells(SourceView);
	const int32 Side = FMath::Max(1, ViewSettings.Fine.ShardSideCells);
	return FIntPoint(Minimum, FMath::Max(Minimum, SourceView.VoxelProxyRadiusCells / Side * Side));
}

bool UVoxelModule::SetLocalFineRadiusCells(int32 InRadiusCells)
{
	if (InRadiusCells < INDEX_NONE) return false;
	FVoxelStreamingSourceView Candidate = ViewSettings.DefaultSourceView;
	if (InRadiusCells >= 0)
	{
		const FIntPoint Limits = GetLocalFineRadiusLimits();
		if (InRadiusCells < Limits.X || InRadiusCells > Limits.Y) return false;
		Candidate.FineRadiusCells = InRadiusCells;
	}
	FString Error;
	if (!ViewSettings.Fine.Validate(Candidate, Error)) return false;
	if (LocalFineRadiusCells == InRadiusCells)
	{
		return true;
	}
	LocalFineRadiusCells = InRadiusCells;
	for (const auto& Pair : Sources)
	{
		const FVoxelStreamingSource Source = Pair.Value.Value;
		if (Pair.Value.Owner.IsValid() && Source.bInheritWorldView && Source.bLocalView && Source.Has(EVoxelStreamingCapability::FineVisual))
		{
			UpdateSource(Pair.Key, Source);
		}
	}
	return true;
}

FVoxelStreamingSourceView UVoxelModule::ResolveSourceView(const FVoxelStreamingSource& InSource) const
{
	FVoxelStreamingSourceView Result = InSource.bInheritWorldView ? ViewSettings.DefaultSourceView : InSource.View;
	if (LocalFineRadiusCells >= 0 && InSource.bInheritWorldView && InSource.bLocalView &&
		InSource.Has(EVoxelStreamingCapability::FineVisual))
	{
		Result.FineRadiusCells = LocalFineRadiusCells;
	}
	return Result;
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
	Source.Value.View = ResolveSourceView(Source.Value);
	FString Error;
	if (!VoxelStreamingGeometry::ValidateInitialSide(Source.Value.View.InitialChunkSideCells,
		ViewSettings.Lod.MaximumFineLevel, Error) ||
		(Source.Value.Has(EVoxelStreamingCapability::FineVisual) && !ViewSettings.Fine.Validate(Source.Value.View, Error)))
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
	Updated.View = ResolveSourceView(Updated);
	FString Error;
	if (!VoxelStreamingGeometry::ValidateInitialSide(Updated.View.InitialChunkSideCells,
		ViewSettings.Lod.MaximumFineLevel, Error) ||
		(Updated.Has(EVoxelStreamingCapability::FineVisual) && !ViewSettings.Fine.Validate(Updated.View, Error)))
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
		if (IsCollisionReady(Key))
		{
			++Result.ReadyCollisionSections;
			if (CollisionPresenter->IsReadyEmpty(Key)) ++Result.ReadyEmptyCollisionSections;
			else ++Result.ReadyNonEmptyCollisionSections;
		}
	}
	return Result;
}

FVoxelStreamingReadiness UVoxelModule::QueryBoundsReadiness(const FVoxelGenerationBounds& InBounds,
	const bool bInRequireCollision, TOptional<FIntVector>* OutFailedCollisionSection) const
{
	FVoxelStreamingReadiness Result;
	if (OutFailedCollisionSection) OutFailedCollisionSection->Reset();
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
					if (IsCollisionReady(Key))
					{
						++Result.ReadyCollisionSections;
						if (CollisionPresenter->IsReadyEmpty(Key)) ++Result.ReadyEmptyCollisionSections;
						else ++Result.ReadyNonEmptyCollisionSections;
					}
					else if (OutFailedCollisionSection && !OutFailedCollisionSection->IsSet() && HasCurrentCollisionFailure(Key))
					{
						OutFailedCollisionSection->Emplace(Key);
					}
				}
			}
		}
	}
	return Result;
}

bool UVoxelModule::FindSourceDataFailureInBounds(const FGuid& InSource, const UObject* InOwner, const uint64 InEpoch,
	const FVoxelGenerationBounds& InBounds, FIntVector& OutSection, FString& OutLastError) const
{
	OutSection = FIntVector::ZeroValue;
	OutLastError.Reset();
	const FVoxelWorldRuntime* WorldRuntime = Runtime.Get();
	if (!IsInGameThread() || !IsReady() || !WorldRuntime || InEpoch != Epoch || WorldRuntime->Epoch() != InEpoch ||
		!IsSourceOwnedBy(InSource, InOwner) || !IsSourceAdmitted(InSource) ||
		InBounds.Min.X >= InBounds.Max.X || InBounds.Min.Y >= InBounds.Max.Y || InBounds.Min.Z >= InBounds.Max.Z)
	{
		return false;
	}
	const FVoxelSourceInterest* Interest = CurrentInterest->Sources.Find(InSource);
	if (!Interest) return false;
	const FIntVector Min = VoxelModuleToSection(InBounds.Min);
	const FIntVector Max = VoxelModuleToSection(InBounds.Max - FIntVector(1));
	const FIntVector Size = Max - Min + FIntVector(1);
	if (Size.X > 4096 || Size.Y > 4096 || Size.Z > 4096 || static_cast<int64>(Size.X) * Size.Y * Size.Z > 4096)
	{
		return false;
	}
	for (int32 Z = Min.Z; Z <= Max.Z; ++Z)
	{
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
		{
			for (int32 X = Min.X; X <= Max.X; ++X)
			{
				const FIntVector Key(X, Y, Z);
				if (!Interest->DataSections.Contains(Key) && !Interest->CollisionSections.Contains(Key)) continue;
				const FVoxelSection* Section = WorldRuntime->FindSection(Key);
				if (!Section || Section->Stamp.Epoch != InEpoch || Section->Status != EVoxelSectionStatus::Failed ||
					Section->Retirement.State == EVoxelRuntimeRetirementState::Detaching ||
					Section->Retirement.State == EVoxelRuntimeRetirementState::ReadyToFree ||
					Section->Retirement.State == EVoxelRuntimeRetirementState::Freed) continue;
				OutSection = Key;
				OutLastError = Section->Error;
				return true;
			}
		}
	}
	return false;
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
	UWorld* World = GetWorld();
	if (IsAuthority() || !World || World->bIsTearingDown || !FMath::IsFinite(BlockSize()) || BlockSize() <= 0.0 ||
		InType == EVoxelRepresentationWireType::None || InType > EVoxelRepresentationWireType::MacroTerrain)
	{
		return false;
	}
	const FVoxelTerrainSpatialKey SpatialKey{static_cast<EVoxelTerrainRepresentation>(uint8(InType) + 1), InKey.Coordinate, InKey.Level};
	const FVoxelInterestSet& Interest = GetCurrentInterest();
	const FVoxelTerrainGeometry* Geometry = Interest.Geometry.Find(SpatialKey);
	FIntVector Minimum;
	FIntVector Maximum;
	FString Error;
	if (!Geometry || !Geometry->Validate(Error) ||
		!Geometry->Key.TryBounds(Geometry->WorldMinZ, Geometry->WorldMaxZ, Minimum, Maximum)) return false;
	const FVoxelGenerationBounds Bounds{Minimum, Maximum};
	struct FCandidate
	{
		TWeakObjectPtr<UVoxelModuleNetworkComponent> Network;
		int32 Priority = MAX_int32;
		uint64 RegistrationOrder = 0;
		FGuid SourceId;
	};
	TArray<FCandidate> Candidates;
	for (const auto& Pair : Interest.Sources)
	{
		const FVoxelSourceInterest& Accepted = Pair.Value;
		FVoxelStreamingSource Source = Accepted.Source;
		const FSource* Live = Sources.Find(Pair.Key);
		if (!Live || !Live->Owner.IsValid() || !Live->Value.bLocalView || !Source.bLocalView ||
			!Source.Has(EVoxelStreamingCapability::FineVisual)) continue;
		const bool bLocalOnly = Source.Has(EVoxelStreamingCapability::LocalRefinement) && !Source.Has(EVoxelStreamingCapability::WorldVisual);
		if (!Source.Has(EVoxelStreamingCapability::WorldVisual) && !(bLocalOnly && InType == EVoxelRepresentationWireType::VoxelProxy)) continue;
		const int32 StageIndex = int32(InType) - 1;
		if (Accepted.bHasStageCenters) Source.Center = Accepted.StageCenters[StageIndex];
		if (InType == EVoxelRepresentationWireType::VoxelProxy && Source.View.VoxelProxyVerticalHalfExtentCells <= 0) continue;
		if (InType == EVoxelRepresentationWireType::VoxelProxy && !bLocalOnly && Source.View.VoxelProxyRadiusCells <= 0) continue;
		if (InType == EVoxelRepresentationWireType::SurfaceProxy && Source.View.SurfaceRadiusCells <= 0) continue;
		if (InType == EVoxelRepresentationWireType::MacroTerrain && Source.View.MacroRadiusCells <= 0) continue;
		if (InType == EVoxelRepresentationWireType::MacroTerrain)
		{
			const FVoxelResolvedViewRanges Ranges = VoxelViewLod::ResolveViewRanges(Source);
			const int32 Inner = Source.View.SurfaceRadiusCells > 0 ? Ranges.Surface :
				(Source.View.VoxelProxyRadiusCells > 0 ? Ranges.Proxy : Ranges.Fine);
			const double FarX = FMath::Max(FMath::Abs(double(Minimum.X) - Source.Center.X), FMath::Abs(double(Maximum.X) - Source.Center.X));
			const double FarY = FMath::Max(FMath::Abs(double(Minimum.Y) - Source.Center.Y), FMath::Abs(double(Maximum.Y) - Source.Center.Y));
			if (Ranges.Macro <= Inner || (Inner > 0 && FarX * FarX + FarY * FarY <= FMath::Square(double(Inner)))) continue;
		}
		if (bLocalOnly)
		{
			const FVoxelResolvedViewRanges Ranges = VoxelViewLod::ResolveViewRanges(Source);
			double Radius = double(Ranges.Fine) + double(Source.View.InitialChunkSideCells) * 2.0;
			if (Accepted.FineBounds.IsValid())
			{
				const double X = FMath::Max(FMath::Abs(double(Accepted.FineBounds.Min.X) - Source.Center.X), FMath::Abs(double(Accepted.FineBounds.Max.X) - Source.Center.X));
				const double Y = FMath::Max(FMath::Abs(double(Accepted.FineBounds.Min.Y) - Source.Center.Y), FMath::Abs(double(Accepted.FineBounds.Max.Y) - Source.Center.Y));
				Radius = FMath::CeilToDouble(FMath::Sqrt(X * X + Y * Y)) + double(Source.View.InitialChunkSideCells) * 2.0;
			}
			if (!FMath::IsFinite(Radius) || Radius > MAX_int32) continue;
			// 本地细化的外圈来自实际 FineBounds，不能再被配置中的 Fine 半径扩大。
			Source.View.FineRadiusCells = 0;
			Source.View.VoxelProxyRadiusCells = FMath::Max(0, int32(Radius));
			if (Source.View.VoxelProxyRadiusCells == 0) continue;
		}
		if (!UVoxelModuleNetworkComponent::IsRepresentationInRangeForView(FVector(Source.Center), InType, Bounds, Source.View)) continue;
		UObject* SourceOwner = Live->Owner.Get();
		APlayerController* Controller = Cast<APlayerController>(SourceOwner);
		UVoxelAgentComponent* Agent = Cast<UVoxelAgentComponent>(SourceOwner);
		APawn* Pawn = Cast<APawn>(SourceOwner);
		if (!Pawn) Pawn = Cast<APawn>(Live->PlayerClaim.Pawn.Get());
		if (!Agent && Pawn) Agent = Pawn->FindComponentByClass<UVoxelAgentComponent>();
		if (!Controller && Agent) Controller = Agent->GetBoundController();
		if (!Controller && Pawn) Controller = Cast<APlayerController>(Pawn->GetController());
		if (!Controller || !Controller->IsLocalController() || Controller->GetWorld() != World) continue;
		UVoxelModuleNetworkComponent* Network = Controller->FindComponentByClass<UVoxelModuleNetworkComponent>();
		AActor* Observer = Network ? Network->GetAuthorizedObserver() : nullptr;
		if (!Network || !Network->IsSessionReady() || !Observer || Observer->GetWorld() != World || Observer->GetActorLocation().ContainsNaN() ||
			!UVoxelModuleNetworkComponent::IsRepresentationInRangeForView(Observer->GetActorLocation() / BlockSize(), InType, Bounds, GetViewSettings().DefaultSourceView)) continue;
		Candidates.Add({TWeakObjectPtr<UVoxelModuleNetworkComponent>(Network), Source.GetSchedulingPriority(), Source.RegistrationOrder, Pair.Key});
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.Priority != B.Priority) return A.Priority < B.Priority;
		if (A.RegistrationOrder != B.RegistrationOrder) return A.RegistrationOrder < B.RegistrationOrder;
		return A.SourceId < B.SourceId;
	});
	for (const FCandidate& Candidate : Candidates)
	{
		if (UVoxelModuleNetworkComponent* Network = Candidate.Network.Get(); Network && Network->RequestRepresentation(InType, InKey, InKnownRevision)) return true;
	}
	return false;
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
	if (IsWorldSaveSuspended() || bMutating || !IsAuthority() || !IsReady())
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

bool UVoxelModule::BeginWorldSaveSuspension(const FGuid& InOwnerToken, FString& OutError)
{
	check(IsInGameThread());
	OutError.Reset();
	if (!InOwnerToken.IsValid() || IsWorldSaveSuspended() || !IsAuthority() || !IsReady() ||
		bMutating || bSceneTransitionActive || SceneLoadingCount > 0 || TerrainFrameContext.IsActive() ||
		!Runtime || !Scheduler || !RegionStore.IsDirectoryReady())
	{
		OutError = TEXT("世界正在处理场景或地形事务，暂不能冻结最终存档切点。");
		return false;
	}
	WorldSaveSuspension = MakeShared<FVoxelSaveSuspension, ESPMode::ThreadSafe>(InOwnerToken, Epoch);
	return true;
}

bool UVoxelModule::EndWorldSaveSuspension(const FGuid& InOwnerToken, FString& OutError)
{
	check(IsInGameThread());
	OutError.Reset();
	if (!WorldSaveSuspension || !InOwnerToken.IsValid() || InOwnerToken != WorldSaveSuspension->GetToken() || WorldSaveSuspension->GetEpoch() != Epoch ||
		TerrainFrameContext.IsActive())
	{
		OutError = TEXT("最终存档冻结释放的所有者或世界代次不匹配。");
		return false;
	}
	return WorldSaveSuspension->Release(InOwnerToken, Epoch, OutError);
}

bool UVoxelModule::AdvanceSuspendedWorldSave(const FGuid& InOwnerToken, FString& OutError)
{
	check(IsInGameThread());
	OutError.Reset();
	if (!WorldSaveSuspension || !InOwnerToken.IsValid() || InOwnerToken != WorldSaveSuspension->GetToken() || WorldSaveSuspension->GetEpoch() != Epoch ||
		!GetWorld() || !GetWorld()->IsPaused() || !IsAuthority() || !IsReady())
	{
		OutError = TEXT("最终存档推进缺少原世界的暂停与冻结所有权。");
		return false;
	}
	USaveGameModule* Saves = USaveGameModule::GetPtr(GetWorld());
	if (!Saves)
	{
		OutError = TEXT("最终存档推进缺少原世界保存模块。");
		return false;
	}
	const double Started = FPlatformTime::Seconds();
	const bool bDedicated = GetWorld()->GetNetMode() == NM_DedicatedServer;
	const double MissingTiming = std::numeric_limits<double>::quiet_NaN();
	FVoxelTerrainFrameObservation Observation;
	Observation.GameThreadMilliseconds = GGameThreadTime > 0 ? FPlatformTime::ToMilliseconds(GGameThreadTime) : MissingTiming;
	Observation.TerrainGameThreadMilliseconds = TerrainFrameContext.GetLastTerrainMilliseconds();
	Observation.RenderThreadMilliseconds = bDedicated ? 0.0 :
		(GRenderThreadTime > 0 ? FPlatformTime::ToMilliseconds(GRenderThreadTime) : MissingTiming);
	const uint32 GPUCycles = bDedicated ? 0 : RHIGetGPUFrameCycles();
	Observation.GPUMilliseconds = bDedicated ? 0.0 :
		(GPUCycles > 0 ? FPlatformTime::ToMilliseconds(GPUCycles) : MissingTiming);
	if (!TerrainFrameContext.Begin(Epoch, GFrameCounter, Started, Observation)) return true;
	AdvanceSaveCapture();
	Saves->AdvanceWorldSaveCapture(*this);
	TerrainFrameContext.End(FPlatformTime::Seconds());
	return true;
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
			CollisionPresenter->InvalidateNeighbors(Patch.Section);
		}
		if (NavigationPresenter)
		{
			NavigationPresenter->InvalidateSection(Patch.Section);
		}
	}
	bInterestDirty = true;
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
	const bool bCaptureShards = InterestRuntime && InterestRuntime->HasPendingFineWindow();
	const int32 ShardCount = bCaptureShards && CurrentInterest->PublicationInputs ? CurrentInterest->PublicationInputs->FineShards.Num() : 0;
	const int32 MaximumRows = bCaptureShards && CurrentInterest->PublicationInputs ? CurrentInterest->PublicationInputs->MaximumFeedbackSections : 0;
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
		RenderRoot, PhysicsRoot, Runtime->GetChangeHierarchy(), Manifest, Shapes->GetContentHash(), ++SafetyCaptureSerial,
		bCaptureShards, Feedback.ToSharedRef());
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
		const auto ReadSection = [&Capture](const FIntVector& Key) { return Capture->ReadSection(Key); };
		const FVoxelFineSectionLookup SectionLookup(ReadSection);
		Payload->bChanged = Payload->CandidateRuntime->UpdateSnapshot(SourcesCapture->Values, WorldManifest, Settings,
			Payload->Interest, Payload->Delta, Payload->bHasSnapshot, &Capture->GetFeedback().Get(), &SectionLookup);
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
	if (WorldState != EVoxelWorldState::Running && WorldState != EVoxelWorldState::Initializing) return;
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
				if (CollisionPresenter)
				{
					CollisionPresenter->NotifySectionDataReady(Section);
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

bool UVoxelModule::IsTerrainSafeAt(const FGuid& InSource, const FBox& InWorldBounds, const bool bInRequireLocalVisual) const
{
	const auto* Source = CurrentInterest->Sources.Find(InSource);
	const FSource* RegisteredSource = Sources.Find(InSource);
	if (!IsReady() || !Source || !RegisteredSource || !RegisteredSource->Owner.IsValid() ||
		RegisteredSource->Value.Id != Source->Source.Id ||
		RegisteredSource->Value.Capabilities != Source->Source.Capabilities ||
		RegisteredSource->Value.Purpose != Source->Source.Purpose ||
		RegisteredSource->Value.bInheritWorldView != Source->Source.bInheritWorldView || !InWorldBounds.IsValid ||
		InWorldBounds.Min.ContainsNaN() || InWorldBounds.Max.ContainsNaN()) return false;
	const bool bRequiresVisual = Source->Source.Has(EVoxelStreamingCapability::FineVisual);
	if (bInRequireLocalVisual && (!bRequiresVisual || !Source->Source.bLocalView || !RegisteredSource->Value.bLocalView))
	{
		return false;
	}
	// 来源位置或速度变化会使下一窗口暂未准入；实际移动仍以本次范围的最新物理和视觉回执为准。
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

bool UVoxelModule::BindInventoryBridge(TSharedRef<FVoxelInventoryDomainBridge> InBridge, FInventoryVoxelContribution InContribution)
{
    if (!InContribution || (InventoryBridge && InventoryBridge.Get() != &InBridge.Get()))
    {
        return false;
    }
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
    return true;
}

bool UVoxelModule::UnbindInventoryBridge(const TSharedPtr<FVoxelInventoryDomainBridge>& InBridge)
{
    if (!InBridge || InventoryBridge != InBridge)
    {
        return false;
    }
    InBridge->SetPublisher(nullptr);
    InventoryContribution = nullptr;
    InventoryBridge.Reset();
    return true;
}

bool UVoxelModule::ExecuteInventoryWorldEdit(FVoxelInteractionPlan& InOutPlan, AActor* InSource,
    FInventoryBatchRequest& InOutRequest, const FInventoryAuthorityContext& InAuthority,
    FInventoryCommandResult& OutResult, FString& OutError)
{
    UInventoryModule* System = UInventoryModule::GetPtr(GetWorld());
    if(!System || !System->IsReady() || !IsAuthority() || !IsReady() || IsWorldSaveSuspended() || bMutating || !InventoryBridge || !InventoryContribution)
    {
        OutError = TEXT("Inventory.WorldEditUnavailable");
        return false;
    }
    TGuardValue<bool> Guard(bMutating, true);
    if(!InventoryContribution(InOutPlan, InSource, InOutRequest, OutError) ||
        !ValidateInteractionPlan(InOutPlan, OutError))
    {
        return false;
    }
    if(PlacementOverlapsActors(InOutPlan) ||
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
