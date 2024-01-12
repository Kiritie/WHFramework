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
	for (auto& Iter : Sets)
	{
		if(Iter.Name == InName)
		{
			return true;
		}
	}
	ensureEditor(!bEnsured);
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
	return FParameter();
}

TArray<FParameter> FParameters::GetParameters(FName InName, bool bEnsured) const
{
	TArray<FParameter> TmpArr = TArray<FParameter>();
	if(HasParameter(InName, bEnsured))
	{
		for (int32 i = 0; i < Sets.Num(); i++)
		{
			if(Sets[i].Name == InName)
			{
				TmpArr.Add(Sets[i].Parameter);
			}
		}
	}
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
