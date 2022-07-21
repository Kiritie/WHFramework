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

// Sets default values
AAbilityModule::AAbilityModule()
{
	ModuleName = FName("AbilityModule");

	InteractActionMap = TMap<EInteractAction, FString>();
	InteractActionMap.Add(EInteractAction::Custom1, TEXT("EVoxelInteractAction"));
	InteractActionMap.Add(EInteractAction::Custom2, TEXT("EVoxelInteractAction"));
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

void AAbilityModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
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

FText AAbilityModule::GetInteractActionDisplayName(int32 InInteractAction)
{
	FString EnumName = TEXT("EInteractAction");
	if(InteractActionMap.Contains((EInteractAction)InInteractAction))
	{
		EnumName = InteractActionMap[(EInteractAction)InInteractAction];
	}
	return UGlobalBPLibrary::GetEnumValueDisplayName(EnumName, InInteractAction);
}

AAbilityPickUpBase* AAbilityModule::SpawnPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer)
{
	if(InItem == FAbilityItem::Empty) return nullptr;

	AAbilityPickUpBase* PickUp = nullptr;

	if(InItem.GetData().EqualType(EAbilityItemType::Voxel))
	{
		PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpVoxel>();
	}
	else if(InItem.GetData().EqualType(EAbilityItemType::Equip))
	{
		PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpEquip>(nullptr, InItem.GetData<UAbilityEquipDataBase>().EquipPickUpClass);
	}
	else if(InItem.GetData().EqualType(EAbilityItemType::Prop))
	{
		PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpProp>(nullptr, InItem.GetData<UAbilityPropDataBase>().PropPickUpClass);
	}
	else if(InItem.GetData().EqualType(EAbilityItemType::Skill))
	{
		PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpSkill>(nullptr, InItem.GetData<UAbilitySkillDataBase>().SkillPickUpClass);
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
	const auto SaveData = InSaveData->CastRef<FPickUpSaveData>();
	return SpawnPickUp(SaveData.Item, SaveData.Location);
}

void AAbilityModule::DestroyPickUp(AAbilityPickUpBase* InPickUp)
{
	if(!InPickUp || !InPickUp->IsValidLowLevel()) return;

	UObjectPoolModuleBPLibrary::DespawnObject(InPickUp);
}

AAbilityCharacterBase* AAbilityModule::SpawnCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	const auto SaveData = InSaveData->CastRef<FCharacterSaveData>();
	if(AAbilityCharacterBase* Character = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityCharacterBase>(nullptr, SaveData.GetCharacterData().Class))
	{
		Character->LoadSaveData(InSaveData, true);
		Character->SpawnDefaultController();
		if(InContainer)
		{
			InContainer->AddSceneActor(Character);
		}
		return Character;
	}
	return nullptr;
}

void AAbilityModule::DestroyCharacter(AAbilityCharacterBase* InCharacter)
{
	if(!InCharacter || !InCharacter->IsValidLowLevel()) return;

	UObjectPoolModuleBPLibrary::DespawnObject(InCharacter);
}

AAbilityVitalityBase* AAbilityModule::SpawnVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	const auto SaveData = InSaveData->CastRef<FVitalitySaveData>();
	if(AAbilityVitalityBase* Vitality = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityVitalityBase>(nullptr, SaveData.GetVitalityData().Class))
	{
		Vitality->LoadSaveData(InSaveData, true);
		if(InContainer)
		{
			InContainer->AddSceneActor(Vitality);
		}
		return Vitality;
	}
	return nullptr;
}

void AAbilityModule::DestroyVitality(AAbilityVitalityBase* InVitality)
{
	if(!InVitality || !InVitality->IsValidLowLevel()) return;

	UObjectPoolModuleBPLibrary::DespawnObject(InVitality);
}
