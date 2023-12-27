// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Common/EventHandle_SendGameplayTag.h"

UEventHandle_SendGameplayTag::UEventHandle_SendGameplayTag()
{
	Tag = FGameplayTag();
}

void UEventHandle_SendGameplayTag::OnDespawn_Implementation(bool bRecovery)
{
	Tag = FGameplayTag();
}

void UEventHandle_SendGameplayTag::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Tag = InParams[0].GetTagValue();
	}
}

TArray<FParameter> UEventHandle_SendGameplayTag::Pack_Implementation()
{
	return { Tag };
}
