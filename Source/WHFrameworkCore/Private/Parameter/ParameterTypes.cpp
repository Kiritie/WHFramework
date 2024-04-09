// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterTypes.h"

#include "Debug/DebugTypes.h"

bool FParameters::HasParameter(FName InName, bool bEnsured) const
{
	for (auto& Iter : Sets)
	{
		if(Iter.Name == InName)
		{
			return true;
		}
	}
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("Parameter not exist, parameter name: %s"), *InName.ToString()), EDC_Parameter, EDV_Error);
	return false;
}

void FParameters::SetParameter(FName InName, const FParameter& InParameter)
{
	if(HasParameter(InName, false))
	{
		for (int32 i = 0; i < Sets.Num(); i++)
		{
			if(Sets[i].Name == InName)
			{
				Sets[i].Parameter = InParameter;
				break;
			}
		}
	}
	else
	{
		Sets.Add(FParameterSet(InName, InParameter));
	}
}

FParameter FParameters::GetParameter(FName InName, bool bEnsured) const
{
	for (int32 i = 0; i < Sets.Num(); i++)
	{
		if(Sets[i].Name == InName)
		{
			return Sets[i].Parameter;
		}
	}
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("Failed to get parameter, parameter name: %s"), *InName.ToString()), EDC_Parameter, EDV_Error);
	return FParameter();
}

TArray<FParameter> FParameters::GetParameters(FName InName, bool bEnsured) const
{
	TArray<FParameter> TmpArr = TArray<FParameter>();
	for (int32 i = 0; i < Sets.Num(); i++)
	{
		if(Sets[i].Name == InName)
		{
			TmpArr.Add(Sets[i].Parameter);
		}
	}
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("Failed to get parameters, parameters name: %s"), *InName.ToString()), EDC_Parameter, EDV_Error);
	return TmpArr;
}

void FParameters::RemoveParameter(FName InName)
{
	for (int32 i = 0; i < Sets.Num(); i++)
	{
		if(Sets[i].Name == InName)
		{
			Sets.RemoveAt(i);
			break;
		}
	}
}

void FParameters::RemoveParameters(FName InName)
{
	while(HasParameter(InName, false))
	{
		RemoveParameter(InName);
	}
}

void FParameters::ClearAllParameter()
{
	Sets.Empty();
}
