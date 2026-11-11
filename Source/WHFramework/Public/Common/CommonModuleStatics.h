// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/LifecycleTypes.h"
#include "WHFrameworkCoreMacros.h"
#include "CoreMinimal.h"
#include "Common/CommonTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Gameplay/WHGameInstance.h"
#include "Gameplay/WHPlayerController.h"
#include "Kismet/KismetStringLibrary.h"

#include "CommonModuleStatics.generated.h"

class APawn;
class AWHPlayerController;
class AWHGameState;
class AWHGameMode;
class UWHGameInstance;
class UWHLocalPlayer;
class AWHGameManager;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCommonModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// State
	/*
	 * 当前是否为播放状态
	 */
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics")
	static bool IsPlaying() { return GIsPlaying; }
	/*
	 * 当前是否为模拟状态
	 */
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics")
	static bool IsSimulating() { return GIsSimulating; }
	/*
	 * 当前是否为编辑器状态运行
	 */
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics")
	static bool IsEditor() { return GIsEditor; }
	/*
	 * 当前是否为暂停状态
	 */
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static bool IsPaused(const UObject* WorldContextObject);
	/*
	 * 设置暂停状态
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static void SetPaused(const UObject* WorldContextObject, bool bPaused);
	/*
	 * 获取时间缩放
	 */
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static float GetTimeScale(const UObject* WorldContextObject);
	/*
	 * 获取时间缩放
	 */
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static float GetDeltaSeconds(const UObject* WorldContextObject);
	/*
	 * 设置时间缩放
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static void SetTimeScale(const UObject* WorldContextObject, float TimeScale);
	/*
	 * 暂停游戏
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static void PauseGame(const UObject* WorldContextObject, EPauseMode PauseMode = EPauseMode::Default);
	/*
	 * 恢复游戏
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static void UnPauseGame(const UObject* WorldContextObject, EPauseMode PauseMode = EPauseMode::Default);
	/*
	 * 退出游戏
	 */
	UFUNCTION(BlueprintCallable, Category = "CommonModuleStatics")
	static void QuitGame(APlayerController* InPlayer, TEnumAsByte<EQuitPreference::Type> QuitPreference, bool bIgnorePlatformRestrictions);

	//////////////////////////////////////////////////////////////////////////
	// Viewport
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics")
	static bool IsInScreenViewport(APlayerController* InPlayer, const FVector& InWorldLocation);

	//////////////////////////////////////////////////////////////////////////
	// Clipboard
	UFUNCTION(BlueprintCallable, Category = "CommonModuleStatics")
	static void ClipboardCopy(const FString& InStr);

	UFUNCTION(BlueprintCallable, Category = "CommonModuleStatics")
	static void ClipboardPaste(FString& OutStr);

	//////////////////////////////////////////////////////////////////////////
	// Phase
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics")
	static bool PhaseC(EPhase A, EPhase B)
	{
		return PHASEC(A, B);
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Enum
	/*
	* 获取枚举值显示名称
	* @param InEnumName 枚举名称
	*/
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static int32 GetEnumItemNum(const UObject* WorldContextObject, const FString& InEnumName);
	
	/*
	 * 获取枚举值变量名称
	 * @param InEnumName 枚举名称
	 * @param InEnumValue 枚举值
	 */
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FString GetEnumAuthoredNameByValue(const UObject* WorldContextObject, const FString& InEnumName, int32 InEnumValue);

	/*
	* 获取枚举值显示名称
	* @param InEnumName 枚举名称
	* @param InEnumValue 枚举值
	*/
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FText GetEnumDisplayNameByValue(const UObject* WorldContextObject, const FString& InEnumName, int32 InEnumValue);

	/*
	* 获取枚举值显示名称
	* @param InEnumName 枚举名称
	* @param InEnumValue 枚举值
	*/
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FText GetEnumDisplayNameByAuthoredName(const UObject* WorldContextObject, const FString& InEnumName, const FString& InEnumAuthoredName);

	/*
	 * 通过获取枚举值名称获取枚举值
	 * @param InEnumName 枚举名称
	 * @param InAuthoredName 枚举值名称
	 */
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static int32 GetEnumValueByAuthoredName(const UObject* WorldContextObject, const FString& InEnumName, const FString& InEnumAuthoredName);
	
	//////////////////////////////////////////////////////////////////////////
	// Enum
	/*
	* 获取属性显示名称
	* @param InProperty 属性
	*/
	static FText GetPropertyDisplayName(const FProperty* InProperty);

	//////////////////////////////////////////////////////////////////////////
	// Serialize
	/*
	* 将目标对象数据保存到内存
	* @param InObject 目标对象
	* @param OutObjectData 保存到的字节数组
	*/
	UFUNCTION(BlueprintCallable, Category = "CommonModuleStatics")
	static bool SaveObjectDataToMemory(UObject* InObject, TArray<uint8>& OutObjectData);

	/*
	* 从内存加载目标对象数据
	* @param InObject 目标对象
	* @param InObjectData 目标对象字节数组
	*/
	UFUNCTION(BlueprintCallable, Category = "CommonModuleStatics")
	static bool LoadObjectDataFromMemory(UObject* InObject, const TArray<uint8>& InObjectData);

	//////////////////////////////////////////////////////////////////////////
	// Regex
	/*
	* 正则表达式匹配字串，匹配到返回true，没匹配到返回false
	* @param InSourceStr 源字符串
	* @param InPattern 正则表达式匹配规则
	* @param OutResult 匹配到的字符串
	*/
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics")
	static bool RegexMatch(const FString& InSourceStr, const FString& InPattern, TArray<FString>& OutResult);
			
	//////////////////////////////////////////////////////////////////////////
	// String
	UFUNCTION(BlueprintPure, meta = (CompactNodeTitle = "->"), Category = "CommonModuleStatics")
	static FString BoolToString(bool InBool);

	UFUNCTION(BlueprintPure, meta = (CompactNodeTitle = "->"), Category = "CommonModuleStatics")
	static bool StringToBool(const FString& InString);
		
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics")
	static FString SanitizeFloat(double InFloat, int32 InMaxDigits = -1);

	//////////////////////////////////////////////////////////////////////////
	// Text
private:
	static TArray<FString> NotNumberSymbols;
public:
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics")
	static bool TextIsNumber(const FText& InText);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Number (Text)", CompactNodeTitle = "->", BlueprintAutocast), Category = "CommonModuleStatics")
	static int32 TextToNumber(const FText& InText, TMap<int32, FString>& OutSymbols);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Text (Number)", CompactNodeTitle = "->", BlueprintAutocast), Category = "CommonModuleStatics")
	static FText NumberToText(int32 InNumber, const TMap<int32, FString>& InSymbols);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Name (Text)", CompactNodeTitle = "->", BlueprintAutocast), Category = "CommonModuleStatics")
	static FName TextToName(const FText& InText);

	//////////////////////////////////////////////////////////////////////////
	// Tag
	UFUNCTION(BlueprintPure, meta = (CompactNodeTitle = "->"), Category = "CommonModuleStatics")
	static FGameplayTag NameToTag(const FName InName);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag", CompactNodeTitle = "->"), Category = "CommonModuleStatics")
	static FName TagToName(const FGameplayTag& InTag);

	UFUNCTION(BlueprintPure, meta = (CompactNodeTitle = "->"), Category = "CommonModuleStatics")
	static FGameplayTag StringToTag(const FString& InString);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag", CompactNodeTitle = "->"), Category = "CommonModuleStatics")
	static FString TagToString(const FGameplayTag& InTag);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"), Category = "CommonModuleStatics")
	static int32 GetTagHierarchy(const FGameplayTag& InTag);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag,InTagContainer"), Category = "CommonModuleStatics")
	static int32 GetTagIndexForContainer(const FGameplayTag& InTag, const FGameplayTagContainer& InTagContainer);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"), Category = "CommonModuleStatics")
	static FGameplayTagContainer GetTagChildren(const FGameplayTag& InTag);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"), Category = "CommonModuleStatics")
	static FName MakeLiteralNameTag(const FGameplayTag& InTag);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"), Category = "CommonModuleStatics")
	static FString MakeLiteralStringTag(const FGameplayTag& InTag);

	//////////////////////////////////////////////////////////////////////////
	// Json
	static bool StringToJsonObject(const FString& InJsonString, TSharedPtr<FJsonObject>& OutJsonObject);

	static bool JsonObjectToString(const TSharedPtr<FJsonObject>& InJsonObject, FString& OutJsonString);

	//////////////////////////////////////////////////////////////////////////
	// Input
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static bool HasMouseCapture(const UObject* WorldContextObject);

	//////////////////////////////////////////////////////////////////////////
	// Object
	static bool ExecuteObjectFunc(UObject* InObject, const FName& InFuncName, void* Params);

	//////////////////////////////////////////////////////////////////////////
	// Class
	static bool IsClassHasChildren(const UClass* InClass, EClassFlags InDisabledFlags = CLASS_None);

	static TArray<UClass*> GetClassChildren(const UClass* InClass, bool bIncludeSelf = false, EClassFlags InDisabledFlags = CLASS_None);

	//////////////////////////////////////////////////////////////////////////
	// Texture
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics")
	static UTexture2D* LoadTextureFromFile(const FString& InFilePath);

	UFUNCTION(BlueprintCallable, Category = "CommonModuleStatics")
	static void SaveTextureToFile(UTexture2D* InTexture, const FString& InFilePath);

	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics")
	static UTexture2D* CompositeTextures(const TArray<UTexture2D*>& InTextures, FVector2D InTexSize, UTexture2D* InTemplate = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Blueprint
	static bool IsImplementedInBlueprint(const UFunction* Func);

	//////////////////////////////////////////////////////////////////////////
	// Widget
	UFUNCTION(BlueprintPure, DisplayName = "Get Position", Category = "CommonModuleStatics")
	static FVector2D GetGeometryPosition(const FGeometry& InGeometry);
	
	UFUNCTION(BlueprintPure, DisplayName = "Get Absolute Position", Category = "CommonModuleStatics")
	static FVector2D GetGeometryAbsolutePosition(const FGeometry& InGeometry);
	
	UFUNCTION(BlueprintPure, DisplayName = "Get Viewport Position", meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static FVector2D GetGeometryViewportPosition(const UObject* WorldContextObject, const FGeometry& InGeometry);

	//////////////////////////////////////////////////////////////////////////
	// Gameplay
	UFUNCTION(BlueprintPure, Category = "CommonModuleStatics")
	static UWorld* GetWorldFromObjectExisted(const UObject* InObject)
	{
		return InObject ? InObject->GetWorld() : nullptr;
	}

	template<class T>
	static TArray<T*> GetAllActorsOfDataLayer(const UObject* WorldContextObject, UDataLayerAsset* InDataLayer)
	{
		TArray<T*> ReturnValues;
		for(auto Iter : GetAllActorsOfDataLayer(WorldContextObject, InDataLayer, T::StaticClass()))
		{
			if(T* Actor = Cast<T>(Iter))
			{
				ReturnValues.Add(Actor);
			}
		}
		return ReturnValues;
	}
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "CommonModuleStatics")
	static TArray<AActor*> GetAllActorsOfDataLayer(const UObject* WorldContextObject, UDataLayerAsset* InDataLayer, TSubclassOf<AActor> InClass = nullptr);
	
	template<class T>
	static TArray<T*> GetAllActorsOfLevel(const UObject* WorldContextObject, const FName InLevelName)
	{
		TArray<T*> ReturnValues;
		for(auto Iter : GetAllActorsOfLevel(WorldContextObject, InLevelName))
		{
			if(T* Actor = Cast<T>(Iter))
			{
				ReturnValues.Add(Actor);
			}
		}
		return ReturnValues;
	}
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static TArray<AActor*> GetAllActorsOfLevel(const UObject* WorldContextObject, const FName InLevelName);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "CommonModuleStatics")
	static UWHGameInstance* GetGameInstance(const UObject* WorldContextObject, TSubclassOf<UWHGameInstance> InClass = nullptr);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "CommonModuleStatics")
	static AWHGameMode* GetGameMode(const UObject* WorldContextObject, TSubclassOf<AWHGameMode> InClass = nullptr);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "CommonModuleStatics")
	static AWHGameState* GetGameState(const UObject* WorldContextObject, TSubclassOf<AWHGameState> InClass = nullptr);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "CommonModuleStatics")
	static AWHGameManager* GetGameManagerByClass(const UObject* WorldContextObject, TSubclassOf<AWHGameManager> InClass);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static AWHGameManager* GetGameManagerByName(const UObject* WorldContextObject, FName InName);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "CommonModuleStatics")
	static AWHPlayerController* GetPlayerController(const UObject* WorldContextObject, int32 InPlayerIndex, TSubclassOf<AWHPlayerController> InClass = nullptr);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "CommonModuleStatics")
	static AWHPlayerController* GetPlayerControllerByID(const UObject* WorldContextObject, int32 InPlayerID, TSubclassOf<AWHPlayerController> InClass = nullptr);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonModuleStatics")
	static AWHPlayerController* GetLocalPlayerController(UWHLocalPlayer* InPlayer, TSubclassOf<AWHPlayerController> InClass = nullptr);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonModuleStatics")
	static APawn* GetPossessedPawn(APlayerController* InPlayer, TSubclassOf<APawn> InClass = nullptr);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonModuleStatics")
	static APawn* GetPlayerPawn(AWHPlayerController* InPlayer, TSubclassOf<APawn> InClass = nullptr);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static TArray<UWHLocalPlayer*> GetLocalPlayers(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "CommonModuleStatics")
	static UWHLocalPlayer* GetLocalPlayer(const UObject* WorldContextObject, int32 InPlayerIndex, TSubclassOf<UWHLocalPlayer> InClass = nullptr);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "CommonModuleStatics")
	static int32 GetLocalPlayerNum(const UObject* WorldContextObject);


};
