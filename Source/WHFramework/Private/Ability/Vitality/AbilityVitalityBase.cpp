// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/AbilityVitalityBase.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Components/VitalityInteractionComponent.h"
#include "Ability/Abilities/VitalityAbilityBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Death.h"
#include "Ability/Vitality/States/AbilityVitalityState_Default.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Components/BoxComponent.h"
#include "FSM/Components/FSMComponent.h"
#include "Global/GlobalBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Scene/SceneModuleBPLibrary.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"

// Sets default values
AAbilityVitalityBase::AAbilityVitalityBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(FName("BoxComponent"));
	BoxComponent->SetCollisionProfileName(FName("Vitality"));
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	SetRootComponent(BoxComponent);

	// AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));
	// AbilitySystem->SetIsReplicated(true);
	// AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// AttributeSet = CreateDefaultSubobject<UVitalityAttributeSetBase>(FName("AttributeSet"));
	
	Interaction = CreateDefaultSubobject<UVitalityInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetRelativeLocation(FVector(0, 0, 0));

	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->bAutoSwitchDefault = true;
	FSM->GroupName = FName("Vitality");
	FSM->DefaultState = UAbilityVitalityState_Default::StaticClass();
	FSM->States.Add(UAbilityVitalityState_Default::StaticClass());
	FSM->States.Add(UAbilityVitalityState_Death::StaticClass());
	
	// stats
	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	RaceID = NAME_None;
	Level = 0;
	GenerateVoxelID = FPrimaryAssetId();
}

void AAbilityVitalityBase::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();
}

void AAbilityVitalityBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);

	FSM->SwitchDefaultState();
}

void AAbilityVitalityBase::OnDespawn_Implementation()
{
	Super::OnDespawn_Implementation();

	FSM->SwitchState(nullptr);

	RaceID = NAME_None;
	Level = 0;
}

void AAbilityVitalityBase::LoadData(FSaveData* InSaveData, bool bForceMode)
{
	auto SaveData = InSaveData->CastRef<FVitalitySaveData>();
	
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

FSaveData* AAbilityVitalityBase::ToData()
{
	static FVitalitySaveData SaveData;
	SaveData = FVitalitySaveData();

	SaveData.ID = AssetID;
	SaveData.Name = Name;
	SaveData.RaceID = RaceID;
	SaveData.Level = Level;

	SaveData.SpawnLocation = GetActorLocation();
	SaveData.SpawnRotation = GetActorRotation();

	return &SaveData;
}

void AAbilityVitalityBase::OnFiniteStateChanged(UFiniteStateBase* InFiniteState)
{
}

// Called every frame
void AAbilityVitalityBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if(IsDead()) return;
}

void AAbilityVitalityBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(!GetAttributeSet<UVitalityAttributeSetBase>()) return;

	if(Ar.ArIsSaveGame && GetAttributeSet<UVitalityAttributeSetBase>()->GetPersistentAttributes().Num() > 0)
	{
		float BaseValue = 0.f;
		float CurrentValue = 0.f;
		for(FGameplayAttribute& Attribute : GetAttributeSet<UVitalityAttributeSetBase>()->GetPersistentAttributes())
		{
			if(FGameplayAttributeData* AttributeData = Attribute.GetGameplayAttributeData(GetAttributeSet<UVitalityAttributeSetBase>()))
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

void AAbilityVitalityBase::Death(IAbilityVitalityInterface* InKiller)
{
	FSM->GetStateByClass<UAbilityVitalityState_Death>()->Killer = InKiller;
	FSM->SwitchStateByClass<UAbilityVitalityState_Death>();
}

void AAbilityVitalityBase::Revive(IAbilityVitalityInterface* InRescuer)
{
	FSM->SwitchDefaultState();
}

void AAbilityVitalityBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityVitalityBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

bool AAbilityVitalityBase::CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	return false;
}

void AAbilityVitalityBase::OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	IInteractionAgentInterface::OnInteract(InInteractionAgent, InInteractAction);
}

bool AAbilityVitalityBase::GenerateVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::GenerateVoxel(InVoxelHitResult);
}

bool AAbilityVitalityBase::DestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::DestroyVoxel(InVoxelHitResult);
}

bool AAbilityVitalityBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GetVitalityData().DeadTag) || bCheckDying && IsDying();
}

bool AAbilityVitalityBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GetVitalityData().DyingTag);
}

FString AAbilityVitalityBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name.ToString());
}

UAbilityVitalityDataBase& AAbilityVitalityBase::GetVitalityData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UAbilityVitalityDataBase>(AssetID);
}

UAttributeSetBase* AAbilityVitalityBase::GetAttributeSet() const
{
	return AttributeSet;
}

UAbilitySystemComponent* AAbilityVitalityBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

UInteractionComponent* AAbilityVitalityBase::GetInteractionComponent() const
{
	return Interaction;
}

void AAbilityVitalityBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	if(InAttributeChangeData.Attribute == GetAttributeSet<UVitalityAttributeSetBase>()->GetHealthAttribute())
	{
		const float DeltaValue = InAttributeChangeData.NewValue - InAttributeChangeData.OldValue;
		if(DeltaValue != 0.f)
		{
			USceneModuleBPLibrary::SpawnWorldText(FString::FromInt(FMath::Abs(DeltaValue)), FColor::Green, DeltaValue < GetMaxHealth() ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation());
		}
	}
}

void AAbilityVitalityBase::HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	if (GetHealth() <= 0.f)
	{
		Death(Cast<IAbilityVitalityInterface>(SourceActor));
	}
}
