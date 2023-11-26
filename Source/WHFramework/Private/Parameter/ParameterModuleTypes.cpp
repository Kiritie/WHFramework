// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterModuleTypes.h"

#include "Debug/DebugModuleTypes.h"

// FString FParameterMap::ToJsonString() const
// {
// 	FString JsonString;
// 	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonString);
// 	JsonWriter->WriteObjectStart();
// 	for (auto& It : Map)
// 	{
// 		JsonWriter->WriteValue(It.Key, It.Value);
// 	}
// 	JsonWriter->WriteObjectEnd();
// 	JsonWriter->Close();
// 	return JsonString;
// }

bool FParameters::HasParameter(FName InName, bool bEnsured) const
{
	for (auto& Iter : ParamSets)
	{
		if(Iter.Name == InName)
		{
			return true;
		}
	}
	ensureEditor(!bEnsured);
	return false;
}

void FParameters::SetParameter(FName InName, FParameter InParameter)
{
	if(HasParameter(InName, false))
	{
		for (int32 i = 0; i < ParamSets.Num(); i++)
		{
			if(ParamSets[i].Name == InName)
			{
				ParamSets[i].Parameter = InParameter;
				break;
			}
		}
	}
	else
	{
		ParamSets.Add(FParameterSet(InName, InParameter));
	}
}

FParameter FParameters::GetParameter(FName InName, bool bEnsured) const
{
	for (int32 i = 0; i < ParamSets.Num(); i++)
	{
		if(ParamSets[i].Name == InName)
		{
			return ParamSets[i].Parameter;
		}
	}
	return FParameter();
}

TArray<FParameter> FParameters::GetParameters(FName InName, bool bEnsured) const
{
	TArray<FParameter> TmpArr = TArray<FParameter>();
	if(HasParameter(InName, bEnsured))
	{
		for (int32 i = 0; i < ParamSets.Num(); i++)
		{
			if(ParamSets[i].Name == InName)
			{
				TmpArr.Add(ParamSets[i].Parameter);
			}
		}
	}
	return TmpArr;
}

void FParameters::RemoveParameter(FName InName)
{
	for (int32 i = 0; i < ParamSets.Num(); i++)
	{
		if(ParamSets[i].Name == InName)
		{
			ParamSets.RemoveAt(i);
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
	ParamSets.Empty();
}
