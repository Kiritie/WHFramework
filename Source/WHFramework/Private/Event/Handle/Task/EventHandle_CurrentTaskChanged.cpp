// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_CurrentTaskChanged.h"

UEventHandle_CurrentTaskChanged::UEventHandle_CurrentTaskChanged()
{
	Task = nullptr;
}

void UEventHandle_CurrentTaskChanged::OnDespawn_Implementation(bool bRecovery)
{
	Task = nullptr;
}

void UEventHandle_CurrentTaskChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Task = InParams[0];
	}
}

TArray<FParameter> UEventHandle_CurrentTaskChanged::Pack_Implementation()
{
	return Super::Pack_Implementation();
}
