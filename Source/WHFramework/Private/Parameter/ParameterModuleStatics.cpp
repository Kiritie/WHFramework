#include "Parameter/ParameterModuleStatics.h"

#include "Parameter/ParameterModule.h"
#include "UObject/UnrealType.h"

namespace
{
	template<typename T>
	UScriptStruct* GetNativeStruct()
	{
		if constexpr(requires { T::StaticStruct(); })
		{
			return T::StaticStruct();
		}
		else
		{
			return TBaseStructure<T>::Get();
		}
	}

	template<typename T>
	bool TrySetStructValue(FParameter& Parameter, const FStructProperty* Property, const void* ValuePtr)
	{
		if(Property->Struct != GetNativeStruct<T>()) return false;
		Parameter.Set(*static_cast<const T*>(ValuePtr));
		return true;
	}

	template<typename T>
	bool TryGetStructValue(const FParameter& Parameter, const FStructProperty* Property, void* ValuePtr)
	{
		if(Property->Struct != GetNativeStruct<T>()) return false;
		T Value;
		if(!Parameter.TryGet(Value)) return false;
		Property->Struct->CopyScriptStruct(ValuePtr, &Value);
		return true;
	}

#define WH_PARAMETER_STRUCT_TYPES(Operation) \
	Operation(FEnumParameterValue) || Operation(FVector) || Operation(FRotator) || Operation(FTransform) \
	|| Operation(FColor) || Operation(FLinearColor) || Operation(FKey) || Operation(FGameplayTag) \
	|| Operation(FGameplayTagContainer) || Operation(FSlateBrush) || Operation(FGuid) || Operation(FPrimaryAssetId)

	bool SetKnownStructValue(FParameter& Parameter, const FStructProperty* Property, const void* ValuePtr)
	{
#define WH_SET_STRUCT(Type) TrySetStructValue<Type>(Parameter, Property, ValuePtr)
		return WH_PARAMETER_STRUCT_TYPES(WH_SET_STRUCT);
#undef WH_SET_STRUCT
	}

	bool GetKnownStructValue(const FParameter& Parameter, const FStructProperty* Property, void* ValuePtr)
	{
#define WH_GET_STRUCT(Type) TryGetStructValue<Type>(Parameter, Property, ValuePtr)
		return WH_PARAMETER_STRUCT_TYPES(WH_GET_STRUCT);
#undef WH_GET_STRUCT
	}
#undef WH_PARAMETER_STRUCT_TYPES
}

bool UParameterModuleStatics::HasGlobalParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().HasParameter(InName, bEnsured);
}

void UParameterModuleStatics::SetGlobalParameter(FName InName, FParameter InParameter)
{
	UParameterModule::Get().SetParameter(InName, InParameter);
}

FParameter UParameterModuleStatics::GetGlobalParameter(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetParameter(InName, bEnsured);
}

TArray<FParameter> UParameterModuleStatics::GetGlobalParameters(FName InName, bool bEnsured)
{
	return UParameterModule::Get().GetParameters(InName, bEnsured);
}

void UParameterModuleStatics::RemoveGlobalParameter(FName InName)
{
	UParameterModule::Get().RemoveParameter(InName);
}

void UParameterModuleStatics::RemoveGlobalParameters(FName InName)
{
	UParameterModule::Get().RemoveParameters(InName);
}

void UParameterModuleStatics::ClearAllGlobalParameter()
{
	UParameterModule::Get().ClearAllParameter();
}

FParameter UParameterModuleStatics::MakeParameter(const int32&, const FText&)
{
	checkNoEntry();
	return {};
}

void UParameterModuleStatics::GetParameterValue(const FParameter&, int32&, bool&)
{
	checkNoEntry();
}

void UParameterModuleStatics::SetParameterValue(FParameter&, const int32&)
{
	checkNoEntry();
}

bool UParameterModuleStatics::IsParameterType(const FParameter&, const int32&)
{
	checkNoEntry();
	return false;
}

DEFINE_FUNCTION(UParameterModuleStatics::execMakeParameter)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	const FProperty* ValueProperty = Stack.MostRecentProperty;
	const void* ValuePtr = Stack.MostRecentPropertyAddress;
	P_GET_PROPERTY_REF(FTextProperty, Description);
	P_FINISH;
	P_NATIVE_BEGIN;
	FParameter& Result = *static_cast<FParameter*>(RESULT_PARAM);
	Result = FParameter();
	if(ValueProperty && ValuePtr) SetValueFromProperty(Result, ValueProperty, ValuePtr);
	Result.SetDescription(Description);
	P_NATIVE_END;
}

DEFINE_FUNCTION(UParameterModuleStatics::execGetParameterValue)
{
	P_GET_STRUCT_REF(FParameter, Parameter);
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	const FProperty* ValueProperty = Stack.MostRecentProperty;
	void* ValuePtr = Stack.MostRecentPropertyAddress;
	P_GET_UBOOL_REF(Success);
	P_FINISH;
	P_NATIVE_BEGIN;
	if(ValueProperty && ValuePtr)
	{
		ValueProperty->ClearValue(ValuePtr);
	}
	Success = ValueProperty && ValuePtr && GetValueToProperty(Parameter, ValueProperty, ValuePtr);
	P_NATIVE_END;
}

DEFINE_FUNCTION(UParameterModuleStatics::execSetParameterValue)
{
	P_GET_STRUCT_REF(FParameter, Parameter);
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	const FProperty* ValueProperty = Stack.MostRecentProperty;
	const void* ValuePtr = Stack.MostRecentPropertyAddress;
	P_FINISH;
	P_NATIVE_BEGIN;
	if(ValueProperty && ValuePtr) SetValueFromProperty(Parameter, ValueProperty, ValuePtr);
	P_NATIVE_END;
}

DEFINE_FUNCTION(UParameterModuleStatics::execIsParameterType)
{
	P_GET_STRUCT_REF(FParameter, Parameter);
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	const FProperty* ValueProperty = Stack.MostRecentProperty;
	P_FINISH;
	P_NATIVE_BEGIN;
	*static_cast<bool*>(RESULT_PARAM) = ValueProperty && IsValuePropertyType(Parameter, ValueProperty);
	P_NATIVE_END;
}

bool UParameterModuleStatics::SetValueFromProperty(FParameter& Parameter, const FProperty* Property, const void* ValuePtr)
{
	if(const FBoolProperty* Typed = CastField<FBoolProperty>(Property)) { Parameter.Set(Typed->GetPropertyValue(ValuePtr)); return true; }
	if(const FEnumProperty* Typed = CastField<FEnumProperty>(Property))
	{
		Parameter.Set(FEnumParameterValue(Typed->GetEnum(), static_cast<uint8>(Typed->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr))));
		return true;
	}
	if(const FByteProperty* Typed = CastField<FByteProperty>(Property))
	{
		const uint8 Value = Typed->GetPropertyValue(ValuePtr);
		if(Typed->Enum) Parameter.Set(FEnumParameterValue(Typed->Enum, Value)); else Parameter.Set(Value);
		return true;
	}
	if(const FIntProperty* Typed = CastField<FIntProperty>(Property)) { Parameter.Set(Typed->GetPropertyValue(ValuePtr)); return true; }
	if(const FInt64Property* Typed = CastField<FInt64Property>(Property)) { Parameter.Set(Typed->GetPropertyValue(ValuePtr)); return true; }
	if(const FFloatProperty* Typed = CastField<FFloatProperty>(Property)) { Parameter.Set(Typed->GetPropertyValue(ValuePtr)); return true; }
	if(const FDoubleProperty* Typed = CastField<FDoubleProperty>(Property)) { Parameter.Set(Typed->GetPropertyValue(ValuePtr)); return true; }
	if(const FStrProperty* Typed = CastField<FStrProperty>(Property)) { Parameter.Set(Typed->GetPropertyValue(ValuePtr)); return true; }
	if(const FNameProperty* Typed = CastField<FNameProperty>(Property)) { Parameter.Set(Typed->GetPropertyValue(ValuePtr)); return true; }
	if(const FTextProperty* Typed = CastField<FTextProperty>(Property)) { Parameter.Set(Typed->GetPropertyValue(ValuePtr)); return true; }
	if(const FSoftClassProperty* Typed = CastField<FSoftClassProperty>(Property)) { Parameter.Set(TSoftClassPtr<UObject>(Typed->GetPropertyValue(ValuePtr).ToSoftObjectPath())); return true; }
	if(const FSoftObjectProperty* Typed = CastField<FSoftObjectProperty>(Property)) { Parameter.Set(TSoftObjectPtr<UObject>(Typed->GetPropertyValue(ValuePtr).ToSoftObjectPath())); return true; }
	if(const FClassProperty* Typed = CastField<FClassProperty>(Property)) { Parameter.Set(Cast<UClass>(Typed->GetObjectPropertyValue(ValuePtr))); return true; }
	if(const FObjectPropertyBase* Typed = CastField<FObjectPropertyBase>(Property)) { Parameter.Set(Typed->GetObjectPropertyValue(ValuePtr)); return true; }
	if(const FStructProperty* Typed = CastField<FStructProperty>(Property))
	{
		if(Typed->Struct->IsChildOf(FParameterValueBase::StaticStruct()))
		{
			Parameter.Value.InitializeAsScriptStruct(Typed->Struct, static_cast<const uint8*>(ValuePtr));
			return true;
		}
		if(SetKnownStructValue(Parameter, Typed, ValuePtr))
		{
			return true;
		}

		FParameterStructValue& StructValue = Parameter.Value.InitializeAs<FParameterStructValue>();
		StructValue.Value.InitializeAs(Typed->Struct, static_cast<const uint8*>(ValuePtr));
		return true;
	}
	return false;
}

bool UParameterModuleStatics::GetValueToProperty(const FParameter& Parameter, const FProperty* Property, void* ValuePtr)
{
	if(const FBoolProperty* Typed = CastField<FBoolProperty>(Property)) { bool Value; if(!Parameter.TryGet(Value)) return false; Typed->SetPropertyValue(ValuePtr, Value); return true; }
	if(const FEnumProperty* Typed = CastField<FEnumProperty>(Property))
	{
		FEnumParameterValue Value;
		if(!Parameter.TryGet(Value) || Value.EnumType != Typed->GetEnum()) return false;
		Typed->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, static_cast<int64>(Value.EnumValue));
		return true;
	}
	if(const FByteProperty* Typed = CastField<FByteProperty>(Property))
	{
		uint8 Value = 0;
		if(Typed->Enum) { FEnumParameterValue EnumValue; if(!Parameter.TryGet(EnumValue) || EnumValue.EnumType != Typed->Enum) return false; Value = EnumValue.EnumValue; }
		else if(!Parameter.TryGet(Value)) return false;
		Typed->SetPropertyValue(ValuePtr, Value);
		return true;
	}
#define WH_GET_PROPERTY(PropertyType, NativeType) \
	if(const PropertyType* Typed = CastField<PropertyType>(Property)) { NativeType Value; if(!Parameter.TryGet(Value)) return false; Typed->SetPropertyValue(ValuePtr, Value); return true; }
	WH_GET_PROPERTY(FIntProperty, int32)
	WH_GET_PROPERTY(FInt64Property, int64)
	WH_GET_PROPERTY(FFloatProperty, float)
	WH_GET_PROPERTY(FDoubleProperty, double)
	WH_GET_PROPERTY(FStrProperty, FString)
	WH_GET_PROPERTY(FNameProperty, FName)
	WH_GET_PROPERTY(FTextProperty, FText)
#undef WH_GET_PROPERTY
	if(const FSoftClassProperty* Typed = CastField<FSoftClassProperty>(Property)) { TSoftClassPtr<UObject> Value; if(!Parameter.TryGet(Value)) return false; Typed->SetPropertyValue(ValuePtr, FSoftObjectPtr(Value.ToSoftObjectPath())); return true; }
	if(const FSoftObjectProperty* Typed = CastField<FSoftObjectProperty>(Property)) { TSoftObjectPtr<UObject> Value; if(!Parameter.TryGet(Value)) return false; Typed->SetPropertyValue(ValuePtr, FSoftObjectPtr(Value.ToSoftObjectPath())); return true; }
	if(const FClassProperty* Typed = CastField<FClassProperty>(Property))
	{
		UClass* Value = nullptr;
		if(!Parameter.TryGet(Value) || (Value && !Value->IsChildOf(Typed->MetaClass))) return false;
		Typed->SetObjectPropertyValue(ValuePtr, Value);
		return true;
	}
	if(const FObjectPropertyBase* Typed = CastField<FObjectPropertyBase>(Property))
	{
		UObject* Value = nullptr;
		if(!Parameter.TryGet(Value) || (Value && !Value->IsA(Typed->PropertyClass))) return false;
		Typed->SetObjectPropertyValue(ValuePtr, Value);
		return true;
	}
	if(const FStructProperty* Typed = CastField<FStructProperty>(Property))
	{
		if(Parameter.GetValueStruct() == Typed->Struct && Typed->Struct->IsChildOf(FParameterValueBase::StaticStruct()))
		{
			Typed->Struct->CopyScriptStruct(ValuePtr, Parameter.Value.GetMemory());
			return true;
		}
		if(GetKnownStructValue(Parameter, Typed, ValuePtr))
		{
			return true;
		}

		const FParameterStructValue* StructValue = Parameter.Value.GetPtr<FParameterStructValue>();
		if(!StructValue || StructValue->Value.GetScriptStruct() != Typed->Struct)
		{
			return false;
		}

		Typed->Struct->CopyScriptStruct(ValuePtr, StructValue->Value.GetMemory());
		return true;
	}
	return false;
}

bool UParameterModuleStatics::IsValuePropertyType(const FParameter& Parameter, const FProperty* Property)
{
	if(!Parameter.HasValue()) return false;
	TArray<uint8, TInlineAllocator<256>> Storage;
	Storage.SetNumUninitialized(Property->GetSize());
	Property->InitializeValue(Storage.GetData());
	const bool bMatches = GetValueToProperty(Parameter, Property, Storage.GetData());
	Property->DestroyValue(Storage.GetData());
	return bMatches;
}

FParameterMap UParameterModuleStatics::MakeParameterMap(const TMap<FString, FString>& ParameterMap)
{
	return ParameterMap;
}

FParameterMap& UParameterModuleStatics::AddParameterMapValue(FParameterMap& ParameterMap, const FString& Key, const FString& Value)
{
	ParameterMap.Add(Key, Value);
	return ParameterMap;
}

FParameterMap& UParameterModuleStatics::SetParameterMapValue(FParameterMap& ParameterMap, const FString& Key, const FString& Value)
{
	ParameterMap.Set(Key, Value);
	return ParameterMap;
}

FParameterMap& UParameterModuleStatics::RemoveParameterMapKey(FParameterMap& ParameterMap, const FString& Key)
{
	ParameterMap.Remove(Key);
	return ParameterMap;
}

FParameterMap& UParameterModuleStatics::ClearParameterMap(FParameterMap& ParameterMap)
{
	ParameterMap.Clear();
	return ParameterMap;
}
