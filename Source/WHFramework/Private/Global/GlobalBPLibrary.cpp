// Fill out your copyright notice in the Description page of Project Settings.


#include "Global/GlobalBPLibrary.h"

#include "Debug/DebugModuleTypes.h"
#include "GameFramework/InputSettings.h"
#include "Global/GlobalTypes.h"
#include "Kismet/GameplayStatics.h"

FString UGlobalBPLibrary::GetEnumValueAuthoredName(const FString& InEnumName, int32 InEnumValue)
{
	if(UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, *InEnumName, true))
	{
		return EnumPtr->GetAuthoredNameStringByValue(InEnumValue);
	}
	return TEXT("");
}

FText UGlobalBPLibrary::GetEnumValueDisplayName(const FString& InEnumName, int32 InEnumValue)
{
	if(UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, *InEnumName, true))
	{
		return EnumPtr->GetDisplayNameTextByValue(InEnumValue);
	}
	return FText::GetEmpty();
}

void UGlobalBPLibrary::SaveObjectToMemory(UObject* InObject, TArray<uint8>& OutObjectData)
{
	if(InObject)
	{
		FMemoryWriter MemoryWriter(OutObjectData, true);
		FSaveDataArchive Ar(MemoryWriter);
		InObject->Serialize(Ar);
	}
}

void UGlobalBPLibrary::LoadObjectFromMemory(UObject* InObject, const TArray<uint8>& InObjectData)
{
	if(InObject && InObjectData.Num() > 0)
	{
		FMemoryReader MemoryReader(InObjectData, true);
		FSaveDataArchive Ar(MemoryReader);
		InObject->Serialize(Ar);
	}
}

void UGlobalBPLibrary::SerializeExposedParam(UObject* InObject, const TMap<FString, FString>& InParam, bool bParamHavePropertyType)
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

void UGlobalBPLibrary::ExportExposedParam(UClass* InClass, TMap<FString, FString>& OutParams, bool bDisplayPropertyType)
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
				if(PropertyName != "Param" && PropertyName != "Instigator")
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

bool UGlobalBPLibrary::RegexMatch(const FString& InSourceStr, const FString& InPattern, TArray<FString>& OutResult)
{
	const FRegexPattern MatherPatter(InPattern);
	FRegexMatcher Matcher(MatherPatter, InSourceStr);
	while (Matcher.FindNext())
	{
		OutResult.Add(Matcher.GetCaptureGroup(0));
	}

	return OutResult.Num() == 0 ? false : true;
}

FText UGlobalBPLibrary::GetInputActionKeyCodeByName(const FString& InInputActionName)
{
	TArray<FInputActionKeyMapping> KeyMappings;
	UInputSettings::GetInputSettings()->GetActionMappingByName(*InInputActionName, KeyMappings);
	for(auto Iter : KeyMappings)
	{
		return FText::FromString(Iter.Key.GetFName().ToString());
	}
	return FText::GetEmpty();
}

UGameInstance* UGlobalBPLibrary::K2_GetGameInstance(const UObject* InWorldContext, TSubclassOf<UGameInstance> InClass)
{
	return UGameplayStatics::GetGameInstance(InWorldContext);
}

AGameModeBase* UGlobalBPLibrary::K2_GetGameMode(const UObject* InWorldContext, TSubclassOf<AGameModeBase> InClass)
{
	return UGameplayStatics::GetGameMode(InWorldContext);
}

AGameStateBase* UGlobalBPLibrary::K2_GetGameState(const UObject* InWorldContext, TSubclassOf<AGameStateBase> InClass)
{
	return UGameplayStatics::GetGameState(InWorldContext);
}

APlayerController* UGlobalBPLibrary::K2_GetPlayerController(const UObject* InWorldContext, TSubclassOf<APlayerController> InClass, int32 InPlayerIndex)
{
	return UGameplayStatics::GetPlayerController(InWorldContext, InPlayerIndex);
}

APlayerController* UGlobalBPLibrary::K2_GetPlayerControllerByID(const UObject* InWorldContext, TSubclassOf<APlayerController> InClass, int32 InPlayerID)
{
	return UGameplayStatics::GetPlayerControllerFromID(InWorldContext, InPlayerID);
}

APlayerController* UGlobalBPLibrary::K2_GetLocalPlayerController(const UObject* InWorldContext, TSubclassOf<APlayerController> InClass)
{
	if(UWorld* World = GEngine->GetWorldFromContextObject(InWorldContext, EGetWorldErrorMode::LogAndReturnNull))
	{
		for(auto Iter = World->GetPlayerControllerIterator(); Iter; ++Iter)
		{
			APlayerController* PlayerController = Iter->Get();
			if(PlayerController->IsLocalController())
			{
				return PlayerController;
			}
		}
	}
	return nullptr;
}

APawn* UGlobalBPLibrary::K2_GetPlayerPawn(const UObject* InWorldContext, TSubclassOf<APawn> InClass, int32 InPlayerIndex)
{
	return UGameplayStatics::GetPlayerPawn(InWorldContext, InPlayerIndex);
}

APawn* UGlobalBPLibrary::K2_GetPlayerPawnByID(const UObject* InWorldContext, TSubclassOf<APawn> InClass, int32 InPlayerID)
{
	if(APlayerController* PlayerController = GetPlayerControllerByID<APlayerController>(InWorldContext, InPlayerID))
	{
		return PlayerController->GetPawn();
	}
	return nullptr;
}

APawn* UGlobalBPLibrary::K2_GetLocalPlayerPawn(const UObject* InWorldContext, TSubclassOf<APawn> InClass)
{
	if(APlayerController* PlayerController = GetLocalPlayerController<APlayerController>(InWorldContext))
	{
		return PlayerController->GetPawn();
	}
	return nullptr;
}

ACharacter* UGlobalBPLibrary::K2_GetPlayerCharacter(const UObject* InWorldContext, TSubclassOf<ACharacter> InClass, int32 InPlayerIndex)
{
	return UGameplayStatics::GetPlayerCharacter(InWorldContext, InPlayerIndex);
}

ACharacter* UGlobalBPLibrary::K2_GetPlayerCharacterByID(const UObject* InWorldContext, TSubclassOf<ACharacter> InClass, int32 InPlayerID)
{
	if(APlayerController* PlayerController = GetPlayerControllerByID<APlayerController>(InWorldContext, InPlayerID))
	{
		return PlayerController->GetCharacter();
	}
	return nullptr;
}

ACharacter* UGlobalBPLibrary::K2_GetLocalPlayerCharacter(const UObject* InWorldContext, TSubclassOf<ACharacter> InClass)
{
	if(APlayerController* PlayerController = GetLocalPlayerController<APlayerController>(InWorldContext))
	{
		return PlayerController->GetCharacter();
	}
	return nullptr;
}
