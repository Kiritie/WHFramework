// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModule.h"

#include "Ability/Actor/AbilityActorDataBase.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
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
	
	DON_WITHINDEX(UCommonStatics::GetEnumItemNum(TEXT("/Script/WHFramework.EAbilityItemType")), i,
		if(i > 0) UAssetModuleStatics::RegisterPrimaryAssetType(*UCommonStatics::GetEnumValueAuthoredName(TEXT("/Script/WHFramework.EAbilityItemType"), i));
	);

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

ECollisionChannel UAbilityModule::GetPickUpTraceChannel() const
{
	return ECC_MAX;
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

AAbilityActorBase* UAbilityModule::SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FActorSaveData>();
	if(AAbilityActorBase* Actor = UObjectPoolModuleStatics::SpawnObject<AAbilityActorBase>(nullptr, { &SaveData.ActorID, &SaveData.AssetID }, false, SaveData.GetItemData<UAbilityActorDataBase>().Class))
	{
		Actor->LoadSaveData(InSaveData);
		if(InContainer)
		{
			InContainer->AddSceneActor(Actor);
		}
		return Actor;
	}
	return nullptr;
}

AAbilityVitalityBase* UAbilityModule::SpawnAbilityVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FVitalitySaveData>();
	if(AAbilityVitalityBase* Vitality = UObjectPoolModuleStatics::SpawnObject<AAbilityVitalityBase>(nullptr, { &SaveData.ActorID, &SaveData.AssetID }, false, SaveData.GetItemData<UAbilityVitalityDataBase>().Class))
	{
		Vitality->LoadSaveData(InSaveData);
		if(InContainer)
		{
			InContainer->AddSceneActor(Vitality);
		}
		return Vitality;
	}
	return nullptr;
}

AAbilityPawnBase* UAbilityModule::SpawnAbilityPawn(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FPawnSaveData>();
	if(AAbilityPawnBase* Pawn = UObjectPoolModuleStatics::SpawnObject<AAbilityPawnBase>(nullptr, { &SaveData.ActorID, &SaveData.AssetID }, false, SaveData.GetItemData<UAbilityPawnDataBase>().Class))
	{
		Pawn->LoadSaveData(InSaveData);
		Pawn->SpawnDefaultController();
		if(InContainer)
		{
			InContainer->AddSceneActor(Pawn);
		}
		return Pawn;
	}
	return nullptr;
}

AAbilityCharacterBase* UAbilityModule::SpawnAbilityCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FCharacterSaveData>();
	if(AAbilityCharacterBase* Character = UObjectPoolModuleStatics::SpawnObject<AAbilityCharacterBase>(nullptr, { &SaveData.ActorID, &SaveData.AssetID }, false, SaveData.GetItemData<UAbilityCharacterDataBase>().Class))
	{
		Character->LoadSaveData(InSaveData);
		Character->SpawnDefaultController();
		if(InContainer)
		{
			InContainer->AddSceneActor(Character);
		}
		return Character;
	}
	return nullptr;
}
