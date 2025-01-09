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
#include "Ability/Abilities/VitalityActionAbilityBase.h"
#include "Ability/Character/AbilityCharacterInventoryBase.h"
#include "Ability/Character/States/AbilityCharacterState_Climb.h"
#include "Ability/Character/States/AbilityCharacterState_Crouch.h"
#include "Ability/Character/States/AbilityCharacterState_Float.h"
#include "Ability/Character/States/AbilityCharacterState_Fly.h"
#include "Ability/Character/States/AbilityCharacterState_Interrupt.h"
#include "Ability/Character/States/AbilityCharacterState_Swim.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
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
	
	FSM->States.Add(UAbilityCharacterState_Climb::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Crouch::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Death::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Spawn::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Fall::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Float::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Fly::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Interrupt::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Jump::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Static::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Swim::StaticClass());
	FSM->States.Add(UAbilityCharacterState_Walk::StaticClass());

	// stats
	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	RaceID = NAME_None;
	Level = 0;

	BirthTransform = FTransform::Identity;

	MovementRate = 1.f;
	RotationRate = 1.f;

	ActionAbilities = TMap<FGameplayTag, FVitalityActionAbilityData>();
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

	SetMotionRate(1.f, 1.f);
	
	AssetID = FPrimaryAssetId();
	RaceID = NAME_None;
	Level = 0;

	BirthTransform = FTransform::Identity;
	
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

void AAbilityCharacterBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(!AttributeSet || HasAnyFlags(EObjectFlags::RF_ClassDefaultObject)) return;

	if(Ar.ArIsSaveGame)
	{
		if(Ar.IsLoading())
		{
			Ar << Level;
		}
		else if(Ar.IsSaving())
		{
			Ar << Level;
		}
		AttributeSet->SerializeAttributes(Ar);
	}
}

void AAbilityCharacterBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FCharacterSaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		SetActorTransform(SaveData.SpawnTransform);
		if(!SaveData.IsSaved())
		{
			BirthTransform = SaveData.SpawnTransform;

			const UAbilityCharacterDataBase& CharacterData = GetCharacterData<UAbilityCharacterDataBase>();
			
			for(auto Iter : CharacterData.ActionAbilities)
			{
				Iter.AbilityHandle = AbilitySystem->K2_GiveAbility(Iter.AbilityClass, Iter.Level);
				ActionAbilities.Add(Iter.AbilityClass->GetDefaultObject<UAbilityBase>()->AbilityTags.GetByIndex(0), Iter);
			}
		}
		else
		{
			BirthTransform = SaveData.BirthTransform;
			
			ActionAbilities = SaveData.ActionAbilities;
			for(auto& Iter : ActionAbilities)
			{
				Iter.Value.AbilityHandle = AbilitySystem->K2_GiveAbility(Iter.Value.AbilityClass, Iter.Value.Level);
			}
		}
	}
	if(PHASEC(InPhase, EPhase::All))
	{
		SetNameA(SaveData.Name);
		SetLevelA(SaveData.Level);
		SetRaceID(SaveData.RaceID);
	
		Inventory->LoadSaveData(&SaveData.InventoryData, InPhase);
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
	SaveData.BirthTransform = BirthTransform;

	SaveData.ActionAbilities = ActionAbilities;

	return &SaveData;
}

void AAbilityCharacterBase::ResetData()
{
	SetHealth(GetMaxHealth());
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
	FSM->SwitchDefaultState({ InRescuer });
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

void AAbilityCharacterBase::Interrupt(float InDuration /*= -1*/)
{
	FSM->SwitchStateByClass<UAbilityCharacterState_Interrupt>({ InDuration });
}

void AAbilityCharacterBase::UnInterrupt()
{
	if(FSM->IsCurrentStateClass<UAbilityCharacterState_Interrupt>())
	{
		FSM->SwitchState(nullptr);
	}
}

void AAbilityCharacterBase::FreeToAnim()
{
	if(!IsFreeToAnim())
	{
		AbilitySystem->AddLooseGameplayTag(GameplayTags::State_Character_FreeToAnim);
	}
}

void AAbilityCharacterBase::LimitToAnim()
{
	if(IsFreeToAnim())
	{
		AbilitySystem->RemoveLooseGameplayTag(GameplayTags::State_Character_FreeToAnim);
	}
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

void AAbilityCharacterBase::Crouch(bool bClientSimulation)
{
	if(!FSM->IsCurrentStateClass<UAbilityCharacterState_Crouch>())
	{
		FSM->SwitchStateByClass<UAbilityCharacterState_Crouch>({ bClientSimulation });
	}
	else
	{
		Super::Crouch(bClientSimulation);
	}
}

void AAbilityCharacterBase::UnCrouch(bool bClientSimulation)
{
	if(FSM->IsCurrentStateClass<UAbilityCharacterState_Crouch>())
	{
		FSM->SwitchState(nullptr);
	}
	else
	{
		Super::UnCrouch(bClientSimulation);
	}
}

void AAbilityCharacterBase::Swim()
{
	FSM->SwitchStateByClass<UAbilityCharacterState_Swim>();
}

void AAbilityCharacterBase::UnSwim()
{
	if(FSM->IsCurrentStateClass<UAbilityCharacterState_Swim>())
	{
		FSM->SwitchState(nullptr);
	}
}

void AAbilityCharacterBase::Float(float InWaterPosZ)
{
	FSM->SwitchStateByClass<UAbilityCharacterState_Float>({ InWaterPosZ });
}

void AAbilityCharacterBase::UnFloat()
{
	if(FSM->IsCurrentStateClass<UAbilityCharacterState_Float>())
	{
		FSM->SwitchState(nullptr);
	}
}

void AAbilityCharacterBase::Climb()
{
	FSM->SwitchStateByClass<UAbilityCharacterState_Climb>();
}

void AAbilityCharacterBase::UnClimb()
{
	if(FSM->IsCurrentStateClass<UAbilityCharacterState_Climb>())
	{
		FSM->SwitchState(nullptr);
	}
}

void AAbilityCharacterBase::Fly()
{
	FSM->SwitchStateByClass<UAbilityCharacterState_Fly>();
}

void AAbilityCharacterBase::UnFly()
{
	if(FSM->IsCurrentStateClass<UAbilityCharacterState_Fly>())
	{
		FSM->SwitchState(nullptr);
	}
}

bool AAbilityCharacterBase::DoAction(const FGameplayTag& InActionTag)
{
	if(!HasActionAbility(InActionTag)) return false;
	
	const FVitalityActionAbilityData AbilityData = GetActionAbility(InActionTag);
	const bool bSuccess = AbilitySystem->TryActivateAbility(AbilityData.AbilityHandle);
	const FGameplayAbilitySpec Spec = AbilitySystem->FindAbilitySpecForHandle(AbilityData.AbilityHandle);
	return bSuccess;
}

bool AAbilityCharacterBase::StopAction(const FGameplayTag& InActionTag)
{
	if(!HasActionAbility(InActionTag)) return false;
	
	const FVitalityActionAbilityData AbilityData = GetActionAbility(InActionTag);
	const FGameplayAbilitySpec Spec = AbilitySystem->FindAbilitySpecForHandle(AbilityData.AbilityHandle);
	if(UVitalityActionAbilityBase* Ability = Cast<UVitalityActionAbilityBase>(Spec.GetPrimaryInstance()))
	{
		Ability->SetStopped(true);
	}
	AbilitySystem->CancelAbilityHandle(AbilityData.AbilityHandle);
	return true;
}

void AAbilityCharacterBase::EndAction(const FGameplayTag& InActionTag, bool bWasCancelled)
{
	if(InActionTag.MatchesTag(GameplayTags::Ability_Vitality_Action_Death))
	{
		if(FSM->IsCurrentStateClass<UAbilityCharacterState_Death>())
		{
			FSM->GetCurrentState<UAbilityCharacterState_Death>()->DeathEnd();
		}
	}
	else if(InActionTag.MatchesTag(GameplayTags::Ability_Vitality_Action_Static))
	{
		UnStatic();
	}
	else if(InActionTag.MatchesTag(GameplayTags::Ability_Vitality_Action_Interrupt))
	{
		UnInterrupt();
	}
	else if(InActionTag.MatchesTag(GameplayTags::Ability_Character_Action_Jump))
	{
		UnJump();
	}
	else if(InActionTag.MatchesTag(GameplayTags::Ability_Character_Action_Crouch))
	{
		UnCrouch(false);
	}
	else if(InActionTag.MatchesTag(GameplayTags::Ability_Character_Action_Swim))
	{
		UnSwim();
	}
	else if(InActionTag.MatchesTag(GameplayTags::Ability_Character_Action_Float))
	{
		UnFloat();
	}
	else if(InActionTag.MatchesTag(GameplayTags::Ability_Character_Action_Climb))
	{
		UnClimb();
	}
	else if(InActionTag.MatchesTag(GameplayTags::Ability_Character_Action_Fly))
	{
		UnFly();
	}
}

void AAbilityCharacterBase::OnPickUp(AAbilityPickUpBase* InPickUp)
{
	if(InPickUp)
	{
		Inventory->AddItemByRange(InPickUp->GetItem(), -1);
	}
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

void AAbilityCharacterBase::OnPreChangeItem(const FAbilityItem& InOldItem)
{
	
}

void AAbilityCharacterBase::OnChangeItem(const FAbilityItem& InNewItem)
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
	FVector Pos = GetActorLocation() + FMath::RandPointInBox(FBox(FVector(-20.f, -20.f, -10.f), FVector(20.f, 20.f, 10.f)));
	if(!bInPlace) Pos += GetActorForwardVector() * (GetRadius() + 35.f);
	UAbilityModuleStatics::SpawnAbilityPickUp(InItem, Pos, Container.GetInterface());
}

void AAbilityCharacterBase::OnSelectItem(const FAbilityItem& InItem)
{
	if(InItem.InventorySlot->GetSplitType() == ESlotSplitType::Shortcut)
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

bool AAbilityCharacterBase::OnGenerateVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	if(!GenerateVoxelID.IsValid()) return false;

	FItemQueryData ItemQueryData = Inventory->QueryItemByRange(EItemQueryType::Remove, FAbilityItem(GenerateVoxelID, 1), -1);
	if(!ItemQueryData.IsValid()) return false;

	switch(InInteractEvent)
	{
		case EInputInteractEvent::Started:
		{
			return IVoxelAgentInterface::OnGenerateVoxel(InInteractEvent, InHitResult);
		}
		case EInputInteractEvent::Triggered:
		{
			return IVoxelAgentInterface::OnGenerateVoxel(InInteractEvent, InHitResult);
		}
		case EInputInteractEvent::Completed:
		{
			if(IVoxelAgentInterface::OnGenerateVoxel(InInteractEvent, InHitResult))
			{
				Inventory->RemoveItemByQueryData(ItemQueryData);
				return true;
			}
			break;
		}
	}
	return false;
}

bool AAbilityCharacterBase::OnDestroyVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	return IVoxelAgentInterface::OnDestroyVoxel(InInteractEvent, InHitResult);
}

void AAbilityCharacterBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	if(InAttributeChangeData.Attribute == GetExpAttribute())
	{
		if(InAttributeChangeData.NewValue >= GetMaxExp())
		{
			const float Exp = InAttributeChangeData.NewValue - GetMaxExp();
			SetLevelA(GetLevelA() + 1);
			SetExp(Exp);
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

void AAbilityCharacterBase::OnActorAttached(AActor* InActor)
{
	
}

void AAbilityCharacterBase::OnActorDetached(AActor* InActor)
{
	
}

void AAbilityCharacterBase::HandleDamage(const FGameplayAttribute& DamageAttribute, float DamageValue, float DefendValue, bool bHasCrited, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(-DamageValue);

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

	if(DamageValue >= 1.f)
	{
		USceneModuleStatics::SpawnWorldText(FString::FromInt(DamageValue), DamageAttribute != GetMagicDamageAttribute() ? FColor::Red : FColor::Cyan, !bHasCrited ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation(), FVector(20.f));
	}
	if(DefendValue >= 1.f)
	{
		USceneModuleStatics::SpawnWorldText(FString::FromInt(DefendValue), FColor::White, EWorldTextStyle::Normal, GetActorLocation(), FVector(20.f));
	}
}

void AAbilityCharacterBase::HandleRecovery(const FGameplayAttribute& RecoveryAttribute, float RecoveryValue, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	if(RecoveryAttribute == GetHealthRecoveryAttribute())
	{
		ModifyHealth(RecoveryValue);

		if(RecoveryValue > 1.f)
		{
			USceneModuleStatics::SpawnWorldText(FString::FromInt(RecoveryValue), FColor::Green, EWorldTextStyle::Normal, GetActorLocation(), FVector(20.f));
		}
	}
}

void AAbilityCharacterBase::HandleInterrupt(const FGameplayAttribute& InterruptAttribute, float InterruptDuration, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
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

UMeshComponent* AAbilityCharacterBase::GetMeshComponent() const
{
	return GetMesh();
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
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Active) && (!bNeedNotDead || !IsDead());
}

bool AAbilityCharacterBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Dead) || bCheckDying && IsDying();
}

bool AAbilityCharacterBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Dying);
}

bool AAbilityCharacterBase::IsWalking(bool bReally) const
{
	return !bReally ? AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Walking) : GetCharacterMovement()->IsWalking();
}

bool AAbilityCharacterBase::IsInterrupting() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Interrupting);
}

bool AAbilityCharacterBase::IsMoving() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Pawn_Moving);
}

bool AAbilityCharacterBase::IsFreeToAnim() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Character_FreeToAnim);
}

bool AAbilityCharacterBase::IsAnimating() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Character_Animating);
}

bool AAbilityCharacterBase::IsJumping() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Character_Jumping);
}

bool AAbilityCharacterBase::IsFalling(bool bReally) const
{
	return !bReally ? AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Character_Falling) : GetCharacterMovement()->IsFalling();
}

bool AAbilityCharacterBase::IsCrouching(bool bReally) const
{
	return !bReally ? AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Character_Crouching) : GetCharacterMovement()->IsCrouching();
}

bool AAbilityCharacterBase::IsSwimming(bool bReally) const
{
	return !bReally ? AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Character_Swimming) : GetCharacterMovement()->IsSwimming();
}

bool AAbilityCharacterBase::IsFloating() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Character_Floating);
}

bool AAbilityCharacterBase::IsClimbing() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Character_Climbing);
}

bool AAbilityCharacterBase::IsFlying(bool bReally) const
{
	return !bReally ? AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Character_Flying) : GetCharacterMovement()->IsFlying();
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

void AAbilityCharacterBase::SetNameA(FName InName)
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
	InLevel = CharacterData.ClampLevel(InLevel);

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
		Inventory->ResetItems();
		ResetData();
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

float AAbilityCharacterBase::GetDistance(AActor* InTargetActor, bool bIgnoreRadius /*= true*/, bool bIgnoreZAxis /*= true*/) const
{
	if(!InTargetActor) return -1;

	IAbilityActorInterface* TargetAbilityActor = Cast<IAbilityActorInterface>(InTargetActor);

	return FVector::Distance(FVector(GetActorLocation().X, GetActorLocation().Y, bIgnoreZAxis ? 0 : GetActorLocation().Z), FVector(InTargetActor->GetActorLocation().X, InTargetActor->GetActorLocation().Y, bIgnoreZAxis ? 0 : InTargetActor->GetActorLocation().Z)) - (bIgnoreRadius ? 0 : TargetAbilityActor->GetRadius());
}

void AAbilityCharacterBase::GetMotionRate(float& OutMovementRate, float& OutRotationRate)
{
	OutMovementRate = MovementRate;
	OutRotationRate = RotationRate;
}

void AAbilityCharacterBase::SetMotionRate(float InMovementRate, float InRotationRate)
{
	MovementRate = InMovementRate;
	RotationRate = InRotationRate;

	GetCharacterMovement()->MaxWalkSpeed = GetMoveSpeed() * MovementRate;
	GetCharacterMovement()->RotationRate = FRotator(0, GetRotationSpeed() * RotationRate, 0);

	Looking->LookingRotationSpeed = GetRotationSpeed() * RotationRate;
}

bool AAbilityCharacterBase::HasActionAbility(const FGameplayTag& InActionTag) const
{
	return ActionAbilities.Contains(InActionTag);
}

FVitalityActionAbilityData AAbilityCharacterBase::GetActionAbility(const FGameplayTag& InActionTag)
{
	if(HasActionAbility(InActionTag))
	{
		return ActionAbilities[InActionTag];
	}
	return FVitalityActionAbilityData();
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
