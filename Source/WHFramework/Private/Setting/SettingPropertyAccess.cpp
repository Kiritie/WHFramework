#include "Setting/SettingPropertyAccess.h"
#include "Setting/SettingPresentationTypes.h"

#include "InputCoreTypes.h"
#include "UObject/UnrealType.h"

bool FSettingPropertyAccess::IsSupported(const FProperty* InProperty)
{
	if (CastField<FBoolProperty>(InProperty) || CastField<FIntProperty>(InProperty) || CastField<FInt64Property>(InProperty) ||
	    CastField<FFloatProperty>(InProperty) || CastField<FDoubleProperty>(InProperty) || CastField<FNameProperty>(InProperty) ||
	    CastField<FStrProperty>(InProperty) || CastField<FTextProperty>(InProperty) || CastField<FEnumProperty>(InProperty))
	{
		return true;
	}
	if (const FByteProperty* ByteProperty = CastField<FByteProperty>(InProperty))
	{
		return ByteProperty->Enum != nullptr;
	}
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
	{
		return StructProperty->Struct == TBaseStructure<FKey>::Get() || StructProperty->Struct == TBaseStructure<FIntPoint>::Get();
	}
	return false;
}

ESettingRendererType FSettingPropertyAccess::InferRenderer(const FProperty* InProperty)
{
	if (CastField<FBoolProperty>(InProperty))
	{
		return ESettingRendererType::Bool;
	}
	if (CastField<FEnumProperty>(InProperty) || CastField<FByteProperty>(InProperty))
	{
		return ESettingRendererType::Enum;
	}
	if (CastField<FIntProperty>(InProperty) || CastField<FInt64Property>(InProperty) || CastField<FFloatProperty>(InProperty) ||
	    CastField<FDoubleProperty>(InProperty))
	{
		return ESettingRendererType::Number;
	}
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(InProperty); StructProperty && StructProperty->Struct == TBaseStructure<FKey>::Get())
	{
		return ESettingRendererType::Key;
	}
	return ESettingRendererType::Text;
}

bool FSettingPropertyAccess::Resolve(UStruct* InRootStruct, void* InRootData, const FString& InPath, FResolvedSettingProperty& OutResolved)
{
	TArray<FString> Segments;
	InPath.ParseIntoArray(Segments, TEXT("."), true);
	UStruct* CurrentStruct = InRootStruct;
	void* CurrentContainer = InRootData;
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		FProperty* Property = FindFProperty<FProperty>(CurrentStruct, *Segments[Index]);
		if (!Property)
		{
			return false;
		}
		if (Index == Segments.Num() - 1)
		{
			OutResolved.ContainerPtr = CurrentContainer;
			OutResolved.Property = Property;
			return true;
		}

		FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (!StructProperty)
		{
			return false;
		}
		CurrentContainer = StructProperty->ContainerPtrToValuePtr<void>(CurrentContainer);
		CurrentStruct = StructProperty->Struct;
	}
	return false;
}

FParameter FSettingPropertyAccess::Read(const FResolvedSettingProperty& InResolved)
{
	FProperty* Property = InResolved.Property;
	void* ValuePtr = Property ? Property->ContainerPtrToValuePtr<void>(InResolved.ContainerPtr) : nullptr;
	if (!ValuePtr)
		return FParameter();
	if (const FBoolProperty* Typed = CastField<FBoolProperty>(Property))
		return FParameter(Typed->GetPropertyValue(ValuePtr));
	if (const FIntProperty* Typed = CastField<FIntProperty>(Property))
		return FParameter(Typed->GetPropertyValue(ValuePtr));
	if (const FInt64Property* Typed = CastField<FInt64Property>(Property))
		return FParameter(Typed->GetPropertyValue(ValuePtr));
	if (const FFloatProperty* Typed = CastField<FFloatProperty>(Property))
		return FParameter(Typed->GetPropertyValue(ValuePtr));
	if (const FDoubleProperty* Typed = CastField<FDoubleProperty>(Property))
		return FParameter(Typed->GetPropertyValue(ValuePtr));
	if (const FNameProperty* Typed = CastField<FNameProperty>(Property))
		return FParameter(Typed->GetPropertyValue(ValuePtr));
	if (const FStrProperty* Typed = CastField<FStrProperty>(Property))
		return FParameter(Typed->GetPropertyValue(ValuePtr));
	if (const FTextProperty* Typed = CastField<FTextProperty>(Property))
		return FParameter(Typed->GetPropertyValue(ValuePtr));
	if (const FEnumProperty* Typed = CastField<FEnumProperty>(Property))
		return FParameter(Typed->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr));
	if (const FByteProperty* Typed = CastField<FByteProperty>(Property))
		return FParameter(static_cast<int64>(Typed->GetPropertyValue(ValuePtr)));
	if (const FStructProperty* Typed = CastField<FStructProperty>(Property); Typed && Typed->Struct == TBaseStructure<FKey>::Get())
		return FParameter(*static_cast<FKey*>(ValuePtr));
	if (const FStructProperty* Typed = CastField<FStructProperty>(Property); Typed && Typed->Struct == TBaseStructure<FIntPoint>::Get())
		return FParameter(*static_cast<FIntPoint*>(ValuePtr));
	return FParameter();
}

bool FSettingPropertyAccess::Write(const FResolvedSettingProperty& InResolved, const FParameter& InValue)
{
	FProperty* Property = InResolved.Property;
	void* ValuePtr = Property ? Property->ContainerPtrToValuePtr<void>(InResolved.ContainerPtr) : nullptr;
	if (!ValuePtr)
		return false;
	if (FBoolProperty* Typed = CastField<FBoolProperty>(Property); Typed && InValue.Is<bool>())
	{
		Typed->SetPropertyValue(ValuePtr, InValue.Get<bool>());
		return true;
	}
	if (FIntProperty* Typed = CastField<FIntProperty>(Property); Typed && InValue.Is<int32>())
	{
		Typed->SetPropertyValue(ValuePtr, InValue.Get<int32>());
		return true;
	}
	if (FInt64Property* Typed = CastField<FInt64Property>(Property); Typed && InValue.Is<int64>())
	{
		Typed->SetPropertyValue(ValuePtr, InValue.Get<int64>());
		return true;
	}
	if (FFloatProperty* Typed = CastField<FFloatProperty>(Property); Typed && InValue.Is<float>())
	{
		Typed->SetPropertyValue(ValuePtr, InValue.Get<float>());
		return true;
	}
	if (FDoubleProperty* Typed = CastField<FDoubleProperty>(Property); Typed && InValue.Is<double>())
	{
		Typed->SetPropertyValue(ValuePtr, InValue.Get<double>());
		return true;
	}
	if (FNameProperty* Typed = CastField<FNameProperty>(Property); Typed && InValue.Is<FName>())
	{
		Typed->SetPropertyValue(ValuePtr, InValue.Get<FName>());
		return true;
	}
	if (FStrProperty* Typed = CastField<FStrProperty>(Property); Typed && InValue.Is<FString>())
	{
		Typed->SetPropertyValue(ValuePtr, InValue.Get<FString>());
		return true;
	}
	if (FTextProperty* Typed = CastField<FTextProperty>(Property); Typed && InValue.Is<FText>())
	{
		Typed->SetPropertyValue(ValuePtr, InValue.Get<FText>());
		return true;
	}
	if (FEnumProperty* Typed = CastField<FEnumProperty>(Property); Typed && InValue.Is<int64>())
	{
		Typed->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, InValue.Get<int64>());
		return true;
	}
	if (FByteProperty* Typed = CastField<FByteProperty>(Property); Typed && InValue.Is<int64>())
	{
		Typed->SetPropertyValue(ValuePtr, static_cast<uint8>(InValue.Get<int64>()));
		return true;
	}
	if (const FStructProperty* Typed = CastField<FStructProperty>(Property); Typed && Typed->Struct == TBaseStructure<FKey>::Get() && InValue.Is<FKey>())
	{
		*static_cast<FKey*>(ValuePtr) = InValue.Get<FKey>();
		return true;
	}
	if (const FStructProperty* Typed = CastField<FStructProperty>(Property);
	    Typed && Typed->Struct == TBaseStructure<FIntPoint>::Get() && InValue.Is<FIntPoint>())
	{
		*static_cast<FIntPoint*>(ValuePtr) = InValue.Get<FIntPoint>();
		return true;
	}
	return false;
}
