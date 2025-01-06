// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_SetActorVisible.h"

UEventHandle_SetActorVisible::UEventHandle_SetActorVisible()
{
	ActorPath = nullptr;
	bVisible = false;
}

void UEventHandle_SetActorVisible::OnDespawn_Implementation(bool bRecovery)
{
	ActorPath = nullptr;
	bVisible = false;
}

void UEventHandle_SetActorVisible::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		ActorPath = InParams[0];
	}
	if(InParams.IsValidIndex(1))
	{
		bVisible = InParams[1];
	}
}

TArray<FParameter> UEventHandle_SetActorVisible::Pack_Implementation()
{
	return { ActorPath, bVisible };
}
