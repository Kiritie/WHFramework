// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Base/CharacterBase.h"
#include "WHFrameworkCoreMacros.h"
#include "Scene/Actor/SceneActorIdentity.h"
#include "Scene/SceneModule.h"
#include "Ability/AbilityActorTypes.h"
#include "Common/LifecycleTypes.h"
#include "Camera/CameraConfigTypes.h"
#include "Scene/SceneActorSaveTypes.h"

#include "AIController.h"
#include "Net/UnrealNetwork.h"
#include "AI/Base/AIControllerBase.h"
#include "Main/MainModule.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/PlayerController.h"
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
#include "Voxel/Scene/VoxelSceneRegion.h"
#include "Voxel/Agent/VoxelAgentComponent.h"

ACharacterBase::ACharacterBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

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
	Looking->OnCanLookAtTarget.BindDynamic(this, &ACharacterBase::CanLookAtTarget);
	Looking->OnTargetLookAtOn.BindDynamic(this, &ACharacterBase::OnTargetLookAtOn);
	Looking->OnTargetLookAtOff.BindDynamic(this, &ACharacterBase::OnTargetLookAtOff);
	VoxelAgentComponent = CreateDefaultSubobject<UVoxelAgentComponent>(TEXT("VoxelAgentComponent"));

	Name = NAME_None;
	Anim = nullptr;
	DefaultController = nullptr;

	bInitialized = false;
	bBlockAllInput = false;
	bVisible = true;
	Container = nullptr;
}

void ACharacterBase::PostActorCreated()
{
	Super::PostActorCreated();
	SceneActorIdentity::InitializeCreated(*this, ActorID);
}

#if WITH_EDITOR
void ACharacterBase::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	SceneActorIdentity::InitializeEditorDuplicate(*this, ActorID, DuplicateMode);
}

void ACharacterBase::PostEditImport()
{
	Super::PostEditImport();
	SceneActorIdentity::InitializeEditorImport(*this, ActorID);
}
#endif

bool ACharacterBase::SetActorID_Implementation(FGuid InID)
{
	if ((Container || bSceneIdentityCommitted) && ActorID != InID) return false;
	return SceneActorIdentity::Assign(*this, ActorID, InID);
}

void ACharacterBase::OnRep_ActorID()
{
	SceneActorIdentity::QueueReplication(*this, SceneIdentityRetry);
}

void ACharacterBase::OnSpawn_Implementation(const FParameter& InParam)
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

void ACharacterBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
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

UVoxelAgentComponent* ACharacterBase::GetVoxelAgentComponent() const
{
	return VoxelAgentComponent;
}

void ACharacterBase::OnInitialize_Implementation()
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

	Anim = Cast<UCharacterAnimBase>(GetMesh()->GetAnimInstance());
}

void ACharacterBase::OnPreparatory_Implementation()
{
}

void ACharacterBase::OnRefresh_Implementation(float DeltaSeconds)
{
}

void ACharacterBase::OnTermination_Implementation()
{
	SceneActorIdentity::CancelReplication(SceneIdentityRetry);
	if (USceneModule* Scene = UMainModuleStatics::GetModuleForCleanup<USceneModule>(GetWorld())) Scene->RemoveSceneActor(this);
	AssetBinding.Reset();
}

void ACharacterBase::LoadData(const FParameter& InSaveData, EPhase InPhase)
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
		SetActorTransform(SaveData.SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

FParameter ACharacterBase::ToData()
{
	FSceneActorSaveData SaveData;

	SaveData.ActorID = ActorID;
	SaveData.SpawnTransform = GetActorTransform();

	return FParameter(MoveTemp(SaveData));
}

void ACharacterBase::BeginPlay()
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

void ACharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SceneActorIdentity::CancelReplication(SceneIdentityRetry);
	if (USceneModule* Scene = UMainModuleStatics::GetModuleForCleanup<USceneModule>(GetWorld())) Scene->RemoveSceneActor(this);
	Super::EndPlay(EndPlayReason);

	if (Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this);
	}
}

void ACharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

void ACharacterBase::SpawnDefaultController()
{
	Super::SpawnDefaultController();

	DefaultController = Controller;
}

void ACharacterBase::OnSwitch_Implementation()
{
}

void ACharacterBase::OnUnSwitch_Implementation()
{
}

void ACharacterBase::Switch_Implementation(APlayerController* InController)
{
    UCharacterModuleStatics::SwitchCharacter(InController, this);
}

void ACharacterBase::UnSwitch_Implementation(APlayerController* InController)
{
    if (IsCurrent(InController)) UCharacterModuleStatics::SwitchCharacter(InController, nullptr);
}

bool ACharacterBase::IsCurrent_Implementation(APlayerController* InController) const
{
    return UCharacterModuleStatics::GetCurrentCharacter(InController) == this;
}

void ACharacterBase::OnRep_Name()
{
	SetNameP(Name);
}

void ACharacterBase::OnRep_AssetID()
{
	AssetBinding.Bind(this, AssetID);
}

void ACharacterBase::SetAssetID_Implementation(const FPrimaryAssetId& InID)
{
	AssetID = InID;
	AssetBinding.Bind(this, AssetID);
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
	if (GetCharacterMovement()->IsMovingOnGround())
		return;

	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	AddMovementInput(FVector(Direction.X * 0.2f, Direction.Y * 0.2f, 0.5f * InValue), 1.f);
}

void ACharacterBase::JumpN_Implementation()
{
	Jump();
}

FCameraTargetParams ACharacterBase::GetCameraTargetParams_Implementation() const
{
	return CameraTargetParams;
}

void ACharacterBase::SetActorVisible_Implementation(bool bInVisible)
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

bool ACharacterBase::IsUseControllerRotation() const
{
	return bUseControllerRotationYaw;
}

void ACharacterBase::SetUseControllerRotation(bool bValue)
{
	bUseControllerRotationYaw = bValue;
	GetCharacterMovement()->bOrientRotationToMovement = !bValue;
}

FSoundPlaybackResult ACharacterBase::PlaySound(USoundBase* InSound, float InVolume, bool bMulticast)
{
	const FSoundPlaybackResult Result = UAudioModuleStatics::PlaySingleSoundAtLocation(this, InSound, GetActorLocation(), InVolume, bMulticast, Cast<APlayerController>(GetController()));
	if (Result.Handle.IsValid() || Result.Handle.IsPending()) SoundHandle = Result.Handle;
	return Result;
}

FNetworkDispatchResult ACharacterBase::StopSound(bool bMulticast)
{
	return UAudioModuleStatics::StopSingleSound(this, SoundHandle, bMulticast, Cast<APlayerController>(GetController()));
}

void ACharacterBase::PlayMontage(UAnimMontage* InMontage, bool bMulticast)
{
    if (bMulticast)
    {
        if (HasAuthority())
        {
            MultiPlayMontage(InMontage);
        }
        return;
    }
    if (Anim && InMontage)
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
	const UCharacterDataBase* CharacterData = GetCharacterData<UCharacterDataBase>();
	if (CharacterData && CharacterData->AnimMontages.Contains(InMontageName))
	{
		PlayMontage(CharacterData->AnimMontages[InMontageName], bMulticast);
	}
}

void ACharacterBase::StopMontage(UAnimMontage* InMontage, bool bMulticast)
{
    if (bMulticast)
    {
        if (HasAuthority())
        {
            MultiStopMontage(InMontage);
        }
        return;
    }
    if (Anim)
    {
        Anim->Montage_Stop(0.2f, InMontage);
    }
}

void ACharacterBase::MultiStopMontage_Implementation(UAnimMontage* InMontage)
{
	StopMontage(InMontage, false);
}

void ACharacterBase::StopMontageByName(const FName InMontageName, bool bMulticast)
{
	const UCharacterDataBase* CharacterData = GetCharacterData<UCharacterDataBase>();
	if (CharacterData && CharacterData->AnimMontages.Contains(InMontageName))
	{
		StopMontage(CharacterData->AnimMontages[InMontageName], bMulticast);
	}
}

void ACharacterBase::TransformTowards(FTransform InTransform, float InDuration, bool bMulticast)
{
    if (InTransform.ContainsNaN() || !InTransform.GetRotation().IsNormalized() ||
        !FMath::IsFinite(InDuration) || InDuration < 0.f)
    {
        return;
    }
    if (bMulticast)
    {
        if (HasAuthority())
        {
            MultiTransformTowards(InTransform, InDuration);
        }
        return;
    }
    SetActorTransform(InTransform);
}

void ACharacterBase::MultiTransformTowards_Implementation(FTransform InTransform, float InDuration)
{
    TransformTowards(InTransform, InDuration, false);
}

void ACharacterBase::RotationTowards(FRotator InRotation, float InDuration, bool bMulticast)
{
    if (InRotation.ContainsNaN() || !FMath::IsFinite(InDuration) || InDuration < 0.f)
    {
        return;
    }
    if (bMulticast)
    {
        if (HasAuthority())
        {
            MultiRotationTowards(InRotation, InDuration);
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
    if (!HasAuthority() || InLocation.ContainsNaN() || !FMath::IsFinite(InStopDistance) || InStopDistance < 0.f)
    {
        return;
    }
    // AI 寻路只在权威端推进；位置由角色现有 Movement 复制。
    if (AAIController* AIController = GetController<AAIController>())
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
    if (!HasAuthority())
    {
        return;
    }
    if (AAIController* AIController = GetController<AAIController>())
    {
        AIController->StopMovement();
    }
}

void ACharacterBase::MultiStopAIMove_Implementation()
{
	StopAIMove(false);
}

void ACharacterBase::OnTargetLookAtOn(AActor* InTargetActor)
{
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

void ACharacterBase::OnTargetLookAtOff(AActor* InTargetActor)
{
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

bool ACharacterBase::IsLookAtAble_Implementation(AActor* InLookerActor) const
{
	return true;
}

bool ACharacterBase::CanLookAtTarget()
{
	return true;
}

const UPrimaryAssetBase* ACharacterBase::GetCharacterData() const
{
	return AssetBinding.Get();
}

UBehaviorTree* ACharacterBase::GetBehaviorTreeAsset() const
{
	return nullptr;
}

AAIControllerBase* ACharacterBase::GetAIController() const
{
	return GetDefaultController<AAIControllerBase>();
}

FVector ACharacterBase::GetMoveVelocity(bool bIgnoreZ) const
{
	FVector Velocity = GetMovementComponent()->Velocity;
	if (bIgnoreZ)
		Velocity.Z = 0;
	return Velocity;
}

FVector ACharacterBase::GetMoveDirection(bool bIgnoreZ) const
{
	return GetMoveVelocity(bIgnoreZ).GetSafeNormal();
}

void ACharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACharacterBase, ActorID);
	DOREPLIFETIME(ACharacterBase, Name);
	DOREPLIFETIME(ACharacterBase, AssetID);
}
