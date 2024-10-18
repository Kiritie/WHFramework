// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Character/AbilityCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Attributes/CharacterAttributeSetBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
#include "Ability/Character/States/AbilityCharacterState_Death.h"
#include "Ability/Character/States/AbilityCharacterState_Spawn.h"
#include "Ability/Character/States/AbilityCharacterState_Fall.h"
#include "Ability/Character/States/AbilityCharacterState_Jump.h"
#include "Ability/Character/States/AbilityCharacterState_Static.h"
#include "Ability/Character/States/AbilityCharacterState_Walk.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "FSM/Components/FSMComponent.h"
#include "Common/CommonStatics.h"
#include "Scene/SceneModuleStatics.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Character/AbilityCharacterInventoryBase.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Camera/CameraModuleStatics.h"
#include "Common/Looking/LookingComponent.h"
#include "Scene/Actor/PhysicsVolume/PhysicsVolumeBase.h"

//////////////////////////////////////////////////////////////////////////
// AAbilityCharacterBase
AAbilityCharacterBase::AAbilityCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance ifyou don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AutoPossessAI = EAutoPossessAI::Disabled;
	
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UCharacterAttributeSetBase>(FName("AttributeSet"));
	
	Inventory = CreateDefaultSubobject<UAbilityCharacterInventoryBase>(FName("Inventory"));

	Interaction = CreateDefaultSubobject<UInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetInteractable(false);

	Interaction->AddInteractAction(EInteractAction::Revive);

	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->GroupName = FName("Character");
	
	FSM->DefaultState = UAbilityCharacterState_Spawn::StaticClass();
	FSM->FinalState = UAbilityCharacterState_Death::StaticClass();
	
	FSM->States.Add(UAbilityCharacterState_Death::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Spawn::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Fall::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Jump::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Static::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Walk::StaticClass());

	// stats
	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	RaceID = NAME_None;
	Level = 0;

	MovementRate = 1;
	RotationRate = 1;
}

void AAbilityCharacterBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	InitializeAbilities();

	SpawnDefaultController();

	FSM->SwitchDefaultState();
}

void AAbilityCharacterBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	SetMotionRate(1, 1);
	
	RaceID = NAME_None;
	Level = 0;

	Inventory->UnloadSaveData();
}

void AAbilityCharacterBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	DefaultGravityScale = GetCharacterMovement()->GravityScale;
	DefaultAirControl = GetCharacterMovement()->AirControl;
}

void AAbilityCharacterBase::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		RefreshAttributes();
	}
}

void AAbilityCharacterBase::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(IsDead()) return;

	if(IsActive())
	{
		if(GetMoveVelocity(true).Size() > 0.2f)
		{
			if(!IsMoving())
			{
				AbilitySystem->AddLooseGameplayTag(GameplayTags::State_Pawn_Moving);
			}
		}
		else if(IsMoving())
		{
			AbilitySystem->RemoveLooseGameplayTag(GameplayTags::State_Pawn_Moving);
		}
	}
}

void AAbilityCharacterBase::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
}

void AAbilityCharacterBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(!AttributeSet || HasAnyFlags(EObjectFlags::RF_ClassDefaultObject)) return;

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

void AAbilityCharacterBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FCharacterSaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		SetActorTransform(SaveData.SpawnTransform);
	}
	if(PHASEC(InPhase, EPhase::All))
	{
		SetNameV(SaveData.Name);
		SetRaceID(SaveData.RaceID);
		SetLevelA(SaveData.Level);
	
		Inventory->LoadSaveData(&SaveData.InventoryData, InPhase);

		if(!SaveData.IsSaved())
		{
			ResetData();
		}
	}
}

FSaveData* AAbilityCharacterBase::ToData()
{
	static FCharacterSaveData SaveData;
	SaveData = FCharacterSaveData();

	SaveData.ActorID = ActorID;
	SaveData.AssetID = AssetID;
	SaveData.Name = Name;
	SaveData.RaceID = RaceID;
	SaveData.Level = Level;

	SaveData.InventoryData = Inventory->GetSaveDataRef<FInventorySaveData>(true);

	SaveData.SpawnTransform = GetActorTransform();
	
	SaveData.CameraRotation = UCameraModuleStatics::GetCameraRotation();
	SaveData.CameraDistance = UCameraModuleStatics::GetCameraDistance();

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
	if(!bASCInputBound && IsValid(AbilitySystem) && IsValid(InputComponent))
	{
		AbilitySystem->BindAbilityActivationToInputComponent(InputComponent, FGameplayAbilityInputBinds(FString("ConfirmTarget"),
			FString("CancelTarget"), FTopLevelAssetPath("/Script/WHFramework", FName("EAbilityInputID")), static_cast<int32>(EAbilityInputID::Confirm), static_cast<int32>(EAbilityInputID::Cancel)));

		bASCInputBound = true;
	}
}

void AAbilityCharacterBase::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

void AAbilityCharacterBase::OnFiniteStateRefresh(UFiniteStateBase* InCurrentState)
{
	switch (GetCharacterMovement()->MovementMode)
	{
		case MOVE_Walking:
		{
			if(!IsJumping())
			{
				FSM->SwitchStateByClass<UAbilityCharacterState_Walk>();
			}
			break;
		}
		case MOVE_Falling:
		{
			FSM->SwitchStateByClass<UAbilityCharacterState_Fall>();
			break;
		}
		default: break;
	}
}

void AAbilityCharacterBase::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if(!IsActive(true)) return;
	
	FSM->RefreshState();

	switch (GetCharacterMovement()->MovementMode)
	{
		case MOVE_Walking:
		{
			if(GetCharacterMovement()->UpdatedComponent && USceneModuleStatics::GetDefaultPhysicsVolume())
			{
				GetCharacterMovement()->UpdatedComponent->SetPhysicsVolume(USceneModuleStatics::GetDefaultPhysicsVolume(), true);
			}
			break;
		}
		case MOVE_Swimming:
		{
			if(GetCharacterMovement()->UpdatedComponent && USceneModuleStatics::HasPhysicsVolumeByName(FName("Water")))
			{
				GetCharacterMovement()->UpdatedComponent->SetPhysicsVolume(USceneModuleStatics::GetPhysicsVolumeByName(FName("Water")), true);
			}
			break;
		}
		case MOVE_Flying:
		{
			// if(GetCharacterMovement()->UpdatedComponent && USceneModuleStatics::HasPhysicsVolumeByName(FName("Sky")))
			// {
			// 	GetCharacterMovement()->UpdatedComponent->SetPhysicsVolume(USceneModuleStatics::GetPhysicsVolumeByName(FName("Sky")), true);
			// }
			break;
		}
		default: break;
	}
}

void AAbilityCharacterBase::Death(IAbilityVitalityInterface* InKiller)
{
	FSM->SwitchFinalState({ InKiller });
}

void AAbilityCharacterBase::Kill(IAbilityVitalityInterface* InTarget)
{
	InTarget->Death(this);
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

void AAbilityCharacterBase::Static()
{
	FSM->SwitchStateByClass<UAbilityCharacterState_Static>();
}

void AAbilityCharacterBase::UnStatic()
{
	if(FSM->IsCurrentStateClass<UAbilityCharacterState_Static>())
	{
		FSM->SwitchState(nullptr);
	}
}

bool AAbilityCharacterBase::OnPickUp(AAbilityPickUpBase* InPickUp)
{
	if(InPickUp)
	{
		Inventory->AddItemByRange(InPickUp->GetItem(), -1);
		return InPickUp->GetItem().Count <= 0;
	}
	return false;
}

bool AAbilityCharacterBase::CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent)
{
	return false;
}

void AAbilityCharacterBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityCharacterBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityCharacterBase::OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassive)
{
	
}

void AAbilityCharacterBase::OnAdditionItem(const FAbilityItem& InItem)
{
	
}

void AAbilityCharacterBase::OnRemoveItem(const FAbilityItem& InItem)
{
	
}

void AAbilityCharacterBase::OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess)
{

}

void AAbilityCharacterBase::OnDeactiveItem(const FAbilityItem& InItem, bool bPassive)
{

}

void AAbilityCharacterBase::OnDiscardItem(const FAbilityItem& InItem, bool bInPlace)
{
	FVector tmpPos = GetActorLocation() + FMath::RandPointInBox(FBox(FVector(-20.f, -20.f, -10.f), FVector(20.f, 20.f, 10.f)));
	if(!bInPlace) tmpPos += GetActorForwardVector() * (GetRadius() + 35.f);
	UAbilityModuleStatics::SpawnAbilityPickUp(InItem, tmpPos, Container.GetInterface());
}

void AAbilityCharacterBase::OnSelectItem(ESlotSplitType InSplitType, const FAbilityItem& InItem)
{
	if(InSplitType == ESlotSplitType::Shortcut)
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
}

void AAbilityCharacterBase::OnAuxiliaryItem(const FAbilityItem& InItem)
{

}

UAbilitySystemComponent* AAbilityCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

UAttributeSetBase* AAbilityCharacterBase::GetAttributeSet() const
{
	return AttributeSet;
}

UShapeComponent* AAbilityCharacterBase::GetCollisionComponent() const
{
	return GetCapsuleComponent();
}

UInteractionComponent* AAbilityCharacterBase::GetInteractionComponent() const
{
	return Interaction;
}

UAbilityInventoryBase* AAbilityCharacterBase::GetInventory() const
{
	return Inventory;
}

bool AAbilityCharacterBase::IsPlayer() const
{
	return UCommonStatics::GetPlayerPawn() == this;
}

bool AAbilityCharacterBase::IsEnemy(IAbilityPawnInterface* InTarget) const
{
	return !InTarget->GetRaceID().IsEqual(RaceID);
}

bool AAbilityCharacterBase::IsActive(bool bNeedNotDead) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Pawn_Active) && (!bNeedNotDead || !IsDead());
}

bool AAbilityCharacterBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Dead) || bCheckDying && IsDying();
}

bool AAbilityCharacterBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Dying);
}

bool AAbilityCharacterBase::IsMoving() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Pawn_Moving);
}

bool AAbilityCharacterBase::IsFalling(bool bReally) const
{
	return !bReally ? AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Character_Falling) : GetCharacterMovement()->IsFalling();
}

bool AAbilityCharacterBase::IsWalking(bool bReally) const
{
	return !bReally ? AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Character_Walking) : GetCharacterMovement()->IsWalking();
}

bool AAbilityCharacterBase::IsJumping() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Character_Jumping);
}

void AAbilityCharacterBase::SetNameV(FName InName)
{
	Name = InName;
}

void AAbilityCharacterBase::SetRaceID(FName InRaceID)
{
	RaceID = InRaceID;
}

bool AAbilityCharacterBase::SetLevelA(int32 InLevel)
{
	const auto& CharacterData = GetCharacterData<UAbilityCharacterDataBase>();
	InLevel = FMath::Clamp(InLevel, 0, CharacterData.MaxLevel != -1 ? CharacterData.MaxLevel : InLevel);

	if(Level != InLevel)
	{
		Level = InLevel;

		auto EffectContext = AbilitySystem->MakeEffectContext();
		EffectContext.AddSourceObject(this);
		auto SpecHandle = AbilitySystem->MakeOutgoingSpec(CharacterData.PEClass, InLevel, EffectContext);
		if(SpecHandle.IsValid())
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
	Looking->LookingRotationSpeed = GetRotationSpeed() * RotationRate;
}

void AAbilityCharacterBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	const float DeltaValue = InAttributeChangeData.NewValue - InAttributeChangeData.OldValue;
	
	if(InAttributeChangeData.Attribute == GetExpAttribute())
	{
		if(InAttributeChangeData.NewValue >= GetMaxExp())
		{
			SetLevelA(GetLevelA() + 1);
			SetExp(0.f);
		}
	}
	else if(InAttributeChangeData.Attribute == GetMoveSpeedAttribute())
	{
		GetCharacterMovement()->MaxWalkSpeed = InAttributeChangeData.NewValue * MovementRate;
	}
	else if(InAttributeChangeData.Attribute == GetRotationSpeedAttribute())
	{
		GetCharacterMovement()->RotationRate = FRotator(0, InAttributeChangeData.NewValue * RotationRate, 0);
		Looking->LookingRotationSpeed = InAttributeChangeData.NewValue * RotationRate;
	}
	else if(InAttributeChangeData.Attribute == GetJumpForceAttribute())
	{
		GetCharacterMovement()->JumpZVelocity = InAttributeChangeData.NewValue;
	}
}

void AAbilityCharacterBase::HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, bool bHasDefend, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(-LocalDamageDone);

	if(GetHealth() <= 0.f)
	{
		if(IAbilityVitalityInterface* SourceVitality = Cast<IAbilityVitalityInterface>(SourceActor))
		{
			SourceVitality->Kill(this);
		}
		else
		{
			Death(nullptr);
		}
	}

	USceneModuleStatics::SpawnWorldText(FString::FromInt(LocalDamageDone), IsPlayer() ? FColor::Red : FColor::White, !bHasCrited ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation(), FVector(20.f));
}

void AAbilityCharacterBase::HandleRecovery(const float LocalRecoveryDone, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(LocalRecoveryDone);
	
	USceneModuleStatics::SpawnWorldText(FString::FromInt(LocalRecoveryDone), FColor::Green, EWorldTextStyle::Normal, GetActorLocation(), FVector(20.f));
}

void AAbilityCharacterBase::HandleInterrupt(const float InterruptDuration, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
}

bool AAbilityCharacterBase::IsTargetAble_Implementation(APawn* InPlayerPawn) const
{
	return !IsDead();
}

bool AAbilityCharacterBase::IsLookAtAble_Implementation(AActor* InLookerActor) const
{
	return !IsDead();
}

bool AAbilityCharacterBase::CanLookAtTarget()
{
	return Super::CanLookAtTarget() && IsActive(true);
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
