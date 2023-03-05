// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModule.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/Item/Equip/AbilityEquipDataBase.h"
#include "Ability/Item/Prop/AbilityPropDataBase.h"
#include "Ability/Item/Skill/AbilitySkillDataBase.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Ability/PickUp/AbilityPickUpEquip.h"
#include "Ability/PickUp/AbilityPickUpProp.h"
#include "Ability/PickUp/AbilityPickUpSkill.h"
#include "Ability/PickUp/AbilityPickUpVoxel.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "Global/GlobalBPLibrary.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"
#include "Scene/Container/SceneContainerInterface.h"
		
IMPLEMENTATION_MODULE(AAbilityModule)

// Sets default values
AAbilityModule::AAbilityModule()
{
	ModuleName = FName("AbilityModule");

	CustomInteractActionMap = TMap<EInteractAction, FString>();
}

AAbilityModule::~AAbilityModule()
{
	TERMINATION_MODULE(AAbilityModule)
}

#if WITH_EDITOR
void AAbilityModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AAbilityModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AAbilityModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAbilityModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
}

void AAbilityModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAbilityModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AAbilityModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AAbilityModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();
}

void AAbilityModule::AddCustomInteractAction(int32 InInteractAction, const FString& InTypeName)
{
	CustomInteractActionMap.Emplace((EInteractAction)InInteractAction, InTypeName);
}

void AAbilityModule::RemoveCustomInteractAction(int32 InInteractAction)
{
	if(CustomInteractActionMap.Contains((EInteractAction)InInteractAction))
	{
		CustomInteractActionMap.Remove((EInteractAction)InInteractAction);
	}
}

FText AAbilityModule::GetInteractActionDisplayName(int32 InInteractAction)
{
	FString EnumName = TEXT("EInteractAction");
	if(CustomInteractActionMap.Contains((EInteractAction)InInteractAction))
	{
		EnumName = CustomInteractActionMap[(EInteractAction)InInteractAction];
	}
	return UGlobalBPLibrary::GetEnumValueDisplayName(EnumName, InInteractAction);
}

AAbilityPickUpBase* AAbilityModule::SpawnPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer)
{
	if(!InItem.IsValid()) return nullptr;

	AAbilityPickUpBase* PickUp = nullptr;
	switch (InItem.GetType())
	{
		case EAbilityItemType::Voxel:
		{
			PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpVoxel>();
			break;
		}
		case EAbilityItemType::Prop:
		{
			PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpProp>(nullptr, InItem.GetData<UAbilityPropDataBase>().PropPickUpClass);
			break;
		}
		case EAbilityItemType::Equip:
		{
			PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpEquip>(nullptr, InItem.GetData<UAbilityEquipDataBase>().EquipPickUpClass);
			break;
		}
		case EAbilityItemType::Skill:
		{
			PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpSkill>(nullptr, InItem.GetData<UAbilitySkillDataBase>().SkillPickUpClass);
			break;
		}
		default: break;
	}

	if(PickUp)
	{
		PickUp->Initialize(InItem);
		PickUp->SetActorLocationAndRotation(InLocation, FRotator::ZeroRotator);
		if(InContainer)
		{
			InContainer->AddSceneActor(PickUp);
		}
	}
	return PickUp;
}

AAbilityPickUpBase* AAbilityModule::SpawnPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	const auto& SaveData = InSaveData->CastRef<FPickUpSaveData>();
	return SpawnPickUp(SaveData.Item, SaveData.Location);
}

AAbilityCharacterBase* AAbilityModule::SpawnCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FCharacterSaveData>();
	if(AAbilityCharacterBase* Character = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityCharacterBase>({ FParameter::MakePointer(&SaveData.ID) }, SaveData.GetCharacterData().Class))
	{
		Character->LoadSaveData(InSaveData, EPhase::Primary, true);
		Character->SpawnDefaultController();
		if(InContainer)
		{
			InContainer->AddSceneActor(Character);
		}
		return Character;
	}
	return nullptr;
}

AAbilityVitalityBase* AAbilityModule::SpawnVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FVitalitySaveData>();
	if(AAbilityVitalityBase* Vitality = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityVitalityBase>({ FParameter::MakePointer(&SaveData.ID) }, SaveData.GetVitalityData().Class))
	{
		Vitality->LoadSaveData(InSaveData, EPhase::Primary, true);
		if(InContainer)
		{
			InContainer->AddSceneActor(Vitality);
		}
		return Vitality;
	}
	return nullptr;
}
