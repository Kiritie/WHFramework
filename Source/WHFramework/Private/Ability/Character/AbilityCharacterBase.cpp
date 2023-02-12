// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Character/AbilityCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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
#include "FSM/Components/FSMComponent.h"
#include "Global/GlobalBPLibrary.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Scene/SceneModuleBPLibrary.h"
#include "Ability/Inventory/CharacterInventory.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Ability/PickUp/AbilityPickUpBase.h"

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
	
	//Inventory = CreateDefaultSubobject<UCharacterInventory>(FName("Inventory"));

	Interaction = CreateDefaultSubobject<UCharacterInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetRelativeLocation(FVector(0, 0, 0));

	Interaction->AddInteractionAction(EInteractAction::Revive);

	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->GroupName = FName("Character");
	FSM->DefaultState = UAbilityCharacterState_Default::StaticClass();
	FSM->States.Add(UAbilityCharacterState_Death::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Default::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Fall::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Jump::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Static::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Walk::StaticClass());

	// stats
	Name = NAME_None;
	RaceID = NAME_None;
	Level = 0;

	MovementRate = 1;
	RotationRate = 1;

	AutoPossessAI = EAutoPossessAI::Disabled;
}

// Called when the game starts or when spawned
void AAbilityCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	DefaultGravityScale = GetCharacterMovement()->GravityScale;
	DefaultAirControl = GetCharacterMovement()->AirControl;
}

void AAbilityCharacterBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);

	InitializeAbilitySystem();

	FSM->SwitchDefaultState();
}

void AAbilityCharacterBase::OnDespawn_Implementation()
{
	Super::OnDespawn_Implementation();

	FSM->SwitchState(nullptr);

	SetMotionRate(1, 1);
	
	RaceID = NAME_None;
	Level = 0;
}

void AAbilityCharacterBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FCharacterSaveData>();

	switch(InPhase)
	{
		case EPhase::Primary:
		{
			SetActorLocation(SaveData.SpawnLocation);
			SetActorRotation(SaveData.SpawnRotation);
		}
		case EPhase::Lesser:
		case EPhase::Final:
		{
			SetNameV(SaveData.Name);
			SetRaceID(SaveData.RaceID);
			SetLevelV(SaveData.Level);
		
			Inventory->LoadSaveData(&SaveData.InventoryData, InPhase);

			if(!SaveData.IsSaved())
			{
				ResetData();
			}
			break;
		}
	}
}

FSaveData* AAbilityCharacterBase::ToData()
{
	static FCharacterSaveData SaveData;
	SaveData = FCharacterSaveData();

	SaveData.ID = AssetID;
	SaveData.Name = Name;
	SaveData.RaceID = RaceID;
	SaveData.Level = Level;

	SaveData.InventoryData = Inventory->ToSaveDataRef<FInventorySaveData>();

	SaveData.SpawnLocation = GetActorLocation();
	SaveData.SpawnRotation = GetActorRotation();

	return &SaveData;
}

void AAbilityCharacterBase::ResetData()
{
	SetHealth(GetMaxHealth());
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
			FString("CancelTarget"), FTopLevelAssetPath(NAME_None, FName("EAbilityInputID")), static_cast<int32>(EAbilityInputID::Confirm), static_cast<int32>(EAbilityInputID::Cancel)));

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

	if(IsActive(true))
	{
		RefreshState();
	}
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
	if(Ar.IsLoading())
	{
		RefreshAttributes();
	}
}

void AAbilityCharacterBase::Death(IAbilityVitalityInterface* InKiller)
{
	if(InKiller)
	{
		FSM->GetStateByClass<UAbilityCharacterState_Death>()->Killer = InKiller;
		InKiller->Kill(this);
	}
	FSM->SwitchStateByClass<UAbilityCharacterState_Death>();
}

void AAbilityCharacterBase::Kill(IAbilityVitalityInterface* InTarget)
{
	if(InTarget == this)
	{
		Death(this);
	}
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
	if(InPickUp)
	{
		Inventory->AddItemByRange(InPickUp->GetItem(), -1);
	}
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

void AAbilityCharacterBase::OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess)
{

}

void AAbilityCharacterBase::OnCancelItem(const FAbilityItem& InItem, bool bPassive)
{

}

void AAbilityCharacterBase::OnAssembleItem(const FAbilityItem& InItem)
{

}

void AAbilityCharacterBase::OnDischargeItem(const FAbilityItem& InItem)
{

}

void AAbilityCharacterBase::OnDiscardItem(const FAbilityItem& InItem, bool bInPlace)
{
	FVector tmpPos = GetActorLocation() + FMath::RandPointInBox(FBox(FVector(-20.f, -20.f, -10.f), FVector(20.f, 20.f, 10.f)));
	if(!bInPlace) tmpPos += GetActorForwardVector() * (GetRadius() + 35.f);
	UAbilityModuleBPLibrary::SpawnPickUp(InItem, tmpPos, Container.GetInterface());
}

void AAbilityCharacterBase::OnSelectItem(const FAbilityItem& InItem)
{
	if(InItem.IsValid() && InItem.GetType() == EAbilityItemType::Voxel)
	{
		SetGenerateVoxelID(InItem.ID);
	}
	else
	{
		SetGenerateVoxelID(FPrimaryAssetId());
	}
}

void AAbilityCharacterBase::OnAuxiliaryItem(const FAbilityItem& InItem)
{

}

void AAbilityCharacterBase::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

UAbilitySystemComponent* AAbilityCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

UAttributeSetBase* AAbilityCharacterBase::GetAttributeSet() const
{
	return AttributeSet;
}

UInteractionComponent* AAbilityCharacterBase::GetInteractionComponent() const
{
	return Interaction;
}

UInventory* AAbilityCharacterBase::GetInventory() const
{
	return Inventory;
}

bool AAbilityCharacterBase::IsActive(bool bNeedNotDead) const
{
	return AbilitySystem->HasMatchingGameplayTag(GetCharacterData<UAbilityCharacterDataBase>().ActiveTag) && (!bNeedNotDead || !IsDead());
}

bool AAbilityCharacterBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GetCharacterData<UAbilityCharacterDataBase>().DeadTag) || bCheckDying && IsDying();
}

bool AAbilityCharacterBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GetCharacterData<UAbilityCharacterDataBase>().DyingTag);
}

bool AAbilityCharacterBase::IsFalling(bool bMovementMode) const
{
	return !bMovementMode ? AbilitySystem->HasMatchingGameplayTag(GetCharacterData<UAbilityCharacterDataBase>().FallingTag) : GetCharacterMovement()->IsFalling();
}

bool AAbilityCharacterBase::IsWalking(bool bMovementMode) const
{
	return !bMovementMode ? AbilitySystem->HasMatchingGameplayTag(GetCharacterData<UAbilityCharacterDataBase>().WalkingTag) : GetCharacterMovement()->IsWalking();
}

bool AAbilityCharacterBase::IsJumping() const
{
	return AbilitySystem->HasMatchingGameplayTag(GetCharacterData<UAbilityCharacterDataBase>().JumpingTag);
}

void AAbilityCharacterBase::SetNameV(FName InName)
{
	Name = InName;
}

void AAbilityCharacterBase::SetRaceID(FName InRaceID)
{
	RaceID = InRaceID;
}

bool AAbilityCharacterBase::SetLevelV(int32 InLevel)
{
	const auto& CharacterData = GetCharacterData<UAbilityCharacterDataBase>();
	InLevel = FMath::Clamp(InLevel, 0, CharacterData.MaxLevel != -1 ? CharacterData.MaxLevel : InLevel);

	if(Level != InLevel)
	{
		Level = InLevel;

		auto EffectContext = AbilitySystem->MakeEffectContext();
		EffectContext.AddSourceObject(this);
		auto SpecHandle = AbilitySystem->MakeOutgoingSpec(CharacterData.PEClass, InLevel, EffectContext);
		if (SpecHandle.IsValid())
		{
			AbilitySystem->BP_ApplyGameplayEffectSpecToSelf(SpecHandle);
		}

		return true;
	}
	return false;
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
	GetCharacterMovement()->RotationRate = FRotator(0, GetRotationSpeed() * RotationRate, 0);
}

void AAbilityCharacterBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	const float DeltaValue = InAttributeChangeData.NewValue - InAttributeChangeData.OldValue;
	
	if(InAttributeChangeData.Attribute == AttributeSet->GetExpAttribute())
	{
		if(InAttributeChangeData.NewValue >= AttributeSet->GetMaxExp())
		{
			SetLevelV(GetLevelV() + 1);
			SetExp(0.f);
		}
	}
	else if(InAttributeChangeData.Attribute == AttributeSet->GetHealthAttribute())
	{
		if(DeltaValue > 0.f)
		{
			USceneModuleBPLibrary::SpawnWorldText(FString::FromInt(DeltaValue), FColor::Green, DeltaValue < GetMaxHealth() ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation(), this);
		}
	}
	else if(InAttributeChangeData.Attribute == AttributeSet->GetMoveSpeedAttribute())
	{
		GetCharacterMovement()->MaxWalkSpeed = InAttributeChangeData.NewValue * MovementRate;
	}
	else if(InAttributeChangeData.Attribute == AttributeSet->GetRotationSpeedAttribute())
	{
		GetCharacterMovement()->RotationRate = FRotator(0, InAttributeChangeData.NewValue * RotationRate, 0);
	}
	else if(InAttributeChangeData.Attribute == AttributeSet->GetJumpForceAttribute())
	{
		GetCharacterMovement()->JumpZVelocity = InAttributeChangeData.NewValue;
	}
}

void AAbilityCharacterBase::HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, bool bHasDefend, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(-LocalDamageDone);

	USceneModuleBPLibrary::SpawnWorldText(FString::FromInt(LocalDamageDone), UGlobalBPLibrary::GetPlayerCharacter() == this ? FColor::Red : FColor::White, !bHasCrited ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation(), this);

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
