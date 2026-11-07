#include "Voxel/VoxelModule.h"
#include "Ability/Inventory/InventorySnapshotTypes.h"
#include "Ability/Inventory/InventoryCommandTypes.h"
#include "Ability/Inventory/InventoryModule.h"
#include "Ability/Inventory/InventoryItemRegistry.h"
#include "Voxel/Interaction/VoxelInventoryDomainBridge.h"
#include "Voxel/Save/InventoryVoxelEntityCodec.h"
#include "Misc/SecureHash.h"
#include "Misc/ScopeExit.h"
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
#include "Engine/World.h"
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

bool UVoxelModule::CreateWorldFromProfile(const int32 InSeed, FString& OutError, const EVoxelTerrainType InTerrainType)
{
	if (!WorldGenerationProfile)
	{
		OutError = TEXT("No baked world generation profile");
		return false;
	}
	FVoxelGenerationSettings Settings = WorldGenerationProfile->Defaults;
	Settings.Seed = InSeed;
	return CreateWorld(Settings, WorldGenerationProfile->TargetCellCentimeters, OutError, InTerrainType);
}

bool UVoxelModule::CreateWorld(
	const FVoxelGenerationSettings& InSettings,
	const int32 InBlockSizeCentimeters,
	FString& OutError,
	const EVoxelTerrainType InTerrainType)
{
	if (!IsBusinessAdmissionOpen() || !IsAuthority() ||
		!Registry.GetSnapshot() ||
		!WorldGenerationProfile ||
		Runtime || bWorldStartInProgress || bPartialWorldRetirementInProgress)
	{
		OutError =
			TEXT("Creating a voxel world requires an idle authoritative initialized module");

		return false;
	}
	FVoxelGenerationSettings Settings = InSettings;
	if (!NormalizeVoxelWorldSettings(Settings, InTerrainType, OutError)) return false;

	FVoxelGenerationRuntimeConfig Config;

	if (!FVoxelGenerationBinding::Build(
		*WorldGenerationProfile,
		*Registry.GetSnapshot(),
		Settings,
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
	if (!ValidateSuperflatGround(InTerrainType, Registry.GetSnapshot().Get(), Shapes.Get(), Config, OutError)) return false;

	FVoxelWorldManifest NewManifest;
	NewManifest.GeneratorVersion = Config.Recipe->AlgorithmVersion;
	NewManifest.WorldId =
		FGuid::NewGuid();

	NewManifest.Settings =
		Settings;
	NewManifest.TerrainType = InTerrainType;

	NewManifest.BlockSizeCentimeters =
		InBlockSizeCentimeters;

	NewManifest.RegistryHash =
		Registry.GetSnapshot()->Hash;

	NewManifest.RecipeHash =
		Config.Recipe->RecipeHash;

	uint64 GenerationIdentitySalt = 0;
	if (!ResolveGenerationIdentitySalt(NewManifest.TerrainType, GenerationIdentitySalt))
	{
		OutError = TEXT("Voxel generation identity requires the current registry and shapes");
		return false;
	}
	NewManifest.BaseSampleHash =
		BuildGenerationSignature(
			NewManifest,
			GenerationIdentitySalt);

	if (NewManifest.BaseSampleHash == 0)
	{
		OutError =
			TEXT("Voxel generation signature could not be built");

		return false;
	}

	WorldData = NewWorldData();
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
	if (!IsBusinessAdmissionOpen())
	{
		OutError = TEXT("Voxel module is not accepting world startup");
		return false;
	}
	if (bWorldStartInProgress || bPartialWorldRetirementInProgress)
	{
		OutError = TEXT("Voxel startup or partial retirement is already in progress");
		return false;
	}
	if ((bPartialWorldStartNeedsRetirement || Scheduler || GenerationCache || GenerationConfig || Generator) &&
		!Runtime && !RetirePartialWorldStart(OutError)) return false;
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
	bool bWorldStartupSucceeded = false;
	ON_SCOPE_EXIT
	{
		if (!bWorldStartupSucceeded && Runtime)
		{
			FString RetirementError;
			if (!StopWorld(false, RetirementError))
				UE_LOG(LogTemp, Error, TEXT("Voxel failed startup retains its runtime for retirement: %s"), *RetirementError);
			WorldState = EVoxelWorldState::Failed;
		}
		if (bPartialWorldStartNeedsRetirement)
		{
			FString RetirementError;
			if (!RetirePartialWorldStart(RetirementError))
				UE_LOG(LogTemp, Error, TEXT("Voxel partial startup retirement remains pending: %s"), *RetirementError);
			WorldState = EVoxelWorldState::Failed;
		}
	};
	TGuardValue<bool> Starting(bWorldStartInProgress, true);

	WorldState = EVoxelWorldState::Initializing;
	RetirementStatus = EVoxelRetirementStatus::None;
	RetirementError.Reset();
	Manifest = InManifest;
	const auto FailInitialization = [this]()
	{
		if (PendingTerrainFactResources && PendingTerrainFactControl.WorldEpoch > Epoch)
			Epoch = PendingTerrainFactControl.WorldEpoch;
		bPartialWorldStartNeedsRetirement = true;
		WorldState = EVoxelWorldState::Failed;
	};
	if (Manifest.RegistryHash != Registry.GetSnapshot()->Hash)
	{
		OutError = TEXT("Voxel registry hash mismatch");
		FailInitialization();
		return false;
	}
	if (Manifest.TerrainType == EVoxelTerrainType::Superflat &&
		(Manifest.Settings.MinZ != VoxelGeneration::SuperflatMinZ || Manifest.Settings.MaxZ <= 0))
	{
		OutError = TEXT("Superflat manifest has an invalid world height");
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
		Config.Recipe->RecipeHash != Manifest.RecipeHash ||
		Config.Recipe->AlgorithmVersion != Manifest.GeneratorVersion)
	{
		FailInitialization();
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Voxel frozen recipe hash mismatch");
		}
		return false;
	}
	Config.TerrainType = Manifest.TerrainType;
	if (!ValidateSuperflatGround(Manifest.TerrainType, Registry.GetSnapshot().Get(), Shapes.Get(), Config, OutError))
	{
		FailInitialization();
		return false;
	}

	if (Config.TerrainType != EVoxelTerrainType::Superflat &&
		((Config.Recipe->Settings.Ecology.Tree.bEnabled &&
			(Config.Recipe->Ecology.TreeTrunk == MAX_uint16 ||
			 Config.Recipe->Ecology.TreeLeaves == MAX_uint16)) ||
		(Config.Recipe->Settings.Ecology.Grass.bEnabled &&
		 Config.Recipe->Ecology.GrassPlant == MAX_uint16)))
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
		if (!ViewSettings.Fine.Validate(ViewSettings.DefaultSourceView, OutError) || !VoxelViewLod::ValidateTopology(ViewSettings.Lod, OutError) ||
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
	uint64 GenerationIdentitySalt = 0;
	if (!ResolveGenerationIdentitySalt(Manifest.TerrainType, GenerationIdentitySalt))
	{
		FailInitialization();
		OutError = TEXT("Voxel generation identity requires the current registry and shapes");
		return false;
	}
	const uint64 ExpectedGenerationSignature =
		BuildGenerationSignature(
			Manifest,
			GenerationIdentitySalt);
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
	TaskBudget.MaxConcurrentTasks = FVoxelTerrainFrameBudget::ResolveMaximumWorkers(ViewSettings.FrameBudget);
	TaskBudget.bBorrowIdleKindSlots = ViewSettings.FrameBudget.bUseAutomaticWorkerCount;
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
	Scheduler->SetPaused(GetModuleState() == EModuleState::Paused || GetModuleState() == EModuleState::UnRunning);
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
		Generator.ToSharedRef(), Scheduler->GetRuntimeResources(), Shapes.ToSharedRef());
	GenerationConfig->InputLease->MarkPublished();
	GenerationConfig->RecipeInputLease->MarkPublished();
	GenerationCache->MarkPublished();
	NaturalGenerator->MarkPublished();
	Generator->MarkPublished();
	Runtime->GetChangeHierarchy().SetTopology(ViewSettings.Lod);
	if (GenerationConfig->TerrainType != EVoxelTerrainType::Superflat &&
		GenerationConfig->Recipe->Settings.Ecology.Tree.bEnabled)
	{
		const FVoxelTreeGenerationSettings& Tree =
			GenerationConfig->Recipe->Settings.Ecology.Tree;
		Runtime->GetChangeHierarchy().SetNaturalInfluence(
			Tree.CrownRadius, Tree.MaxHeight + Tree.CrownRadius);
	}
	if (IsAuthority() && RegionStore.IsDirectoryReady()) Runtime->GetChangeIndex().SetBaseline(RegionStore.GetDirectorySnapshot());
	if (!IsAuthority()) OverlayReplica = MakeUnique<FVoxelOverlayReplica>(Epoch, Scheduler->GetRuntimeResources());
	Scheduler->BindFrameContext(TerrainFrameContext);
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
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		MapTileCache = MakeUnique<FVoxelMapTileCache>(
			*Scheduler, *EmergeManager, Generator.ToSharedRef(), GenerationConfig.ToSharedRef(),
			Epoch, BlockSize());
	}
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
		WorldState = EVoxelWorldState::Failed;
		return false;
	}
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		ViewManager = MakeUnique<FVoxelViewManager>(*this, *Scheduler, Epoch);
		EmergeManager->SetExternalConsumerWakeCallback([this](const FVoxelGenerationConsumerKey& Consumer)
		{
			if (ViewManager) ViewManager->OnGenerationDependencyReady(Consumer);
			if (MapTileCache) MapTileCache->OnGenerationDependencyReady(Consumer);
		});
	}
	// 服务端同样需要权威碰撞，碰撞发布不依赖视觉地形模块。
	CollisionPresenter = MakeUnique<FVoxelCollisionPresenter>(*this, *Scheduler, Epoch);
	if (IsAuthority() && bEnableProjectSceneSimulation)
	{
		NavigationPresenter = MakeUnique<FVoxelNavigationPresenter>(*this, *Scheduler);
	}
	if (Manifest.TerrainType != EVoxelTerrainType::Superflat)
	{
		DetailView = MakeUnique<FVoxelDetailView>(*this, *Scheduler, Epoch);
		if (!DetailView->Initialize(OutError))
		{
			WorldState = EVoxelWorldState::Failed;
			return false;
		}
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
	WorldState = IsAuthority() && !RegionStore.IsDirectoryReady() ? EVoxelWorldState::Initializing : EVoxelWorldState::Running;
	if (!WorldData)
	{
		WorldData = NewWorldData();
	}
	WorldData->GenerationProfile =
		ActiveGenerationProfileAsset;
	WorldData->Seed =
		Manifest.Settings.Seed;
	WorldData->TerrainType = Manifest.TerrainType;
	FVoxelManifestCodec::Encode(Manifest, WorldData->ManifestBytes);
	// 远端客户端没有 GameMode 的模块启动阶段；握手建立运行时后启用自身刷新。
	if (bInRemote && GetModuleState() == EModuleState::None && IsModuleAutoRun()) Run();
	if (!Runtime)
	{
		OutError = TEXT("Voxel world startup was interrupted before initialization callbacks");
		return false;
	}
	if (WorldState == EVoxelWorldState::Running)
	{
		OnWorldInitialized.Broadcast();
		if (!Runtime || WorldState != EVoxelWorldState::Running)
		{
			OutError = TEXT("Voxel world initialization callback failed or interrupted the original world");
			return false;
		}
	}
	bWorldStartupSucceeded = true;
	OutError.Reset();
	return true;
}
