// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_AsyncUnloadLevels.h"

UEventHandle_AsyncUnloadLevels::UEventHandle_AsyncUnloadLevels()
{
	SoftLevelPaths = TArray<FSoftLevelPath>();
	FinishDelayTime = 1.f;
	bCreateLoadingWidget = false;
}

void UEventHandle_AsyncUnloadLevels::OnDespawn_Implementation(bool bRecovery)
{
}

void UEventHandle_AsyncUnloadLevels::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		if(const FSoftLevelPathArrayParameterValue* Value = InParams[0].GetStructPtr<FSoftLevelPathArrayParameterValue>())
		{
			SoftLevelPaths = Value->Value;
		}
	}
	if(InParams.IsValidIndex(1))
	{
		FinishDelayTime = InParams[1].Get<float>();
	}
	if(InParams.IsValidIndex(2))
	{
		bCreateLoadingWidget = InParams[2].Get<bool>();
	}
}

TArray<FParameter> UEventHandle_AsyncUnloadLevels::Pack_Implementation()
{
	FSoftLevelPathArrayParameterValue PathsValue;
	PathsValue.Value = SoftLevelPaths;
	return { MoveTemp(PathsValue), FinishDelayTime, bCreateLoadingWidget };
}
