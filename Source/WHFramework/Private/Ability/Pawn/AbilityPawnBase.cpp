// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/AbilityPawnBase.h"

#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Ability/Pawn/AbilityPawnDataBase.h"
#include "Ability/Pawn/States/AbilityPawnState_Death.h"
#include "Ability/Pawn/States/AbilityPawnState_Spawn.h"
#include "Components/BoxComponent.h"
#include "FSM/Components/FSMComponent.h"
#include "Common/CommonStatics.h"
#include "Scene/SceneModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Abilities/VitalityActionAbilityBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Ability/Pawn/AbilityPawnInventoryBase.h"
#include "Ability/Pawn/States/AbilityPawnState_Interrupt.h"
#include "Ability/Pawn/States/AbilityPawnState_Static.h"
#include "Ability/Pawn/States/AbilityPawnState_Walk.h"

AAbilityPawnBase::AAbilityPawnBase(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	AutoPossessAI = EAutoPossessAI::Disabled;

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UVitalityAttributeSetBase>(FName("AttributeSet"));
		
	Inventory = CreateDefaultSubobject<UAbilityPawnInventoryBase>(FName("Inventory"));

	Interaction = CreateDefaultSubobject<UInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetInteractable(false);

	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->GroupName = FName("Vitality");
	
	FSM->DefaultState = UAbilityPawnState_Spawn::StaticClass();
	FSM->FinalState = UAbilityPawnState_Death::StaticClass();
	
	FSM->States.Add(UAbilityPawnState_Death::StaticClass());
	FSM->States.Add(UAbilityPawnState_Interrupt::StaticClass());
	FSM->States.Add(UAbilityPawnState_Spawn::StaticClass());
	FSM->States.Add(UAbilityPawnState_Static::StaticClass());
	FSM->States.Add(UAbilityPawnState_Walk::StaticClass());

	// stats
	RaceID = NAME_None;
	Level = 0;

	BirthTransform = FTransform::Identity;

	MovementRate = 1.f;
	RotationRate = 1.f;

	ActionAbilities = TMap<FGameplayTag, FVitalityActionAbilityData>();
}

void AAbilityPawnBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	InitializeAbilities();

	SpawnDefaultController();

	FSM->SwitchDefaultState();
}

void AAbilityPawnBase::OnDespawn_Implementation(bool bRecovery)
{
	RaceID = NAME_None;
	Level = 0;

	BirthTransform = FTransform::Identity;

	Inventory->UnloadSaveData();
}

void AAbilityPawnBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAbilityPawnBase::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		RefreshAttributes();
	}
}

void AAbilityPawnBase::OnRefresh_Implementation(float DeltaSeconds)
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

void AAbilityPawnBase::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
}

void AAbilityPawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BindASCInput();
}

void AAbilityPawnBase::BindASCInput()
{
	if(!bASCInputBound && IsValid(AbilitySystem) && IsValid(InputComponent))
	{
		AbilitySystem->BindAbilityActivationToInputComponent(InputComponent, FGameplayAbilityInputBinds(FString("ConfirmTarget"),
			FString("CancelTarget"), FTopLevelAssetPath("/Script/WHFramework", FName("EAbilityInputID")), static_cast<int32>(EAbilityInputID::Confirm), static_cast<int32>(EAbilityInputID::Cancel)));
		bASCInputBound = true;
	}
}

void AAbilityPawnBase::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

void AAbilityPawnBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

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

void AAbilityPawnBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FPawnSaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		SetActorTransform(SaveData.SpawnTransform);
		if(!SaveData.IsSaved())
		{
			BirthTransform = SaveData.SpawnTransform;
			
			const UAbilityPawnDataBase& PawnData = GetPawnData<UAbilityPawnDataBase>();
			
			for(auto Iter : PawnData.ActionAbilities)
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

FSaveData* AAbilityPawnBase::ToData()
{
	static FPawnSaveData SaveData;
	SaveData = FPawnSaveData();

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

void AAbilityPawnBase::ResetData()
{
	SetHealth(GetMaxHealth());
}

void AAbilityPawnBase::OnFiniteStateRefresh(UFiniteStateBase* InCurrentState)
{
	FSM->SwitchStateByClass<UAbilityPawnState_Walk>();
}

void AAbilityPawnBase::Death(IAbilityVitalityInterface* InKiller)
{
	FSM->SwitchFinalState({ InKiller });
}

void AAbilityPawnBase::Kill(IAbilityVitalityInterface* InTarget)
{
	InTarget->Death(this);
}

void AAbilityPawnBase::Revive(IAbilityVitalityInterface* InRescuer)
{
	FSM->SwitchDefaultState({ InRescuer });
}

void AAbilityPawnBase::Static()
{
	FSM->SwitchStateByClass<UAbilityPawnState_Static>();
}

void AAbilityPawnBase::UnStatic()
{
	if(FSM->IsCurrentStateClass<UAbilityPawnState_Static>())
	{
		FSM->SwitchState(nullptr);
	}
}

void AAbilityPawnBase::Interrupt(float InDuration /*= -1*/)
{
	FSM->SwitchStateByClass<UAbilityPawnState_Interrupt>({ InDuration });
}

void AAbilityPawnBase::UnInterrupt()
{
	if(FSM->IsCurrentStateClass<UAbilityPawnState_Interrupt>())
	{
		FSM->SwitchState(nullptr);
	}
}

bool AAbilityPawnBase::DoAction(const FGameplayTag& InActionTag)
{
	if(!HasActionAbility(InActionTag)) return false;
	
	const FVitalityActionAbilityData AbilityData = GetActionAbility(InActionTag);
	const bool bSuccess = AbilitySystem->TryActivateAbility(AbilityData.AbilityHandle);
	const FGameplayAbilitySpec Spec = AbilitySystem->FindAbilitySpecForHandle(AbilityData.AbilityHandle);
	return bSuccess;
}

bool AAbilityPawnBase::StopAction(const FGameplayTag& InActionTag)
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

void AAbilityPawnBase::EndAction(const FGameplayTag& InActionTag, bool bWasCancelled)
{
	if(InActionTag.MatchesTag(GameplayTags::Ability_Vitality_Action_Death))
	{
		if(FSM->IsCurrentStateClass<UAbilityPawnState_Death>())
		{
			FSM->GetCurrentState<UAbilityPawnState_Death>()->DeathEnd();
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
}

bool AAbilityPawnBase::CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent)
{
	return false;
}

void AAbilityPawnBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityPawnBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityPawnBase::OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassive)
{
	
}

void AAbilityPawnBase::OnAdditionItem(const FAbilityItem& InItem)
{
	
}

void AAbilityPawnBase::OnRemoveItem(const FAbilityItem& InItem)
{
	
}

void AAbilityPawnBase::OnPreChangeItem(const FAbilityItem& InOldItem)
{
	
}

void AAbilityPawnBase::OnChangeItem(const FAbilityItem& InNewItem)
{
	
}

void AAbilityPawnBase::OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess)
{

}

void AAbilityPawnBase::OnDeactiveItem(const FAbilityItem& InItem, bool bPassive)
{

}

void AAbilityPawnBase::OnDiscardItem(const FAbilityItem& InItem, bool bInPlace)
{
	FVector Pos = GetActorLocation() + FMath::RandPointInBox(FBox(FVector(-20.f, -20.f, -10.f), FVector(20.f, 20.f, 10.f)));
	if(!bInPlace) Pos += GetActorForwardVector() * (GetRadius() + 35.f);
	UAbilityModuleStatics::SpawnAbilityPickUp(InItem, Pos, Container.GetInterface());
}

void AAbilityPawnBase::OnSelectItem(const FAbilityItem& InItem)
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

void AAbilityPawnBase::OnAuxiliaryItem(const FAbilityItem& InItem)
{

}

bool AAbilityPawnBase::OnGenerateVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
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

bool AAbilityPawnBase::OnDestroyVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	return IVoxelAgentInterface::OnDestroyVoxel(InInteractEvent, InHitResult);
}

void AAbilityPawnBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
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
}

void AAbilityPawnBase::OnActorAttached(AActor* InActor)
{
	
}

void AAbilityPawnBase::OnActorDetached(AActor* InActor)
{
	
}

void AAbilityPawnBase::HandleDamage(const FGameplayAttribute& DamageAttribute, float DamageValue, float DefendValue, bool bHasCrited, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(-DamageValue);

	if (GetHealth() <= 0.f)
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

void AAbilityPawnBase::HandleRecovery(const FGameplayAttribute& RecoveryAttribute, float RecoveryValue, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
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

void AAbilityPawnBase::HandleInterrupt(const FGameplayAttribute& InterruptAttribute, float InterruptDuration, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	
}

UAttributeSetBase* AAbilityPawnBase::GetAttributeSet() const
{
	return AttributeSet;
}

UShapeComponent* AAbilityPawnBase::GetCollisionComponent() const
{
	return BoxComponent;
}

UMeshComponent* AAbilityPawnBase::GetMeshComponent() const
{
	return nullptr;
}

UAbilitySystemComponent* AAbilityPawnBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

UInteractionComponent* AAbilityPawnBase::GetInteractionComponent() const
{
	return Interaction;
}

UAbilityInventoryBase* AAbilityPawnBase::GetInventory() const
{
	return Inventory;
}

bool AAbilityPawnBase::IsPlayer() const
{
	return UCommonStatics::GetPlayerPawn() == this;
}

bool AAbilityPawnBase::IsEnemy(IAbilityPawnInterface* InTarget) const
{
	return !InTarget->GetRaceID().IsEqual(RaceID);
}

bool AAbilityPawnBase::IsActive(bool bNeedNotDead) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Active) && (!bNeedNotDead || !IsDead());
}

bool AAbilityPawnBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Dead) || bCheckDying && IsDying();
}

bool AAbilityPawnBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Dying);
}

bool AAbilityPawnBase::IsWalking(bool bReally) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Walking);
}

bool AAbilityPawnBase::IsInterrupting() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Interrupting);
}

bool AAbilityPawnBase::IsMoving() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Pawn_Moving);
}

bool AAbilityPawnBase::IsTargetAble_Implementation(APawn* InPlayerPawn) const
{
	return !IsDead();
}

bool AAbilityPawnBase::SetLevelA(int32 InLevel)
{
	const auto& PawnData = GetPawnData<UAbilityPawnDataBase>();
	InLevel = PawnData.ClampLevel(InLevel);

	if(Level != InLevel)
	{
		Level = InLevel;

		auto EffectContext = AbilitySystem->MakeEffectContext();
		EffectContext.AddSourceObject(this);
		auto SpecHandle = AbilitySystem->MakeOutgoingSpec(PawnData.PEClass, InLevel, EffectContext);
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

FString AAbilityPawnBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name.ToString());
}

float AAbilityPawnBase::GetRadius() const
{
	return FMath::Max(BoxComponent->GetScaledBoxExtent().X, BoxComponent->GetScaledBoxExtent().Y);
}

float AAbilityPawnBase::GetHalfHeight() const
{
	return BoxComponent->GetScaledBoxExtent().Z;
}

float AAbilityPawnBase::GetDistance(AActor* InTargetActor, bool bIgnoreRadius /*= true*/, bool bIgnoreZAxis /*= true*/) const
{
	if(!InTargetActor) return -1;

	IAbilityActorInterface* TargetAbilityActor = Cast<IAbilityActorInterface>(InTargetActor);

	return FVector::Distance(FVector(GetActorLocation().X, GetActorLocation().Y, bIgnoreZAxis ? 0 : GetActorLocation().Z), FVector(InTargetActor->GetActorLocation().X, InTargetActor->GetActorLocation().Y, bIgnoreZAxis ? 0 : InTargetActor->GetActorLocation().Z)) - (bIgnoreRadius ? 0 : TargetAbilityActor->GetRadius());
}

void AAbilityPawnBase::GetMotionRate(float& OutMovementRate, float& OutRotationRate)
{
	OutMovementRate = MovementRate;
	OutRotationRate = RotationRate;
}

void AAbilityPawnBase::SetMotionRate(float InMovementRate, float InRotationRate)
{
	MovementRate = InMovementRate;
	RotationRate = InRotationRate;
}

bool AAbilityPawnBase::HasActionAbility(const FGameplayTag& InActionTag) const
{
	return ActionAbilities.Contains(InActionTag);
}

FVitalityActionAbilityData AAbilityPawnBase::GetActionAbility(const FGameplayTag& InActionTag)
{
	if(HasActionAbility(InActionTag))
	{
		return ActionAbilities[InActionTag];
	}
	return FVitalityActionAbilityData();
}

void AAbilityPawnBase::OnRep_Controller()
{
	Super::OnRep_Controller();
}

void AAbilityPawnBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if(GetPlayerState())
	{
		BindASCInput();
	}
}
