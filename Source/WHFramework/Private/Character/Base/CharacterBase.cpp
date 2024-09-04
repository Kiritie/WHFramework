// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Base/CharacterBase.h"

#include "AIController.h"
#include "Main/MainModule.h"
#include "Animation/AnimInstance.h"
#include "Asset/AssetModuleStatics.h"
#include "Audio/AudioModuleStatics.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Character/CharacterModuleStatics.h"
#include "Character/Base/CharacterAnimBase.h"
#include "Character/Base/CharacterDataBase.h"
#include "Common/Looking/LookingComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Scene/SceneModuleStatics.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Sight.h"
#include "Tasks/AITask_MoveTo.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

ACharacterBase::ACharacterBase(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	bReplicates = true;

	GetCapsuleComponent()->InitCapsuleSize(30.f, 89.f);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Character"));
	GetCapsuleComponent()->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;

	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCastShadow(false);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);
	GetCharacterMovement()->JumpZVelocity = 420.f;
	GetCharacterMovement()->AirControl = 0.2f;
	GetCharacterMovement()->bComponentShouldUpdatePhysicsVolume = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(FName("StimuliSource"));
	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	StimuliSource->RegisterForSense(UAISense_Damage::StaticClass());

	Looking = CreateDefaultSubobject<ULookingComponent>(FName("Looking"));
	Looking->LookingMaxDistance = 1500.f;
	Looking->OnCanLockAtTarget.BindDynamic(this, &ACharacterBase::CanLookAtTarget);

	Name = NAME_None;
	Anim = nullptr;
	DefaultController = nullptr;

	ActorID = FGuid::NewGuid();
	bVisible = true;
	Container = nullptr;

	GenerateVoxelID = FPrimaryAssetId();
}

void ACharacterBase::OnInitialize_Implementation()
{
	Execute_SetActorVisible(this, bVisible);

	Anim = Cast<UCharacterAnimBase>(GetMesh()->GetAnimInstance());
}

void ACharacterBase::OnPreparatory_Implementation(EPhase InPhase)
{
	IWHActorInterface::OnPreparatory_Implementation(InPhase);
}

void ACharacterBase::OnRefresh_Implementation(float DeltaSeconds)
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

void ACharacterBase::OnTermination_Implementation(EPhase InPhase)
{
	IWHActorInterface::OnTermination_Implementation(InPhase);
}

void ACharacterBase::OnSwitch_Implementation()
{
	
}

void ACharacterBase::OnUnSwitch_Implementation()
{
	
}

void ACharacterBase::Switch_Implementation()
{
	UCharacterModuleStatics::SwitchCharacter(this);
}

void ACharacterBase::UnSwitch_Implementation()
{
	if(IsCurrent())
	{
		UCharacterModuleStatics::SwitchCharacter(nullptr);
	}
}

bool ACharacterBase::IsCurrent_Implementation() const
{
	return UCharacterModuleStatics::GetCurrentCharacter() == this;
}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnInitialize(this);
		Execute_OnPreparatory(this, EPhase::All);
	}
}

void ACharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::All);
	}
}

void ACharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

void ACharacterBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
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

void ACharacterBase::OnDespawn_Implementation(bool bRecovery)
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

void ACharacterBase::SpawnDefaultController()
{
	Super::SpawnDefaultController();

	DefaultController = Controller;
}

void ACharacterBase::Turn_Implementation(float InValue)
{
}

void ACharacterBase::MoveForward_Implementation(float InValue)
{
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation = FRotator(0, Rotation.Yaw, 0);
	const FVector Direction = FRotationMatrix(GetCharacterMovement()->IsMovingOnGround() ? YawRotation : Rotation).GetUnitAxis(EAxis::X);
	AddMovementInput(Direction, InValue);
}

void ACharacterBase::MoveRight_Implementation(float InValue)
{
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(Direction, InValue);
}

void ACharacterBase::MoveUp_Implementation(float InValue)
{
	if(GetCharacterMovement()->IsMovingOnGround()) return;

	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	AddMovementInput(FVector(Direction.X * 0.2f, Direction.Y * 0.2f, 0.5f * InValue), 1.f);
}

void ACharacterBase::JumpN_Implementation()
{
	Jump();
}

FVector ACharacterBase::GetCameraOffset_Implementation() const
{
	return FVector::ZeroVector;
}

float ACharacterBase::GetCameraDistance_Implementation() const
{
	return -1.f;
}

float ACharacterBase::GetCameraMinPitch_Implementation() const
{
	return -1.f;
}

float ACharacterBase::GetCameraMaxPitch_Implementation() const
{
	return -1.f;
}

ECameraTrackMode ACharacterBase::GetCameraTrackMode_Implementation() const
{
	return ECameraTrackMode::LocationAndRotationOnceAndDistanceOnce;
}

ECameraSmoothMode ACharacterBase::GetCameraSmoothMode_Implementation() const
{
	return ECameraSmoothMode::All;
}

ECameraControlMode ACharacterBase::GetCameraControlMode_Implementation() const
{
	return ECameraControlMode::All;
}

void ACharacterBase::SetActorVisible_Implementation(bool bInVisible)
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

bool ACharacterBase::OnGenerateVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::OnGenerateVoxel(InVoxelHitResult);
}

bool ACharacterBase::OnDestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::OnDestroyVoxel(InVoxelHitResult);
}

void ACharacterBase::PlaySound(USoundBase* InSound, float InVolume, bool bMulticast)
{
	SoundHandle = UAudioModuleStatics::PlaySingleSoundAtLocation(InSound, GetActorLocation(), InVolume, bMulticast);
}

void ACharacterBase::StopSound(bool bMulticast)
{
	UAudioModuleStatics::StopSingleSound(SoundHandle, bMulticast);
}

void ACharacterBase::PlayMontage(UAnimMontage* InMontage, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiPlayMontage(InMontage);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerPlayMontageMulticast(this, InMontage);
		}
		return;
	}
	if(Anim)
	{
		Anim->Montage_Play(InMontage);
	}
}

void ACharacterBase::MultiPlayMontage_Implementation(UAnimMontage* InMontage)
{
	PlayMontage(InMontage, false);
}

void ACharacterBase::PlayMontageByName(const FName InMontageName, bool bMulticast)
{
	auto& CharacterData = GetCharacterData<UCharacterDataBase>();
	if(CharacterData.AnimMontages.Contains(InMontageName))
	{
		PlayMontage(CharacterData.AnimMontages[InMontageName], bMulticast);
	}
}

void ACharacterBase::StopMontage(UAnimMontage* InMontage, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiStopMontage(InMontage);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerStopMontageMulticast(this, InMontage);
		}
		return;
	}
	if(Anim)
	{
		Anim->Montage_Stop(.2f, InMontage);
	}
}

void ACharacterBase::MultiStopMontage_Implementation(UAnimMontage* InMontage)
{
	StopMontage(InMontage, false);
}

void ACharacterBase::StopMontageByName(const FName InMontageName, bool bMulticast)
{
	auto& CharacterData = GetCharacterData<UCharacterDataBase>();
	if(CharacterData.AnimMontages.Contains(InMontageName))
	{
		StopMontage(CharacterData.AnimMontages[InMontageName], bMulticast);
	}
}

void ACharacterBase::TransformTowards(FTransform InTransform, float InDuration, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiTransformTowards(InTransform);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerTransformTowardsToMulticast(this, InTransform);
		}
		return;
	}
	SetActorTransform(InTransform);
}

void ACharacterBase::MultiTransformTowards_Implementation(FTransform InTransform, float InDuration)
{
	TransformTowards(InTransform, false);
}

void ACharacterBase::RotationTowards(FRotator InRotation, float InDuration, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiRotationTowards(InRotation, InDuration);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerRotationTowardsMulticast(this, InRotation, InDuration);
		}
		return;
	}
	SetActorRotation(InRotation);
}

void ACharacterBase::MultiRotationTowards_Implementation(FRotator InRotation, float InDuration)
{
	RotationTowards(InRotation, InDuration, false);
}

void ACharacterBase::AIMoveTo(FVector InLocation, float InStopDistance, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiAIMoveTo(InLocation, InStopDistance);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerAIMoveToMulticast(this, InLocation, InStopDistance);
		}
		return;
	}
	if(AAIController* AIController = GetController<AAIController>())
	{
		AIController->MoveToLocation(InLocation, InStopDistance);
	}
}

void ACharacterBase::MultiAIMoveTo_Implementation(FVector InLocation, float InStopDistance)
{
	AIMoveTo(InLocation, InStopDistance, false);
}

void ACharacterBase::StopAIMove(bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiStopAIMove();
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerStopAIMoveMulticast(this);
		}
		return;
	}
	if(AAIController* AIController = GetController<AAIController>())
	{
		AIController->StopMovement();
	}
}

void ACharacterBase::MultiStopAIMove_Implementation()
{
	StopAIMove(false);
}

bool ACharacterBase::IsLookAtAble_Implementation(AActor* InLookerActor) const
{
	return true;
}

bool ACharacterBase::CanLookAtTarget()
{
	return true;
}

UPrimaryAssetBase& ACharacterBase::GetCharacterData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UPrimaryAssetBase>(AssetID);
}

UBehaviorTree* ACharacterBase::GetBehaviorTreeAsset() const
{
	return nullptr;
}

FVector ACharacterBase::GetMoveVelocity(bool bIgnoreZ) const
{
	FVector Velocity = GetMovementComponent()->Velocity;
	if(bIgnoreZ) Velocity.Z = 0;
	return Velocity;
}

FVector ACharacterBase::GetMoveDirection(bool bIgnoreZ) const
{
	return GetMoveVelocity(bIgnoreZ).GetSafeNormal();
}

void ACharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
