// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterModuleTypes.h"

#include "Debug/DebugModuleTypes.h"

bool FParameters::HasParameter(FName InName, bool bEnsured) const
{
	for (auto Iter : ParamSets)
	{
		if(Iter.Name == InName)
		{
			return true;
		}
	}
	return bEnsured ? ensureEditor(false) : false;
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

void FParameters::SetIntegerParameter(FName InName, int32 InValue)
{
	SetParameter(InName, FParameter::MakeInteger(InValue));
}

int32 FParameters::GetIntegerParameter(FName InName, bool bEnsured) const
{
	if(HasParameter(InName, bEnsured))
	{
		return GetParameter(InName, bEnsured).GetIntegerValue();
	}
	return 0;
}

TArray<int32> FParameters::GetIntegerParameters(FName InName, bool bEnsured) const
{
	TArray<int32> TmpArr = TArray<int32>();
	if(HasParameter(InName, bEnsured))
	{
		for (auto Iter : GetParameters(InName, bEnsured))
		{
			TmpArr.Add(Iter.GetIntegerValue());
		}
	}
	return TmpArr;
}

void FParameters::SetFloatParameter(FName InName, float InValue)
{
	SetParameter(InName, FParameter::MakeInteger(InValue));
}

float FParameters::GetFloatParameter(FName InName, bool bEnsured) const
{
	if(HasParameter(InName, bEnsured))
	{
		return GetParameter(InName, bEnsured).GetFloatValue();
	}
	return 0;
}

TArray<float> FParameters::GetFloatParameters(FName InName, bool bEnsured) const
{
	TArray<float> TmpArr = TArray<float>();
	if(HasParameter(InName, bEnsured))
	{
		for (auto Iter : GetParameters(InName, bEnsured))
		{
			TmpArr.Add(Iter.GetFloatValue());
		}
	}
	return TmpArr;
}

void FParameters::SetStringParameter(FName InName, FString InValue)
{
	SetParameter(InName, FParameter::MakeString(InValue));
}

FString FParameters::GetStringParameter(FName InName, bool bEnsured) const
{
	if(HasParameter(InName, bEnsured))
	{
		return GetParameter(InName, bEnsured).GetStringValue();
	}
	return TEXT("");
}

TArray<FString> FParameters::GetStringParameters(FName InName, bool bEnsured) const
{
	TArray<FString> TmpArr = TArray<FString>();
	if(HasParameter(InName, bEnsured))
	{
		for (auto Iter : GetParameters(InName, bEnsured))
		{
			TmpArr.Add(Iter.GetStringValue());
		}
	}
	return TmpArr;
}

void FParameters::SetTextParameter(FName InName, FText InValue)
{
	SetParameter(InName, FParameter::MakeText(InValue));
}

FText FParameters::GetTextParameter(FName InName, bool bEnsured) const
{
	if(HasParameter(InName, bEnsured))
	{
		return GetParameter(InName, bEnsured).GetTextValue();
	}
	return FText::GetEmpty();
}

TArray<FText> FParameters::GetTextParameters(FName InName, bool bEnsured) const
{
	TArray<FText> TmpArr = TArray<FText>();
	if(HasParameter(InName, bEnsured))
	{
		for (auto Iter : GetParameters(InName, bEnsured))
		{
			TmpArr.Add(Iter.GetTextValue());
		}
	}
	return TmpArr;
}

void FParameters::SetBooleanParameter(FName InName, bool InValue)
{
	SetParameter(InName, FParameter::MakeBoolean(InValue));
}

bool FParameters::GetBooleanParameter(FName InName, bool bEnsured) const
{
	if(HasParameter(InName, bEnsured))
	{
		return GetParameter(InName, bEnsured).GetBooleanValue();
	}
	return false;
}

TArray<bool> FParameters::GetBooleanParameters(FName InName, bool bEnsured) const
{
	TArray<bool> TmpArr = TArray<bool>();
	if(HasParameter(InName, bEnsured))
	{
		for (auto Iter : GetParameters(InName, bEnsured))
		{
			TmpArr.Add(Iter.GetBooleanValue());
		}
	}
	return TmpArr;
}

void FParameters::SetVectorParameter(FName InName, FVector InValue)
{
	SetParameter(InName, FParameter::MakeVector(InValue));
}

FVector FParameters::GetVectorParameter(FName InName, bool bEnsured) const
{
	if(HasParameter(InName, bEnsured))
	{
		return GetParameter(InName, bEnsured).GetVectorValue();
	}
	return FVector();
}

TArray<FVector> FParameters::GetVectorParameters(FName InName, bool bEnsured) const
{
	TArray<FVector> TmpArr = TArray<FVector>();
	if(HasParameter(InName, bEnsured))
	{
		for (auto Iter : GetParameters(InName, bEnsured))
		{
			TmpArr.Add(Iter.GetVectorValue());
		}
	}
	return TmpArr;
}

void FParameters::SetRotatorParameter(FName InName, FRotator InValue)
{
	SetParameter(InName, FParameter::MakeRotator(InValue));
}

FRotator FParameters::GetRotatorParameter(FName InName, bool bEnsured) const
{
	if(HasParameter(InName, bEnsured))
	{
		return GetParameter(InName, bEnsured).GetRotatorValue();
	}
	return FRotator();
}

TArray<FRotator> FParameters::GetRotatorParameters(FName InName, bool bEnsured) const
{
	TArray<FRotator> TmpArr = TArray<FRotator>();
	if(HasParameter(InName, bEnsured))
	{
		for (auto Iter : GetParameters(InName, bEnsured))
		{
			TmpArr.Add(Iter.GetRotatorValue());
		}
	}
	return TmpArr;
}

void FParameters::SetClassParameter(FName InName, UClass* InValue)
{
	SetParameter(InName, FParameter::MakeClass(InValue));
}

UClass* FParameters::GetClassParameter(FName InName, bool bEnsured) const
{
	if(HasParameter(InName, bEnsured))
	{
		return GetParameter(InName, bEnsured).GetClassValue();
	}
	return nullptr;
}

TArray<UClass*> FParameters::GetClassParameters(FName InName, bool bEnsured) const
{
	TArray<UClass*> TmpArr = TArray<UClass*>();
	if(HasParameter(InName, bEnsured))
	{
		for (auto Iter : GetParameters(InName, bEnsured))
		{
			TmpArr.Add(Iter.GetClassValue());
		}
	}
	return TmpArr;
}

void FParameters::SetObjectParameter(FName InName, UObject* InValue)
{
	SetParameter(InName, FParameter::MakeObject(InValue));
}

UObject* FParameters::GetObjectParameter(FName InName, bool bEnsured) const
{
	if(HasParameter(InName, bEnsured))
	{
		return GetParameter(InName, bEnsured).GetObjectValue();
	}
	return nullptr;
}

TArray<UObject*> FParameters::GetObjectParameters(FName InName, bool bEnsured) const
{
	TArray<UObject*> TmpArr = TArray<UObject*>();
	if(HasParameter(InName, bEnsured))
	{
		for (auto Iter : GetParameters(InName, bEnsured))
		{
			TmpArr.Add(Iter.GetObjectValue());
		}
	}
	return TmpArr;
}

void FParameters::SetPointerParameter(FName InName, void* InValue)
{
	SetParameter(InName, FParameter::MakePointer(InValue));
}

void* FParameters::GetPointerParameter(FName InName, bool bEnsured) const
{
	if(HasParameter(InName, bEnsured))
	{
		return GetParameter(InName, bEnsured).GetPointerValue();
	}
	return nullptr;
}

TArray<void*> FParameters::GetPointerParameters(FName InName, bool bEnsured) const
{
	TArray<void*> TmpArr = TArray<void*>();
	if(HasParameter(InName, bEnsured))
	{
		for (auto Iter : GetParameters(InName, bEnsured))
		{
			TmpArr.Add(Iter.GetPointerValue());
		}
	}
	return TmpArr;
}
