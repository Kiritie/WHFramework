// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModuleBPLibrary.h"

#include "Global/GlobalBPLibrary.h"

FPrimaryAssetType UAbilityModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType InItemType)
{
	return *UGlobalBPLibrary::GetEnumValueAuthoredName(TEXT("EItemType"), (int32)InItemType);
}
