#include "Parameter/ParameterValueTypes.h"

FEnumParameterValue::FEnumParameterValue()
{
}

FEnumParameterValue::FEnumParameterValue(UEnum* InEnumType, uint8 InEnumValue)
	: EnumType(InEnumType)
{
	EnumValue = EnumType ? FMath::Clamp<int32>(InEnumValue, 0, FMath::Max(EnumType->NumEnums() - 1, 0)) : 0;
}

FEnumParameterValue::FEnumParameterValue(const FString& InEnumName, uint8 InEnumValue)
	: FEnumParameterValue(LoadObject<UEnum>(nullptr, *InEnumName), InEnumValue)
{
	EnumName = InEnumName;
}

FEnumParameterValue::FEnumParameterValue(const TArray<FString>& InEnumNames, uint8 InEnumValue)
	: EnumNames(InEnumNames)
{
	EnumValue = FMath::Clamp<int32>(InEnumValue, 0, FMath::Max(EnumNames.Num() - 1, 0));
}
