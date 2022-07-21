// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModuleBPLibrary.h"

#include "Ability/AbilityModule.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Global/GlobalBPLibrary.h"

FPrimaryAssetType UAbilityModuleBPLibrary::GetAssetTypeByItemType(EAbilityItemType InItemType)
{
	return *UGlobalBPLibrary::GetEnumValueAuthoredName(TEXT("EAbilityItemType"), (int32)InItemType);
}

FText UAbilityModuleBPLibrary::GetInteractActionDisplayName(int32 InInteractAction)
{
	if(AAbilityModule* AbilityModule = AMainModule::GetModuleByClass<AAbilityModule>())
	{
		return AbilityModule->GetInteractActionDisplayName(InInteractAction);
	}
	return FText::GetEmpty();
}

FVitalityRaceData UAbilityModuleBPLibrary::RandomVitalityRaceData()
{
	TArray<FVitalityRaceData> raceDatas;
	if(UAssetModuleBPLibrary::ReadDataTable<FVitalityRaceData>(raceDatas))
	{
		float tmpNum1 = 0;
		float tmpNum2 = 0;
		for (int32 i = 0; i < raceDatas.Num(); i++)
		{
			tmpNum1 += raceDatas[i].Proportion;
		}
		tmpNum1 = FMath::FRandRange(0, tmpNum1);
		for (int32 i = 0; i < raceDatas.Num(); i++)
		{
			tmpNum2 += raceDatas[i].Proportion;
			if (tmpNum1 <= tmpNum2)
			{
				return raceDatas[i];
			}
		}
	}
	return FVitalityRaceData();
}

FCharacterRaceData UAbilityModuleBPLibrary::RandomCharacterRaceData()
{
	TArray<FCharacterRaceData> raceDatas;
	if(UAssetModuleBPLibrary::ReadDataTable<FCharacterRaceData>(raceDatas))
	{
		float tmpNum1 = 0;
		float tmpNum2 = 0;
		for (int32 i = 0; i < raceDatas.Num(); i++)
		{
			tmpNum1 += raceDatas[i].Proportion;
		}
		tmpNum1 = FMath::FRandRange(0, tmpNum1);
		for (int32 i = 0; i < raceDatas.Num(); i++)
		{
			tmpNum2 += raceDatas[i].Proportion;
			if (tmpNum1 <= tmpNum2)
			{
				return raceDatas[i];
			}
		}
	}
	return FCharacterRaceData();
}

AAbilityPickUpBase* UAbilityModuleBPLibrary::SpawnPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AMainModule::GetModuleByClass<AAbilityModule>())
	{
		return AbilityModule->SpawnPickUp(InItem, InLocation, InContainer);
	}
	return nullptr;
}

AAbilityPickUpBase* UAbilityModuleBPLibrary::SpawnPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AMainModule::GetModuleByClass<AAbilityModule>())
	{
		return AbilityModule->SpawnPickUp(InSaveData, InContainer);
	}
	return nullptr;
}

AAbilityCharacterBase* UAbilityModuleBPLibrary::SpawnCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AMainModule::GetModuleByClass<AAbilityModule>())
	{
		return AbilityModule->SpawnCharacter(InSaveData, InContainer);
	}
	return nullptr;
}

AAbilityVitalityBase* UAbilityModuleBPLibrary::SpawnVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AMainModule::GetModuleByClass<AAbilityModule>())
	{
		return AbilityModule->SpawnVitality(InSaveData, InContainer);
	}
	return nullptr;
}
