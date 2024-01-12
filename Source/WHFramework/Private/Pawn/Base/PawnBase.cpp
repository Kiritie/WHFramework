// Fill out your copyright notice in the Description page of Project Settings.


#include "Pawn/Base/PawnBase.h"

#include "AIController.h"
#include "Main/MainModule.h"
#include "Animation/AnimInstance.h"
#include "Asset/AssetModuleStatics.h"
#include "Components/BoxComponent.h"
#include "Pawn/PawnModuleNetworkComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Pawn/PawnModuleStatics.h"
#include "Scene/SceneModuleStatics.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Sight.h"
#include "Tasks/AITask_MoveTo.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

APawnBase::APawnBase(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	bReplicates = true;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(FName("BoxComponent"));
	BoxComponent->SetCollisionProfileName(FName("Vitality"));
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	BoxComponent->CanCharacterStepUpOn = ECB_No;
	SetRootComponent(BoxComponent);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(FName("StimuliSource"));
	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	StimuliSource->RegisterForSense(UAISense_Damage::StaticClass());

	Name = NAME_None;
	DefaultController = nullptr;

	ActorID = FGuid::NewGuid();
	bVisible = true;
	Container = nullptr;

	GenerateVoxelID = FPrimaryAssetId();
}

void APawnBase::OnInitialize_Implementation()
{
	Execute_SetActorVisible(this, bVisible);
}

void APawnBase::OnPreparatory_Implementation(EPhase InPhase)
{
	IWHActorInterface::OnPreparatory_Implementation(InPhase);
}

void APawnBase::OnRefresh_Implementation(float DeltaSeconds)
{
	IWHActorInterface::OnRefresh_Implementation(DeltaSeconds);

	if(AMainModule::IsExistModuleByClass<UVoxelModule>())
	{
		if(AVoxelChunk* Chunk = UVoxelModuleStatics::FindChunkByLocation(GetActorLocation()))
		{
			Chunk->AddSceneActor(this);
		}
		else if(Container)
		{
			Container->RemoveSceneActor(this);
		}
	}
}

void APawnBase::OnTermination_Implementation(EPhase InPhase)
{
	IWHActorInterface::OnTermination_Implementation(InPhase);
}

void APawnBase::OnSwitch_Implementation()
{
	
}

void APawnBase::OnUnSwitch_Implementation()
{
	
}

void APawnBase::Switch_Implementation()
{
	UPawnModuleStatics::SwitchPawn(this);
}

void APawnBase::UnSwitch_Implementation()
{
	if(IsCurrent())
	{
		UPawnModuleStatics::SwitchPawn(nullptr);
	}
}

bool APawnBase::IsCurrent_Implementation() const
{
	return UPawnModuleStatics::GetCurrentPawn() == this;
}

void APawnBase::BeginPlay()
{
	Super::BeginPlay();
	
	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnInitialize(this);
		Execute_OnPreparatory(this, EPhase::None);
	}
}

void APawnBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::None);
	}
}

void APawnBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

void APawnBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		ActorID = InParams[0].GetPointerValueRef<FGuid>();
	}
	if(InParams.IsValidIndex(1))
	{
		AssetID = InParams[1].GetPointerValueRef<FPrimaryAssetId>();
	}

	USceneModuleStatics::AddSceneActor(this);

	Execute_SetActorVisible(this, true);
}

void APawnBase::OnDespawn_Implementation(bool bRecovery)
{
	Execute_SetActorVisible(this, false);

	SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);

	USceneModuleStatics::RemoveSceneActor(this);
	if(Container)
	{
		Container->RemoveSceneActor(this);
	}

	Container = nullptr;
	DefaultController = nullptr;

	SetGenerateVoxelID(FPrimaryAssetId());
}

void APawnBase::SpawnDefaultController()
{
	Super::SpawnDefaultController();

	DefaultController = Controller;
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

FVector APawnBase::GetCameraOffset_Implementation() const
{
	return FVector::ZeroVector;
}

float APawnBase::GetCameraDistance_Implementation() const
{
	return -1.f;
}

float APawnBase::GetCameraMinPitch_Implementation() const
{
	return -1.f;
}

float APawnBase::GetCameraMaxPitch_Implementation() const
{
	return -1.f;
}

ECameraTrackMode APawnBase::GetCameraTrackMode_Implementation() const
{
	return ECameraTrackMode::LocationAndRotationOnceAndDistanceOnce;
}

ECameraSmoothMode APawnBase::GetCameraSmoothMode_Implementation() const
{
	return ECameraSmoothMode::All;
}

ECameraControlMode APawnBase::GetCameraControlMode_Implementation() const
{
	return ECameraControlMode::All;
}

void APawnBase::SetActorVisible_Implementation(bool bInVisible)
{
	bVisible = bInVisible;
	GetRootComponent()->SetVisibility(bInVisible, true);
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for(auto Iter : AttachedActors)
	{
		if(Iter && Iter->Implements<USceneActorInterface>())
		{
			ISceneActorInterface::Execute_SetActorVisible(Iter, bInVisible);
		}
	}
}

bool APawnBase::OnGenerateVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::OnGenerateVoxel(InVoxelHitResult);
}

bool APawnBase::OnDestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::OnDestroyVoxel(InVoxelHitResult);
}

void APawnBase::TransformTowards(FTransform InTransform, float InDuration, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiTransformTowards(InTransform);
		}
		else if(UPawnModuleNetworkComponent* PawnModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UPawnModuleNetworkComponent>())
		{
			PawnModuleNetworkComponent->ServerTransformTowardsToMulticast(this, InTransform);
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
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiRotationTowards(InRotation, InDuration);
		}
		else if(UPawnModuleNetworkComponent* PawnModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UPawnModuleNetworkComponent>())
		{
			PawnModuleNetworkComponent->ServerRotationTowardsMulticast(this, InRotation, InDuration);
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
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiAIMoveTo(InLocation, InStopDistance);
		}
		else if(UPawnModuleNetworkComponent* PawnModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UPawnModuleNetworkComponent>())
		{
			PawnModuleNetworkComponent->ServerAIMoveToMulticast(this, InLocation, InStopDistance);
		}
		return;
	}
	if(AAIController* AIController = GetController<AAIController>())
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
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiStopAIMove();
		}
		else if(UPawnModuleNetworkComponent* PawnModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UPawnModuleNetworkComponent>())
		{
			PawnModuleNetworkComponent->ServerStopAIMoveMulticast(this);
		}
		return;
	}
	if(AAIController* AIController = GetController<AAIController>())
	{
		AIController->StopMovement();
	}
}

void APawnBase::MultiStopAIMove_Implementation()
{
	StopAIMove(false);
}

UPrimaryAssetBase& APawnBase::GetPawnData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UPrimaryAssetBase>(AssetID);
}

UBehaviorTree* APawnBase::GetBehaviorTreeAsset() const
{
	return nullptr;
}

FVector APawnBase::GetMoveVelocity(bool bIgnoreZ) const
{
	FVector Velocity = GetMovementComponent()->Velocity;
	if(bIgnoreZ) Velocity.Z = 0;
	return Velocity;
}

FVector APawnBase::GetMoveDirection(bool bIgnoreZ) const
{
	return GetMoveVelocity(bIgnoreZ).GetSafeNormal();
}

void APawnBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
