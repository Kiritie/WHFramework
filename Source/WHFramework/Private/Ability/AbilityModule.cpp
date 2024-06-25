// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModule.h"

#include "Ability/Actor/AbilityActorDataBase.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
#include "Ability/Item/AbilityItemBase.h"
#include "Ability/Item/Equip/AbilityEquipDataBase.h"
#include "Ability/Item/Prop/AbilityPropDataBase.h"
#include "Ability/Item/Raw/AbilityRawDataBase.h"
#include "Ability/Item/Skill/AbilitySkillDataBase.h"
#include "Ability/Pawn/AbilityPawnBase.h"
#include "Ability/Pawn/AbilityPawnDataBase.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Ability/PickUp/AbilityPickUpEquip.h"
#include "Ability/PickUp/AbilityPickUpProp.h"
#include "Ability/PickUp/AbilityPickUpRaw.h"
#include "Ability/PickUp/AbilityPickUpSkill.h"
#include "Ability/PickUp/AbilityPickUpVoxel.h"
#include "Ability/Item/Equip/AbilityEquipBase.h"
#include "Ability/Item/Prop/AbilityPropBase.h"
#include "Ability/Item/Raw/AbilityRawBase.h"
#include "Ability/Item/Skill/AbilitySkillBase.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "Asset/AssetModuleStatics.h"
#include "Common/CommonStatics.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "Scene/Container/SceneContainerInterface.h"
		
IMPLEMENTATION_MODULE(UAbilityModule)

// Sets default values
UAbilityModule::UAbilityModule()
{
	ModuleName = FName("AbilityModule");
	ModuleDisplayName = FText::FromString(TEXT("Ability Module"));
}

UAbilityModule::~UAbilityModule()
{
	TERMINATION_MODULE(UAbilityModule)
}

#if WITH_EDITOR
void UAbilityModule::OnGenerate()
{
	Super::OnGenerate();
}

void UAbilityModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UAbilityModule)
}
#endif

void UAbilityModule::OnInitialize()
{
	Super::OnInitialize();

	UAssetModuleStatics::AddStaticObject(FName("EAbilityItemType"), FStaticObject(UEnum::StaticClass(), TEXT("/Script/WHFramework.EAbilityItemType")));
}

void UAbilityModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UAbilityModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UAbilityModule::OnPause()
{
	Super::OnPause();
}

void UAbilityModule::OnUnPause()
{
	Super::OnUnPause();
}

void UAbilityModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

AAbilityItemBase* UAbilityModule::SpawnAbilityItem(FAbilityItem InItem, FVector InLocation, FRotator InRotation, ISceneContainerInterface* InContainer)
{
	AAbilityItemBase* Item = nullptr;
	switch (InItem.GetType())
	{
		case EAbilityItemType::Prop:
		{
			Item = UObjectPoolModuleStatics::SpawnObject<AAbilityPropBase>(nullptr, nullptr, false, InItem.GetData<UAbilityPropDataBase>().PropClass);
			break;
		}
		case EAbilityItemType::Equip:
		{
			Item = UObjectPoolModuleStatics::SpawnObject<AAbilityEquipBase>(nullptr, nullptr, false, InItem.GetData<UAbilityEquipDataBase>().EquipClass);
			break;
		}
		case EAbilityItemType::Skill:
		{
			Item = UObjectPoolModuleStatics::SpawnObject<AAbilitySkillBase>(nullptr, nullptr, false, InItem.GetData<UAbilitySkillDataBase>().SkillClass);
			break;
		}
		case EAbilityItemType::Raw:
		{
			Item = UObjectPoolModuleStatics::SpawnObject<AAbilityRawBase>(nullptr, nullptr, false, InItem.GetData<UAbilityRawDataBase>().RawClass);
			break;
		}
		default: break;
	}

	if(Item)
	{
		Item->SetActorLocationAndRotation(InLocation, InRotation);
		if(InContainer)
		{
			InContainer->AddSceneActor(Item);
		}
	}
	return Item;
}

AAbilityItemBase* UAbilityModule::SpawnAbilityItem(FAbilityItem InItem, AActor* InOwnerActor)
{
	AAbilityItemBase* Item = SpawnAbilityItem(InItem);
	if(Item)
	{
		Item->Initialize(InOwnerActor, InItem);
	}
	return Item;
}

AAbilityPickUpBase* UAbilityModule::SpawnAbilityPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer)
{
	if(!InItem.IsValid()) return nullptr;

	FPickUpSaveData SaveData;
	SaveData.Item = InItem;
	SaveData.Location = InLocation;

	return SpawnAbilityPickUp(&SaveData, InContainer);
}

AAbilityPickUpBase* UAbilityModule::SpawnAbilityPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	const auto& SaveData = InSaveData->CastRef<FPickUpSaveData>();
	
	AAbilityPickUpBase* PickUp = nullptr;
	switch (SaveData.Item.GetType())
	{
		case EAbilityItemType::Voxel:
		{
			PickUp = UObjectPoolModuleStatics::SpawnObject<AAbilityPickUpVoxel>();
			break;
		}
		case EAbilityItemType::Prop:
		{
			PickUp = UObjectPoolModuleStatics::SpawnObject<AAbilityPickUpProp>(nullptr, nullptr, false, SaveData.Item.GetData<UAbilityPropDataBase>().PropPickUpClass);
			break;
		}
		case EAbilityItemType::Equip:
		{
			PickUp = UObjectPoolModuleStatics::SpawnObject<AAbilityPickUpEquip>(nullptr, nullptr, false, SaveData.Item.GetData<UAbilityEquipDataBase>().EquipPickUpClass);
			break;
		}
		case EAbilityItemType::Skill:
		{
			PickUp = UObjectPoolModuleStatics::SpawnObject<AAbilityPickUpSkill>(nullptr, nullptr, false, SaveData.Item.GetData<UAbilitySkillDataBase>().SkillPickUpClass);
			break;
		}
		case EAbilityItemType::Raw:
		{
			PickUp = UObjectPoolModuleStatics::SpawnObject<AAbilityPickUpRaw>(nullptr, nullptr, false, SaveData.Item.GetData<UAbilityRawDataBase>().RawPickUpClass);
			break;
		}
		default: break;
	}

	if(PickUp)
	{
		PickUp->LoadSaveData(InSaveData);
		if(InContainer)
		{
			InContainer->AddSceneActor(PickUp);
		}
	}
	return PickUp;
}

AActor* UAbilityModule::SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FActorSaveData>();
	if(AActor* Actor = UObjectPoolModuleStatics::SpawnObject<AActor>(nullptr, { &SaveData.ActorID, &SaveData.AssetID }, false, SaveData.GetItemData<UAbilityActorDataBase>().Class))
	{
		Cast<ISaveDataInterface>(Actor)->LoadSaveData(InSaveData);
		if(InContainer)
		{
			InContainer->AddSceneActor(Actor);
		}
		return Actor;
	}
	return nullptr;
}
