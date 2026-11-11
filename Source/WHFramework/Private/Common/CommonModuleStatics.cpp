// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/CommonModuleStatics.h"
#include "Common/LifecycleTypes.h"

#include "BlueprintGameplayTagLibrary.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "WHFrameworkCoreStatics.h"
#include "Asset/AssetModuleStatics.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Gameplay/WHGameInstance.h"
#include "Gameplay/WHGameMode.h"
#include "Gameplay/WHGameState.h"
#include "Common/CommonTypes.h"
#include "Event/Events/Common/Game/Event_GamePaused.h"
#include "Event/Events/Common/Game/Event_GameUnPaused.h"
#include "Event/Events/Common/Game/Event_GameExited.h"
#include "Gameplay/WHLocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetStringLibrary.h"
#include "Main/MainModule.h"
#include "Main/MainModuleStatics.h"
#include "SaveGame/SaveGameModule.h"
#include "Widgets/SViewport.h"
#include "Windows/WindowsPlatformApplicationMisc.h"

bool UCommonModuleStatics::IsPaused(const UObject* WorldContextObject)
{
	return WorldContextObject && WorldContextObject->GetWorld() && UGameplayStatics::IsGamePaused(WorldContextObject);
}

void UCommonModuleStatics::SetPaused(const UObject* WorldContextObject, bool bPaused)
{
	if (WorldContextObject && WorldContextObject->GetWorld()) UGameplayStatics::SetGamePaused(WorldContextObject, bPaused);
}

float UCommonModuleStatics::GetTimeScale(const UObject* WorldContextObject)
{
	return WorldContextObject && WorldContextObject->GetWorld() ? UGameplayStatics::GetGlobalTimeDilation(WorldContextObject) : 1.f;
}

float UCommonModuleStatics::GetDeltaSeconds(const UObject* WorldContextObject)
{
	return WorldContextObject && WorldContextObject->GetWorld() ? UGameplayStatics::GetWorldDeltaSeconds(WorldContextObject) : 0.f;
}

void UCommonModuleStatics::SetTimeScale(const UObject* WorldContextObject, float TimeScale)
{
	if (WorldContextObject && WorldContextObject->GetWorld()) UGameplayStatics::SetGlobalTimeDilation(WorldContextObject, TimeScale);
}

void UCommonModuleStatics::PauseGame(const UObject* WorldContextObject, EPauseMode PauseMode)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World) return;
	switch(PauseMode)
	{
		case EPauseMode::Default:
		{
			UGameplayStatics::SetGamePaused(World, true);
			break;
		}
		case EPauseMode::TimeOnly:
		{
			UGameplayStatics::SetGlobalTimeDilation(World, 0.f);
			break;
		}
		case EPauseMode::ModuleOnly:
		{
			if (AMainModule* Main = AMainModule::GetPtr(World)) Main->PauseModules();
			break;
		}
		case EPauseMode::TimeAndModule:
		{
			UGameplayStatics::SetGlobalTimeDilation(World, 0.f);
			if (AMainModule* Main = AMainModule::GetPtr(World)) Main->PauseModules();
			break;
		}
	}
	if (UEventModule* Events = UEventModule::GetPtr(World)) Events->BroadcastEvent<FEventGamePaused>(nullptr, { PauseMode });
}

void UCommonModuleStatics::UnPauseGame(const UObject* WorldContextObject, EPauseMode PauseMode)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World) return;
	switch(PauseMode)
	{
		case EPauseMode::Default:
		{
			UGameplayStatics::SetGamePaused(World, false);
			if (AMainModule* Main = AMainModule::GetPtr(World)) Main->UnPauseModules();
			break;
		}
		case EPauseMode::TimeOnly:
		{
			UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
			break;
		}
		case EPauseMode::ModuleOnly:
		{
			if (AMainModule* Main = AMainModule::GetPtr(World)) Main->UnPauseModules();
			break;
		}
		case EPauseMode::TimeAndModule:
		{
			UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
			if (AMainModule* Main = AMainModule::GetPtr(World)) Main->UnPauseModules();
			break;
		}
	}
	if (UEventModule* Events = UEventModule::GetPtr(World)) Events->BroadcastEvent<FEventGameUnPaused>(nullptr, { PauseMode });
}

void UCommonModuleStatics::QuitGame(APlayerController* InPlayer, TEnumAsByte<EQuitPreference::Type> QuitPreference, bool bIgnorePlatformRestrictions)
{
	UWorld* World = ::IsValid(InPlayer) && InPlayer->IsLocalController() ? InPlayer->GetWorld() : nullptr;
	if (!World) return;
	if (auto* Save = USaveGameModule::GetPtr(World))
	{
		Save->RequestUserQuit(InPlayer, uint8(QuitPreference.GetValue()), bIgnorePlatformRestrictions);
		return;
	}
	// 未配置保存模块的世界保留直接退出行为。
	if (UEventModule* Events = UEventModule::GetPtr(World)) Events->BroadcastEvent<FEventGameExited>(nullptr, { false });
	UKismetSystemLibrary::QuitGame(World, InPlayer, QuitPreference, bIgnorePlatformRestrictions);
}

bool UCommonModuleStatics::IsInScreenViewport(APlayerController* InPlayer, const FVector& InWorldLocation)
{
	const ULocalPlayer* LP = InPlayer && InPlayer->IsLocalController() ? InPlayer->GetLocalPlayer() : nullptr;
	if (LP && LP->ViewportClient && LP->ViewportClient->Viewport)
	{
		FSceneViewProjectionData ProjectionData;
		if (LP->GetProjectionData(LP->ViewportClient->Viewport, ProjectionData))
		{
			FMatrix const ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();
			FVector2D ScreenPosition;
			const bool bResult = FSceneView::ProjectWorldToScreen(InWorldLocation, ProjectionData.GetConstrainedViewRect(), ViewProjectionMatrix, ScreenPosition);
			const FIntRect ViewRect = ProjectionData.GetConstrainedViewRect();
			if (bResult && ScreenPosition.X >= ViewRect.Min.X && ScreenPosition.X < ViewRect.Max.X
				&& ScreenPosition.Y >= ViewRect.Min.Y && ScreenPosition.Y < ViewRect.Max.Y)
			{
				return true;
			}
		}
	}
	return false;
}

void UCommonModuleStatics::ClipboardCopy(const FString& InStr)
{
	FPlatformApplicationMisc::ClipboardCopy(*InStr);
}

void UCommonModuleStatics::ClipboardPaste(FString& OutStr)
{
	FPlatformApplicationMisc::ClipboardPaste(OutStr);
}

int32 UCommonModuleStatics::GetEnumItemNum(const UObject* WorldContextObject, const FString& InEnumName)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindObject<UEnum>(WorldContextObject, InEnumName, true))
	{
		return Enum->NumEnums() - 1;
	}
	return 0;
}

FString UCommonModuleStatics::GetEnumAuthoredNameByValue(const UObject* WorldContextObject, const FString& InEnumName, int32 InEnumValue)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindEnumByValue(WorldContextObject, InEnumName, InEnumValue, true))
	{
		return Enum->GetAuthoredNameStringByValue(InEnumValue);
	}
	return TEXT("");
}

FText UCommonModuleStatics::GetEnumDisplayNameByValue(const UObject* WorldContextObject, const FString& InEnumName, int32 InEnumValue)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindEnumByValue(WorldContextObject, InEnumName, InEnumValue, true))
	{
		FText DisplayName;
		if(FText::FindTextInLiveTable_Advanced(TEXT("UObjectDisplayNames"), Enum->GetNameByValue(InEnumValue).ToString().Replace(TEXT("::"), TEXT(".")), DisplayName))
		{
			return DisplayName;
		}
		return FText::FromString(Enum->GetAuthoredNameStringByValue(InEnumValue));
	}
	return FText::GetEmpty();
}

FText UCommonModuleStatics::GetEnumDisplayNameByAuthoredName(const UObject* WorldContextObject, const FString& InEnumName, const FString& InEnumAuthoredName)
{
	return GetEnumDisplayNameByValue(WorldContextObject, InEnumName, GetEnumValueByAuthoredName(WorldContextObject, InEnumName, InEnumAuthoredName));
}

int32 UCommonModuleStatics::GetEnumValueByAuthoredName(const UObject* WorldContextObject, const FString& InEnumName, const FString& InEnumAuthoredName)
{
	if(const UEnum* Enum = UAssetModuleStatics::FindEnumByAuthoredName(WorldContextObject, InEnumName, InEnumAuthoredName))
	{
		return Enum->GetValueByNameString(InEnumAuthoredName);
	}
	return -1;
}

FText UCommonModuleStatics::GetPropertyDisplayName(const FProperty* InProperty)
{
	FText PropertyName;
	if(!FText::FindTextInLiveTable_Advanced(TEXT("UObjectDisplayNames"), FString::Printf(TEXT("%s:%s"), *InProperty->GetOwnerStruct()->GetName(), *InProperty->GetName()), PropertyName))
	{
		PropertyName = FText::FromString(InProperty->GetName());
	}
	return PropertyName;
}

bool UCommonModuleStatics::SaveObjectDataToMemory(UObject* InObject, TArray<uint8>& OutObjectData)
{
	return FCoreStatics::SaveObjectDataToMemory(InObject, OutObjectData);
}

bool UCommonModuleStatics::LoadObjectDataFromMemory(UObject* InObject, const TArray<uint8>& InObjectData)
{
	return FCoreStatics::LoadObjectDataFromMemory(InObject, InObjectData);
}

bool UCommonModuleStatics::RegexMatch(const FString& InSourceStr, const FString& InPattern, TArray<FString>& OutResult)
{
	return FCoreStatics::RegexMatch(InSourceStr, InPattern, OutResult);
}

FString UCommonModuleStatics::BoolToString(bool InBool)
{
	return FCoreStatics::BoolToString(InBool);
}

bool UCommonModuleStatics::StringToBool(const FString& InString)
{
	return FCoreStatics::StringToBool(InString);
}

FString UCommonModuleStatics::SanitizeFloat(double InFloat, int32 InMaxDigits)
{
	return FCoreStatics::SanitizeFloat(InFloat, InMaxDigits);
}

TArray<FString> UCommonModuleStatics::NotNumberSymbols = TArray<FString>{ TEXT("."), TEXT(","), TEXT(" ") };

bool UCommonModuleStatics::TextIsNumber(const FText& InText)
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

int32 UCommonModuleStatics::TextToNumber(const FText& InText, TMap<int32, FString>& OutSymbols)
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

FText UCommonModuleStatics::NumberToText(int32 InNumber, const TMap<int32, FString>& InSymbols)
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

FName UCommonModuleStatics::TextToName(const FText& InText)
{
	return *InText.ToString();
}

FGameplayTag UCommonModuleStatics::NameToTag(const FName InName)
{
	return FGameplayTag::RequestGameplayTag(InName);
}

FName UCommonModuleStatics::TagToName(const FGameplayTag& InTag)
{
	return UBlueprintGameplayTagLibrary::GetTagName(InTag);
}

FGameplayTag UCommonModuleStatics::StringToTag(const FString& InString)
{
	return FGameplayTag::RequestGameplayTag(*InString);
}

FString UCommonModuleStatics::TagToString(const FGameplayTag& InTag)
{
	return UBlueprintGameplayTagLibrary::GetTagName(InTag).ToString();
}

int32 UCommonModuleStatics::GetTagHierarchy(const FGameplayTag& InTag)
{
	const FString TagName = TagToName(InTag).ToString();
	TArray<FString> TagNames;
	return TagName.ParseIntoArray(TagNames, TEXT("."));
}

int32 UCommonModuleStatics::GetTagIndexForContainer(const FGameplayTag& InTag, const FGameplayTagContainer& InTagContainer)
{
	return InTagContainer.GetGameplayTagArray().IndexOfByKey(InTag);
}

FGameplayTagContainer UCommonModuleStatics::GetTagChildren(const FGameplayTag& InTag)
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

FName UCommonModuleStatics::MakeLiteralNameTag(const FGameplayTag& InTag)
{
	return InTag.GetTagName();
}

FString UCommonModuleStatics::MakeLiteralStringTag(const FGameplayTag& InTag)
{
	return InTag.GetTagName().ToString();
}

bool UCommonModuleStatics::StringToJsonObject(const FString& InJsonString, TSharedPtr<FJsonObject>& OutJsonObject)
{
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(InJsonString);
    return FJsonSerializer::Deserialize(JsonReader, OutJsonObject);
}

bool UCommonModuleStatics::JsonObjectToString(const TSharedPtr<FJsonObject>& InJsonObject, FString& OutJsonString)
{
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&OutJsonString);
	return FJsonSerializer::Serialize(InJsonObject.ToSharedRef(), JsonWriter);
}

bool UCommonModuleStatics::HasMouseCapture(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
	return Widget.IsValid() && Widget->HasMouseCapture();
}

bool UCommonModuleStatics::ExecuteObjectFunc(UObject* InObject, const FName& InFuncName, void* Params)
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

bool UCommonModuleStatics::IsClassHasChildren(const UClass* InClass, EClassFlags InDisabledFlags)
{
	return !GetClassChildren(InClass, false, InDisabledFlags).IsEmpty();
}

TArray<UClass*> UCommonModuleStatics::GetClassChildren(const UClass* InClass, bool bIncludeSelf, EClassFlags InDisabledFlags)
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

UTexture2D* UCommonModuleStatics::LoadTextureFromFile(const FString& InFilePath)
{
	return FCoreStatics::LoadTextureFromFile(InFilePath);
}

void UCommonModuleStatics::SaveTextureToFile(UTexture2D* InTexture, const FString& InFilePath)
{
	FCoreStatics::SaveTextureToFile(InTexture, InFilePath);
}

UTexture2D* UCommonModuleStatics::CompositeTextures(const TArray<UTexture2D*>& InTextures, FVector2D InTexSize, UTexture2D* InTemplate)
{
	return FCoreStatics::CompositeTextures(InTextures, InTexSize, InTemplate);
}

bool UCommonModuleStatics::IsImplementedInBlueprint(const UFunction* Func)
{
	return Func && ensure(Func->GetOuter()) && Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
}

FVector2D UCommonModuleStatics::GetGeometryPosition(const FGeometry& InGeometry)
{
	return FVector2D(InGeometry.Position);
}

FVector2D UCommonModuleStatics::GetGeometryAbsolutePosition(const FGeometry& InGeometry)
{
	return FVector2D(InGeometry.AbsolutePosition);
}

FVector2D UCommonModuleStatics::GetGeometryViewportPosition(const UObject* WorldContextObject, const FGeometry& InGeometry)
{
	FVector2D PixelPosition = FVector2D::ZeroVector, ViewportPosition = FVector2D::ZeroVector;
	if (WorldContextObject) USlateBlueprintLibrary::AbsoluteToViewport(const_cast<UObject*>(WorldContextObject), InGeometry.GetAbsolutePosition(), PixelPosition, ViewportPosition);
	return ViewportPosition;
}

TArray<AActor*> UCommonModuleStatics::GetAllActorsOfDataLayer(const UObject* WorldContextObject, UDataLayerAsset* InDataLayer, TSubclassOf<AActor> InClass)
{
	if(!InClass) InClass = AActor::StaticClass();

	TArray<AActor*> ReturnValues;
	if(InDataLayer)
	{
		if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
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

TArray<AActor*> UCommonModuleStatics::GetAllActorsOfLevel(const UObject* WorldContextObject, const FName InLevelName)
{
	TArray<AActor*> ReturnValues;
	if(!InLevelName.IsNone())
	{
		if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				FString LevelName = UWorld::RemovePIEPrefix(Actor->GetPackage()->GetName());
				if (LevelName == InLevelName)
				{
					ReturnValues.Add(Actor);
				}
			}
		}
	}
	return ReturnValues;
}

UWHGameInstance* UCommonModuleStatics::GetGameInstance(const UObject* WorldContextObject, TSubclassOf<UWHGameInstance> InClass)
{
	return GetDeterminesOutputObject(Cast<UWHGameInstance>(WorldContextObject ? UGameplayStatics::GetGameInstance(WorldContextObject) : nullptr), InClass);
}

AWHGameMode* UCommonModuleStatics::GetGameMode(const UObject* WorldContextObject, TSubclassOf<AWHGameMode> InClass)
{
	return GetDeterminesOutputObject(Cast<AWHGameMode>(WorldContextObject ? UGameplayStatics::GetGameMode(WorldContextObject) : nullptr), InClass);
}

AWHGameState* UCommonModuleStatics::GetGameState(const UObject* WorldContextObject, TSubclassOf<AWHGameState> InClass)
{
	return GetDeterminesOutputObject(Cast<AWHGameState>(WorldContextObject ? UGameplayStatics::GetGameState(WorldContextObject) : nullptr), InClass);
}

AWHGameManager* UCommonModuleStatics::GetGameManagerByClass(const UObject* WorldContextObject, TSubclassOf<AWHGameManager> InClass)
{
	if (AWHGameMode* Mode = GetGameMode(WorldContextObject)) return Mode->GetManagerByClass(InClass);
	return nullptr;
}

AWHGameManager* UCommonModuleStatics::GetGameManagerByName(const UObject* WorldContextObject, FName InName)
{
	if (AWHGameMode* Mode = GetGameMode(WorldContextObject)) return Mode->GetManagerByName(InName);
	return nullptr;
}

AWHPlayerController* UCommonModuleStatics::GetPlayerController(const UObject* WorldContextObject, int32 InPlayerIndex, TSubclassOf<AWHPlayerController> InClass)
{
	return GetDeterminesOutputObject(Cast<AWHPlayerController>(WorldContextObject ? UGameplayStatics::GetPlayerController(WorldContextObject, InPlayerIndex) : nullptr), InClass);
}

AWHPlayerController* UCommonModuleStatics::GetPlayerControllerByID(const UObject* WorldContextObject, int32 InPlayerID, TSubclassOf<AWHPlayerController> InClass)
{
	return GetDeterminesOutputObject(Cast<AWHPlayerController>(WorldContextObject ? UGameplayStatics::GetPlayerControllerFromID(WorldContextObject, InPlayerID) : nullptr), InClass);
}

AWHPlayerController* UCommonModuleStatics::GetLocalPlayerController(UWHLocalPlayer* InPlayer, TSubclassOf<AWHPlayerController> InClass)
{
	return GetDeterminesOutputObject(Cast<AWHPlayerController>(InPlayer ? InPlayer->GetPlayerController(InPlayer->GetWorld()) : nullptr), InClass);
}

APawn* UCommonModuleStatics::GetPossessedPawn(APlayerController* InPlayer, TSubclassOf<APawn> InClass)
{
	return GetDeterminesOutputObject(InPlayer ? InPlayer->GetPawn().Get() : nullptr, InClass);
}

APawn* UCommonModuleStatics::GetPlayerPawn(AWHPlayerController* InPlayer, TSubclassOf<APawn> InClass)
{
	return GetDeterminesOutputObject(InPlayer ? InPlayer->GetPlayerPawn() : nullptr, InClass);
}

TArray<UWHLocalPlayer*> UCommonModuleStatics::GetLocalPlayers(const UObject* WorldContextObject)
{
	TArray<UWHLocalPlayer*> Players;
	if (const UWHGameInstance* Instance = GetGameInstance(WorldContextObject))
	{
		for (ULocalPlayer* Player : Instance->GetLocalPlayers()) if (UWHLocalPlayer* WHPlayer = Cast<UWHLocalPlayer>(Player)) Players.Add(WHPlayer);
	}
	return Players;
}

UWHLocalPlayer* UCommonModuleStatics::GetLocalPlayer(const UObject* WorldContextObject, int32 InPlayerIndex, TSubclassOf<UWHLocalPlayer> InClass)
{
	const TArray<UWHLocalPlayer*> Players = GetLocalPlayers(WorldContextObject);
	return GetDeterminesOutputObject(Players.IsValidIndex(InPlayerIndex) ? Players[InPlayerIndex] : nullptr, InClass);
}

int32 UCommonModuleStatics::GetLocalPlayerNum(const UObject* WorldContextObject)
{
	return GetLocalPlayers(WorldContextObject).Num();
}

