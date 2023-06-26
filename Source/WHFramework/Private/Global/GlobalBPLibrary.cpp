// Fill out your copyright notice in the Description page of Project Settings.


#include "Global/GlobalBPLibrary.h"

#include "Character/Base/CharacterBase.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Global/EventHandle_PauseGame.h"
#include "Event/Handle/Global/EventHandle_UnPauseGame.h"
#include "GameFramework/InputSettings.h"
#include "Gameplay/WHGameInstance.h"
#include "Gameplay/WHGameMode.h"
#include "Gameplay/WHGameState.h"
#include "Global/GlobalTypes.h"
#include "Internationalization/Regex.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetStringLibrary.h"
#include "Main/MainModuleBPLibrary.h"

bool UGlobalBPLibrary::IsPaused()
{
	return UGameplayStatics::IsGamePaused(GetWorldContext());
}

void UGlobalBPLibrary::SetPaused(bool bPaused)
{
	UGameplayStatics::SetGamePaused(GetWorldContext(), bPaused);
}

float UGlobalBPLibrary::GetTimeScale()
{
	return UGameplayStatics::GetGlobalTimeDilation(GetWorldContext());
}

void UGlobalBPLibrary::SetTimeScale(float TimeScale)
{
	return UGameplayStatics::SetGlobalTimeDilation(GetWorldContext(), TimeScale);
}

void UGlobalBPLibrary::PauseGame(EPauseMode PauseMode)
{
	switch(PauseMode)
	{
		case EPauseMode::Default:
		{
			SetPaused(true);
			break;
		}
		case EPauseMode::OnlyTime:
		{
			SetTimeScale(0.f);
			break;
		}
		case EPauseMode::OnlyModules:
		{
			UMainModuleBPLibrary::PauseAllModule();
			break;
		}
	}
	UEventModuleBPLibrary::BroadcastEvent<UEventHandle_PauseGame>(EEventNetType::Single, nullptr, { FParameter::MakeInteger((int32)PauseMode) } );
}

void UGlobalBPLibrary::UnPauseGame(EPauseMode PauseMode)
{
	switch(PauseMode)
	{
		case EPauseMode::Default:
		{
			SetPaused(false);
			break;
		}
		case EPauseMode::OnlyTime:
		{
			SetTimeScale(1.f);
			break;
		}
		case EPauseMode::OnlyModules:
		{
			UMainModuleBPLibrary::UnPauseAllModule();
			break;
		}
	}
	UEventModuleBPLibrary::BroadcastEvent<UEventHandle_UnPauseGame>(EEventNetType::Single, nullptr, { FParameter::MakeInteger((int32)PauseMode) } );
}

void UGlobalBPLibrary::QuitGame(TEnumAsByte<EQuitPreference::Type> QuitPreference, bool bIgnorePlatformRestrictions)
{
	UKismetSystemLibrary::QuitGame(GetWorldContext(), GetPlayerController<AWHPlayerController>(), QuitPreference, bIgnorePlatformRestrictions);
}

ETraceTypeQuery UGlobalBPLibrary::GetGameTraceChannel(ECollisionChannel InGameTraceType)
{
	return UEngineTypes::ConvertToTraceType(InGameTraceType);
}

int32 UGlobalBPLibrary::GetEnumItemNum(const FString& InEnumName)
{
	if(UEnum* EnumPtr = FindObject<UEnum>(nullptr, *InEnumName, true))
	{
		return EnumPtr->NumEnums();
	}
	return 0;
}

FString UGlobalBPLibrary::GetEnumValueAuthoredName(const FString& InEnumName, int32 InEnumValue)
{
	if(UEnum* EnumPtr = FindObject<UEnum>(nullptr, *InEnumName, true))
	{
		return EnumPtr->GetAuthoredNameStringByValue(InEnumValue);
	}
	return TEXT("");
}

FText UGlobalBPLibrary::GetEnumValueDisplayName(const FString& InEnumName, int32 InEnumValue)
{
	if(UEnum* EnumPtr = FindObject<UEnum>(nullptr, *InEnumName, true))
	{
		return EnumPtr->GetDisplayNameTextByValue(InEnumValue);
	}
	return FText::GetEmpty();
}

int32 UGlobalBPLibrary::GetEnumIndexByValueName(const FString& InEnumName, const FString& InValueName)
{
	if(UEnum* EnumPtr = FindObject<UEnum>(nullptr, *InEnumName, true))
	{
		return EnumPtr->GetValueByNameString(InValueName);
	}
	return -1;
}

void UGlobalBPLibrary::SaveObjectDataToMemory(UObject* InObject, TArray<uint8>& OutObjectData)
{
	if(InObject)
	{
		FMemoryWriter MemoryWriter(OutObjectData, true);
		FSaveDataArchive Ar(MemoryWriter);
		InObject->Serialize(Ar);
	}
}

void UGlobalBPLibrary::LoadObjectDataFromMemory(UObject* InObject, const TArray<uint8>& InObjectData)
{
	if(InObject && InObjectData.Num() > 0)
	{
		FMemoryReader MemoryReader(InObjectData, true);
		FSaveDataArchive Ar(MemoryReader);
		InObject->Serialize(Ar);
	}
}

void UGlobalBPLibrary::SerializeExposedParam(UObject* InObject, FParameterMap InParam, bool bParamHavePropertyType)
{
	for(auto pair: InParam.GetMap())
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
			FoundProperty->ImportText_Direct(*pair.Value, Value, InObject, PPF_None);
		}
		else
		{
			WHLog(FString::Printf(TEXT("%s : Not Found Property : %s"),*InObject->GetName(),*PropName), EDebugCategory::Default, EDebugVerbosity::Error);
		}
	}
}

void UGlobalBPLibrary::ExportExposedParam(UClass* InClass, FParameterMap& OutParams, bool bDisplayPropertyType)
{
	if(InClass)
	{
		TArray<FString> NewProps;
		TArray<FString> DiffProps;
		const FString PropClassNameSuffix = "Property",ReplaceTo = "";
		for (TFieldIterator<FProperty> PropertyIt(InClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			const bool bIsDelegate = Property->IsA<FMulticastDelegateProperty>();
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
						Property->ExportTextItem_Direct(Temp, Value, nullptr, nullptr, PPF_None);
						OutParams.Add(PropertyName,Temp);
					}
				}
			}
		}
		for(auto pair:OutParams.GetMap())
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

FString UGlobalBPLibrary::BoolToString(bool InBool)
{
	return InBool ? TEXT("true") : TEXT("false");
}

bool UGlobalBPLibrary::StringToBool(const FString& InString)
{
	return InString == TEXT("true");
}

TArray<FString> UGlobalBPLibrary::NotNumberSymbols = TArray<FString>{ TEXT("."), TEXT(","), TEXT(" ") };

bool UGlobalBPLibrary::TextIsNumber(const FText& InText)
{
	const FString TextStr = InText.ToString();
	for(auto Iter : NotNumberSymbols)
	{
		if(TextStr.Contains(Iter))
		{
			return false;
		}
	}
	return true;
}

int32 UGlobalBPLibrary::TextToNumber(const FText& InText, TMap<int32, FString>& OutSymbols)
{
	FString TextStr = InText.ToString();
	TArray<FString> TextArr = UKismetStringLibrary::GetCharacterArrayFromString(TextStr);
	OutSymbols.Empty();
	for(int32 i = TextArr.Num() - 1; i >= 0; i--)
	{
		if(NotNumberSymbols.Contains(TextArr[i]))
		{
			OutSymbols.Add(TextArr.Num() - i - 1, TextArr[i]);
		}
	}
	for(auto Iter : NotNumberSymbols)
	{
		TextStr = TextStr.Replace(*Iter, *FString(""));
	}
	return FCString::Atoi(*TextStr);
}

FText UGlobalBPLibrary::NumberToText(int32 InNumber, const TMap<int32, FString>& InSymbols)
{
	FString TextStr = FString::FromInt(InNumber);
	for(auto Iter : InSymbols)
	{
		const int32 InsertIndex = TextStr.Len() - Iter.Key;
		if(InsertIndex > 0)
		{
			TextStr.InsertAt(InsertIndex, Iter.Value);
		}
	}
	return FText::FromString(TextStr);
}

bool UGlobalBPLibrary::ParseJsonObjectFromString(const FString& InJsonString, TSharedPtr<FJsonObject>& OutJsonObject)
{
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(InJsonString);
    return FJsonSerializer::Deserialize(JsonReader, OutJsonObject);
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

bool UGlobalBPLibrary::ExecuteObjectFunc(UObject* InObject, const FName& InFuncName, void* Params)
{
	if (ensureEditor(InObject))
	{
		UFunction* Func = InObject->FindFunction(InFuncName);
		if (ensureEditorMsgf(Func, TEXT("错误的函数调用, %s 对应函数名称 : %s ,并不存在"), *InObject->GetName(), *InFuncName.ToString()))
		{
			InObject->ProcessEvent(Func, Params);
			return true;
		}
	}
	return false;
}

bool UGlobalBPLibrary::IsImplementedInBlueprint(const UFunction* Func)
{
	return Func && ensure(Func->GetOuter()) && Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
}

UObject* UGlobalBPLibrary::GetWorldContext(bool bInEditor)
{
	return AMainModule::Get(bInEditor);
}

UWHGameInstance* UGlobalBPLibrary::GetGameInstance(TSubclassOf<UWHGameInstance> InClass)
{
	return Cast<UWHGameInstance>(UGameplayStatics::GetGameInstance(GetWorldContext()));
}

AWHGameMode* UGlobalBPLibrary::GetGameMode(TSubclassOf<AWHGameMode> InClass)
{
	return Cast<AWHGameMode>(UGameplayStatics::GetGameMode(GetWorldContext()));
}

AWHGameState* UGlobalBPLibrary::GetGameState(TSubclassOf<AWHGameState> InClass)
{
	return Cast<AWHGameState>(UGameplayStatics::GetGameState(GetWorldContext()));
}

AWHPlayerController* UGlobalBPLibrary::GetPlayerController(TSubclassOf<AWHPlayerController> InClass, int32 InPlayerIndex)
{
	return Cast<AWHPlayerController>(UGameplayStatics::GetPlayerController(GetWorldContext(), InPlayerIndex));
}

AWHPlayerController* UGlobalBPLibrary::GetPlayerControllerByID(TSubclassOf<AWHPlayerController> InClass, int32 InPlayerID)
{
	return Cast<AWHPlayerController>(UGameplayStatics::GetPlayerControllerFromID(GetWorldContext(), InPlayerID));
}

AWHPlayerController* UGlobalBPLibrary::GetLocalPlayerController(TSubclassOf<AWHPlayerController> InClass)
{
	for(auto Iter = GetCurrentWorld()->GetPlayerControllerIterator(); Iter; ++Iter)
	{
		AWHPlayerController* PlayerController = Cast<AWHPlayerController>(Iter->Get());
		if(PlayerController && PlayerController->IsLocalController())
		{
			return PlayerController;
		}
	}
	return nullptr;
}

APawn* UGlobalBPLibrary::GetPlayerPawn(TSubclassOf<APawn> InClass, int32 InPlayerIndex)
{
	if(AWHPlayerController* PlayerController = GetPlayerController<AWHPlayerController>(InPlayerIndex))
	{
		return PlayerController->GetPawn();
	}
	return nullptr;
}

APawn* UGlobalBPLibrary::GetPlayerPawnByID(TSubclassOf<APawn> InClass, int32 InPlayerID)
{
	if(AWHPlayerController* PlayerController = GetPlayerControllerByID<AWHPlayerController>(InPlayerID))
	{
		return PlayerController->GetPawn();
	}
	return nullptr;
}

APawn* UGlobalBPLibrary::GetLocalPlayerPawn(TSubclassOf<APawn> InClass)
{
	if(AWHPlayerController* PlayerController = GetLocalPlayerController<AWHPlayerController>())
	{
		return PlayerController->GetPawn();
	}
	return nullptr;
}

ACharacterBase* UGlobalBPLibrary::GetPlayerCharacter(TSubclassOf<ACharacterBase> InClass, int32 InPlayerIndex)
{
	if(AWHPlayerController* PlayerController = GetPlayerController<AWHPlayerController>(InPlayerIndex))
	{
		return PlayerController->GetPlayerPawn<ACharacterBase>();
	}
	return nullptr;
}

ACharacterBase* UGlobalBPLibrary::GetPlayerCharacterByID(TSubclassOf<ACharacterBase> InClass, int32 InPlayerID)
{
	if(AWHPlayerController* PlayerController = GetPlayerControllerByID<AWHPlayerController>(InPlayerID))
	{
		return PlayerController->GetPlayerPawn<ACharacterBase>();
	}
	return nullptr;
}

ACharacterBase* UGlobalBPLibrary::GetLocalPlayerCharacter(TSubclassOf<ACharacterBase> InClass)
{
	if(AWHPlayerController* PlayerController = GetLocalPlayerController<AWHPlayerController>())
	{
		return PlayerController->GetPlayerPawn<ACharacterBase>();
	}
	return nullptr;
}
