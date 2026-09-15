// Fill out your copyright notice in the Description page of Project Settings.

#include "Parameter/ParameterTypes.h"
#include "Debug/DebugTypes.h"

FParameter::FParameter()
{
}

const UScriptStruct* FParameter::GetStructType() const
{
	const FParameterStructValue* Wrapper = Value.GetPtr<FParameterStructValue>();
	return Wrapper && Wrapper->Value.IsValid() ? Wrapper->Value.GetScriptStruct() : nullptr;
}

const uint8* FParameter::GetStructMemory() const
{
	const FParameterStructValue* Wrapper = Value.GetPtr<FParameterStructValue>();
	return Wrapper && Wrapper->Value.IsValid() ? Wrapper->Value.GetMemory() : nullptr;
}

uint8* FParameter::GetMutableStructMemory()
{
	FParameterStructValue* Wrapper = Value.GetMutablePtr<FParameterStructValue>();
	return Wrapper && Wrapper->Value.IsValid() ? Wrapper->Value.GetMutableMemory() : nullptr;
}

FParameterSet::FParameterSet()
{
}

FParameterMap::FParameterMap()
{
}

bool FParameterSets::HasParameter(FName InName, bool bEnsured) const
{
	for (auto& Iter : Sets)
	{
		if (Iter.Name == InName)
		{
			return true;
		}
	}
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("Parameter not exist, parameter name: %s"), *InName.ToString()), EDC_Parameter,
					 EDV_Error);
	return false;
}

void FParameterSets::SetParameter(FName InName, const FParameter& InParam)
{
	if(FParameterSet* Set = Sets.FindByPredicate([InName](const FParameterSet& Candidate)
	{
		return Candidate.Name == InName;
	}))
	{
		const FText Description = Set->Parameter.GetDescription();
		Set->Parameter = InParam;
		Set->Parameter.SetDescription(Description);
		return;
	}

	Sets.Add(FParameterSet(InName, InParam));
}

FParameter FParameterSets::GetParameter(FName InName, bool bEnsured) const
{
	for (int32 i = 0; i < Sets.Num(); i++)
	{
		if (Sets[i].Name == InName)
		{
			return Sets[i].Parameter;
		}
	}
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("Failed to get parameter, parameter name: %s"), *InName.ToString()), EDC_Parameter,
					 EDV_Error);
	return FParameter();
}

TArray<FParameter> FParameterSets::GetParameters(FName InName, bool bEnsured) const
{
	TArray<FParameter> Result;
	for(const FParameterSet& Set : Sets)
	{
		if(Set.Name == InName)
		{
			Result.Add(Set.Parameter);
		}
	}
	ensureEditorMsgf(!bEnsured || !Result.IsEmpty(), FString::Printf(TEXT("Failed to get parameters, parameters name: %s"), *InName.ToString()), EDC_Parameter,
					 EDV_Error);
	return Result;
}

void FParameterSets::RemoveParameter(FName InName)
{
	for (int32 i = 0; i < Sets.Num(); i++)
	{
		if (Sets[i].Name == InName)
		{
			Sets.RemoveAt(i);
			break;
		}
	}
}

void FParameterSets::RemoveParameters(FName InName)
{
	Sets.RemoveAll([InName](const FParameterSet& Set)
	{
		return Set.Name == InName;
	});
}

void FParameterSets::ClearAllParameter()
{
	Sets.Empty();
}

FString FParameterMap::ToString() const
{
	FString Result;
	for (const TPair<FString, FString>& Pair : Map)
	{
		if (!Result.IsEmpty())
			Result += TEXT(",");
		Result += Pair.Key + TEXT("=") + Pair.Value;
	}
	return Result;
}

FString FParameterMap::ToJsonString() const
{
	FString Result;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
	Writer->WriteObjectStart();
	for (const TPair<FString, FString>& Pair : Map)
		Writer->WriteValue(Pair.Key, Pair.Value);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Result;
}
