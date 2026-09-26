#include "Voxel/VoxelModule.h"

#include "Async/Async.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/PickUp/AbilityPickUpVoxel.h"
#include "Asset/AssetModuleStatics.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Main/MainModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Voxel/Authoring/VoxelWorldGenerationProfile.h"
#include "Voxel/Authoring/VoxelViewProfile.h"
#include "Voxel/Collision/VoxelCollisionPresenter.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelBuiltinFeatures.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Agent/VoxelAgentComponent.h"
#include "Voxel/Interaction/VoxelEditTransaction.h"
#include "Voxel/Interaction/VoxelInventoryTransaction.h"
#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "Voxel/Map/VoxelMapTileCache.h"
#include "Voxel/Map/VoxelMapSurfaceResolver.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Rendering/VoxelDetailView.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Scene/VoxelSceneRegion.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Voxels/VoxelItemBridge.h"

namespace
{
	struct FVoxelInterestTaskPayload : FVoxelTaskCustomPayload
	{
		TSharedPtr<FVoxelInterestSet, ESPMode::ThreadSafe> Interest;

		virtual uint64 GetAllocatedBytes() const override
		{
			return sizeof(*this) + (Interest ? Interest->GetAllocatedBytes() : 0);
		}
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

	bool StreamingSourceAffectsInterest(
		const FVoxelStreamingSource& InA,
		const FVoxelStreamingSource& InB,
		const FVoxelViewSettings& InViewSettings)
	{
		int32 CenterRefreshDistance = FMath::Max(1, FMath::RoundToInt(
			InViewSettings.FineRadius * InViewSettings.StreamingReplanFineRadiusFraction));
		if (InB.bCollision)
		{
			CenterRefreshDistance = FMath::Min(CenterRefreshDistance,
				FMath::Max(1, InB.CollisionRadius / 2));
		}
		const int64 CenterRefreshDistanceSquared =
			static_cast<int64>(CenterRefreshDistance) * CenterRefreshDistance;
		const FIntVector CenterDelta = InA.Center - InB.Center;
		if (static_cast<int64>(CenterDelta.X) * CenterDelta.X +
			static_cast<int64>(CenterDelta.Y) * CenterDelta.Y +
			static_cast<int64>(CenterDelta.Z) * CenterDelta.Z >= CenterRefreshDistanceSquared ||
			InA.ExactRadius != InB.ExactRadius ||
			InA.CollisionRadius != InB.CollisionRadius ||
			InA.SimulationRadius != InB.SimulationRadius ||
			InA.VerticalExactRadius != InB.VerticalExactRadius ||
			InA.RenderMode != InB.RenderMode ||
			InA.Purpose != InB.Purpose ||
			InA.bAffectsGlobalReadiness != InB.bAffectsGlobalReadiness ||
			InA.bRetainGenerationCache != InB.bRetainGenerationCache ||
			InA.RetentionRadiusCells != InB.RetentionRadiusCells ||
			InA.MovementCriticalCollisionRadius != InB.MovementCriticalCollisionRadius ||
			InA.bCollision != InB.bCollision ||
			InA.bSimulation != InB.bSimulation)
		{
			return true;
		}

		if (FMath::Abs(InA.VerticalFovDegrees - InB.VerticalFovDegrees) > 0.5f ||
			FMath::Abs(InA.ViewportHeightPixels - InB.ViewportHeightPixels) >= 32)
		{
			return true;
		}

		// Direction only changes admission priority, not which cells are needed.
		return false;
	}
}

IMPLEMENTATION_MODULE(UVoxelModule)

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

UVoxelModule* UVoxelModule::Find(UWorld* InWorld)
{
	if (!InWorld)
	{
		return nullptr;
	}
	for (TActorIterator<AMainModule> Iterator(InWorld); Iterator; ++Iterator)
	{
		for (UModuleBase* Module : Iterator->GetModules())
		{
			if (UVoxelModule* VoxelModule = Cast<UVoxelModule>(Module))
			{
				return VoxelModule;
			}
		}
	}
	return nullptr;
}

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
		if (!CreateWorldFromProfile(DefaultWorldSeed, Error))
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

	if (bInEditor ||
		!IsReady())
	{
		return;
	}

	const double VoxelTickStart = FPlatformTime::Seconds();

	Scheduler->Tick(
		[this](
			FVoxelTaskResult&& InResult)
		{
			ApplyTask(
				MoveTemp(InResult));
		});

	const double AfterScheduler = FPlatformTime::Seconds();

	if (GenerationCache)
	{
		GenerationCache->TickMaintenance(256);
	}

	const double Now =
		FPlatformTime::Seconds();

	if (bInterestDirty &&
		(LastInterestRefresh < 0.0 ||
			Now - LastInterestRefresh >= 0.1))
	{
		RefreshInterest(Now);
	}

	const double AfterInterest = FPlatformTime::Seconds();

	EmergeManager->Tick(
		CurrentInterest,
		InterestRevision,
		Now,
		ViewManager ? ViewManager->GetDataAdmissionLimit() : MAX_dbl,
		ViewSettings.DataBuildsPerFrame,
		ViewSettings.BuildAdmissionMilliseconds);

	const double AfterEmerge = FPlatformTime::Seconds();

	ResidencyManager->Tick(
		CurrentInterest.Exact,
		InterestRevision,
		Now);

	const double AfterResidency = FPlatformTime::Seconds();

	if (CollisionPresenter)
	{
		CollisionPresenter->Tick(
			CurrentInterest.Exact,
			InterestRevision);
	}

	const double AfterCollision = FPlatformTime::Seconds();

	if (ViewManager)
	{
		ViewManager->Tick(
			InterestRevision,
			CollectLocalViewObservers());
	}

	const double AfterView = FPlatformTime::Seconds();

	if (DetailView)
	{
		DetailView->Tick(
			CollectDetailObservers());
	}

	for (const TPair<
		FIntVector,
		TObjectPtr<UVoxelSceneRegion>>& Pair :
		SceneRegions)
	{
		if (Pair.Value)
		{
			Pair.Value->TickSceneActors(
				InDeltaSeconds);
		}
	}

	const double AfterScenes = FPlatformTime::Seconds();

	UpdateReadiness();

#if !UE_BUILD_SHIPPING
	static double LastSlowTickLog = 0.0;
	if (FPlatformTime::Seconds() - VoxelTickStart > 0.008 && Now - LastSlowTickLog > 2.0)
	{
		LastSlowTickLog = Now;
		UE_LOG(LogTemp, Display, TEXT("Voxel slow tick ms: scheduler=%.2f interest=%.2f emerge=%.2f residency=%.2f collision=%.2f view=%.2f scenes=%.2f readiness=%.2f"),
			(AfterScheduler - VoxelTickStart) * 1000, (AfterInterest - AfterScheduler) * 1000,
			(AfterEmerge - AfterInterest) * 1000, (AfterResidency - AfterEmerge) * 1000,
			(AfterCollision - AfterResidency) * 1000, (AfterView - AfterCollision) * 1000,
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
			UE_LOG(LogTemp, Display, TEXT("Voxel frame timing samples=%d frameMs p50=%.2f p95=%.2f p99=%.2f max=%.2f voxelGT p95=%.2f max=%.2f"),
				DiagnosticFrameTimes.Num(), Percentile(DiagnosticFrameTimes, 0.5), Percentile(DiagnosticFrameTimes, 0.95),
				Percentile(DiagnosticFrameTimes, 0.99), DiagnosticFrameTimes.Last(),
				Percentile(DiagnosticModuleTimes, 0.95), DiagnosticModuleTimes.Last());
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
				static_cast<int32>(Source.RenderMode),
				Source.ExactRadius,
				Source.CollisionRadius);
		}
	}
#endif
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
	if (IsReady())
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
	if (Manifest.RegistryHash != Registry.GetSnapshot()->Hash)
	{
		OutError = TEXT("Voxel registry hash mismatch");
		WorldState = EVoxelWorldState::Failed;
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
		WorldState = EVoxelWorldState::Failed;
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
		WorldState = EVoxelWorldState::Failed;
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

	GenerationConfig = MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(Config));
	ViewSettings = {};
	if (UVoxelViewProfile* ViewProfile = WorldGenerationProfile->View.LoadSynchronous())
	{
		const int32 CellCentimeters = FMath::Max(1, Manifest.BlockSizeCentimeters);
		auto ToCells = [CellCentimeters](const int32 InCentimeters)
		{
			return FMath::Max(0, FMath::DivideAndRoundUp(InCentimeters, CellCentimeters));
		};
		ViewSettings.WarmupDataRadius = ToCells(ViewProfile->WarmupDataRadiusCentimeters);
		ViewSettings.WarmupCollisionRadius = ToCells(ViewProfile->WarmupCollisionRadiusCentimeters);
		ViewSettings.FineRadius = ToCells(ViewProfile->FineRadiusCentimeters);
		ViewSettings.PlayableFineRadiusFraction = FMath::Clamp(ViewProfile->PlayableFineRadiusFraction, 0.01f, 1.0f);
		ViewSettings.StreamingReplanFineRadiusFraction = FMath::Clamp(ViewProfile->StreamingReplanFineRadiusFraction, 0.001f, 1.0f);
		ViewSettings.FineVerticalRadius = ToCells(ViewProfile->FineVerticalRadiusCentimeters);
		ViewSettings.FinePreload = ToCells(ViewProfile->FinePreloadCentimeters);
		ViewSettings.MaximumTextureStretchCells = FMath::Clamp(ViewProfile->MaximumTextureStretchCells, 1.0f, 16.0f);
		ViewSettings.VoxelProxyRadius = ToCells(ViewProfile->VoxelProxyRadiusCentimeters);
		ViewSettings.SurfaceRadius = ToCells(ViewProfile->SurfaceRadiusCentimeters);
		ViewSettings.MacroRadius = ToCells(ViewProfile->MacroRadiusCentimeters);
		ViewSettings.TargetScreenErrorPixels = FMath::Max(0.1f, ViewProfile->TargetScreenErrorPixels);
		ViewSettings.MaximumVoxelProxyLevel = ViewProfile->MaximumVoxelProxyLevel;
		ViewSettings.MaximumSurfaceLevel = ViewProfile->MaximumSurfaceLevel;
		ViewSettings.MaximumMacroLevel = ViewProfile->MaximumMacroLevel;
		ViewSettings.MaximumSurfaceTilesPerSource = FMath::Clamp(
			ViewProfile->MaximumSurfaceTilesPerSource,
			32,
			2048);
		ViewSettings.MaximumMacroTilesPerSource = FMath::Clamp(
			ViewProfile->MaximumMacroTilesPerSource,
			32,
			2048);
		ViewSettings.FineBuildsPerFrame = FMath::Clamp(ViewProfile->FineBuildsPerFrame, 1, 256);
		ViewSettings.VoxelProxyBuildsPerFrame = FMath::Clamp(ViewProfile->VoxelProxyBuildsPerFrame, 1, 256);
		ViewSettings.SurfaceBuildsPerFrame = FMath::Clamp(ViewProfile->SurfaceBuildsPerFrame, 1, 256);
		ViewSettings.MacroBuildsPerFrame = FMath::Clamp(ViewProfile->MacroBuildsPerFrame, 1, 256);
		ViewSettings.DataBuildsPerFrame = FMath::Clamp(ViewProfile->DataBuildsPerFrame, 1, 1024);
		ViewSettings.CompletedResultsPerFrame = FMath::Clamp(ViewProfile->CompletedResultsPerFrame, 1, 256);
		ViewSettings.HeavyResultsPerFrame = FMath::Clamp(ViewProfile->HeavyResultsPerFrame, 1, 256);
		ViewSettings.BuildAdmissionMilliseconds = FMath::Clamp(ViewProfile->BuildAdmissionMilliseconds, 0.1f, 8.0f);
	}
	const uint64 ExpectedGenerationSignature =
		BuildGenerationSignature(
			Manifest,
			GetGenerationIdentitySalt());
	if (ExpectedGenerationSignature == 0 ||
		Manifest.BaseSampleHash != ExpectedGenerationSignature ||
		Epoch == MAX_uint64)
	{
		GenerationConfig.Reset();
		WorldState = EVoxelWorldState::Failed;
		OutError = TEXT("Voxel generation signature mismatch");
		return false;
	}

	GenerationCache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>(false);
	const TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> NaturalGenerator =
		MakeShared<const FVoxelGenerationPipeline, ESPMode::ThreadSafe>(
		GenerationConfig.ToSharedRef(),
		GenerationCache.ToSharedRef());
	TSharedPtr<const IVoxelGenerationOverlay, ESPMode::ThreadSafe> Overlay;
	if (!CreateGenerationOverlay(NaturalGenerator, Registry.GetSnapshot().ToSharedRef(),
		Overlay, OutError))
	{
		GenerationCache.Reset();
		GenerationConfig.Reset();
		WorldState = EVoxelWorldState::Failed;
		return false;
	}
	Generator = Overlay ?
		MakeShared<const FVoxelGenerationPipeline, ESPMode::ThreadSafe>(
			GenerationConfig.ToSharedRef(), GenerationCache.ToSharedRef(), Overlay) :
		NaturalGenerator;

	++Epoch;
	Runtime = MakeUnique<FVoxelWorldRuntime>(
		Epoch,
		IsAuthority(),
		Registry.GetSnapshot().ToSharedRef(),
		Generator.ToSharedRef());
	if (GenerationConfig->Recipe->Settings.Ecology.Tree.bEnabled)
	{
		const FVoxelTreeGenerationSettings& Tree =
			GenerationConfig->Recipe->Settings.Ecology.Tree;
		Runtime->GetChangeHierarchy().SetNaturalInfluence(
			Tree.CrownRadius, Tree.MaxHeight + Tree.CrownRadius);
	}
	if (IsAuthority() && !RegionStore.ScanChangeHeaders(
		[this](const FIntVector& InRegion, const uint64 InRevision, const TArray<uint64>& InMask)
		{
			(void)InRevision;
			Runtime->GetChangeIndex().SetRegionMask(InRegion, InMask);
		},
		OutError))
	{
		Runtime.Reset();
		Generator.Reset();
		GenerationCache.Reset();
		GenerationConfig.Reset();
		WorldState = EVoxelWorldState::Failed;
		return false;
	}
	Scheduler = MakeUnique<FVoxelTaskScheduler>();
	FVoxelTaskBudget TaskBudget;
	TaskBudget.MaxConcurrentTasks = FMath::Clamp(FPlatformMisc::NumberOfCores() * 2, 1, 16);
	TaskBudget.MaxReservedBytes = 256ull * 1024ull * 1024ull;
	TaskBudget.MaxCompletedResultsPerFrame = ViewSettings.CompletedResultsPerFrame;
	TaskBudget.MaxHeavyCompletedResultsPerFrame = FMath::Min(ViewSettings.HeavyResultsPerFrame, ViewSettings.CompletedResultsPerFrame);
	TaskBudget.MaxFineApplyPerFrame = ViewSettings.FineBuildsPerFrame;
	TaskBudget.MaxVoxelLODApplyPerFrame = ViewSettings.VoxelProxyBuildsPerFrame;
	TaskBudget.MaxSurfaceApplyPerFrame = ViewSettings.SurfaceBuildsPerFrame;
	TaskBudget.MaxMacroApplyPerFrame = ViewSettings.MacroBuildsPerFrame;
	Scheduler->SetBudget(TaskBudget);
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		MapTileCache = MakeUnique<FVoxelMapTileCache>(
			*Scheduler, Generator.ToSharedRef(), GenerationConfig.ToSharedRef(),
			Epoch, BlockSize());
	}
	UE_LOG(LogTemp, Display, TEXT("Voxel worker budget: cores=%d workers=%d reservedMiB=%llu"),
		FPlatformMisc::NumberOfCores(), TaskBudget.MaxConcurrentTasks, TaskBudget.MaxReservedBytes / (1024ull * 1024ull));
	UE_LOG(LogTemp, Display, TEXT("Voxel frame budget: Fine=%d Proxy=%d Surface=%d Macro=%d Data=%d Results=%d Heavy=%d AdmissionMs=%.2f"),
		ViewSettings.FineBuildsPerFrame, ViewSettings.VoxelProxyBuildsPerFrame,
		ViewSettings.SurfaceBuildsPerFrame, ViewSettings.MacroBuildsPerFrame, ViewSettings.DataBuildsPerFrame,
		TaskBudget.MaxCompletedResultsPerFrame, TaskBudget.MaxHeavyCompletedResultsPerFrame, ViewSettings.BuildAdmissionMilliseconds);
	InterestManager = MakeUnique<FVoxelInterestManager>();
	EmergeManager = MakeUnique<FVoxelEmergeManager>(
		*Runtime,
		*Scheduler,
		Generator.ToSharedRef(),
		RegionStore,
		Manifest,
		Registry.GetSnapshot().ToSharedRef());
	ResidencyManager = MakeUnique<FVoxelResidencyManager>(
		*Runtime,
		*Scheduler,
		[this](const FIntVector& InSection)
		{
			if (CollisionPresenter)
			{
				CollisionPresenter->InvalidateSection(InSection);
			}
			if (UVoxelSceneRegion* Region = GetSceneRegion(InSection, false))
			{
				Region->OnSectionDeactivated(InSection);
				if (!Region->HasActiveSections())
				{
					const FIntVector RegionKey = Region->GetRegionKey();
					if (IsAuthority())
					{
						TArray<uint8> Bytes;
						FString CaptureError;
						if (!Region->CaptureActors(Bytes, CaptureError))
						{
							Region->bSceneFailed = true;
							UE_LOG(LogTemp, Error, TEXT("Voxel scene eviction capture: %s"), *CaptureError);
							return;
						}
						UnloadedSceneFiles.Add(
							FVoxelSceneColumnCodec::RelativePath(FIntPoint(RegionKey.X, RegionKey.Y)),
							MoveTemp(Bytes));
					}
					Region->Shutdown();
					SceneRegions.Remove(RegionKey);
				}
			}
		},
		[this](const FIntVector& InSection)
		{
			return ViewManager && ViewManager->RequiresSectionData(InSection);
		});
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		ViewManager = MakeUnique<FVoxelViewManager>(*this, *Scheduler, Epoch);
		CollisionPresenter = MakeUnique<FVoxelCollisionPresenter>(*this, *Scheduler, Epoch);
	}
	DetailView = MakeUnique<FVoxelDetailView>(*this, *Scheduler, Epoch);
	if (!DetailView->Initialize(OutError))
	{
		StopWorld(true, OutError);
		WorldState = EVoxelWorldState::Failed;
		return false;
	}

	SessionId = IsAuthority() ? FGuid::NewGuid() : FGuid();
	Sources.Reset();
	CurrentInterest = {};
	InterestRevision = 0;
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
	OnWorldInitialized.Broadcast();
	OutError.Reset();
	return true;
}

void UVoxelModule::OnWorldStopping()
{
}

bool UVoxelModule::StopWorld(const bool bDiscardDirty, FString& OutError)
{
	if (!Runtime)
	{
		WorldState = EVoxelWorldState::None;
		OutError.Reset();
		return true;
	}
	if (SaveAdapter.IsBusy())
	{
		OutError = TEXT("Finish the SaveGame transaction before closing the voxel world");
		return false;
	}
	if (!bDiscardDirty && IsAuthority())
	{
		for (const FIntVector& Section : Runtime->ResidentSections())
		{
			if (Runtime->GetChangeIndex().IsModified(Section))
			{
				OutError = TEXT("World contains uncommitted voxel edits");
				return false;
			}
		}
	}

	OnWorldStopping();
	WorldState = EVoxelWorldState::Closing;
	++Epoch;
	if (Scheduler)
	{
		Scheduler->StopAndJoin();
	}
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
	CollisionPresenter.Reset();
	DetailView.Reset();
	for (const TPair<FIntVector, TObjectPtr<UVoxelSceneRegion>>& Pair : SceneRegions)
	{
		if (Pair.Value)
		{
			Pair.Value->Shutdown();
		}
	}
	SceneRegions.Reset();
	CapturedSceneFiles.Reset();
	EmergeManager.Reset();
	ResidencyManager.Reset();
	InterestManager.Reset();
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
	Sources.Reset();
	CurrentInterest = {};
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
	return CollisionPresenter && CollisionPresenter->IsReady(InSection);
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

FVoxelWorldRuntime* UVoxelModule::GetRuntime()
{
	return Runtime.Get();
}

const FVoxelWorldRuntime* UVoxelModule::GetRuntime() const
{
	return Runtime.Get();
}

bool UVoxelModule::CaptureOverlays(const FVoxelGenerationBounds& InBounds,
	FVoxelOverlaySnapshotSet& OutSnapshot, FString& OutError) const
{
	check(IsInGameThread());
	if (!Runtime || !GetRegistry())
	{
		OutError = TEXT("Voxel runtime is unavailable for overlay capture");
		return false;
	}
	FVoxelOverlaySnapshotSet Snapshot;
	TArray<FIntVector> Modified;
	Runtime->GetChangeIndex().Enumerate(InBounds, Modified);
	for (const FIntVector& Key : Modified)
	{
		FVoxelOverlaySnapshot Overlay;
		Overlay.Section = Key;
		if (const FVoxelSection* Section = Runtime->FindSection(Key);
			Section && Section->Status == EVoxelSectionStatus::DataReady)
		{
			Overlay.Revision = Section->CommittedRevision;
			Overlay.Blocks = Section->Overlay;
		}
		else
		{
			TArray<uint8> Bytes;
			FVoxelPersistentSection Persistent;
			if (GetRegionStore().ReadSection(Key, Bytes, OutError) != EVoxelRegionRead::Loaded ||
				!FVoxelDeltaCodec::Decode(Bytes, GetManifest(), *GetRegistry(), Persistent))
			{
				OutError = TEXT("Modified representation overlay could not be read");
				return false;
			}
			Overlay.Revision = Persistent.Revision;
			Overlay.Blocks = MoveTemp(Persistent.Blocks);
		}
		Snapshot.Sections.Add(Key, MoveTemp(Overlay));
	}
	OutSnapshot = MoveTemp(Snapshot);
	OutError.Reset();
	return true;
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
	return CurrentInterest;
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

void UVoxelModule::SetPersistenceEnabled(const bool bInEnabled)
{
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

FGuid UVoxelModule::RegisterSource(UObject* InOwner, const FVoxelStreamingSource& InSource)
{
	if (!InOwner || !IsReady())
	{
		return {};
	}
	FGuid Id = FGuid::NewGuid();
	FSource Source;
	Source.Owner = InOwner;
	Source.Value = InSource;
	Source.Value.Id = Id;
	Source.PlannedValue = Source.Value;
	Sources.Add(Id, MoveTemp(Source));
	bInterestDirty = true;
	LastInterestRefresh = -1.0;
	return Id;
}

bool UVoxelModule::UpdateSource(
	const FGuid& InId,
	const FVoxelStreamingSource& InSource)
{
	FSource* Source =
		Sources.Find(InId);

	if (!Source ||
		!Source->Owner.IsValid())
	{
		return false;
	}

	FVoxelStreamingSource Updated = InSource;
	Updated.Id = InId;

	if (StreamingSourceAffectsInterest(Source->PlannedValue, Updated, ViewSettings))
	{
		bInterestDirty = true;
	}

	Source->Value = MoveTemp(Updated);

	return true;
}

void UVoxelModule::UnregisterSource(const FGuid& InId)
{
	if (Sources.Remove(InId) > 0)
	{
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
	const FVoxelSourceInterest* Admitted = CurrentInterest.Sources.Find(InId);
	return Source && Source->Owner.IsValid() && Admitted &&
		Source->Value.Center == Admitted->Source.Center &&
		!StreamingSourceAffectsInterest(Admitted->Source, Source->Value, ViewSettings);
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
	const FVoxelSourceInterest& Interest = CurrentInterest.Sources.FindChecked(InId);
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

void UVoxelModule::SetRemoteChangeState(
	const FIntVector& InSection,
	const EVoxelSectionChangeState InState)
{
	if (EmergeManager)
	{
		EmergeManager->SetRemoteChangeState(InSection, InState);
	}
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

void UVoxelModule::InvalidateRemoteRepresentations(
	const FVoxelRepresentationInvalidate& InInvalidate)
{
	if (ViewManager)
	{
		ViewManager->InvalidateRemoteRepresentations(InInvalidate);
	}
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
	}
	OnBlocksCommitted.Broadcast(InBatch);
}

bool UVoxelModule::IsProjectSceneSimulationEnabled() const
{
	return bEnableProjectSceneSimulation;
}

AActor* UVoxelModule::FindSceneActor(const FGuid& InId) const
{
	for (const TPair<FIntVector, TObjectPtr<UVoxelSceneRegion>>& Pair : SceneRegions)
	{
		if (Pair.Value)
		{
			if (AActor* Actor = Pair.Value->GetSceneActors().FindRef(InId))
			{
				return Actor;
			}
		}
	}
	return nullptr;
}

UVoxelSceneRegion* UVoxelModule::GetSceneRegion(const FIntVector& InSection, const bool bCreate)
{
	const FIntVector Region(
		VoxelGeneration::FloorDivide(InSection.X, 8),
		VoxelGeneration::FloorDivide(InSection.Y, 8),
		0);
	if (TObjectPtr<UVoxelSceneRegion>* Existing = SceneRegions.Find(Region))
	{
		return *Existing;
	}
	if (!bCreate || !SceneRegionClass)
	{
		return nullptr;
	}
	UVoxelSceneRegion* NewRegion = NewObject<UVoxelSceneRegion>(this, SceneRegionClass);
	if (!NewRegion)
	{
		return nullptr;
	}
	NewRegion->Initialize(this, Region);
	const FString ScenePath = FVoxelSceneColumnCodec::RelativePath(FIntPoint(Region.X, Region.Y));
	if (TArray<uint8>* SavedBytes = UnloadedSceneFiles.Find(ScenePath))
	{
		FString RestoreError;
		if (!NewRegion->RestoreActors(*SavedBytes, RestoreError))
		{
			NewRegion->Shutdown();
			UE_LOG(LogTemp, Error, TEXT("Voxel scene restore: %s"), *RestoreError);
			return nullptr;
		}
		UnloadedSceneFiles.Remove(ScenePath);
	}
	else
	{
		NewRegion->bSceneReady = true;
	}
	SceneRegions.Add(Region, NewRegion);
	return NewRegion;
}

void UVoxelModule::RefreshInterest(
	const double InNow)
{
	TArray<FVoxelStreamingSource>
		ActiveSources;

	ActiveSources.Reserve(
		Sources.Num());

	for (auto Iterator =
		Sources.CreateIterator();
		Iterator;
		++Iterator)
	{
		if (!Iterator.Value().
			Owner.IsValid())
		{
			Iterator.RemoveCurrent();
			bInterestDirty = true;
			continue;
		}

		ActiveSources.Add(
			Iterator.Value().Value);
	}

	// 一个观察者快照最多有一个规划任务；移动期间合并后续请求，旧分区保留到新结果提交。
	if (bInterestBuildPending)
	{
		return;
	}
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildInterest;
	Request.WorkClass = EVoxelWorkClass::Interactive;
	Request.Stamp.WorldEpoch = Epoch;
	Request.Stamp.Token = InterestRevision + 1;
	Request.ReservedBytes = 32ull * 1024ull * 1024ull;
	// 滞回只持有后台规划产生的不可变集合，移动时不再在主线程复制数万需求。
	FVoxelInterestSet Previous;
	Previous.FineSections = CurrentInterest.FineSections;
	Request.InputBytes = Previous.GetAllocatedBytes();
	Request.Execute = [ActiveSources, WorldManifest = Manifest, Settings = ViewSettings,
		Previous = MoveTemp(Previous)](const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		if (Cancel.Load())
		{
			return Result;
		}
		const auto Payload = MakeShared<FVoxelInterestTaskPayload, ESPMode::ThreadSafe>();
		Payload->Interest = MakeShared<FVoxelInterestSet, ESPMode::ThreadSafe>(
			FVoxelInterestManager().Compute(ActiveSources, WorldManifest, Settings, &Previous));
		Result.bSuccess = !Cancel.Load();
		Result.CustomPayload = Payload;
		return Result;
	};
	Request.Apply = [this](FVoxelTaskResult&& Result)
	{
		if (Result.Stamp.WorldEpoch != Epoch)
		{
			return;
		}
		bInterestBuildPending = false;
		if (!Result.bSuccess || Result.bCanceled)
		{
			bInterestDirty = true;
			return;
		}
		const auto Payload = StaticCastSharedPtr<const FVoxelInterestTaskPayload>(Result.CustomPayload);
		CurrentInterest = MoveTemp(*Payload->Interest);
		InterestRevision = InterestRevision == MAX_uint64 ? 1 : InterestRevision + 1;
		// 不清除执行期间新产生的移动请求，避免连续行走丢失末次位置。
	};
	if (!Scheduler->Enqueue(MoveTemp(Request)))
	{
		return;
	}
	bInterestBuildPending = true;
	LastInterestRefresh = InNow;
	bInterestDirty = false;
	for (auto& Pair : Sources)
	{
		Pair.Value.PlannedValue = Pair.Value.Value;
	}

	if (GenerationCache)
	{
		FVoxelGenerationCacheRetention Retention;
		Retention.HydrologyRegionSide = FMath::Max(8, Manifest.Settings.HydrologyRegionSide);
		Retention.HydrologyCellSize = FMath::Max(1, Manifest.Settings.HydrologyCellSize);
		const int64 HydrologySide = static_cast<int64>(Retention.HydrologyRegionSide) * Retention.HydrologyCellSize;
		for (const FVoxelStreamingSource& Source : ActiveSources)
		{
			if (!Source.bRetainGenerationCache) continue;
			FVoxelGenerationCacheRetentionPoint& Point = Retention.Points.AddDefaulted_GetRef();
			Point.Center = FIntPoint(Source.Center.X, Source.Center.Y);
			Point.NaturalRadiusCells = Source.RetentionRadiusCells > 0 ? Source.RetentionRadiusCells :
				FMath::Max(ViewSettings.SurfaceRadius, ViewSettings.VoxelProxyRadius) + 512;
			Point.PlanRadiusCells = Source.RetentionRadiusCells > 0 ? Source.RetentionRadiusCells : Point.NaturalRadiusCells + 512;
			Point.HydrologyRadiusCells = Source.RetentionRadiusCells > 0 ? Source.RetentionRadiusCells :
				static_cast<int32>(FMath::Min<int64>(MAX_int32, Point.PlanRadiusCells + HydrologySide));
		}
		Retention.Revision = InterestRevision + 1;
		GenerationCache->UpdateRetention(Retention);
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
				if (UVoxelSceneRegion* Region = GetSceneRegion(Section, true))
				{
					Region->OnSectionActivated(Section);
				}
			}
		}
		return;
	}
	case EVoxelTaskKind::BuildCollision:
		if (CollisionPresenter)
		{
			CollisionPresenter->OnTask(MoveTemp(InResult));
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
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_Readiness);

	const EVoxelWorldReadyStage PreviousReadyStage = ReadyStage;
	FVoxelWorldReadinessSnapshot Snapshot;
	Snapshot.WorldEpoch = static_cast<int64>(Epoch);
	Snapshot.bAssetsValidated = Registry.GetSnapshot().IsValid() && Shapes.IsValid();
	Snapshot.bRecipeFrozen = GenerationConfig && GenerationConfig->Recipe;
	Snapshot.bSpawnPlanReady = false;
	for (const TPair<FIntVector, FVoxelExactDemand>& Pair : CurrentInterest.Warmup)
	{
		if (!Pair.Value.bWarmupData)
		{
			continue;
		}
		Snapshot.bSpawnPlanReady = true;
		++Snapshot.RequiredSpawnSections;
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
	FVoxelPrimaryFineReadiness PrimaryFine;
	FVoxelPrimaryFineReadiness PlayableFine;
	if (ViewManager)
	{
		PrimaryFine = ViewManager->GetPrimaryFineReadiness(CurrentInterest.Warmup);
		PlayableFine = ViewManager->GetFineRadiusReadiness(CurrentInterest.PlayableFineKeys);
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
	const bool bPrimaryViewReady = bSpawnCollisionReady && PrimaryFine.IsComplete() && PlayableFine.IsComplete();
	if (WorldState == EVoxelWorldState::Failed)
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
	return CollectLocalViewObservers();
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
	if (InIntent.Action == EVoxelEditAction::ContainerTake ||
		InIntent.Action == EVoxelEditAction::ContainerPut)
	{
		return TransferContainer(InController, InSource, InIntent, Hit);
	}

	UAbilityInventoryBase* Inventory = ResolveInventory(InController, InSource);
	UAbilityInventorySlotBase* Slot = nullptr;
	uint16 PlaceType = 0;
	const UVoxelAgentComponent* Agent = InSource->FindComponentByClass<UVoxelAgentComponent>();
	bTrustedStandaloneCreative = bTrustedStandaloneCreative &&
		GetWorld()->GetNetMode() == NM_Standalone &&
		Agent &&
		Agent->bCreativeInStandalone;
	FAbilityItem Before;
	FAbilityItem After;
	if (InIntent.Action == EVoxelEditAction::Place)
	{
		const FVoxelRuntimeDefinition* Definition = Registry.GetSnapshot()->Find(InIntent.ExpectedItemID);
		if (!Definition)
		{
			Reply.Reason = TEXT("Unknown inventory block");
			return Reply;
		}
		PlaceType = Definition->TypeId;
		if (!bTrustedStandaloneCreative)
		{
			Slot = Inventory ? Inventory->GetSlotBySplitTypeAndIndex(
				ESlotSplitType::Shortcut,
				InIntent.InventorySlot) : nullptr;
			if (!Slot ||
				Slot != Inventory->GetSelectedSlot(ESlotSplitType::Shortcut) ||
				!Slot->IsEnabled() ||
				Slot->GetItem().ID != InIntent.ExpectedItemID ||
				Slot->GetItem().Count <= 0 ||
				Slot->GetItem().Level != 0)
			{
				Reply.Reason = TEXT("Server inventory slot does not contain this block");
				return Reply;
			}
			Before = Slot->GetItem();
			After = Before;
			--After.Count;
			if (After.Count == 0)
			{
				After = FAbilityItem::Empty;
			}
		}
	}

	FVoxelInteractionPlan Plan;
	if (!BuildInteractionPlan(
		Hit,
		InIntent.Action,
		PlaceType,
		InIntent.Direction,
		InSource,
		Plan,
		Reply.Reason) || !ValidateInteractionPlan(Plan, Reply.Reason))
	{
		return Reply;
	}
	if ((InIntent.Action == EVoxelEditAction::Place || InIntent.Action == EVoxelEditAction::Use) &&
		PlacementOverlapsActors(Plan))
	{
		Reply.Reason = TEXT("Result would overlap an actor");
		return Reply;
	}
	FVoxelPreparedEdit Prepared;
	if (!Runtime->PrepareEdit(Plan.Cells, Plan.Entities, Prepared, Reply.Reason))
	{
		return Reply;
	}

	TGuardValue<bool> Guard(bMutating, true);
	AAbilityPickUpVoxel* Drop = nullptr;
	if (Plan.DropID.IsValid() && Plan.DropCount > 0)
	{
		UVoxelSceneRegion* Region = GetSceneRegion(VoxelModuleToSection(Hit.Index), true);
		Drop = AAbilityPickUpVoxel::CreateReserved(
			GetWorld(),
			FAbilityItem(Plan.DropID, Plan.DropCount),
			Hit.Point,
			Region);
		if (!Drop)
		{
			Reply.Reason = TEXT("Cannot allocate a drop; block was not changed");
			return Reply;
		}
	}
	if (Slot && !FVoxelInventoryTransaction::SetSilent(*Slot, Before, After))
	{
		if (Drop)
		{
			Drop->Destroy();
		}
		Reply.Code = EVoxelEditCode::Stale;
		return Reply;
	}
	FVoxelEditBatch Batch;
	if (!Runtime->CommitPreparedEdit(MoveTemp(Prepared), Batch, Reply.Reason))
	{
		if (Slot)
		{
			FVoxelInventoryTransaction::RestoreSilent(*Slot, Before);
		}
		if (Drop)
		{
			Drop->Destroy();
		}
		Reply.Code = EVoxelEditCode::Stale;
		return Reply;
	}
	if (Drop)
	{
		Drop->ActivateReserved();
	}
	if (Slot)
	{
		FVoxelInventoryTransaction::Notify(*Slot, Before);
	}
	Breaking.Remove(InSource);
	PublishProjectEdit(Batch);
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

FVoxelEditReply UVoxelModule::TransferContainer(
	APlayerController* InController,
	AActor* InSource,
	const FVoxelEditIntent& InIntent,
	const FVoxelTraceResult& InHit)
{
	FVoxelEditReply Reply;
	Reply.RequestId = InIntent.RequestId;
	Reply.Code = EVoxelEditCode::Rejected;
	FVoxelSection* Section = Runtime->FindSection(VoxelModuleToSection(InHit.Index));
	if (!Section ||
		Section->CommittedRevision != InIntent.ExpectedRevision ||
		InIntent.ContainerSlot < 0 ||
		InIntent.ContainerSlot >= 27)
	{
		Reply.Code = EVoxelEditCode::Stale;
		return Reply;
	}
	FVoxelBlockEntityState* Entity = Section->Entities.Find(VoxelModuleToCellIndex(InHit.Index));
	TArray<FVoxelItemStack> Items;
	UAbilityInventoryBase* Inventory = ResolveInventory(InController, InSource);
	UAbilityInventorySlotBase* Slot = Inventory ? Inventory->GetSlotBySplitTypeAndIndex(
		ESlotSplitType::Shortcut,
		InIntent.InventorySlot) : nullptr;
	if (!Slot || !Slot->IsEnabled() || !Entity || !FVoxelBlockEntityCodec::DecodeContainer(*Entity, Items))
	{
		return Reply;
	}
	FAbilityItem Before = Slot->GetItem();
	FAbilityItem After = Before;
	FVoxelItemStack& Cell = Items[InIntent.ContainerSlot];
	const int32 Count = InIntent.Count;
	if (Count <= 0)
	{
		return Reply;
	}
	if (InIntent.Action == EVoxelEditAction::ContainerTake)
	{
		if (Cell.Count < Count)
		{
			return Reply;
		}
		const FAbilityItem Taken(Cell.ID, Count, Cell.Level);
		if (!Slot->MatchItem(Taken, true) || Slot->GetRemainVolume(Taken) < Count)
		{
			return Reply;
		}
		After = Before.Count > 0 ?
			FAbilityItem(Before.ID, Before.Count + Count, Before.Level) :
			Taken;
		Cell.Count -= Count;
		if (Cell.Count == 0)
		{
			Cell = {};
		}
	}
	else
	{
		if (Before.Count < Count ||
			!Before.ID.IsValid() ||
			Before.ID != InIntent.ExpectedItemID ||
			(Cell.Count > 0 && (Cell.ID != Before.ID || Cell.Level != Before.Level)))
		{
			return Reply;
		}
		const UAbilityItemDataBase* Data = UAssetModuleStatics::LoadPrimaryAsset<UAbilityItemDataBase>(Before.ID, false);
		if (!Data || Count > Data->MaxCount - Cell.Count)
		{
			return Reply;
		}
		Cell.ID = Before.ID;
		Cell.Level = Before.Level;
		Cell.Count += Count;
		After.Count -= Count;
		if (After.Count == 0)
		{
			After = FAbilityItem::Empty;
		}
	}

	FVoxelEntityEdit EntityEdit;
	EntityEdit.Position = InHit.Index;
	if (!FVoxelBlockEntityCodec::EncodeContainer(Items, EntityEdit.Value))
	{
		return Reply;
	}
	FVoxelPreparedEdit Prepared;
	if (!Runtime->PrepareEdit({}, { EntityEdit }, Prepared, Reply.Reason))
	{
		return Reply;
	}
	TGuardValue<bool> Guard(bMutating, true);
	if (!FVoxelInventoryTransaction::SetSilent(*Slot, Before, After))
	{
		Reply.Code = EVoxelEditCode::Stale;
		return Reply;
	}
	FVoxelEditBatch Batch;
	if (!Runtime->CommitPreparedEdit(MoveTemp(Prepared), Batch, Reply.Reason))
	{
		FVoxelInventoryTransaction::RestoreSilent(*Slot, Before);
		Reply.Code = EVoxelEditCode::Stale;
		return Reply;
	}
	FVoxelInventoryTransaction::Notify(*Slot, Before);
	PublishProjectEdit(Batch);
	Reply.Code = EVoxelEditCode::Accepted;
	return Reply;
}

bool UVoxelModule::ApplyPrefab(
	const FVoxelPrefabSaveData& InPrefab,
	const FIntVector& InOrigin,
	FString& OutError)
{
	if (!IsAuthority() || !IsReady() || bMutating || InPrefab.Cells.IsEmpty() || InPrefab.Cells.Num() > 4096)
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
			!FVoxelItemBridge::ToBlock(*Registry.GetSnapshot(), Cell.Item, Edit.Value))
		{
			OutError = TEXT("Prefab contains duplicate, unloaded, or invalid cells");
			return false;
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
	FVoxelPreparedEdit Prepared;
	if (!Runtime->PrepareEdit(Plan.Cells, Plan.Entities, Prepared, OutError))
	{
		return false;
	}
	TGuardValue<bool> Guard(bMutating, true);
	FVoxelEditBatch Batch;
	if (!Runtime->CommitPreparedEdit(MoveTemp(Prepared), Batch, OutError))
	{
		return false;
	}
	PublishProjectEdit(Batch);
	return true;
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
	if (SizeX <= 0 || SizeY <= 0 || SizeZ <= 0 || SizeX * SizeY * SizeZ > 4096)
	{
		OutError = TEXT("Prefab bounds exceed 4096 cells");
		return false;
	}
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
				Cell.Offset = Position - InMin;
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
	if (!SceneSourceError.IsEmpty())
	{
		OutError = SceneSourceError;
		return false;
	}
	const FVoxelWorldSaveData* Data =
		InData.GetPtr<FVoxelWorldSaveData>();
	if (!Data)
	{
		OutError = TEXT("Expected typed voxel world save data");
		return false;
	}
	FVoxelWorldManifest SavedManifest;
	if (!FVoxelManifestCodec::Decode(Data->ManifestBytes, SavedManifest) ||
		!Registry.GetSnapshot() ||
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
		OutError) ||
		!Config.Recipe ||
		Config.Recipe->RecipeHash != SavedManifest.RecipeHash)
	{
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
		SceneSourceError.Reset();

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
	RegionStore.SetSource(InSaveId, InGeneration, InStorage);
	if (InStorage &&
		InSaveId.IsValid() &&
		InGeneration > 0)
	{
		bPersistenceEnabledForCurrentWorld = true;
	}
	UnloadedSceneFiles.Reset();
	SceneSourceError.Reset();
	const FString& SourceDirectory = RegionStore.GetSourceDirectory();
	if (SourceDirectory.IsEmpty())
	{
		return;
	}
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(
		Files,
		*FPaths::Combine(SourceDirectory, TEXT("voxel"), TEXT("actors")),
		TEXT("c_*_*.bin"),
		true,
		false);
	for (const FString& File : Files)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *File) || Bytes.Num() > 8 * 1024 * 1024)
		{
			SceneSourceError = TEXT("Voxel scene column could not be read");
			UnloadedSceneFiles.Reset();
			return;
		}
		TArray<FVoxelSavedSceneActor> Records;
		FParameter ProjectData;
		if (!FVoxelSceneColumnCodec::Decode(Bytes, Records, ProjectData, SceneSourceError))
		{
			UnloadedSceneFiles.Reset();
			return;
		}
		const FString Relative = File.RightChop(SourceDirectory.Len() + 1).Replace(TEXT("\\"), TEXT("/"));
		UnloadedSceneFiles.Add(Relative, MoveTemp(Bytes));
	}
}

void UVoxelModule::OnBeforeSaveData()
{
	LastSaveError.Reset();
	CapturedSceneFiles = UnloadedSceneFiles;
	if (!IsSaveEnabled())
	{
		return;
	}
	if (!Runtime)
	{
		LastSaveError = TEXT("Create or load the voxel world before capturing it");
		return;
	}
	if (IsAuthority())
	{
		for (const TPair<FIntVector, TObjectPtr<UVoxelSceneRegion>>& Pair : SceneRegions)
		{
			if (!Pair.Value)
			{
				continue;
			}
			TArray<uint8> Bytes;
			if (!Pair.Value->CaptureActors(Bytes, LastSaveError))
			{
				CapturedSceneFiles.Reset();
				return;
			}
			CapturedSceneFiles.Add(
				FVoxelSceneColumnCodec::RelativePath(FIntPoint(Pair.Key.X, Pair.Key.Y)),
				MoveTemp(Bytes));
		}
	}
	if (bMutating ||
		!SaveAdapter.Capture(
			*Runtime,
			Manifest,
			Registry.GetSnapshot(),
			RegionStore,
			LastSaveError))
	{
		if (LastSaveError.IsEmpty())
		{
			LastSaveError = TEXT("Voxel save capture is busy");
		}
	}
}

bool UVoxelModule::CopySaveCapture(FVoxelModuleSaveCapture& OutCapture, FString& OutError) const
{
	if (!LastSaveError.IsEmpty() || !SaveAdapter.GetCapture())
	{
		OutError = LastSaveError.IsEmpty() ? TEXT("Voxel save was not captured") : LastSaveError;
		return false;
	}
	OutCapture.Voxels = *SaveAdapter.GetCapture();
	OutCapture.SceneFiles = CapturedSceneFiles;
	return true;
}

void UVoxelModule::SetPendingCommitDirectory(const FString& InDirectory)
{
	PendingCommitDirectory = InDirectory;
}

bool UVoxelModule::WriteSaveCapture(
	const FVoxelModuleSaveCapture& InCapture,
	const FString& InTemporaryGeneration,
	FString& OutError)
{
	if (!FVoxelWorldSaveAdapter::WriteCapture(
		InCapture.Voxels,
		InTemporaryGeneration,
		OutError))
	{
		return false;
	}
	for (const TPair<FString, TArray<uint8>>& Pair : InCapture.SceneFiles)
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
		if (!FFileHelper::SaveArrayToFile(Pair.Value, *Path))
		{
			OutError = TEXT("Voxel scene region write failed");
			return false;
		}
	}
	return true;
}

void UVoxelModule::OnAfterSaveData(const bool bInSuccess)
{
	if (Runtime && SaveAdapter.IsBusy())
	{
		SaveAdapter.Complete(*Runtime, RegionStore, bInSuccess, PendingCommitDirectory);
	}
	CapturedSceneFiles.Reset();
	LastSaveError.Reset();
	PendingCommitDirectory.Reset();
}
