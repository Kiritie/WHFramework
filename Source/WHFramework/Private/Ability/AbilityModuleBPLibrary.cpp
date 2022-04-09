// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModuleBPLibrary.h"

#include "Global/GlobalBPLibrary.h"

FPrimaryAssetType UAbilityModuleBPLibrary::GetAssetIDByItemType(EAbilityItemType InItemType)
{
	return *UGlobalBPLibrary::GetEnumValueAuthoredName(TEXT("EAbilityItemType"), (int32)InItemType);
}
