#include "Voxel/Scene/VoxelSceneRegion.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "GameFramework/Pawn.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"
#include "SaveGame/SaveDataSerializer.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/Save/VoxelSceneColumnCapture.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelSectionKey.h"

UWorld* UVoxelSceneRegion::GetWorld() const
{
	return Module ? Module->GetWorld() : nullptr;
}

void UVoxelSceneRegion::Initialize(
	UVoxelModule* InModule,
	const FIntVector& InRegionKey)
{
	check(IsInGameThread());
	check(InModule);
	check(!Module);
	Module = InModule;
	RegionKey = InRegionKey;
	SceneInstanceId = FGuid::NewGuid();
	SceneRevision = 1;
	DurableSceneRevision = 0;
}

void UVoxelSceneRegion::Shutdown()
{
	check(!SimulationSaveHead && !SimulationFailureHead);
	DetachSceneLoad();
	DestroySceneActors();
	ActiveSections.Reset();
	PendingActors.Empty();
	PendingActorStorageLease.Reset();
	ResidencyRefreshSeconds = 0.0f;
	Module = nullptr;
	bSceneReady = false;
	bSceneLoading = false;
	bSceneFailed = false;
	bIdleSaveWaiting = false;
	SceneInstanceId.Invalidate();
}

void UVoxelSceneRegion::OnSectionActivated(const FIntVector& InSection)
{
	ActiveSections.Add(InSection);
	MarkSceneChanged();
	bIdleSaveWaiting = false;
}

void UVoxelSceneRegion::OnSectionDeactivated(const FIntVector& InSection)
{
	ActiveSections.Remove(InSection);
	MarkSceneChanged();
	// 保存与实体迁移已经由 TryDeactivateSection 验证；此钩子仅提交区域和项目模拟状态。
}

bool UVoxelSceneRegion::TryDeactivateSection(const FIntVector& InSection,
	TArray<uint8>& OutRegionBytes, FString& OutError, bool* OutWaitingForScene)
{
	check(IsInGameThread());
	if (OutWaitingForScene) *OutWaitingForScene = false;
	if (!Module || !Module->IsAuthority() || !ActiveSections.Contains(InSection))
	{
		OutError = TEXT("Simulation section has no active scene-region owner");
		return false;
	}
	TArray<FVoxelSavedSceneActor> Actors;
	FParameter ProjectData;
	TArray<uint8> Encoded;
	// 先冻结并验证完整保存输入；失败时没有撤销 ActiveSections、Module 驻留或 Runtime pin。
	if (!CaptureActorStates(Actors, ProjectData, OutError))
	{
		return false;
	}
	const auto PrepareSnapshot = [&](TConstArrayView<FGuid> MigratingIds)
	{
		TArray<FVoxelSavedSceneActor> Remaining;
		Remaining.Reserve(Actors.Num());
		for (const auto& Actor : Actors)
		{
			// 已准备移交到另一Region的Actor不能再写入本Region的休眠文件。
			if (!MigratingIds.Contains(Actor.Id))
			{
				Remaining.Add(Actor);
			}
		}
		return CaptureProjectDataForActors(Remaining, ProjectData) &&
			FVoxelSceneColumnCodec::Encode(Remaining, ProjectData, Encoded, OutError);
	};
	const TFunctionRef<bool(TConstArrayView<FGuid>)> BeforeCommit(PrepareSnapshot);
	if (!CaptureInactiveActors(OutError, &InSection, Actors, &BeforeCommit, OutWaitingForScene))
	{
		return false;
	}
	OnSectionDeactivated(InSection);
	if (!HasActiveSections())
	{
		OutRegionBytes = MoveTemp(Encoded);
	}
	else
	{
		OutRegionBytes.Reset();
	}
	OutError.Reset();
	return true;
}

void UVoxelSceneRegion::TickSceneActors(float InDeltaSeconds)
{
	if (!Module || !Module->IsAuthority() || !bSceneReady || bSceneFailed || bIdleSaveWaiting)
	{
		return;
	}
	ResidencyRefreshSeconds += InDeltaSeconds;
	if (ResidencyRefreshSeconds < 0.25f)
	{
		return;
	}
	ResidencyRefreshSeconds = 0.0f;
	FString Error;
	bool bWaitingForScene = false;
	if (!CaptureInactiveActors(Error, nullptr, {}, nullptr, &bWaitingForScene))
	{
		if (bWaitingForScene) return;
		bSceneFailed = true;
		UE_LOG(LogTemp, Error, TEXT("Simulation actor capture failed: %s"), *Error);
		return;
	}
	int32 Remaining = 2;
	int32 Attempts = FMath::Min(16, PendingActors.GetMaxIndex());
	while (Remaining > 0 && Attempts-- > 0 && !PendingActors.IsEmpty())
	{
		PendingActorCursor %= PendingActors.GetMaxIndex();
		const FSetElementId Index = FSetElementId::FromInteger(PendingActorCursor++);
		if (!PendingActors.IsValidId(Index)) continue;
		const FGuid Id = PendingActors.Get(Index).Key;
		if (RestorePendingActor(Id, PendingActors.Get(Index).Value))
		{
			PendingActors.Remove(Id);
			MarkSceneChanged();
			--Remaining;
		}
	}
}

bool UVoxelSceneRegion::CaptureInactiveActors(FString& OutError, const FIntVector* InRetiringSection,
	TConstArrayView<FVoxelSavedSceneActor> InFrozenActors,
	const TFunctionRef<bool(TConstArrayView<FGuid>)>* InBeforeCommit, bool* OutWaitingForScene)
{
	if (OutWaitingForScene) *OutWaitingForScene = false;
	if (!Module || !Module->IsAuthority())
	{
		return true;
	}
	const uint64 CapturedSceneRevision = SceneRevision;
	struct FRetiringActor
	{
		AActor* Actor = nullptr;
		FVoxelSavedSceneActor Saved;
	};
	TArray<FRetiringActor> Retiring;
	TArray<FVoxelSavedSceneActor> Validation;
	TArray<TPair<AActor*, UVoxelSceneRegion*>> Migrating;
	TArray<FGuid> MigratingIds;
	for (const auto& Pair : SceneActorMap)
	{
		AActor* Actor = Pair.Value;
		if (!ShouldPersistActor(Actor))
		{
			continue;
		}
		const FVector Cell = Actor->GetActorLocation() / Module->BlockSize();
		const FVoxelSectionKey Section = VoxelCoord::Section(FIntVector(FMath::FloorToInt(Cell.X), FMath::FloorToInt(Cell.Y), FMath::FloorToInt(Cell.Z)));
		const FIntVector SectionCoordinate(Section.X, Section.Y, Section.Z);
		UVoxelSceneRegion* Destination = Module->GetSceneRegionForActor(Actor, SectionCoordinate);
		if (Destination && Destination->bSceneLoading)
		{
			if (OutWaitingForScene) *OutWaitingForScene = true;
			OutError.Reset();
			return false;
		}
		if (Destination && Destination->bSceneFailed)
		{
			OutError = TEXT("Actor destination Scene region has a retained restore failure");
			return false;
		}
		if (!Destination)
		{
			OutError = TEXT("Scene actor cannot obtain its destination persistence region");
			return false;
		}
		if (Destination != this)
		{
			if (Destination->SceneActorMap.Contains(Pair.Key) || Destination->PendingActors.Contains(Pair.Key))
			{
				OutError = TEXT("Scene migration destination already owns the actor identity");
				return false;
			}
			Migrating.Add({Actor, Destination});
			MigratingIds.Add(Pair.Key);
			continue;
		}
		if (Module->IsSimulationResident(SectionCoordinate) &&
			(!InRetiringSection || *InRetiringSection != SectionCoordinate))
		{
			continue;
		}
		if (PendingActors.Contains(Pair.Key))
		{
			OutError = TEXT("Scene actor has both active and pending persistence owners");
			return false;
		}
		FParameter Data;
		if (InRetiringSection)
		{
			const auto* Frozen = InFrozenActors.FindByPredicate([&Pair](const FVoxelSavedSceneActor& Saved)
			{
				return Saved.Id == Pair.Key;
			});
			if (!Frozen)
			{
				OutError = TEXT("Scene retirement no longer matches its frozen actor set");
				return false;
			}
			Data = Frozen->Data;
		}
		else
		{
			ISaveDataAgentInterface* Agent = Cast<ISaveDataAgentInterface>(Actor);
			if (!Agent)
			{
				OutError = TEXT("Scene actor has no save data owner");
				return false;
			}
			Data = Agent->GetSaveData(true);
			Validation.Add({Pair.Key, Data});
		}
		Retiring.Add({Actor, {Pair.Key, MoveTemp(Data)}});
	}
	if (!Validation.IsEmpty())
	{
		// 普通区域维护也必须先验证序列化，不能把不支持的 Data 放入 Pending 后就销毁实体。
		TArray<uint8> Encoded;
		if (!FVoxelSceneColumnCodec::Encode(Validation, FParameter(), Encoded, OutError))
		{
			return false;
		}
	}
	if (InBeforeCommit && !(*InBeforeCommit)(MigratingIds))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Scene retirement project snapshot capture failed");
		}
		return false;
	}
	if (SceneRevision != CapturedSceneRevision)
	{
		OutError = TEXT("Scene actor cut changed during capture or project validation");
		return false;
	}
	int32 AcceptedTransfers = 0;
	for (const auto& Transfer : Migrating)
	{
		if (!Transfer.Value->AddSceneActor(Transfer.Key) || SceneRevision != CapturedSceneRevision)
		{
			// 原Region仍持全体Actor；只有所有目标都接纳后才撤原Map。
			// 包括可能先登记、随后拒收的当前目标，回滚目标登记和Actor的container。
			for (int32 Index = 0; Index <= AcceptedTransfers; ++Index)
			{
				const auto& Rollback = Migrating[Index];
				Rollback.Value->RemoveSceneActor(Rollback.Key);
				if (IsValid(Rollback.Key))
					ISceneActorInterface::Execute_SetContainer(Rollback.Key, TScriptInterface<ISceneContainerInterface>(this));
			}
			OutError = TEXT("Scene actor region transfer rejected");
			return false;
		}
		++AcceptedTransfers;
	}
	for (const auto& Transfer : Migrating)
	{
		RemoveSceneActor(Transfer.Key);
	}
	for (auto& Item : Retiring)
	{
		PendingActors.Add(Item.Saved.Id, MoveTemp(Item.Saved.Data));
		RemoveSceneActor(Item.Actor);
		Item.Actor->Destroy();
	}
	return true;
}

bool UVoxelSceneRegion::RestorePendingActor(const FGuid& InId, const FParameter& InData)
{
	FVector Location;
	const FPickUpSaveData* Pickup = InData.GetPtr<FPickUpSaveData>();
	const FSceneActorSaveData* SceneActor = InData.GetPtr<FSceneActorSaveData>();
	if (Pickup)
	{
		Location = Pickup->Location;
	}
	else if (SceneActor)
	{
		Location = SceneActor->SpawnTransform.GetLocation();
	}
	else
	{
		bSceneFailed = true;
		return false;
	}
	const FVector Cell = Location / Module->BlockSize();
	const FVoxelSectionKey Key = VoxelCoord::Section(FIntVector(FMath::FloorToInt(Cell.X), FMath::FloorToInt(Cell.Y), FMath::FloorToInt(Cell.Z)));
	const FIntVector Section(Key.X, Key.Y, Key.Z);
	if (!Module->IsSimulationResident(Section) || !Module->IsCollisionReady(Section) ||
		(InData.GetPtr<FPawnSaveData>() && !Module->IsNavigationReady(Section)))
	{
		return false;
	}
	if (Module->FindSceneActor(InId))
	{
		bSceneFailed = true;
		return false;
	}
	TGuardValue<FGuid> RestoreIdentity(RestoringActorId, InId);
	AActor* Actor = Pickup ? static_cast<AActor*>(UAbilityModuleStatics::SpawnAbilityPickUp(InData, this))
		: UAbilityModuleStatics::SpawnAbilityActor(InData, this);
	if (!Actor || !Actor->Implements<USceneActorInterface>())
	{
		if (Actor)
		{
			Actor->Destroy();
		}
		bSceneFailed = true;
		return false;
	}
	RemoveSceneActor(Actor);
	ISceneActorInterface::Execute_SetActorID(Actor, InId.ToString());
	Actor->SetReplicates(true);
	if (!AddSceneActor(Actor))
	{
		Actor->Destroy();
		return false;
	}
	return true;
}

bool UVoxelSceneRegion::CaptureProjectData(FParameter& OutData) const
{
	OutData = FParameter();
	return true;
}

bool UVoxelSceneRegion::CaptureProjectDataForActors(TConstArrayView<FVoxelSavedSceneActor> InActors,
	FParameter& OutData) const
{
	(void)InActors;
	return CaptureProjectData(OutData);
}

bool UVoxelSceneRegion::RestoreProjectData(
	const FParameter& InData,
	FString& OutError)
{
	if (InData.HasValue())
	{
		OutError = TEXT("Unexpected voxel scene region project data");
		return false;
	}
	OutError.Reset();
	return true;
}

bool UVoxelSceneRegion::ShouldPersistActor(AActor* InActor) const
{
	return IsValid(InActor) &&
		!InActor->IsActorBeingDestroyed() &&
		!(Cast<APawn>(InActor) && CastChecked<APawn>(InActor)->IsPlayerControlled());
}

bool UVoxelSceneRegion::CaptureActors(
	TArray<uint8>& OutBytes,
	FString& OutError) const
{
	TArray<FVoxelSavedSceneActor> Actors;
	FParameter ProjectData;
	return CaptureActorStates(Actors, ProjectData, OutError) &&
		FVoxelSceneColumnCodec::Encode(Actors, ProjectData, OutBytes, OutError);
}

bool UVoxelSceneRegion::CaptureActorStates(TArray<FVoxelSavedSceneActor>& OutActors,
	FParameter& OutProjectData, FString& OutError) const
{
	OutActors.Reset();
	for (const auto& Pair : PendingActors)
	{
		OutActors.Add({Pair.Key, Pair.Value});
	}
	for (const TPair<FGuid, TObjectPtr<AActor>>& Pair : SceneActorMap)
	{
		if (!ShouldPersistActor(Pair.Value))
		{
			continue;
		}
		ISaveDataAgentInterface* Agent = Cast<ISaveDataAgentInterface>(Pair.Value);
		if (!Agent)
		{
			OutError = TEXT("Voxel scene actor does not implement SaveDataAgent");
			return false;
		}
		FVoxelSavedSceneActor Saved;
		Saved.Id = Pair.Key;
		Saved.Data = Agent->GetSaveData(true);
		OutActors.Add(MoveTemp(Saved));
	}

	if (!CaptureProjectData(OutProjectData))
	{
		OutError = TEXT("Voxel scene region project data capture failed");
		return false;
	}
	return true;
}

EVoxelSceneCaptureStep UVoxelSceneRegion::AdvanceProjectSaveCapture(
	FVoxelSceneColumnCaptureCursor& InCursor, FString& OutError) const
{
	if (InCursor.ProjectRevision != MAX_uint64) return EVoxelSceneCaptureStep::Complete;
	if (!CaptureProjectData(InCursor.ProjectData))
	{
		OutError = TEXT("Scene project payload capture failed");
		return EVoxelSceneCaptureStep::Failed;
	}
	InCursor.ProjectRevision = 0;
	return EVoxelSceneCaptureStep::Complete;
}

EVoxelSceneCaptureStep UVoxelSceneRegion::AdvanceSavePayloads(
	FVoxelSceneColumnCaptureCursor& InCursor, FString& OutError) const
{
	check(IsInGameThread());
	if (bSceneLoading) return EVoxelSceneCaptureStep::Pending;
	if (bSceneFailed || !bSceneReady)
	{
		OutError = TEXT("Scene save cannot capture an unrestored or failed region");
		return EVoxelSceneCaptureStep::Failed;
	}
	const auto Resources = Module ? Module->GetTerrainFactResources() : nullptr;
	if (!Module || !Module->IsAuthority() || !Resources || !SceneInstanceId.IsValid())
	{
		OutError = TEXT("Scene payload capture has no authoritative resource owner");
		return EVoxelSceneCaptureStep::Failed;
	}
	if (InCursor.WorldEpoch != 0 && (InCursor.WorldEpoch != Module->GetWorldEpoch() ||
		InCursor.RegionInstanceId != SceneInstanceId || InCursor.Revision != SceneRevision))
	{
		OutError = TEXT("Scene column producer no longer matches its frozen Region cut");
		return EVoxelSceneCaptureStep::Failed;
	}
	if (InCursor.WorldEpoch == 0)
	{
		const int64 ActorCount = int64(PendingActors.Num()) + SceneActorMap.Num();
		InCursor.WorldEpoch = Module->GetWorldEpoch();
		InCursor.RegionInstanceId = SceneInstanceId;
		InCursor.Revision = SceneRevision;
		InCursor.ActorCapacity = int32(FMath::Min<int64>(1024, ActorCount));
		InCursor.Consumer = FVoxelGenerationConsumerKey::Section(InCursor.WorldEpoch, SceneInstanceId,
			SceneRevision, RegionKey, 1);
	}
	if (InCursor.bCapacityBlocked)
	{
		uint64 Version = 0;
		if (!Resources->TryGetReleaseVersionExcludingOwner(InCursor.Consumer, Version) || Version <= InCursor.BlockedReleaseVersion)
			return EVoxelSceneCaptureStep::CapacityBlocked;
	}
	InCursor.bCapacityBlocked = false;
	OutError.Reset();
	FVoxelGenerationRequiredSourceScope PendingSource(Resources.Get(), PendingActorStorageLease.Get());
	FVoxelGenerationRequiredSourceScope ProjectStateSource(Resources.Get(), InCursor.ProjectStateLease.Get());
	FVoxelGenerationRequiredSourceScope CaptureSource(Resources.Get(), InCursor.Capture.Get(),
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Capture = *static_cast<const FVoxelSceneColumnCapture*>(Context);
			Capture.VisitRequiredSources(*Capture.Resources, Visit);
		});
	if (InCursor.Phase == 0)
	{
		const int32 Capacity = InCursor.ActorCapacity;
		const auto Consumer = InCursor.Consumer;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		const uint64 MetadataBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSceneColumnCapture>() +
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSceneColumnCompletion>() +
			VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelSceneActorPayload, FDefaultAllocator>(Capacity);
		const uint64 BeforeGrant = Resources->ReleaseVersionExcludingOwner(Consumer);
		const auto Admission = FVoxelSceneColumnCapture::TryGrantCapacity(*Resources, Consumer,
			EVoxelTerrainResourcePool::Input, MetadataBytes, MetadataLease, OutError);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			InCursor.bCapacityBlocked = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
			InCursor.BlockedReleaseVersion = BeforeGrant;
			return InCursor.bCapacityBlocked ? EVoxelSceneCaptureStep::CapacityBlocked : EVoxelSceneCaptureStep::Failed;
		}
		auto Capture = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSceneColumnCapture>(MetadataLease);
		Capture->MetadataLease = MetadataLease;
		Capture->Resources = Resources;
		Capture->Consumer = Consumer;
		Capture->Actors.Reserve(Capacity);
		check((VoxelGenerationAllocation::AllocatedBytes(Capture->Actors) <=
			VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelSceneActorPayload, FDefaultAllocator>(Capacity)));
		Capture->Completion = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSceneColumnCompletion>(MetadataLease);
		InCursor.Capture = MoveTemp(Capture);
		InCursor.WorldEpoch = Module->GetWorldEpoch();
		InCursor.RegionInstanceId = SceneInstanceId;
		InCursor.Revision = SceneRevision;
		InCursor.ActorCapacity = Capacity;
		InCursor.Phase = 1;
		return EVoxelSceneCaptureStep::Pending;
	}
	const auto Capture = InCursor.Capture;
	if (!Capture)
	{
		OutError = TEXT("Scene column producer lost its granted payload root");
		return EVoxelSceneCaptureStep::Failed;
	}
	const auto Consumer = Capture->Consumer;
	bool bCapacityBlocked = false;
	uint64 BlockedVersion = Resources->ReleaseVersionExcludingOwner(Consumer);
	auto Serialize = [&](FParameter Data, const int32 MaximumBytes, TArray<uint8>& Bytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		FVoxelGenerationRequiredSourceScope ProjectSource(Resources.Get(), InCursor.ProjectStateLease.Get());
		if (!FSaveDataSerializer::MarkSaveDataSaved(Data))
		{
			OutError = TEXT("Scene payload has no supported SaveData owner");
			return false;
		}
		const auto Reserve = [&](TArray<uint8>& Buffer, const int32 Required)
		{
			if (Required <= Buffer.Max()) return true;
			const int32 Grow = Buffer.Max() <= MaximumBytes / 2 ? Buffer.Max() * 2 : MaximumBytes;
			const int32 Target = FMath::Min(MaximumBytes, FMath::Max(Required, FMath::Max(64, Grow)));
			const uint64 NewBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(Target);
			const uint64 Peak = VoxelGenerationAllocation::AllocatedBytes(Buffer) + NewBytes;
			BlockedVersion = Resources->ReleaseVersionExcludingOwner(Consumer);
			const auto Admission = FVoxelSceneColumnCapture::TryGrantCapacity(*Resources, Consumer,
				EVoxelTerrainResourcePool::Input, Peak, Lease, OutError);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				bCapacityBlocked = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
				return false;
			}
			Buffer.Reserve(Target);
			check(VoxelGenerationAllocation::AllocatedBytes(Buffer) <= NewBytes);
			verify(Lease->TryResize(VoxelGenerationAllocation::AllocatedBytes(Buffer)) == EVoxelTerrainResourceAdmission::Granted);
			return true;
		};
		if (!FSaveDataSerializer::SerializeParameterBounded(Data, MaximumBytes, Bytes, Reserve))
		{
			if (OutError.IsEmpty()) OutError = TEXT("Scene serialized payload is invalid or exceeds its protocol limit");
			return false;
		}
		return true;
	};
	auto AppendActor = [&](const FGuid& Id, FParameter Data)
	{
		if (!Id.IsValid() || Capture->Actors.Num() >= InCursor.ActorCapacity)
		{
			OutError = TEXT("Scene payload actor identity/count is invalid");
			return false;
		}
		auto& Actor = Capture->Actors.AddDefaulted_GetRef();
		Actor.Id = Id;
		if (Serialize(MoveTemp(Data), 256 * 1024, Actor.Bytes, Actor.ResourceLease)) return true;
		Capture->Actors.RemoveAt(Capture->Actors.Num() - 1, 1, EAllowShrinking::No);
		return false;
	};
	bool bSuccess = true;
	if (InCursor.Phase == 1)
	{
		if (InCursor.PendingCursor >= PendingActors.GetMaxIndex()) InCursor.Phase = 2;
		else
		{
			const auto Id = FSetElementId::FromInteger(InCursor.PendingCursor++);
			if (PendingActors.IsValidId(Id))
			{
				const auto& Pair = PendingActors.Get(Id);
				bSuccess = AppendActor(Pair.Key, Pair.Value);
				if (!bSuccess && bCapacityBlocked) --InCursor.PendingCursor;
			}
		}
	}
	else if (InCursor.Phase == 2)
	{
		if (InCursor.ActorCursor >= SceneActorMap.GetMaxIndex()) InCursor.Phase = 3;
		else
		{
			const auto Id = FSetElementId::FromInteger(InCursor.ActorCursor++);
			if (SceneActorMap.IsValidId(Id))
			{
				const auto& Pair = SceneActorMap.Get(Id);
				const FGuid ActorId = Pair.Key;
				if (ShouldPersistActor(Pair.Value))
				{
					auto* Agent = Cast<ISaveDataAgentInterface>(Pair.Value);
					bSuccess = Agent && AppendActor(ActorId, Agent->GetSaveData(true));
					if (!bSuccess && OutError.IsEmpty()) OutError = TEXT("Scene actor has no save data agent");
					if (!bSuccess && bCapacityBlocked) --InCursor.ActorCursor;
				}
			}
		}
	}
	else if (InCursor.Phase == 3)
	{
		BlockedVersion = Resources->ReleaseVersionExcludingOwner(Consumer);
		const auto Step = AdvanceProjectSaveCapture(InCursor, OutError);
		if (Step == EVoxelSceneCaptureStep::Complete) InCursor.Phase = 4;
		else if (Step == EVoxelSceneCaptureStep::Failed) return Step;
		else if (Step == EVoxelSceneCaptureStep::CapacityBlocked) bCapacityBlocked = true;
	}
	else if (InCursor.Phase == 4)
	{
		// 项目游标已完成后仍必须验证其版本，序列化不能接受较早帧的失效值。
		BlockedVersion = Resources->ReleaseVersionExcludingOwner(Consumer);
		const auto ProjectStep = AdvanceProjectSaveCapture(InCursor, OutError);
		if (ProjectStep == EVoxelSceneCaptureStep::CapacityBlocked) bCapacityBlocked = true;
		else if (ProjectStep != EVoxelSceneCaptureStep::Complete) return ProjectStep;
		else
		{
			bSuccess = !InCursor.ProjectData.HasValue() || Serialize(InCursor.ProjectData, 1024 * 1024, Capture->ProjectBytes, Capture->ProjectLease);
			if (bSuccess) InCursor.Phase = 5;
		}
	}
	if (!Module || InCursor.RegionInstanceId != SceneInstanceId || InCursor.Revision != SceneRevision || InCursor.WorldEpoch != Module->GetWorldEpoch())
	{
		OutError = TEXT("Scene cut changed during its local Actor/project capture callback");
		return EVoxelSceneCaptureStep::Failed;
	}
	if (bCapacityBlocked)
	{
		InCursor.bCapacityBlocked = true;
		InCursor.BlockedReleaseVersion = BlockedVersion;
		return EVoxelSceneCaptureStep::CapacityBlocked;
	}
	if (!bSuccess) return EVoxelSceneCaptureStep::Failed;
	OutError.Reset();
	return InCursor.Phase == 5 ? EVoxelSceneCaptureStep::Complete : EVoxelSceneCaptureStep::Pending;
}

bool UVoxelSceneRegion::RestoreActors(
	TConstArrayView<uint8> InBytes,
	FString& OutError)
{
	if (!Module || !Module->IsAuthority() || bSceneReady)
	{
		OutError = TEXT("Voxel scene region is not ready for restore");
		return false;
	}

	TArray<FVoxelSavedSceneActor> Records;
	FParameter ProjectData;
	if (!FVoxelSceneColumnCodec::Decode(InBytes, Records, ProjectData, OutError))
	{
		return false;
	}

	for (const FVoxelSavedSceneActor& Record : Records)
	{
		if (Module->FindSceneActor(Record.Id) || PendingActors.Contains(Record.Id))
		{
			OutError = TEXT("Duplicate stored voxel scene actor GUID");
			return false;
		}
	}
	// Validate the whole actor cut before project restoration can publish side effects.
	if (!RestoreProjectData(ProjectData, OutError)) return false;
	for (auto& Record : Records) PendingActors.Add(Record.Id, MoveTemp(Record.Data));

	bSceneReady = true;
	bSceneLoading = false;
	MarkSceneChanged();
	OutError.Reset();
	return true;
}

bool UVoxelSceneRegion::HasSceneActor(
	const FString& InId,
	bool bInEnsured) const
{
	return GetSceneActor(InId, nullptr, bInEnsured) != nullptr;
}

AActor* UVoxelSceneRegion::GetSceneActor(
	const FString& InId,
	TSubclassOf<AActor> InClass,
	bool bInEnsured) const
{
	(void)bInEnsured;
	FGuid Id;
	if (!FGuid::Parse(InId, Id))
	{
		return nullptr;
	}
	AActor* Actor = SceneActorMap.FindRef(Id);
	return IsValid(Actor) && (!InClass || Actor->IsA(InClass)) ? Actor : nullptr;
}

bool UVoxelSceneRegion::AddSceneActor(AActor* InActor)
{
	if (!IsValid(InActor) ||
		!Module ||
		!bSceneReady || bSceneLoading || bSceneFailed ||
		!Module->IsAuthority() ||
		!InActor->Implements<USceneActorInterface>())
	{
		return false;
	}
	const FGuid Id = ISceneActorInterface::Execute_GetActorID(InActor);
	if (!Id.IsValid())
	{
		return false;
	}
	if ((PendingActors.Contains(Id) && RestoringActorId != Id) ||
		(SceneActorMap.Contains(Id) && SceneActorMap.FindRef(Id) != InActor)) return false;
	if (AActor* Existing = Module->FindSceneActor(Id))
	{
		if (Existing != InActor)
		{
			return false;
		}
	}
	SceneActorMap.Add(Id, InActor);
	MarkSceneChanged();
	bIdleSaveWaiting = false;
	ISceneActorInterface::Execute_SetContainer(InActor, TScriptInterface<ISceneContainerInterface>(this));
	return true;
}

bool UVoxelSceneRegion::RemoveSceneActor(AActor* InActor)
{
	if (!InActor)
	{
		return false;
	}
	for (auto Iterator = SceneActorMap.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator.Value() == InActor)
		{
			Iterator.RemoveCurrent();
			MarkSceneChanged();
			return true;
		}
	}
	return false;
}

void UVoxelSceneRegion::DestroySceneActors()
{
	TArray<TObjectPtr<AActor>> Actors;
	SceneActorMap.GenerateValueArray(Actors);
	if (!SceneActorMap.IsEmpty()) MarkSceneChanged();
	SceneActorMap.Reset();
	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor) &&
			!(Cast<APawn>(Actor) && CastChecked<APawn>(Actor)->IsPlayerControlled()))
		{
			Actor->Destroy();
		}
	}
}

void UVoxelSceneRegion::MarkSceneChanged()
{
	check(SceneRevision < MAX_uint64);
	++SceneRevision;
	if (Module) Module->NotifySceneCaptureChanged();
}

const FIntVector& UVoxelSceneRegion::GetRegionKey() const
{
	return RegionKey;
}

const TMap<FGuid, TObjectPtr<AActor>>& UVoxelSceneRegion::GetSceneActors() const
{
	return SceneActorMap;
}

bool UVoxelSceneRegion::HasActiveSections() const
{
	return !ActiveSections.IsEmpty();
}

bool UVoxelSceneRegion::HasStoredActor(const FGuid& InId) const
{
	return SceneActorMap.Contains(InId) || PendingActors.Contains(InId);
}

const TMap<FGuid, FParameter>& UVoxelSceneRegion::GetPendingActors() const
{
	return PendingActors;
}
