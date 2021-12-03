// Fill out your copyright notice in the Description page of Project Settings.


#include "Tool/GlobalToolsBPLibrary.h"

#include "Debug/DebugModuleTypes.h"

FString UGlobalToolsBPLibrary::EnumValueToString(const FString& InEnumName, int32 InEnumValue)
{
	if(UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, *InEnumName, true))
	{
		return EnumPtr->GetAuthoredNameStringByValue(InEnumValue);
	}
	return TEXT("");
}

void UGlobalToolsBPLibrary::SaveObjectToMemory(UObject* InObject, TArray<uint8>& OutObjectData)
{
	if(InObject)
	{
		FMemoryWriter MemoryWriter(OutObjectData, true);
		FSaveDataArchive Ar(MemoryWriter);
		InObject->Serialize(Ar);
	}
}

void UGlobalToolsBPLibrary::LoadObjectFromMemory(UObject* InObject, const TArray<uint8>& InObjectData)
{
	if(InObject && InObjectData.Num() > 0)
	{
		FMemoryReader MemoryReader(InObjectData, true);
		FSaveDataArchive Ar(MemoryReader);
		InObject->Serialize(Ar);
	}
}

void UGlobalToolsBPLibrary::SerializeExposedParam(UObject* InObject, const TMap<FString, FString>& InParam, bool bParamHavePropertyType)
{
	for(auto pair: InParam)
	{
		FString LeftS,PropName;
		if(bParamHavePropertyType)
		{
			pair.Key.Split("> ",&LeftS,&PropName);
		}
		else
		{
			PropName = pair.Key;
		}	
		FProperty* FoundProperty = FindFProperty<FProperty>(InObject->GetClass(), *PropName);
		if(FoundProperty)
		{
			void* Value = FoundProperty->ContainerPtrToValuePtr<void*>(InObject);
			FoundProperty->ImportText(*pair.Value,Value,PPF_None,InObject);
		}
		else
		{
			WH_LOG(LogTemp,Error,TEXT("%s : Not Found Property : %s"),*InObject->GetName(),*PropName);
		}
	}
}

void UGlobalToolsBPLibrary::ExportExposedParam(UClass* InClass, TMap<FString, FString>& OutParams, bool bDisplayPropertyType)
{
	if(InClass)
	{
		TArray<FString> NewProps;
		TArray<FString> DiffProps;
		const FString PropClassNameSuffix = "Property",ReplaceTo = "";
		for (TFieldIterator<FProperty> PropertyIt(InClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			const bool bIsDelegate = Property->IsA(FMulticastDelegateProperty::StaticClass());
			const bool bFlag = Property->HasAllPropertyFlags(CPF_ExposeOnSpawn);
			const bool bIsExposedToSpawn = bFlag;
			const bool bIsSettableExternally = !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance);
			if(	bIsExposedToSpawn &&
				!Property->HasAnyPropertyFlags(CPF_Parm) && 
				bIsSettableExternally &&
				Property->HasAllPropertyFlags(CPF_BlueprintVisible) &&
				!bIsDelegate )
			{
				FString PropertyName = Property->GetFName().ToString();
				if(PropertyName!="Param"&&PropertyName!="Instigator")
				{
					if(bDisplayPropertyType)
					{
						PropertyName = "<" + Property->GetClass()->GetName().Replace(*PropClassNameSuffix,*ReplaceTo) + "> " + PropertyName;
					}
					NewProps.Add(PropertyName);
					if(!OutParams.Contains(PropertyName))
					{
						FString Temp;
						void* Value = Property->ContainerPtrToValuePtr<void*>(InClass->GetDefaultObject());
						Property->ExportTextItem(Temp,Value,nullptr,nullptr,PPF_None);
						OutParams.Add(PropertyName,Temp);
					}
				}
			}
		}
		for(auto pair:OutParams)
		{
			if(!NewProps.Contains(pair.Key))
				DiffProps.Add(pair.Key);
		}
		for(auto diffPropName : DiffProps)
		{
			OutParams.Remove(diffPropName);
		}
	}
}

bool UGlobalToolsBPLibrary::RegexMatch(const FString& InSourceStr, const FString& InPattern, TArray<FString>& OutResult)
{
	const FRegexPattern MatherPatter(InPattern);
	FRegexMatcher Matcher(MatherPatter, InSourceStr);
	while (Matcher.FindNext())
	{
		OutResult.Add(Matcher.GetCaptureGroup(0));
	}

	return OutResult.Num() == 0 ? false : true;
}
