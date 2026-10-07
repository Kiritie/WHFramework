#include "Voxel/Components/VoxelCollisionComponent.h"

#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsInterfaceTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Collision/VoxelPhysicsRetirement.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"

struct FVoxelDetachedPhysicsCook : FVoxelPhysicsRetirementPayload
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ReclaimTaskLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PhysicalLease;
	TStrongObjectPtr<UBodySetup> Setup;
	TStrongObjectPtr<UPhysicalMaterial> Material;
	TStrongObjectPtr<UVoxelCollisionComponent> InstalledComponent;
	FVoxelGenerationConsumerKey Consumer;
	FBodyCollisionData CollisionData;
	FTransform Transform;
	FPhysicsActorHandle Actor = nullptr;
	TAtomic<bool> bCanceled{false};
	TAtomic<bool> bComplete{false};
	TAtomic<bool> bApplied{false};
	TAtomic<bool> bFailed{false};
	TAtomic<bool> bPhysicsCommitted{false};
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
	TAtomic<bool> bReleaseInputs{false};

	~FVoxelDetachedPhysicsCook()
	{
		// 未安装的 Actor 没有 Scene 读者；最后一个强引用只由 Worker 清理任务释放。
		if (Actor) FPhysicsInterface::ReleaseActor(Actor, nullptr);
		if (bReleaseInputs.Load() && Setup) Setup->AggGeom.BoxElems.Empty();
	}
};

namespace
{
	struct FDetachedPhysicsDeleteTask
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> TaskLease;
		FVoxelDetachedPhysicsCook* Work = nullptr;
		void operator()() const { delete Work; }
	};

	struct FDetachedPhysicsDeleter
	{
		void operator()(FVoxelDetachedPhysicsCook* Work) const
		{
			// Scheduler 的 Apply 也可能持有最后一个引用；回收线程不能由引用释放顺序决定。
			if (IsInGameThread() || IsInRenderingThread())
				UE::Tasks::Launch(UE_SOURCE_LOCATION, FDetachedPhysicsDeleteTask{Work->MetadataLease, Work});
			else delete Work;
		}
	};

	uint64 DetachedPhysicsMetadataBytes()
	{
		using FController = SharedPointerInternals::TReferenceControllerWithDeleter<FVoxelDetachedPhysicsCook,
			FDetachedPhysicsDeleter, ESPMode::ThreadSafe>;
		return FMemory::QuantizeSize(sizeof(FVoxelDetachedPhysicsCook), alignof(FVoxelDetachedPhysicsCook)) +
			FMemory::QuantizeSize(sizeof(FController), alignof(FController)) +
			VoxelBoundedAllocation::TaskBytes<FDetachedPhysicsDeleteTask>();
	}

	struct FDetachedPhysicsCookTask
	{
		TSharedPtr<FVoxelDetachedPhysicsCook, ESPMode::ThreadSafe> Work;

		void operator()() const
		{
			if (!Work->bCanceled.Load())
			{
				FActorCreationParams ActorParams;
				ActorParams.InitialTM = Work->Transform;
				ActorParams.bStatic = true;
				// Scene 为空：生成期间不能进入查询、模拟或 Broadphase 集合。
				FPhysicsInterface::CreateActor(ActorParams, Work->Actor);
				if (Work->Actor)
				{
					FGeometryAddParams Geometry;
					Geometry.bDoubleSided = false;
					Geometry.CollisionData = Work->CollisionData;
					Geometry.CollisionTraceType = CTF_UseSimpleAsComplex;
					Geometry.Scale = FVector::OneVector;
					Geometry.SimpleMaterial = Work->Material.Get();
					Geometry.LocalTransform = FTransform::Identity;
					Geometry.WorldTransform = Work->Transform;
					Geometry.Geometry = &Work->Setup->AggGeom;
					FPhysicsInterface::AddGeometry(Work->Actor, Geometry);
					Work->bFailed.Store(FPhysicsInterface::GetNumShapes(Work->Actor) == 0);
				}
				else Work->bFailed.Store(true);
			}
			if (Work->bCanceled.Load())
			{
				if (Work->Actor) FPhysicsInterface::ReleaseActor(Work->Actor, nullptr);
				Work->Setup->AggGeom.BoxElems.Empty();
			}
			Work->bComplete.Store(true);
		}
	};

	struct FDetachedPhysicsReclaimTask
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> TaskLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PhysicalLease;
		TSharedPtr<FVoxelDetachedPhysicsCook, ESPMode::ThreadSafe> Work;
		TArray<FKBoxElem> Boxes;

		void operator()() const
		{
			// 根引用、未安装的 Chaos Actor 和大数组都在 Worker 析构后归还资源容量。
		}
	};
}

UVoxelCollisionComponent::UVoxelCollisionComponent()
{
	SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetCollisionObjectType(ECC_WorldStatic);
	SetCollisionResponseToAllChannels(ECR_Block);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetMobility(EComponentMobility::Movable);
	SetHiddenInGame(true);
}

bool UVoxelCollisionComponent::ShouldCreatePhysicsState() const
{
	return bInstalled && ExpectedBoxes > 0 && Super::ShouldCreatePhysicsState();
}

void UVoxelCollisionComponent::OnCreatePhysicsState()
{
	if (!Cook)
	{
		Super::OnCreatePhysicsState();
		return;
	}
	// Actor/几何已经在 Worker 完成；不再次调用 UPrimitiveComponent 的 InitBody 全量构建。
	check(bInstalled && BodyInstance.IsValidBodyInstance());
	UActorComponent::OnCreatePhysicsState();
}

void UVoxelCollisionComponent::OnDestroyPhysicsState()
{
	// Root 在安装前已持有 Cook/Setup/材质和组件；TermBody 后仍等真正的代理销毁证明。
	if (Cook && bInstalled && NativeTicket)
	{
		Cook->bReleaseInputs.Store(true);
		Super::OnDestroyPhysicsState();
		NativeTicket->bRemovalIssued = true;
		Cook.Reset();
		PreparedPhysicalLease.Reset();
		PreparedReclaimLease.Reset();
		bInstalled = false;
		return;
	}
	Super::OnDestroyPhysicsState();
}

UBodySetup* UVoxelCollisionComponent::GetBodySetup()
{
	return Setup;
}

FPrimitiveSceneProxy* UVoxelCollisionComponent::CreateSceneProxy()
{
	return nullptr;
}

FBoxSphereBounds UVoxelCollisionComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	return FBoxSphereBounds(LocalBounds).TransformBy(InLocalToWorld);
}

uint64 UVoxelCollisionComponent::PreparedReclaimBytes()
{
	return VoxelBoundedAllocation::TaskBytes<FDetachedPhysicsReclaimTask>();
}

bool UVoxelCollisionComponent::BeginPrepare(const int32 InBoxCount, const double InBlockSize,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InPhysicalLease)
{
	check(IsInGameThread());
	if (IsPhysicsStateCreated() || InBoxCount < 0 || InBoxCount > 16384 ||
		!FMath::IsFinite(InBlockSize) || InBlockSize <= 0.0) return false;
	if (InPhysicalLease)
	{
		auto Reclaim = InPhysicalLease->Split(PreparedReclaimBytes());
		if (!Reclaim) return false;
		PreparedReclaimLease = MoveTemp(Reclaim);
		PreparedPhysicalLease = MoveTemp(InPhysicalLease);
	}
	Setup = NewObject<UBodySetup>(this);
	Setup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Setup->bNeverNeedsCookedCollisionData = true;
	Setup->AggGeom.BoxElems.Reserve(InBoxCount);
	BlockSize = InBlockSize;
	ExpectedBoxes = InBoxCount;
	LocalBounds = FBox(ForceInit);
	bPreparing = true;
	bInstalled = false;
	return true;
}

bool UVoxelCollisionComponent::AppendPreparedBoxes(TConstArrayView<FBox> InBoxes)
{
	check(IsInGameThread());
	if (!bPreparing || !Setup || InBoxes.Num() > ExpectedBoxes - Setup->AggGeom.BoxElems.Num()) return false;
	for (const FBox& Box : InBoxes)
	{
		if (!Box.IsValid || Box.Min.ContainsNaN() || Box.Max.ContainsNaN() ||
			Box.Min.X >= Box.Max.X || Box.Min.Y >= Box.Max.Y || Box.Min.Z >= Box.Max.Z) return false;
	}
	for (const FBox& Box : InBoxes)
	{
		FKBoxElem Element;
		Element.Center = Box.GetCenter() * BlockSize;
		const FVector Size = Box.GetSize() * BlockSize;
		Element.X = Size.X;
		Element.Y = Size.Y;
		Element.Z = Size.Z;
		Setup->AggGeom.BoxElems.Add(Element);
		LocalBounds += FBox(Box.Min * BlockSize, Box.Max * BlockSize);
	}
	return true;
}

bool UVoxelCollisionComponent::BeginDetachedCook(
	FVoxelTaskScheduler& InScheduler,
	const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InPhysicalLease,
	const FVoxelTerrainServicePriority& InPriority, const double InFirstQueuedSeconds,
	TSharedPtr<FVoxelPhysicsRetirement, ESPMode::ThreadSafe> InRetirement)
{
	check(IsInGameThread());
	if (!bPreparing || !Setup || Setup->AggGeom.BoxElems.Num() != ExpectedBoxes) return false;
	if (ExpectedBoxes == 0 || Cook) return true;
	if (!InRetirement) return false;
	const auto InResources = InScheduler.GetTerrainResources();
	FVoxelGenerationConsumerKey CookConsumer = InConsumer;
	CookConsumer.OwnerId = CookOwnerId;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata, Reclaim;
	if (!InResources->Acquire(CookConsumer, EVoxelTerrainResourcePool::Input,
		DetachedPhysicsMetadataBytes(), Metadata) ||
		!InResources->Acquire(CookConsumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::TaskBytes<FDetachedPhysicsReclaimTask>(), Reclaim)) return false;
	auto* RawWork = new FVoxelDetachedPhysicsCook();
	RawWork->MetadataLease = MoveTemp(Metadata);
	TSharedPtr<FVoxelDetachedPhysicsCook, ESPMode::ThreadSafe> Work(RawWork, FDetachedPhysicsDeleter());
	Work->ReclaimTaskLease = MoveTemp(Reclaim);
	Work->PhysicalLease = MoveTemp(InPhysicalLease);
	Work->Consumer = CookConsumer;
	Work->Setup.Reset(Setup);
	BodyInstance.OwnerComponent = this;
	BodyInstance.SourceObject = GetOwner();
	BodyInstance.BodySetup = Setup;
	BodyInstance.Scale3D = FVector::OneVector;
	BodyInstance.BuildBodyFilterData(Work->CollisionData.CollisionFilterData);
	FBodyInstance::BuildBodyCollisionFlags(Work->CollisionData.CollisionFlags, ECollisionEnabled::QueryAndPhysics, false);
	Work->Material.Reset(BodyInstance.GetSimplePhysicalMaterial());
	// 材质的懒初始化只在 GT 执行；Worker 只消费冻结的物理材质。
	if (Work->Material) Work->Material->GetPhysicsMaterial();
	Work->Transform = GetComponentTransform();
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildCollision;
	Request.Stamp.WorldEpoch = InConsumer.WorldEpoch;
	Request.Stamp.OwnerId = CookConsumer.OwnerId;
	Request.Stamp.OwnerIncarnation = InConsumer.OwnerIncarnation;
	Request.Stamp.Section = InConsumer.Min;
	Request.Stamp.Token = InConsumer.RequestSerial;
	Request.WorkClass = FVoxelTerrainWorkSelection::IsNear(InPriority.Band) ? EVoxelWorkClass::Critical : EVoxelWorkClass::Interactive;
	Request.ServiceBand = InPriority.Band;
	Request.SourceStableKey = InPriority.SourceStableKey;
	Request.SourcePriority = InPriority.SourcePriority;
	Request.DistanceScore = InPriority.DistanceCells;
	Request.ForwardScore = InPriority.ForwardScore;
	Request.FirstRunnableSeconds = InFirstQueuedSeconds;
	Request.InputBytes = Work->MetadataLease->Bytes() + Work->ReclaimTaskLease->Bytes();
	Request.Execute = [Work](const TAtomic<bool>& InCancel)
	{
		if (InCancel.Load()) Work->bCanceled.Store(true);
		FDetachedPhysicsCookTask{Work}();
		FVoxelTaskResult Result;
		Result.bSuccess = !Work->bFailed.Load() && !Work->bCanceled.Load();
		Result.bCanceled = Work->bCanceled.Load();
		return Result;
	};
	Request.Apply = [Work](FVoxelTaskResult&& Result)
	{
		Work->WorkReceipt = Result.WorkReceipt;
		if (Work->bPhysicsCommitted.Load()) Result.MarkWorkUsed();
		if (Result.bCanceled || !Result.bSuccess)
		{
			Work->bCanceled.Store(true);
			Work->bComplete.Store(true);
		}
		Work->bApplied.Store(true);
	};
	if (!InScheduler.Enqueue(MoveTemp(Request)))
	{
		Work->bComplete.Store(true);
		Cook = MoveTemp(Work);
		// 尚未有 Worker 读取输入；只撤销元数据，让同一已准备组件可在下一帧重试。
		FDetachedPhysicsReclaimTask Retiring{MoveTemp(Cook->ReclaimTaskLease), nullptr, MoveTemp(Cook), {}};
		UE::Tasks::Launch(UE_SOURCE_LOCATION, MoveTemp(Retiring));
		return false;
	}
	Cook = MoveTemp(Work);
	Retirement = MoveTemp(InRetirement);
	return true;
}

bool UVoxelCollisionComponent::IsDetachedCookRunning() const
{
	return Cook && (!Cook->bComplete.Load() || !Cook->bApplied.Load());
}

bool UVoxelCollisionComponent::HasDetachedCookFailed() const
{
	return Cook && Cook->bComplete.Load() && (Cook->bCanceled.Load() || Cook->bFailed.Load());
}

bool UVoxelCollisionComponent::PrepareNativeRetirement(
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> InInputWorkReceipt)
{
	check(IsInGameThread());
	bNativeCapacityBlocked = false;
	if (ExpectedBoxes == 0) return true;
	if (!Cook || !Cook->bComplete.Load() || !Cook->bApplied.Load() || Cook->bFailed.Load() ||
		Cook->bCanceled.Load() || !Cook->Actor || !Retirement) return false;
	if (!Retirement->PrepareTicket(Cook->Actor, Cook->Consumer, Cook, NativeTicket, bNativeCapacityBlocked)) return false;
	NativeTicket->CookWorkReceipt = Cook->WorkReceipt;
	NativeTicket->InputWorkReceipt = MoveTemp(InInputWorkReceipt);
	Cook->InstalledComponent.Reset(this);
	return true;
}

TSharedPtr<FVoxelPhysicsRetirementTicket, ESPMode::ThreadSafe> UVoxelCollisionComponent::CaptureNativeTicket() const
{
	check(IsInGameThread());
	return NativeTicket;
}

bool UVoxelCollisionComponent::InstallPreparedPhysics()
{
	check(IsInGameThread());
	if (!bPreparing || !Setup || Setup->AggGeom.BoxElems.Num() != ExpectedBoxes) return false;
	if (ExpectedBoxes == 0) LocalBounds = FBox(FVector::ZeroVector, FVector::ZeroVector);
	if (ExpectedBoxes != 0)
	{
		if (!Cook || !Cook->bComplete.Load() || Cook->bFailed.Load() || Cook->bCanceled.Load() || !Cook->Actor ||
			!NativeTicket || !NativeTicket->bRegistered || NativeTicket->bNativeRetired.Load()) return false;
		FPhysScene* Scene = GetWorld() ? GetWorld()->GetPhysicsScene() : nullptr;
		if (!Scene) return false;
		BodyInstance.SetPhysicsActor(Cook->Actor);
		Cook->Actor = nullptr;
		BodyInstance.PhysicsUserData = FPhysicsUserData(&BodyInstance);
		FPhysicsInterface::SetActorUserData_AssumesLocked(BodyInstance.GetPhysicsActor(), &BodyInstance.PhysicsUserData);
		TArray<FPhysicsActorHandle> Actors;
		Actors.Reserve(1);
		Actors.Add(BodyInstance.GetPhysicsActor());
		Scene->AddActorsToScene_AssumesLocked(Actors);
		Scene->AddToComponentMaps(this, BodyInstance.GetPhysicsActor());
		NativeTicket->bInstalled = true;
		bInstalled = true;
		CreatePhysicsState();
	}
	else bInstalled = true;
	bPreparing = false;
	UpdateBounds();
	return HasInstalledPhysics();
}

void UVoxelCollisionComponent::ReleasePreparedInputs(
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InPhysicalLease)
{
	check(IsInGameThread());
	if (bInstalled) return;
	if (!Cook)
	{
		// 已安装票据仍持有 Setup；其数组只能在真实 Native 退休后的 Payload 析构中释放。
		if (NativeTicket && NativeTicket->bInstalled) return;
		if (Setup && PreparedReclaimLease)
		{
			FDetachedPhysicsReclaimTask Reclaim;
			Reclaim.TaskLease = MoveTemp(PreparedReclaimLease);
			Reclaim.PhysicalLease = MoveTemp(PreparedPhysicalLease);
			Reclaim.Boxes = MoveTemp(Setup->AggGeom.BoxElems);
			UE::Tasks::Launch(UE_SOURCE_LOCATION, MoveTemp(Reclaim));
		}
		return;
	}
	if (Retirement) Retirement->CancelDetached(NativeTicket);
	Cook->bReleaseInputs.Store(true);
	FDetachedPhysicsReclaimTask Reclaim;
	Reclaim.TaskLease = MoveTemp(Cook->ReclaimTaskLease);
	Reclaim.PhysicalLease = MoveTemp(InPhysicalLease);
	if (!Cook->bComplete.Load()) Cook->bCanceled.Store(true);
	else if (Setup) Reclaim.Boxes = MoveTemp(Setup->AggGeom.BoxElems);
	Reclaim.Work = MoveTemp(Cook);
	NativeTicket.Reset();
	UE::Tasks::Launch(UE_SOURCE_LOCATION, MoveTemp(Reclaim));
	PreparedPhysicalLease.Reset();
	PreparedReclaimLease.Reset();
}

void UVoxelCollisionComponent::OnComponentDestroyed(const bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
	bInstalled = false;
	if (Cook) Cook->bCanceled.Store(true);
	ReleasePreparedInputs(nullptr);
	NativeTicket.Reset();
	Retirement.Reset();
}

bool UVoxelCollisionComponent::HasInstalledPhysics() const
{
	return bInstalled && !bPreparing && Setup && (ExpectedBoxes == 0 ||
		(IsPhysicsStateCreated() && BodyInstance.IsValidBodyInstance()));
}

void UVoxelCollisionComponent::MarkPhysicsCommitted()
{
	check(IsInGameThread());
	if (!Cook || !HasInstalledPhysics() || (NativeTicket && !NativeTicket->bPhysicsCommitted.Load())) return;
	Cook->bPhysicsCommitted.Store(true);
	if (Cook->WorkReceipt) Cook->WorkReceipt->Resolve(true);
}

bool UVoxelCollisionComponent::Apply(const TArray<FBox>& InBoxes, const double InBlockSize)
{
	if (!BeginPrepare(InBoxes.Num(), InBlockSize) || !AppendPreparedBoxes(InBoxes)) return false;
	if (ExpectedBoxes == 0) LocalBounds = FBox(FVector::ZeroVector, FVector::ZeroVector);
	bInstalled = true;
	bPreparing = false;
	RecreatePhysicsState();
	UpdateBounds();
	return HasInstalledPhysics();
}
