// Fill out your copyright notice in the Description page of Project Settings.


#include "Parameter/ParameterTypes.h"

#include "Debug/DebugTypes.h"

EParameterType FParameter::GetParameterType() const
{
#define WH_TYPE_CASE(Wrapper, Type) if(Value.GetScriptStruct() == Wrapper::StaticStruct()) return EParameterType::Type;
	WH_TYPE_CASE(FParameterIntValue, Integer) WH_TYPE_CASE(FParameterFloatValue, Float)
	WH_TYPE_CASE(FParameterByteValue, Byte) WH_TYPE_CASE(FParameterEnumValue, Enum)
	WH_TYPE_CASE(FParameterStringValue, String) WH_TYPE_CASE(FParameterNameValue, Name)
	WH_TYPE_CASE(FParameterTextValue, Text) WH_TYPE_CASE(FParameterBoolValue, Boolean)
	WH_TYPE_CASE(FParameterVectorValue, Vector) WH_TYPE_CASE(FParameterRotatorValue, Rotator)
	WH_TYPE_CASE(FParameterTransformValue, Transform) WH_TYPE_CASE(FParameterColorValue, Color)
	WH_TYPE_CASE(FParameterLinearColorValue, LinearColor) WH_TYPE_CASE(FParameterKeyValue, Key)
	WH_TYPE_CASE(FParameterTagValue, Tag) WH_TYPE_CASE(FParameterTagsValue, Tags)
	WH_TYPE_CASE(FParameterBrushValue, Brush) WH_TYPE_CASE(FParameterGuidValue, Guid)
	WH_TYPE_CASE(FParameterAssetIdValue, AssetID) WH_TYPE_CASE(FParameterClassValue, Class)
	WH_TYPE_CASE(FParameterSoftClassValue, ClassPtr) WH_TYPE_CASE(FParameterObjectValue, Object)
	WH_TYPE_CASE(FParameterObjectInstanceValue, ObjectInst) WH_TYPE_CASE(FParameterSoftObjectValue, ObjectPtr)
	WH_TYPE_CASE(FParameterDelegateValue, Delegate) WH_TYPE_CASE(FParameterPointerValue, Pointer)
#undef WH_TYPE_CASE
	return Value.IsValid() ? EParameterType::Misc : EParameterType::None;
}

void FParameter::SetParameterType(EParameterType InParameterType)
{
	switch(InParameterType)
	{
#define WH_INIT_CASE(Type, Wrapper) case EParameterType::Type: Value.InitializeAs<Wrapper>(); break;
	WH_INIT_CASE(Integer, FParameterIntValue) WH_INIT_CASE(Float, FParameterFloatValue)
	WH_INIT_CASE(Byte, FParameterByteValue) WH_INIT_CASE(Enum, FParameterEnumValue)
	WH_INIT_CASE(String, FParameterStringValue) WH_INIT_CASE(Name, FParameterNameValue)
	WH_INIT_CASE(Text, FParameterTextValue) WH_INIT_CASE(Boolean, FParameterBoolValue)
	WH_INIT_CASE(Vector, FParameterVectorValue) WH_INIT_CASE(Rotator, FParameterRotatorValue)
	WH_INIT_CASE(Transform, FParameterTransformValue) WH_INIT_CASE(Color, FParameterColorValue)
	WH_INIT_CASE(LinearColor, FParameterLinearColorValue) WH_INIT_CASE(Key, FParameterKeyValue)
	WH_INIT_CASE(Tag, FParameterTagValue) WH_INIT_CASE(Tags, FParameterTagsValue)
	WH_INIT_CASE(Brush, FParameterBrushValue) WH_INIT_CASE(Guid, FParameterGuidValue)
	WH_INIT_CASE(AssetID, FParameterAssetIdValue) WH_INIT_CASE(Class, FParameterClassValue)
	WH_INIT_CASE(ClassPtr, FParameterSoftClassValue) WH_INIT_CASE(Object, FParameterObjectValue)
	WH_INIT_CASE(ObjectInst, FParameterObjectInstanceValue) WH_INIT_CASE(ObjectPtr, FParameterSoftObjectValue)
	WH_INIT_CASE(Delegate, FParameterDelegateValue) WH_INIT_CASE(Pointer, FParameterPointerValue)
#undef WH_INIT_CASE
	default: Value.Reset(); break;
	}
}

UClass* FParameter::GetClassValue() const
{
	const FParameterClassValue* Wrapper = Value.GetPtr<FParameterClassValue>();
	return Wrapper ? Wrapper->Value.Get() : nullptr;
}

void FParameter::SetClassValue(UClass* InValue)
{
	Value.InitializeAs<FParameterClassValue>().Value = InValue;
}

UObject* FParameter::GetObjectValue() const
{
	const FParameterObjectValue* Wrapper = Value.GetPtr<FParameterObjectValue>();
	return Wrapper ? Wrapper->Value.Get() : nullptr;
}

UObject* FParameter::GetObjectInstValue() const
{
	const FParameterObjectInstanceValue* Wrapper = Value.GetPtr<FParameterObjectInstanceValue>();
	return Wrapper ? Wrapper->Value.Get() : nullptr;
}

void FParameter::SetObjectInstValue(UObject* InValue)
{
	Value.InitializeAs<FParameterObjectInstanceValue>().Value = InValue;
}

FSimpleDynamicDelegate FParameter::GetDelegateValue() const
{
	const FParameterDelegateValue* Wrapper = Value.GetPtr<FParameterDelegateValue>();
	return Wrapper ? Wrapper->Value : FSimpleDynamicDelegate();
}

void* FParameter::GetPointerValue() const
{
	const FParameterPointerValue* Wrapper = Value.GetPtr<FParameterPointerValue>();
	return Wrapper ? Wrapper->Value : nullptr;
}

FParameter FParameter::MakeClass(UClass* InValue, const FText& InDescription)
{
	FParameter Result; Result.SetClassValue(InValue); Result.Description = InDescription; return Result;
}

FParameter FParameter::MakeObject(UObject* InValue, const FText& InDescription)
{
	FParameter Result; Result.SetObjectValue(InValue); Result.Description = InDescription; return Result;
}

FParameter FParameter::MakeObjectInst(UObject* InValue, const FText& InDescription)
{
	FParameter Result; Result.SetObjectInstValue(InValue); Result.Description = InDescription; return Result;
}

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
				Sets[i].Parameter.SetParameterValue(InParameter);
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

FString FParameterMap::ToString() const
{
	FString Result;
	for(const TPair<FString, FString>& Pair : Map)
	{
		if(!Result.IsEmpty()) Result += TEXT(",");
		Result += Pair.Key + TEXT("=") + Pair.Value;
	}
	return Result;
}

FString FParameterMap::ToJsonString() const
{
	FString Result;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
	Writer->WriteObjectStart();
	for(const TPair<FString, FString>& Pair : Map) Writer->WriteValue(Pair.Key, Pair.Value);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Result;
}
