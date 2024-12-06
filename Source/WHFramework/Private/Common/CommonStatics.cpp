// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/CommonStatics.h"

#include "BlueprintGameplayTagLibrary.h"
#include "EngineUtils.h"
#include "WHFrameworkCoreStatics.h"
#include "Asset/AssetModuleStatics.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Gameplay/WHGameInstance.h"
#include "Gameplay/WHGameMode.h"
#include "Gameplay/WHGameState.h"
#include "Common/CommonTypes.h"
#include "Event/Handle/Common/Game/EventHandle_GamePaused.h"
#include "Event/Handle/Common/Game/EventHandle_GameUnPaused.h"
#include "Gameplay/WHLocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetStringLibrary.h"
#include "Main/MainModule.h"
#include "Main/MainModuleStatics.h"
#include "Widgets/SViewport.h"

bool UCommonStatics::IsPaused()
{
	return UGameplayStatics::IsGamePaused(GetWorldContext());
}

void UCommonStatics::SetPaused(bool bPaused)
{
	UGameplayStatics::SetGamePaused(GetWorldContext(), bPaused);
}

float UCommonStatics::GetTimeScale()
{
	return UGameplayStatics::GetGlobalTimeDilation(GetWorldContext());
}

float UCommonStatics::GetDeltaSeconds()
{
	return UGameplayStatics::GetWorldDeltaSeconds(GetWorldContext());
}

void UCommonStatics::SetTimeScale(float TimeScale)
{
	UGameplayStatics::SetGlobalTimeDilation(GetWorldContext(), TimeScale);
}

void UCommonStatics::PauseGame(EPauseMode PauseMode)
{
	switch(PauseMode)
	{
		case EPauseMode::Default:
		{
			SetPaused(true);
			break;
		}
		case EPauseMode::TimeOnly:
		{
			SetTimeScale(0.f);
			break;
		}
		case EPauseMode::ModuleOnly:
		{
			UMainModuleStatics::PauseAllModule();
			break;
		}
		case EPauseMode::TimeAndModule:
		{
			SetTimeScale(0.f);
			UMainModuleStatics::PauseAllModule();
			break;
		}
	}
	UEventModuleStatics::BroadcastEvent<UEventHandle_GamePaused>(nullptr, { (int32)PauseMode } );
}

void UCommonStatics::UnPauseGame(EPauseMode PauseMode)
{
	switch(PauseMode)
	{
		case EPauseMode::Default:
		{
			SetPaused(false);
			UMainModuleStatics::UnPauseAllModule();
			break;
		}
		case EPauseMode::TimeOnly:
		{
			SetTimeScale(1.f);
			break;
		}
		case EPauseMode::ModuleOnly:
		{
			UMainModuleStatics::UnPauseAllModule();
			break;
		}
		case EPauseMode::TimeAndModule:
		{
			SetTimeScale(1.f);
			UMainModuleStatics::UnPauseAllModule();
			break;
		}
	}
	UEventModuleStatics::BroadcastEvent<UEventHandle_GameUnPaused>(nullptr, { (int32)PauseMode } );
}

void UCommonStatics::QuitGame(TEnumAsByte<EQuitPreference::Type> QuitPreference, bool bIgnorePlatformRestrictions)
{
	UKismetSystemLibrary::QuitGame(GetWorldContext(), GetPlayerController<AWHPlayerController>(), QuitPreference, bIgnorePlatformRestrictions);
}

bool UCommonStatics::IsInScreenViewport(const FVector& InWorldLocation)
{
	const UWHLocalPlayer* LP = GetLocalPlayer();
	if (LP && LP->ViewportClient)
	{
		// get the projection data
		FSceneViewProjectionData ProjectionData;
		if (LP->GetProjectionData(LP->ViewportClient->Viewport, ProjectionData))
		{
			FMatrix const ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();
			FVector2D ScreenPosition;
			const bool bResult = FSceneView::ProjectWorldToScreen(InWorldLocation, ProjectionData.GetConstrainedViewRect(), ViewProjectionMatrix, ScreenPosition);
			if (bResult && ScreenPosition.X > ProjectionData.GetViewRect().Min.X && ScreenPosition.X < ProjectionData.GetViewRect().Max.X
				&& ScreenPosition.Y > ProjectionData.GetViewRect().Min.Y && ScreenPosition.Y < ProjectionData.GetViewRect().Max.Y)
			{
				return true;
			}
		}
	}
	return false;
}

int32 UCommonStatics::GetEnumItemNum(const FString& InEnumName)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindObject<UEnum>(InEnumName, true))
	{
		return Enum->NumEnums() - 1;
	}
	return 0;
}

FString UCommonStatics::GetEnumValueAuthoredName(const FString& InEnumName, int32 InEnumValue)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindEnumByValue(InEnumName, InEnumValue, true))
	{
		return Enum->GetAuthoredNameStringByValue(InEnumValue);
	}
	return TEXT("");
}

FText UCommonStatics::GetEnumValueDisplayName(const FString& InEnumName, int32 InEnumValue)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindEnumByValue(InEnumName, InEnumValue, true))
	{
		FText DisplayName;
		if(FText::FindText(TEXT("UObjectDisplayNames"), Enum->GetNameByValue(InEnumValue).ToString().Replace(TEXT("::"), TEXT(".")), DisplayName))
		{
			return DisplayName;
		}
		return FText::FromString(Enum->GetAuthoredNameStringByValue(InEnumValue));
	}
	return FText::GetEmpty();
}

int32 UCommonStatics::GetEnumValueByAuthoredName(const FString& InEnumName, const FString& InEnumAuthoredName)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindEnumByAuthoredName(InEnumName, InEnumAuthoredName))
	{
		return Enum->GetValueByNameString(InEnumAuthoredName);
	}
	return -1;
}

FText UCommonStatics::GetPropertyDisplayName(const FProperty* InProperty)
{
	FText PropertyName;
	if(!FText::FindText(TEXT("UObjectDisplayNames"), FString::Printf(TEXT("%s:%s"), *InProperty->GetOwnerStruct()->GetName(), *InProperty->GetName()), PropertyName))
	{
		PropertyName = FText::FromString(InProperty->GetName());
	}
	return PropertyName;
}

void UCommonStatics::SaveObjectDataToMemory(UObject* InObject, TArray<uint8>& OutObjectData)
{
	FCoreStatics::SaveObjectDataToMemory(InObject, OutObjectData);
}

void UCommonStatics::LoadObjectDataFromMemory(UObject* InObject, const TArray<uint8>& InObjectData)
{
	FCoreStatics::LoadObjectDataFromMemory(InObject, InObjectData);
}

bool UCommonStatics::RegexMatch(const FString& InSourceStr, const FString& InPattern, TArray<FString>& OutResult)
{
	return FCoreStatics::RegexMatch(InSourceStr, InPattern, OutResult);
}

FString UCommonStatics::BoolToString(bool InBool)
{
	return FCoreStatics::BoolToString(InBool);
}

bool UCommonStatics::StringToBool(const FString& InString)
{
	return FCoreStatics::StringToBool(InString);
}

TArray<FString> UCommonStatics::NotNumberSymbols = TArray<FString>{ TEXT("."), TEXT(","), TEXT(" ") };

bool UCommonStatics::TextIsNumber(const FText& InText)
{
	const FString TextStr = InText.ToString();
	for(auto& Iter : NotNumberSymbols)
	{
		if(TextStr.Contains(Iter))
		{
			return false;
		}
	}
	return true;
}

int32 UCommonStatics::TextToNumber(const FText& InText, TMap<int32, FString>& OutSymbols)
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
	for(auto& Iter : NotNumberSymbols)
	{
		TextStr = TextStr.Replace(*Iter, *FString(""));
	}
	return FCString::Atoi(*TextStr);
}

FText UCommonStatics::NumberToText(int32 InNumber, const TMap<int32, FString>& InSymbols)
{
	FString TextStr = FString::FromInt(InNumber);
	for(auto& Iter : InSymbols)
	{
		const int32 InsertIndex = TextStr.Len() - Iter.Key;
		if(InsertIndex > 0)
		{
			TextStr.InsertAt(InsertIndex, Iter.Value);
		}
	}
	return FText::FromString(TextStr);
}

FName UCommonStatics::TextToName(const FText& InText)
{
	return *InText.ToString();
}

FGameplayTag UCommonStatics::NameToTag(const FName InName)
{
	return FGameplayTag::RequestGameplayTag(InName);
}

FName UCommonStatics::TagToName(const FGameplayTag& InTag)
{
	return UBlueprintGameplayTagLibrary::GetTagName(InTag);
}

FGameplayTag UCommonStatics::StringToTag(const FString& InString)
{
	return FGameplayTag::RequestGameplayTag(*InString);
}

FString UCommonStatics::TagToString(const FGameplayTag& InTag)
{
	return UBlueprintGameplayTagLibrary::GetTagName(InTag).ToString();
}

int32 UCommonStatics::GetTagHierarchy(const FGameplayTag& InTag)
{
	const FString TagName = TagToName(InTag).ToString();
	TArray<FString> TagNames;
	return TagName.ParseIntoArray(TagNames, TEXT("."));
}

int32 UCommonStatics::GetTagIndexForContainer(const FGameplayTag& InTag, const FGameplayTagContainer& InTagContainer)
{
	return InTagContainer.GetGameplayTagArray().IndexOfByKey(InTag);
}

FGameplayTagContainer UCommonStatics::GetTagChildren(const FGameplayTag& InTag)
{
	FGameplayTagContainer ReturnValue;
	if(const TSharedPtr<FGameplayTagNode> TagNode = UGameplayTagsManager::Get().FindTagNode(InTag.GetTagName()))
	{
		for(const auto Iter : TagNode->GetChildTagNodes())
		{
			ReturnValue.AddTag(Iter->GetCompleteTag());
		}
	}
	return ReturnValue;
}

FName UCommonStatics::MakeLiteralNameTag(const FGameplayTag& InTag)
{
	return InTag.GetTagName();
}

FString UCommonStatics::MakeLiteralStringTag(const FGameplayTag& InTag)
{
	return InTag.GetTagName().ToString();
}

bool UCommonStatics::StringToJsonObject(const FString& InJsonString, TSharedPtr<FJsonObject>& OutJsonObject)
{
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(InJsonString);
    return FJsonSerializer::Deserialize(JsonReader, OutJsonObject);
}

bool UCommonStatics::JsonObjectToString(const TSharedPtr<FJsonObject>& InJsonObject, FString& OutJsonString)
{
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&OutJsonString);
	return FJsonSerializer::Serialize(InJsonObject.ToSharedRef(), JsonWriter);
}

bool UCommonStatics::HasMouseCapture()
{
	return GetCurrentWorld()->GetGameViewport()->GetGameViewportWidget()->HasMouseCapture();
}

bool UCommonStatics::ExecuteObjectFunc(UObject* InObject, const FName& InFuncName, void* Params)
{
	if (ensureEditor(InObject))
	{
		UFunction* Func = InObject->FindFunction(InFuncName);
		if (ensureEditorMsgf(Func, FString::Printf(TEXT("错误的函数调用, %s 对应函数名称 : %s ,并不存在"), *InObject->GetName(), *InFuncName.ToString()), EDC_Default, EDV_Error))
		{
			InObject->ProcessEvent(Func, Params);
			return true;
		}
	}
	return false;
}

bool UCommonStatics::IsClassHasChildren(const UClass* InClass, EClassFlags InDisabledFlags)
{
	return false;
	return !GetClassChildren(InClass, false, InDisabledFlags).IsEmpty();
}

TArray<UClass*> UCommonStatics::GetClassChildren(const UClass* InClass, bool bIncludeSelf, EClassFlags InDisabledFlags)
{
	TArray<UClass*> Children;
	GetDerivedClasses(InClass, Children);
	TArray<UClass*> ReturnValues;
	for (const auto Iter : Children)
	{
		if (!Iter->HasAnyClassFlags(InDisabledFlags) && (bIncludeSelf || Iter != InClass))
		{
			ReturnValues.Add(Iter);
		}
	}
	return ReturnValues;
}

UTexture2D* UCommonStatics::LoadTextureFromFile(const FString& InFilePath)
{
	return FCoreStatics::LoadTextureFromFile(InFilePath);
}

void UCommonStatics::SaveTextureToFile(UTexture2D* InTexture, const FString& InFilePath)
{
	FCoreStatics::SaveTextureToFile(InTexture, InFilePath);
}

UTexture2D* UCommonStatics::CompositeTextures(const TArray<UTexture2D*>& InTextures, FVector2D InTexSize, UTexture2D* InTemplate)
{
	return FCoreStatics::CompositeTextures(InTextures, InTexSize, InTemplate);
}

bool UCommonStatics::IsImplementedInBlueprint(const UFunction* Func)
{
	return Func && ensure(Func->GetOuter()) && Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
}

FVector2D UCommonStatics::GetGeometryPosition(const FGeometry& InGeometry)
{
	return FVector2D(InGeometry.Position);
}

FVector2D UCommonStatics::GetGeometryAbsolutePosition(const FGeometry& InGeometry)
{
	return FVector2D(InGeometry.AbsolutePosition);
}

FVector2D UCommonStatics::GetGeometryViewportPosition(const FGeometry& InGeometry)
{
	FVector2D PixelPosition, ViewportPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(const_cast<UObject*>(GetWorldContext()), InGeometry.GetAbsolutePosition(), PixelPosition, ViewportPosition);
	return ViewportPosition;
}

const UObject* UCommonStatics::GetWorldContext(bool bInEditor)
{
	return AMainModule::GetPtr(bInEditor);
}

UObject* UCommonStatics::GetMutableWorldContext(bool bInEditor)
{
	return AMainModule::GetPtr(bInEditor);
}

TArray<AActor*> UCommonStatics::GetAllActorsOfDataLayer(UDataLayerAsset* InDataLayer, TSubclassOf<AActor> InClass)
{
	if(!InClass) InClass = AActor::StaticClass();
	
	TArray<AActor*> ReturnValues;
	if(InDataLayer)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(GetWorldContext(), EGetWorldErrorMode::LogAndReturnNull))
		{
			for (TActorIterator<AActor> It(World, InClass); It; ++It)
			{
				AActor* Actor = *It;
				if (Actor->ContainsDataLayer(InDataLayer))
				{
					ReturnValues.Add(Actor);
				}
			}
		}
	}
	return ReturnValues;
}

UWHGameInstance* UCommonStatics::GetGameInstance(TSubclassOf<UWHGameInstance> InClass)
{
	return GetDeterminesOutputObject(Cast<UWHGameInstance>(UGameplayStatics::GetGameInstance(GetWorldContext())), InClass);
}

AWHGameMode* UCommonStatics::GetGameMode(TSubclassOf<AWHGameMode> InClass)
{
	return GetDeterminesOutputObject(Cast<AWHGameMode>(UGameplayStatics::GetGameMode(GetWorldContext())), InClass);
}

AWHGameState* UCommonStatics::GetGameState(TSubclassOf<AWHGameState> InClass)
{
	return GetDeterminesOutputObject(Cast<AWHGameState>(UGameplayStatics::GetGameState(GetWorldContext())), InClass);
}

AWHPlayerController* UCommonStatics::GetPlayerController(int32 InPlayerIndex, TSubclassOf<AWHPlayerController> InClass)
{
	return GetDeterminesOutputObject(Cast<AWHPlayerController>(UGameplayStatics::GetPlayerController(GetWorldContext(), InPlayerIndex)), InClass);
}

AWHPlayerController* UCommonStatics::GetPlayerControllerByID(int32 InPlayerID, TSubclassOf<AWHPlayerController> InClass)
{
	return GetDeterminesOutputObject(Cast<AWHPlayerController>(UGameplayStatics::GetPlayerControllerFromID(GetWorldContext(), InPlayerID)), InClass);
}

AWHPlayerController* UCommonStatics::GetLocalPlayerController(int32 InPlayerIndex, TSubclassOf<AWHPlayerController> InClass)
{
	TArray<UWHLocalPlayer*> LocalPlayers = GetLocalPlayers();
	if(LocalPlayers.IsValidIndex(InPlayerIndex))
	{
		return GetDeterminesOutputObject(Cast<AWHPlayerController>(LocalPlayers[InPlayerIndex]->GetPlayerController(GetCurrentWorld())), InClass);
	}
	return nullptr;
}

APawn* UCommonStatics::GetPossessedPawn(int32 InPlayerIndex, TSubclassOf<APawn> InClass)
{
	if(AWHPlayerController* PlayerController = GetPlayerController<AWHPlayerController>(InPlayerIndex))
	{
		return GetDeterminesOutputObject(PlayerController->GetPawn(), InClass);
	}
	return nullptr;
}

APawn* UCommonStatics::GetPossessedPawnByID(int32 InPlayerID, TSubclassOf<APawn> InClass)
{
	if(AWHPlayerController* PlayerController = GetPlayerControllerByID<AWHPlayerController>(InPlayerID))
	{
		return GetDeterminesOutputObject(PlayerController->GetPawn(), InClass);
	}
	return nullptr;
}

APawn* UCommonStatics::GetLocalPossessedPawn(int32 InPlayerIndex, TSubclassOf<APawn> InClass)
{
	if(AWHPlayerController* PlayerController = GetLocalPlayerController<AWHPlayerController>())
	{
		return GetDeterminesOutputObject(PlayerController->GetPawn(), InClass);
	}
	return nullptr;
}

APawn* UCommonStatics::GetPlayerPawn(int32 InPlayerIndex, TSubclassOf<APawn> InClass)
{
	if(AWHPlayerController* PlayerController = GetPlayerController<AWHPlayerController>(InPlayerIndex))
	{
		return GetDeterminesOutputObject(PlayerController->GetPlayerPawn(), InClass);
	}
	return nullptr;
}

APawn* UCommonStatics::GetPlayerPawnByID(int32 InPlayerID, TSubclassOf<APawn> InClass)
{
	if(AWHPlayerController* PlayerController = GetPlayerControllerByID<AWHPlayerController>(InPlayerID))
	{
		return GetDeterminesOutputObject(PlayerController->GetPlayerPawn(), InClass);
	}
	return nullptr;
}

APawn* UCommonStatics::GetLocalPlayerPawn(TSubclassOf<APawn> InClass)
{
	if(AWHPlayerController* PlayerController = GetLocalPlayerController<AWHPlayerController>())
	{
		return GetDeterminesOutputObject(PlayerController->GetPlayerPawn(), InClass);
	}
	return nullptr;
}

TArray<UWHLocalPlayer*> UCommonStatics::GetLocalPlayers()
{
	TArray<UWHLocalPlayer*> LocalPlayers;
	for(auto Iter : GetGameInstance()->GetLocalPlayers())
	{
		if(UWHLocalPlayer* LocalPlayer = Cast<UWHLocalPlayer>(Iter))
		{
			LocalPlayers.Add(LocalPlayer);
		}
	}
	return LocalPlayers;
}

UWHLocalPlayer* UCommonStatics::GetLocalPlayer(int32 InPlayerIndex, TSubclassOf<UWHLocalPlayer> InClass)
{
	TArray<UWHLocalPlayer*> LocalPlayers = GetLocalPlayers();
	if(LocalPlayers.IsValidIndex(InPlayerIndex))
	{
		return GetDeterminesOutputObject(LocalPlayers[InPlayerIndex], InClass);
	}
	return nullptr;
}

int32 UCommonStatics::GetLocalPlayerNum()
{
	return GetLocalPlayers().Num();
}
