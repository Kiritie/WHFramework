// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Gameplay/WHGameInstance.h"
#include "Gameplay/WHPlayerController.h"
#include "Kismet/KismetStringLibrary.h"

#include "CommonStatics.generated.h"

class APawn;
class AWHPlayerController;
class AWHGameState;
class AWHGameMode;
class UWHGameInstance;
class UWHLocalPlayer;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCommonStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// State
	/*
	 * 当前是否为播放状态
	 */
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static bool IsPlaying() { return GIsPlaying; }
	/*
	 * 当前是否为模拟状态
	 */
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static bool IsSimulating() { return GIsSimulating; }
	/*
	 * 当前是否为编辑器状态运行
	 */
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static bool IsEditor() { return GIsEditor; }
	/*
	 * 当前是否为暂停状态
	 */
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static bool IsPaused();
	/*
	 * 设置暂停状态
	 */
	UFUNCTION(BlueprintCallable, Category = "CommonStatics")
	static void SetPaused(bool bPaused);
	/*
	 * 获取时间缩放
	 */
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static float GetTimeScale();
	/*
	 * 获取时间缩放
	 */
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static float GetDeltaSeconds();
	/*
	 * 设置时间缩放
	 */
	UFUNCTION(BlueprintCallable, Category = "CommonStatics")
	static void SetTimeScale(float TimeScale);
	/*
	 * 暂停游戏
	 */
	UFUNCTION(BlueprintCallable, Category = "CommonStatics")
	static void PauseGame(EPauseMode PauseMode = EPauseMode::Default);
	/*
	 * 恢复游戏
	 */
	UFUNCTION(BlueprintCallable, Category = "CommonStatics")
	static void UnPauseGame(EPauseMode PauseMode = EPauseMode::Default);
	/*
	 * 退出游戏
	 */
	UFUNCTION(BlueprintCallable, Category = "CommonStatics")
	static void QuitGame(TEnumAsByte<EQuitPreference::Type> QuitPreference, bool bIgnorePlatformRestrictions);

	//////////////////////////////////////////////////////////////////////////
	// Viewport
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static bool IsInScreenViewport(const FVector& InWorldLocation);

	//////////////////////////////////////////////////////////////////////////
	// Phase
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
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
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static int32 GetEnumItemNum(const FString& InEnumName);
	
	/*
	 * 获取枚举值变量名称
	 * @param InEnumName 枚举名称
	 * @param InEnumValue 枚举值
	 */
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static FString GetEnumValueAuthoredName(const FString& InEnumName, int32 InEnumValue);

	/*
	* 获取枚举值显示名称
	* @param InEnumName 枚举名称
	* @param InEnumValue 枚举值
	*/
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static FText GetEnumValueDisplayName(const FString& InEnumName, int32 InEnumValue);

	/*
	 * 通过获取枚举值名称获取枚举值
	 * @param InEnumName 枚举名称
	 * @param InAuthoredName 枚举值名称
	 */
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static int32 GetEnumValueByAuthoredName(const FString& InEnumName, const FString& InEnumAuthoredName);
	
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
	UFUNCTION(BlueprintCallable, Category = "CommonStatics")
	static void SaveObjectDataToMemory(UObject* InObject, TArray<uint8>& OutObjectData);

	/*
	* 从内存加载目标对象数据
	* @param InObject 目标对象
	* @param InObjectData 目标对象字节数组
	*/
	UFUNCTION(BlueprintCallable, Category = "CommonStatics")
	static void LoadObjectDataFromMemory(UObject* InObject, const TArray<uint8>& InObjectData);

	//////////////////////////////////////////////////////////////////////////
	// Regex
	/*
	* 正则表达式匹配字串，匹配到返回true，没匹配到返回false
	* @param InSourceStr 源字符串
	* @param InPattern 正则表达式匹配规则
	* @param OutResult 匹配到的字符串
	*/
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static bool RegexMatch(const FString& InSourceStr, const FString& InPattern, TArray<FString>& OutResult);
			
	//////////////////////////////////////////////////////////////////////////
	// String
	UFUNCTION(BlueprintPure, meta = (CompactNodeTitle = "->"), Category = "CommonStatics")
	static FString BoolToString(bool InBool);

	UFUNCTION(BlueprintPure, meta = (CompactNodeTitle = "->"), Category = "CommonStatics")
	static bool StringToBool(const FString& InString);
	
	//////////////////////////////////////////////////////////////////////////
	// Text
private:
	static TArray<FString> NotNumberSymbols;
public:
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static bool TextIsNumber(const FText& InText);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Number (Text)", CompactNodeTitle = "->", BlueprintAutocast), Category = "CommonStatics")
	static int32 TextToNumber(const FText& InText, TMap<int32, FString>& OutSymbols);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Text (Number)", CompactNodeTitle = "->", BlueprintAutocast), Category = "CommonStatics")
	static FText NumberToText(int32 InNumber, const TMap<int32, FString>& InSymbols);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Name (Text)", CompactNodeTitle = "->", BlueprintAutocast), Category = "CommonStatics")
	static FName TextToName(const FText& InText);

	//////////////////////////////////////////////////////////////////////////
	// Tag
	UFUNCTION(BlueprintPure, meta = (CompactNodeTitle = "->"), Category = "CommonStatics")
	static FGameplayTag NameToTag(const FName InName);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag", CompactNodeTitle = "->"), Category = "CommonStatics")
	static FName TagToName(const FGameplayTag& InTag);

	UFUNCTION(BlueprintPure, meta = (CompactNodeTitle = "->"), Category = "CommonStatics")
	static FGameplayTag StringToTag(const FString& InString);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag", CompactNodeTitle = "->"), Category = "CommonStatics")
	static FString TagToString(const FGameplayTag& InTag);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"), Category = "CommonStatics")
	static int32 GetTagHierarchy(const FGameplayTag& InTag);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag,InTagContainer"), Category = "CommonStatics")
	static int32 GetTagIndexForContainer(const FGameplayTag& InTag, const FGameplayTagContainer& InTagContainer);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"), Category = "CommonStatics")
	static FGameplayTagContainer GetTagChildren(const FGameplayTag& InTag);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"), Category = "CommonStatics")
	static FName MakeLiteralNameTag(const FGameplayTag& InTag);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"), Category = "CommonStatics")
	static FString MakeLiteralStringTag(const FGameplayTag& InTag);

	//////////////////////////////////////////////////////////////////////////
	// Json
	static bool StringToJsonObject(const FString& InJsonString, TSharedPtr<FJsonObject>& OutJsonObject);

	static bool JsonObjectToString(const TSharedPtr<FJsonObject>& InJsonObject, FString& OutJsonString);

	//////////////////////////////////////////////////////////////////////////
	// Input
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static bool HasMouseCapture();

	//////////////////////////////////////////////////////////////////////////
	// Object
	static bool ExecuteObjectFunc(UObject* InObject, const FName& InFuncName, void* Params);

	//////////////////////////////////////////////////////////////////////////
	// Class
	static bool IsClassHasChildren(const UClass* InClass, EClassFlags InDisabledFlags = CLASS_None);

	static TArray<UClass*> GetClassChildren(const UClass* InClass, bool bIncludeSelf = false, EClassFlags InDisabledFlags = CLASS_None);

	//////////////////////////////////////////////////////////////////////////
	// Texture
	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static UTexture2D* LoadTextureFromFile(const FString& InFilePath);

	UFUNCTION(BlueprintCallable, Category = "CommonStatics")
	static void SaveTextureToFile(UTexture2D* InTexture, const FString& InFilePath);

	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static UTexture2D* CompositeTextures(const TArray<UTexture2D*>& InTextures, FVector2D InTexSize, UTexture2D* InTemplate = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Blueprint
	static bool IsImplementedInBlueprint(const UFunction* Func);

	//////////////////////////////////////////////////////////////////////////
	// Widget
	UFUNCTION(BlueprintPure, DisplayName = "Get Position", Category = "CommonStatics")
	static FVector2D GetGeometryPosition(const FGeometry& InGeometry);
	
	UFUNCTION(BlueprintPure, DisplayName = "Get Absolute Position", Category = "CommonStatics")
	static FVector2D GetGeometryAbsolutePosition(const FGeometry& InGeometry);
	
	UFUNCTION(BlueprintPure, DisplayName = "Get Viewport Position", Category = "CommonStatics")
	static FVector2D GetGeometryViewportPosition(const FGeometry& InGeometry);

	//////////////////////////////////////////////////////////////////////////
	// Gameplay
	template<class T>
	static T* GetObjectInExistedWorld(TFunction<UObject*(UWorld*)>&& Callback, bool bInEditor = false)
	{
		for(const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if(!Context.World()) continue;
			
			if(!bInEditor && Context.World()->IsGameWorld() || bInEditor && Context.World()->IsEditorWorld())
			{
				if(T* Object = Cast<T>(Callback(Context.World())))
				{
					return Object;
				}
			}
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static UWorld* GetWorldFromObjectExisted(const UObject* InObject)
	{
		return InObject->GetWorld();
	}

	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static const UObject* GetWorldContext(bool bInEditor = false);

	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static UObject* GetMutableWorldContext(bool bInEditor = false);

	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static UWorld* GetCurrentWorld(bool bInEditor = false)
	{
		return GetWorldFromObjectExisted(GetWorldContext(bInEditor));
	}

	static FTimerManager& GetCurrentTimerManager(bool bInEditor = false)
	{
		return GetCurrentWorld(bInEditor)->GetTimerManager();
	}
	
	template<class T>
	static TArray<T*> GetAllActorsOfDataLayer(UDataLayerAsset* InDataLayer)
	{
		TArray<T*> ReturnValues;
		for(auto Iter : GetAllActorsOfDataLayer(InDataLayer, T::StaticClass()))
		{
			if(T* Actor = Cast<T>(Iter))
			{
				ReturnValues.Add(Actor);
			}
		}
		return ReturnValues;
	}
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static TArray<AActor*> GetAllActorsOfDataLayer(UDataLayerAsset* InDataLayer, TSubclassOf<AActor> InClass = nullptr);

	template<class T>
	static T* GetGameInstance()
	{
		return Cast<T>(GetGameInstance());
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static UWHGameInstance* GetGameInstance(TSubclassOf<UWHGameInstance> InClass = nullptr);

	template<class T>
	static T* GetGameMode()
	{
		return Cast<T>(GetGameMode());
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static AWHGameMode* GetGameMode(TSubclassOf<AWHGameMode> InClass = nullptr);

	template<class T>
	static T* GetGameState()
	{
		return Cast<T>(UGameplayStatics::GetGameState(GetWorldContext()));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static AWHGameState* GetGameState(TSubclassOf<AWHGameState> InClass = nullptr);

	template<class T>
	static T* GetPlayerController(int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetPlayerController(InPlayerIndex));
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static AWHPlayerController* GetPlayerController(int32 InPlayerIndex = 0, TSubclassOf<AWHPlayerController> InClass = nullptr);

	template<class T>
	static T* GetPlayerControllerByID(int32 InPlayerID = 0)
	{
		return Cast<T>(GetPlayerControllerByID(InPlayerID));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static AWHPlayerController* GetPlayerControllerByID(int32 InPlayerID = 0, TSubclassOf<AWHPlayerController> InClass = nullptr);

	template<class T>
	static T* GetLocalPlayerController(int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetLocalPlayerController(InPlayerIndex));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static AWHPlayerController* GetLocalPlayerController(int32 InPlayerIndex = 0, TSubclassOf<AWHPlayerController> InClass = nullptr);

	template<class T>
	static T* GetPossessedPawn(int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetPossessedPawn(InPlayerIndex));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static APawn* GetPossessedPawn(int32 InPlayerIndex = 0, TSubclassOf<APawn> InClass = nullptr);

	template<class T>
	static T* GetPossessedPawnByID(int32 InPlayerID = 0)
	{
		return Cast<T>(GetPossessedPawnByID(InPlayerID));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static APawn* GetPossessedPawnByID(int32 InPlayerID = 0, TSubclassOf<APawn> InClass = nullptr);

	template<class T>
	static T* GetLocalPossessedPawn(int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetLocalPossessedPawn(InPlayerIndex));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static APawn* GetLocalPossessedPawn(int32 InPlayerIndex = 0, TSubclassOf<APawn> InClass = nullptr);

	template<class T>
	static T* GetPlayerPawn(int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetPlayerPawn(InPlayerIndex));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static APawn* GetPlayerPawn(int32 InPlayerIndex = 0, TSubclassOf<APawn> InClass = nullptr);

	template<class T>
	static T* GetPlayerPawnByID(int32 InPlayerID = 0)
	{
		return Cast<T>(GetPlayerPawnByID(InPlayerID));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static APawn* GetPlayerPawnByID(int32 InPlayerID = 0, TSubclassOf<APawn> InClass = nullptr);

	template<class T>
	static T* GetLocalPlayerPawn()
	{
		if(AWHPlayerController* PlayerController = GetLocalPlayerController<AWHPlayerController>())
		{
			return PlayerController->GetPlayerPawn<T>();
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static APawn* GetLocalPlayerPawn(TSubclassOf<APawn> InClass = nullptr);

	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static TArray<UWHLocalPlayer*> GetLocalPlayers();

	template<class T>
	static T* GetLocalPlayer(int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetLocalPlayer(InPlayerIndex));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CommonStatics")
	static UWHLocalPlayer* GetLocalPlayer(int32 InPlayerIndex = 0, TSubclassOf<UWHLocalPlayer> InClass = nullptr);

	UFUNCTION(BlueprintPure, Category = "CommonStatics")
	static int32 GetLocalPlayerNum();
};
