// Fill out your copyright notice in the Description page of Project Settings.

#include "Pawn/Base/PawnBase.h"
#include "WHFrameworkCoreMacros.h"
#include "Scene/Actor/SceneActorIdentity.h"
#include "Scene/SceneModule.h"
#include "Ability/AbilityActorTypes.h"
#include "Common/LifecycleTypes.h"
#include "Camera/CameraConfigTypes.h"
#include "Scene/SceneActorSaveTypes.h"

#include "AIController.h"
#include "AI/Base/AIControllerBase.h"
#include "Main/MainModule.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"
#include "Components/BoxComponent.h"
#include "Pawn/PawnModuleNetworkComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Pawn/PawnModuleStatics.h"
#include "Scene/SceneModuleStatics.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Sight.h"
#include "Tasks/AITask_MoveTo.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Scene/VoxelSceneRegion.h"
#include "Voxel/Agent/VoxelAgentComponent.h"

APawnBase::APawnBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(FName("BoxComponent"));
	BoxComponent->SetCollisionProfileName(FName("Vitality"));
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	BoxComponent->CanCharacterStepUpOn = ECB_No;
	SetRootComponent(BoxComponent);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(FName("StimuliSource"));
	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	StimuliSource->RegisterForSense(UAISense_Damage::StaticClass());
	VoxelAgentComponent = CreateDefaultSubobject<UVoxelAgentComponent>(TEXT("VoxelAgentComponent"));

	Name = NAME_None;
	DefaultController = nullptr;

	bInitialized = false;
	bBlockAllInput = false;
	bVisible = true;
	Container = nullptr;
}

void APawnBase::PostActorCreated()
{
	Super::PostActorCreated();
	SceneActorIdentity::InitializeCreated(*this, ActorID);
}

#if WITH_EDITOR
void APawnBase::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	SceneActorIdentity::InitializeEditorDuplicate(*this, ActorID, DuplicateMode);
}

void APawnBase::PostEditImport()
{
	Super::PostEditImport();
	SceneActorIdentity::InitializeEditorImport(*this, ActorID);
}
#endif

bool APawnBase::SetActorID_Implementation(FGuid InID)
{
	if ((Container || bSceneIdentityCommitted) && ActorID != InID) return false;
	return SceneActorIdentity::Assign(*this, ActorID, InID);
}

void APawnBase::OnRep_ActorID()
{
	SceneActorIdentity::QueueReplication(*this, SceneIdentityRetry);
}

void APawnBase::OnSpawn_Implementation(const FParameter& InParam)
{
	if (bSceneRegistrationFailed) return;
	const FAbilityActorSpawnParameter* Param = InParam.GetPtr<FAbilityActorSpawnParameter>();
	if (Param)
	{
		SetAssetID_Implementation(Param->AssetID);
	}
	else if (AssetID.IsValid()) AssetBinding.Bind(this, AssetID);
	const bool bRegistered = ActorID.IsValid() && USceneModuleStatics::AddSceneActor(this, this);
	if (bRegistered) bSceneIdentityCommitted = true;
	if (!bRegistered && HasAuthority() && Execute_IsUseDefaultLifecycle(this))
	{
		bSceneRegistrationFailed = true;
		bInitialized = false;
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		SetActorTickEnabled(false);
		UE_LOG(LogTemp, Error, TEXT("场景实体登记失败，停止初始化：%s id=%s"), *GetNameSafe(this), *ActorID.ToString());
		return;
	}
	if (!bRegistered && !HasAuthority() && ActorID.IsValid()) SceneActorIdentity::QueueReplication(*this, SceneIdentityRetry);

	Execute_SetActorVisible(this, true);
	if (VoxelAgentComponent)
	{
		VoxelAgentComponent->SetAgentEnabled(true);
	}
}

void APawnBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	SceneActorIdentity::CancelReplication(SceneIdentityRetry);
	if (VoxelAgentComponent)
	{
		VoxelAgentComponent->SetAgentEnabled(false);
	}

	Execute_SetActorVisible(this, false);

	SetActorLocationAndRotation(UNDER_Vector, FRotator::ZeroRotator);

	if (USceneModule* Scene = UMainModuleStatics::GetModuleForCleanup<USceneModule>(GetWorld())) Scene->RemoveSceneActor(this);
	if (Container)
	{
		Container->RemoveSceneActor(this);
	}

	Container = nullptr;
	DefaultController = nullptr;
	AssetBinding.Reset();
	AssetID = FPrimaryAssetId();
	bSceneIdentityCommitted = false;
	if (HasAuthority()) { ActorID.Invalidate(); ForceNetUpdate(); }
}

UVoxelAgentComponent* APawnBase::GetVoxelAgentComponent() const
{
	return VoxelAgentComponent;
}

void APawnBase::OnInitialize_Implementation()
{
	if (bSceneRegistrationFailed) return;
	bInitialized = true;
	if (AssetID.IsValid()) AssetBinding.Bind(this, AssetID);

	Execute_SetActorVisible(this, bVisible);
	const bool bRegistered = ActorID.IsValid() && USceneModuleStatics::AddSceneActor(this, this);
	if (bRegistered) bSceneIdentityCommitted = true;
	if (!bRegistered && HasAuthority() && Execute_IsUseDefaultLifecycle(this))
	{
		bSceneRegistrationFailed = true;
		bInitialized = false;
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		SetActorTickEnabled(false);
		UE_LOG(LogTemp, Error, TEXT("场景实体登记失败，停止初始化：%s id=%s"), *GetNameSafe(this), *ActorID.ToString());
		return;
	}
	if (!bRegistered && !HasAuthority() && ActorID.IsValid()) SceneActorIdentity::QueueReplication(*this, SceneIdentityRetry);
}

void APawnBase::OnPreparatory_Implementation()
{
}

void APawnBase::OnRefresh_Implementation(float DeltaSeconds)
{
}

void APawnBase::OnTermination_Implementation()
{
	SceneActorIdentity::CancelReplication(SceneIdentityRetry);
	if (USceneModule* Scene = UMainModuleStatics::GetModuleForCleanup<USceneModule>(GetWorld())) Scene->RemoveSceneActor(this);
	AssetBinding.Reset();
}

void APawnBase::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	if (!InSaveData.GetPtr<FSceneActorSaveData>()) { MarkLoadDataFailed(); return; }
	auto& SaveData = InSaveData.GetRef<FSceneActorSaveData>();

	if (PHASEC(InPhase, EPhase::Primary))
	{
		if (!SetActorID_Implementation(SaveData.ActorID))
		{
			MarkLoadDataFailed();
			UE_LOG(LogTemp, Error, TEXT("无法恢复场景实体身份：%s id=%s"), *GetNameSafe(this), *SaveData.ActorID.ToString());
			return;
		}
		SetActorTransform(SaveData.SpawnTransform);
	}
}

FParameter APawnBase::ToData()
{
	FSceneActorSaveData SaveData;

	SaveData.ActorID = ActorID;
	SaveData.SpawnTransform = GetActorTransform();

	return FParameter(MoveTemp(SaveData));
}

void APawnBase::BeginPlay()
{
	Super::BeginPlay();

	if (Execute_IsUseDefaultLifecycle(this))
	{
		if (!Execute_IsInitialized(this))
		{
			Execute_OnInitialize(this);
		}
		if (!Execute_IsInitialized(this)) return;
		Execute_OnPreparatory(this);
	}
}

void APawnBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SceneActorIdentity::CancelReplication(SceneIdentityRetry);
	if (USceneModule* Scene = UMainModuleStatics::GetModuleForCleanup<USceneModule>(GetWorld())) Scene->RemoveSceneActor(this);
	Super::EndPlay(EndPlayReason);

	if (Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this);
	}
}

void APawnBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

void APawnBase::SpawnDefaultController()
{
	Super::SpawnDefaultController();

	DefaultController = Controller;
}

void APawnBase::OnSwitch_Implementation()
{
}

void APawnBase::OnUnSwitch_Implementation()
{
}

void APawnBase::Switch_Implementation(APlayerController* InPlayer)
{
	UPawnModuleStatics::SwitchPawn(InPlayer, this);
}

void APawnBase::UnSwitch_Implementation()
{
	if (APlayerController* Player = Cast<APlayerController>(GetController()))
	{
		UPawnModuleStatics::SwitchPawn(Player, nullptr);
	}
}

bool APawnBase::IsCurrent_Implementation() const
{
	return Cast<APlayerController>(GetController()) != nullptr;
}

void APawnBase::Turn_Implementation(float InValue)
{
}

void APawnBase::MoveForward_Implementation(float InValue)
{
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation = FRotator(0, Rotation.Yaw, 0);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	AddMovementInput(Direction, InValue);
}

void APawnBase::MoveRight_Implementation(float InValue)
{
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(Direction, InValue);
}

void APawnBase::MoveUp_Implementation(float InValue)
{
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	AddMovementInput(FVector(Direction.X * 0.2f, Direction.Y * 0.2f, 0.5f * InValue), 1.f);
}

void APawnBase::JumpN_Implementation()
{
}

FCameraTargetParams APawnBase::GetCameraTargetParams_Implementation() const
{
	return CameraTargetParams;
}

void APawnBase::SetActorVisible_Implementation(bool bInVisible)
{
	bVisible = bInVisible;
	GetRootComponent()->SetVisibility(bInVisible, true);
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for (auto Iter : AttachedActors)
	{
		if (Iter && Iter->Implements<USceneActorInterface>())
		{
			ISceneActorInterface::Execute_SetActorVisible(Iter, bInVisible);
		}
	}
}

bool APawnBase::IsUseControllerRotation() const
{
	return bUseControllerRotationYaw;
}

void APawnBase::SetUseControllerRotation(bool bValue)
{
	bUseControllerRotationYaw = bValue;
}

void APawnBase::TransformTowards(FTransform InTransform, float InDuration, bool bMulticast)
{
	if (bMulticast)
	{
		if (HasAuthority())
		{
			MultiTransformTowards(InTransform);
		}
		else if (UPawnModuleNetworkComponent* PawnModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UPawnModuleNetworkComponent>(Cast<APlayerController>(GetController())))
		{
			PawnModuleNetworkComponent->ServerTransformTowardsToMulticast(PawnModuleNetworkComponent->MaintainLocalWorldRoute(), this, InTransform);
		}
		return;
	}
	SetActorTransform(InTransform);
}

void APawnBase::MultiTransformTowards_Implementation(FTransform InTransform, float InDuration)
{
	TransformTowards(InTransform, false);
}

void APawnBase::RotationTowards(FRotator InRotation, float InDuration, bool bMulticast)
{
	if (bMulticast)
	{
		if (HasAuthority())
		{
			MultiRotationTowards(InRotation, InDuration);
		}
		else if (UPawnModuleNetworkComponent* PawnModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UPawnModuleNetworkComponent>(Cast<APlayerController>(GetController())))
		{
			PawnModuleNetworkComponent->ServerRotationTowardsMulticast(PawnModuleNetworkComponent->MaintainLocalWorldRoute(), this, InRotation, InDuration);
		}
		return;
	}
	SetActorRotation(InRotation);
}

void APawnBase::MultiRotationTowards_Implementation(FRotator InRotation, float InDuration)
{
	RotationTowards(InRotation, InDuration, false);
}

void APawnBase::AIMoveTo(FVector InLocation, float InStopDistance, bool bMulticast)
{
	if (bMulticast)
	{
		if (HasAuthority())
		{
			MultiAIMoveTo(InLocation, InStopDistance);
		}
		else if (UPawnModuleNetworkComponent* PawnModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UPawnModuleNetworkComponent>(Cast<APlayerController>(GetController())))
		{
			PawnModuleNetworkComponent->ServerAIMoveToMulticast(PawnModuleNetworkComponent->MaintainLocalWorldRoute(), this, InLocation, InStopDistance);
		}
		return;
	}
	if (AAIController* AIController = GetController<AAIController>())
	{
		AIController->MoveToLocation(InLocation, InStopDistance);
	}
}

void APawnBase::MultiAIMoveTo_Implementation(FVector InLocation, float InStopDistance)
{
	AIMoveTo(InLocation, InStopDistance, false);
}

void APawnBase::StopAIMove(bool bMulticast)
{
	if (bMulticast)
	{
		if (HasAuthority())
		{
			MultiStopAIMove();
		}
		else if (UPawnModuleNetworkComponent* PawnModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UPawnModuleNetworkComponent>(Cast<APlayerController>(GetController())))
		{
			PawnModuleNetworkComponent->ServerStopAIMoveMulticast(PawnModuleNetworkComponent->MaintainLocalWorldRoute(), this);
		}
		return;
	}
	if (AAIController* AIController = GetController<AAIController>())
	{
		AIController->StopMovement();
	}
}

void APawnBase::MultiStopAIMove_Implementation()
{
	StopAIMove(false);
}

const UPrimaryAssetBase* APawnBase::GetPawnData() const
{
	return AssetBinding.Get();
}

void APawnBase::SetAssetID_Implementation(const FPrimaryAssetId& InID)
{
	AssetID = InID;
	AssetBinding.Bind(this, AssetID);
}

void APawnBase::OnRep_AssetID()
{
	AssetBinding.Bind(this, AssetID);
}

UBehaviorTree* APawnBase::GetBehaviorTreeAsset() const
{
	return nullptr;
}

AAIControllerBase* APawnBase::GetAIController() const
{
	return GetDefaultController<AAIControllerBase>();
}

FVector APawnBase::GetMoveVelocity(bool bIgnoreZ) const
{
	FVector Velocity = GetMovementComponent()->Velocity;
	if (bIgnoreZ)
		Velocity.Z = 0;
	return Velocity;
}

FVector APawnBase::GetMoveDirection(bool bIgnoreZ) const
{
	return GetMoveVelocity(bIgnoreZ).GetSafeNormal();
}

void APawnBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APawnBase, ActorID);
	DOREPLIFETIME(APawnBase, AssetID);
}
