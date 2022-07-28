// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Character/AbilityCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Attributes/CharacterAttributeSetBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
#include "Ability/Character/States/AbilityCharacterState_Death.h"
#include "Ability/Character/States/AbilityCharacterState_Default.h"
#include "Ability/Character/States/AbilityCharacterState_Fall.h"
#include "Ability/Character/States/AbilityCharacterState_Jump.h"
#include "Ability/Character/States/AbilityCharacterState_Static.h"
#include "Ability/Character/States/AbilityCharacterState_Walk.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Components/CharacterInteractionComponent.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "FSM/Components/FSMComponent.h"
#include "Global/GlobalBPLibrary.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Sight.h"
#include "Scene/SceneModuleBPLibrary.h"

//////////////////////////////////////////////////////////////////////////
// AAbilityCharacterBase

AAbilityCharacterBase::AAbilityCharacterBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));
	// AbilitySystem->SetIsReplicated(true);
	// AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// AttributeSet = CreateDefaultSubobject<UCharacterAttributeSetBase>(FName("AttributeSet"));
	
	Interaction = CreateDefaultSubobject<UCharacterInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetRelativeLocation(FVector(0, 0, 0));
	Interaction->AddInteractionAction(EInteractAction::Revive);

	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->bAutoSwitchDefault = true;
	FSM->GroupName = FName("Character");
	FSM->DefaultState = UAbilityCharacterState_Default::StaticClass();
	FSM->States.Add(UAbilityCharacterState_Death::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Default::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Fall::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Jump::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Static::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Walk::StaticClass());

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Character"));
	GetCapsuleComponent()->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;

	GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -90), FRotator(0, -90, 0));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCastShadow(false);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0, 360, 0);
	GetCharacterMovement()->JumpZVelocity = 420;
	GetCharacterMovement()->AirControl = 0.2f;
	GetCharacterMovement()->bComponentShouldUpdatePhysicsVolume = false;

	// stats
	Name = NAME_None;
	RaceID = NAME_None;
	Level = 0;

	MovementRate = 1;
	RotationRate = 1;

	// local
	DefaultAbility = FAbilityData();

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	AutoPossessAI = EAutoPossessAI::Disabled;
}

// Called when the game starts or when spawned
void AAbilityCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	DefaultGravityScale = GetCharacterMovement()->GravityScale;
	DefaultAirControl = GetCharacterMovement()->AirControl;

	InitializeAbilitySystem();

	for(auto Iter : AttributeSet->GetAllAttributes())
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(Iter).AddUObject(this, &AAbilityCharacterBase::OnAttributeChange);
	}
}

void AAbilityCharacterBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);

	FSM->SwitchDefaultState();
}

void AAbilityCharacterBase::OnDespawn_Implementation()
{
	Super::OnDespawn_Implementation();

	FSM->SwitchState(nullptr);

	SetMotionRate(1, 1);
	
	RaceID = NAME_None;
	Level = 0;
	DefaultAbility = FAbilityData();
}

void AAbilityCharacterBase::LoadData(FSaveData* InSaveData, bool bForceMode)
{
	auto SaveData = InSaveData->CastRef<FCharacterSaveData>();
	
	if(bForceMode)
	{
		AssetID = SaveData.ID;
		SetRaceID(SaveData.RaceID);
		SetLevelV(SaveData.Level);
		SetActorLocation(SaveData.SpawnLocation);
		SetActorRotation(SaveData.SpawnRotation);
	}

	SetNameV(SaveData.Name);
}

FSaveData* AAbilityCharacterBase::ToData()
{
	static FCharacterSaveData SaveData;
	SaveData.Reset();

	SaveData.ID = AssetID;
	SaveData.Name = Name;
	SaveData.RaceID = RaceID;
	SaveData.Level = Level;

	SaveData.SpawnLocation = GetActorLocation();
	SaveData.SpawnRotation = GetActorRotation();

	return &SaveData;
}

void AAbilityCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BindASCInput();
}

void AAbilityCharacterBase::BindASCInput()
{
	if (!bASCInputBound && IsValid(AbilitySystem) && IsValid(InputComponent))
	{
		AbilitySystem->BindAbilityActivationToInputComponent(InputComponent, FGameplayAbilityInputBinds(FString("ConfirmTarget"),
			FString("CancelTarget"), FString("EAbilityInputID"), static_cast<int32>(EAbilityInputID::Confirm), static_cast<int32>(EAbilityInputID::Cancel)));

		bASCInputBound = true;
	}
}

void AAbilityCharacterBase::OnFiniteStateChanged(UFiniteStateBase* InFiniteState)
{
	if(!InFiniteState)
	{
		RefreshState();
	}
}

void AAbilityCharacterBase::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	RefreshState();
}

void AAbilityCharacterBase::RefreshState()
{
	switch (GetCharacterMovement()->MovementMode)
	{
		case EMovementMode::MOVE_Walking:
		{
			FSM->SwitchStateByClass<UAbilityCharacterState_Walk>();
			break;
		}
		case EMovementMode::MOVE_Falling:
		{
			FSM->SwitchStateByClass<UAbilityCharacterState_Fall>();
			break;
		}
		default: break;
	}
}

// Called every frame
void AAbilityCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsDead()) return;

}

void AAbilityCharacterBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(!AttributeSet) return;

	if(Ar.ArIsSaveGame && AttributeSet->GetPersistentAttributes().Num() > 0)
	{
		float BaseValue = 0.f;
		float CurrentValue = 0.f;
		for(FGameplayAttribute& Attribute : AttributeSet->GetPersistentAttributes())
		{
			if(FGameplayAttributeData* AttributeData = Attribute.GetGameplayAttributeData(AttributeSet))
			{
				if(Ar.IsLoading())
				{
					Ar << BaseValue;
					Ar << CurrentValue;
					AttributeData->SetBaseValue(BaseValue);
					AttributeData->SetCurrentValue(CurrentValue);
				}
				else if(Ar.IsSaving())
				{
					BaseValue = AttributeData->GetBaseValue();
					CurrentValue = AttributeData->GetCurrentValue();
					Ar << BaseValue;
					Ar << CurrentValue;
				}
			}
		}
	}
}

void AAbilityCharacterBase::Death(IAbilityVitalityInterface* InKiller)
{
	FSM->GetStateByClass<UAbilityCharacterState_Death>()->Killer = InKiller;
	FSM->SwitchStateByClass<UAbilityCharacterState_Death>();
}

void AAbilityCharacterBase::Revive(IAbilityVitalityInterface* InRescuer)
{
	FSM->SwitchDefaultState();
}

void AAbilityCharacterBase::Jump()
{
	if(!FSM->IsCurrentStateClass<UAbilityCharacterState_Jump>())
	{
		FSM->SwitchStateByClass<UAbilityCharacterState_Jump>();
	}
	else
	{
		Super::Jump();
	}
}

void AAbilityCharacterBase::UnJump()
{
	if(FSM->IsCurrentStateClass<UAbilityCharacterState_Jump>())
	{
		FSM->SwitchState(nullptr);
	}
	else
	{
		Super::StopJumping();
	}
}

void AAbilityCharacterBase::PickUp(AAbilityPickUpBase* InPickUp)
{
}

void AAbilityCharacterBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityCharacterBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

bool AAbilityCharacterBase::CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	return false;
}

void AAbilityCharacterBase::OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
}

FGameplayAbilitySpecHandle AAbilityCharacterBase::AcquireAbility(TSubclassOf<UAbilityBase> InAbility, int32 InLevel /*= 1*/)
{
	if (AbilitySystem && InAbility)
	{
		FGameplayAbilitySpecDef SpecDef = FGameplayAbilitySpecDef();
		SpecDef.Ability = InAbility;
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(SpecDef, InLevel);
		return AbilitySystem->GiveAbility(AbilitySpec);
	}
	return FGameplayAbilitySpecHandle();
}

bool AAbilityCharacterBase::ActiveAbility(FGameplayAbilitySpecHandle SpecHandle, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbility(SpecHandle, bAllowRemoteActivation);
	}
	return false;
}

bool AAbilityCharacterBase::ActiveAbilityByClass(TSubclassOf<UAbilityBase> AbilityClass, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbilityByClass(AbilityClass, bAllowRemoteActivation);
	}
	return false;
}

bool AAbilityCharacterBase::ActiveAbilityByTag(const FGameplayTagContainer& AbilityTags, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbilitiesByTag(AbilityTags, bAllowRemoteActivation);
	}
	return false;
}

void AAbilityCharacterBase::CancelAbility(UAbilityBase* Ability)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbility(Ability);
	}
}

void AAbilityCharacterBase::CancelAbilityByHandle(const FGameplayAbilitySpecHandle& AbilityHandle)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbilityHandle(AbilityHandle);
	}
}

void AAbilityCharacterBase::CancelAbilities(const FGameplayTagContainer& WithTags, const FGameplayTagContainer& WithoutTags, UAbilityBase* Ignore/*=nullptr*/)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbilities(&WithTags, &WithoutTags, Ignore);
	}
}

void AAbilityCharacterBase::CancelAllAbilities(UAbilityBase* Ignore/*=nullptr*/)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAllAbilities(Ignore);
	}
}

FActiveGameplayEffectHandle AAbilityCharacterBase::ApplyEffectByClass(TSubclassOf<UGameplayEffect> EffectClass)
{
	if (AbilitySystem)
	{
		auto effectContext = AbilitySystem->MakeEffectContext();
		effectContext.AddSourceObject(this);
		auto specHandle = AbilitySystem->MakeOutgoingSpec(EffectClass, GetLevelV(), effectContext);
		if (specHandle.IsValid())
		{
			return AbilitySystem->ApplyGameplayEffectSpecToSelf(*specHandle.Data.Get());
		}
	}
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle AAbilityCharacterBase::ApplyEffectBySpecHandle(const FGameplayEffectSpecHandle& SpecHandle)
{
	if (AbilitySystem)
	{
		return AbilitySystem->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle AAbilityCharacterBase::ApplyEffectBySpec(const FGameplayEffectSpec& Spec)
{
	if (AbilitySystem)
	{
		return AbilitySystem->ApplyGameplayEffectSpecToSelf(Spec);
	}
	return FActiveGameplayEffectHandle();
}

bool AAbilityCharacterBase::RemoveEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove/*=-1*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->RemoveActiveGameplayEffect(Handle, StacksToRemove);
	}
	return false;
}

void AAbilityCharacterBase::GetActiveAbilities(FGameplayTagContainer AbilityTags, TArray<UAbilityBase*>& ActiveAbilities)
{
	if (AbilitySystem)
	{
		AbilitySystem->GetActiveAbilitiesWithTags(AbilityTags, ActiveAbilities);
	}
}

bool AAbilityCharacterBase::GetAbilityInfo(TSubclassOf<UAbilityBase> AbilityClass, FAbilityInfo& OutAbilityInfo)
{
	if (AbilitySystem && AbilityClass != nullptr)
	{
		float Cost = 0;
		float Cooldown = 0;
		EAbilityCostType CostType = EAbilityCostType::None;
		UAbilityBase* Ability = AbilityClass.GetDefaultObject();
		if (Ability->GetCostGameplayEffect()->Modifiers.Num() > 0)
		{
			const FGameplayModifierInfo ModifierInfo = Ability->GetCostGameplayEffect()->Modifiers[0];
			ModifierInfo.ModifierMagnitude.GetStaticMagnitudeIfPossible(1, Cost);
			if (ModifierInfo.Attribute == AttributeSet->GetHealthAttribute())
			{
				CostType = EAbilityCostType::Health;
			}
		}
		Ability->GetCooldownGameplayEffect()->DurationMagnitude.GetStaticMagnitudeIfPossible(1, Cooldown);
		OutAbilityInfo = FAbilityInfo(CostType, Cost, Cooldown);
		return true;
	}
	return false;
}

FGameplayAbilitySpec AAbilityCharacterBase::GetAbilitySpecByHandle(FGameplayAbilitySpecHandle Handle)
{
	if (AbilitySystem)
	{
		if(FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Handle))
		{
			return *Spec;
		}
	}
	return FGameplayAbilitySpec();
}

FGameplayAbilitySpec AAbilityCharacterBase::GetAbilitySpecByGEHandle(FActiveGameplayEffectHandle GEHandle)
{
	if (AbilitySystem)
	{
		if(FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromGEHandle(GEHandle))
		{
			return *Spec;
		}
	}
	return FGameplayAbilitySpec();
}

FGameplayAbilitySpec AAbilityCharacterBase::GetAbilitySpecByClass(TSubclassOf<UGameplayAbility> InAbilityClass)
{
	if (AbilitySystem)
	{
		if(FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(InAbilityClass))
		{
			return *Spec;
		}
	}
	return FGameplayAbilitySpec();
}

FGameplayAbilitySpec AAbilityCharacterBase::GetAbilitySpecByInputID(int32 InputID)
{
	if (AbilitySystem)
	{
		if(FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromInputID(InputID))
		{
			return *Spec;
		}
	}
	return FGameplayAbilitySpec();
}

void AAbilityCharacterBase::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

UAbilitySystemComponent* AAbilityCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

UAbilityCharacterDataBase& AAbilityCharacterBase::GetCharacterData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UAbilityCharacterDataBase>(AssetID);
}

FGameplayAttributeData AAbilityCharacterBase::GetAttributeData(FGameplayAttribute InAttribute)
{
	return AttributeSet->GetAttributeData(InAttribute);
}

float AAbilityCharacterBase::GetAttributeValue(FGameplayAttribute InAttribute)
{
	return AttributeSet->GetAttributeValue(InAttribute);
}

void AAbilityCharacterBase::SetAttributeValue(FGameplayAttribute InAttribute, float InValue)
{
	AttributeSet->SetAttributeValue(InAttribute, InValue);
}

TArray<FGameplayAttribute> AAbilityCharacterBase::GetAllAttributes() const
{
	return AttributeSet->GetAllAttributes();
}

TArray<FGameplayAttribute> AAbilityCharacterBase::GetPersistentAttributes() const
{
	return AttributeSet->GetPersistentAttributes();
}

UAttributeSetBase* AAbilityCharacterBase::GetAttributeSet() const
{
	return AttributeSet;
}

UInteractionComponent* AAbilityCharacterBase::GetInteractionComponent() const
{
	return Interaction;
}

bool AAbilityCharacterBase::IsActive(bool bNeedNotDead) const
{
	return AbilitySystem->HasMatchingGameplayTag(GetCharacterData<UAbilityCharacterDataBase>().ActiveTag) && (!bNeedNotDead || !IsDead());
}

bool AAbilityCharacterBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GetCharacterData().DeadTag) || bCheckDying && IsDying();
}

bool AAbilityCharacterBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GetCharacterData().DyingTag);
}

bool AAbilityCharacterBase::IsFalling(bool bMovementMode) const
{
	return !bMovementMode ? AbilitySystem->HasMatchingGameplayTag(GetCharacterData().FallingTag) : GetCharacterMovement()->IsFalling();
}

bool AAbilityCharacterBase::IsWalking(bool bMovementMode) const
{
	return !bMovementMode ? AbilitySystem->HasMatchingGameplayTag(GetCharacterData().WalkingTag) : GetCharacterMovement()->IsWalking();
}

bool AAbilityCharacterBase::IsJumping() const
{
	return AbilitySystem->HasMatchingGameplayTag(GetCharacterData().JumpingTag);
}

void AAbilityCharacterBase::SetNameV(FName InName)
{
	Name = InName;
}

void AAbilityCharacterBase::SetRaceID(FName InRaceID)
{
	RaceID = InRaceID;
}

void AAbilityCharacterBase::SetLevelV(int32 InLevel)
{
	Level = InLevel;
	DefaultAbility.AbilityLevel = InLevel;
}

FString AAbilityCharacterBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name.ToString());
}

FVector AAbilityCharacterBase::GetMoveVelocity(bool bIgnoreZ) const
{
	FVector Velocity = GetMovementComponent()->Velocity;
	if(bIgnoreZ) Velocity.Z = 0;
	return Velocity;
}

FVector AAbilityCharacterBase::GetMoveDirection(bool bIgnoreZ) const
{
	return GetMoveVelocity(bIgnoreZ).GetSafeNormal();
}

float AAbilityCharacterBase::GetRadius() const
{
	if(!GetCapsuleComponent()) return 0.f;

	return GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float AAbilityCharacterBase::GetHalfHeight() const
{
	if(!GetCapsuleComponent()) return 0.f;

	return GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

float AAbilityCharacterBase::GetDistance(AAbilityCharacterBase* InTargetCharacter, bool bIgnoreRadius /*= true*/, bool bIgnoreZAxis /*= true*/)
{
	if(!InTargetCharacter || !InTargetCharacter->IsValidLowLevel()) return -1;
	return FVector::Distance(FVector(GetActorLocation().X, GetActorLocation().Y, bIgnoreZAxis ? 0 : GetActorLocation().Z), FVector(InTargetCharacter->GetActorLocation().X, InTargetCharacter->GetActorLocation().Y, bIgnoreZAxis ? 0 : InTargetCharacter->GetActorLocation().Z)) - (bIgnoreRadius ? 0 : InTargetCharacter->GetRadius());
}

void AAbilityCharacterBase::SetMotionRate_Implementation(float InMovementRate, float InRotationRate)
{
	MovementRate = InMovementRate;
	RotationRate = InRotationRate;
	GetCharacterMovement()->MaxWalkSpeed = GetMoveSpeed() * MovementRate;
	GetCharacterMovement()->RotationRate = FRotator(0, GetRotationSpeed(), 0) * RotationRate;
}

void AAbilityCharacterBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	const float DeltaValue = InAttributeChangeData.NewValue - InAttributeChangeData.OldValue;
	
	if(InAttributeChangeData.Attribute == AttributeSet->GetHealthAttribute())
	{
		if(DeltaValue != 0.f)
		{
			USceneModuleBPLibrary::SpawnWorldText(FString::FromInt(FMath::Abs(DeltaValue)), FColor::Green, DeltaValue < GetMaxHealth() ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation());
		}
	}
	else if(InAttributeChangeData.Attribute == AttributeSet->GetMoveSpeedAttribute())
	{
		GetCharacterMovement()->MaxWalkSpeed = InAttributeChangeData.NewValue * MovementRate;
	}
	else if(InAttributeChangeData.Attribute == AttributeSet->GetRotationSpeedAttribute())
	{
		GetCharacterMovement()->RotationRate = FRotator(0, InAttributeChangeData.NewValue, 0) * RotationRate;
	}
	else if(InAttributeChangeData.Attribute == AttributeSet->GetJumpForceAttribute())
	{
		GetCharacterMovement()->JumpZVelocity = InAttributeChangeData.NewValue;
	}
}

void AAbilityCharacterBase::HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	if (GetHealth() <= 0.f)
	{
		Death(Cast<IAbilityVitalityInterface>(SourceActor));
	}
}

void AAbilityCharacterBase::OnRep_Controller()
{
	Super::OnRep_Controller();
}

void AAbilityCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if(GetPlayerState())
	{
		BindASCInput();
	}
}
