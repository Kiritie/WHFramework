// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GlobalTypes.h"
#include "Engine/GameEngine.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Dom/JsonObject.h"
#include "Gameplay/WHPlayerController.h"
#include "Main/MainModuleBPLibrary.h"

#include "GlobalBPLibrary.generated.h"

class APawn;
class AWHPlayerController;
class AWHGameState;
class AWHGameMode;
class UWHGameInstance;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UGlobalBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// State
	/*
	 * 当前是否为播放状态
	 */
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static bool IsPlaying() { return GIsPlaying; }
	/*
	 * 当前是否为模拟状态
	 */
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static bool IsSimulating() { return GIsSimulating; }
	/*
	 * 当前是否为运行状态
	 */
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static bool IsRunning() { return GIsRunning; }
	/*
	 * 当前是否为编辑器状态运行
	 */
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static bool IsEditor() { return GIsEditor; }
	/*
	 * 当前是否为暂停状态
	 */
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static bool IsPaused();
	/*
	 * 设置暂停状态
	 */
	UFUNCTION(BlueprintCallable, Category = "GlobalBPLibrary")
	static void SetPaused(bool bPaused);
	/*
	 * 获取时间缩放
	 */
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static float GetTimeScale();
	/*
	 * 设置时间缩放
	 */
	UFUNCTION(BlueprintCallable, Category = "GlobalBPLibrary")
	static void SetTimeScale(float TimeScale);
	/*
	 * 暂停游戏
	 */
	UFUNCTION(BlueprintCallable, Category = "GlobalBPLibrary")
	static void PauseGame(EPauseMode PauseMode = EPauseMode::Default);
	/*
	 * 取消暂停游戏
	 */
	UFUNCTION(BlueprintCallable, Category = "GlobalBPLibrary")
	static void UnPauseGame(EPauseMode PauseMode = EPauseMode::Default);
	/*
	 * 退出游戏
	 */
	UFUNCTION(BlueprintCallable, Category = "GlobalBPLibrary")
	static void QuitGame(TEnumAsByte<EQuitPreference::Type> QuitPreference, bool bIgnorePlatformRestrictions);

	//////////////////////////////////////////////////////////////////////////
	// Trace
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static ETraceTypeQuery GetGameTraceType(ECollisionChannel InTraceChannel);

	//////////////////////////////////////////////////////////////////////////
	// Enum
	/*
	* 获取枚举值显示名称
	* @param InEnumName 枚举名称
	*/
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static int32 GetEnumItemNum(const FString& InEnumName);
	
	/*
	 * 获取枚举值变量名称
	 * @param InEnumName 枚举名称
	 * @param InEnumValue 枚举值
	 */
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static FString GetEnumValueAuthoredName(const FString& InEnumName, int32 InEnumValue);

	/*
	* 获取枚举值显示名称
	* @param InEnumName 枚举名称
	* @param InEnumValue 枚举值
	*/
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static FText GetEnumValueDisplayName(const FString& InEnumName, int32 InEnumValue);

	/*
	 * 获取枚举值变量名称
	 * @param InEnumName 枚举名称
	 * @param InEnumValue 枚举值
	 */
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static int32 GetEnumIndexByValueName(const FString& InEnumName, const FString& InValueName);

	//////////////////////////////////////////////////////////////////////////
	// Serialize
	/*
	* 将目标对象数据保存到内存
	* @param InObject 目标对象
	* @param OutObjectData 保存到的字节数组
	*/
	UFUNCTION(BlueprintCallable, Category = "GlobalBPLibrary")
	static void SaveObjectDataToMemory(UObject* InObject, TArray<uint8>& OutObjectData);

	/*
	* 从内存加载目标对象数据
	* @param InObject 目标对象
	* @param InObjectData 目标对象字节数组
	*/
	UFUNCTION(BlueprintCallable, Category = "GlobalBPLibrary")
	static void LoadObjectDataFromMemory(UObject* InObject, const TArray<uint8>& InObjectData);

	/*
	* 导入目标对象暴露的参数
	* @param InObject 目标对象
	* @param InJsonObject 导入的Json对象
	*/
	static void ImportExposedProperties(UObject* InObject, TSharedPtr<FJsonObject> InJsonObject);
	
	/*
	* 导出目标对象暴露的参数
	* @param InObject = 目标对象
	* @param InJsonObject 导出的Json对象
	* @param bExportSubObjects 是否导出子对象
	*/
	static void ExportExposedProperties(UObject* InObject, TSharedPtr<FJsonObject>& InJsonObject, bool bExportSubObjects = false);
	
	/*
	* 导出源对象暴露的参数到目标对象
	* @param InSourceObject 源对象
	* @param InTargetObject 目标对象
	* @param bExportSubObjects 是否导出子对象
	*/
	static void ExportPropertiesToObject(UObject* InSourceObject, UObject* InTargetObject, bool bExportSubObjects = false);

	//////////////////////////////////////////////////////////////////////////
	// Regex
	/*
	* 正则表达式匹配字串，匹配到返回true，没匹配到返回false
	* @param InSourceStr 源字符串
	* @param InPattern 正则表达式匹配规则
	* @param OutResult 匹配到的字符串
	*/
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static bool RegexMatch(const FString& InSourceStr, const FString& InPattern, TArray<FString>& OutResult);
			
	//////////////////////////////////////////////////////////////////////////
	// String
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static FString BoolToString(bool InBool);

	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static bool StringToBool(const FString& InString);

	//////////////////////////////////////////////////////////////////////////
	// Text
private:
	static TArray<FString> NotNumberSymbols;
public:
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static bool TextIsNumber(const FText& InText);

	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static int32 TextToNumber(const FText& InText, TMap<int32, FString>& OutSymbols);

	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static FText NumberToText(int32 InNumber, const TMap<int32, FString>& InSymbols);

	//////////////////////////////////////////////////////////////////////////
	// Json
	static bool ParseJsonObjectFromString(const FString& InJsonString, TSharedPtr<FJsonObject>& OutJsonObject);

	//////////////////////////////////////////////////////////////////////////
	// Input
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static FText GetInputActionKeyCodeByName(const FString& InInputActionName);

	//////////////////////////////////////////////////////////////////////////
	// Object
	static bool ExecuteObjectFunc(UObject* InObject, const FName& InFuncName, void* Params);

	//////////////////////////////////////////////////////////////////////////
	// Texture
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static UTexture2D* LoadTextureFormFile(const FString& InFilePath);

	UFUNCTION(BlueprintCallable, Category = "GlobalBPLibrary")
	static void SaveTextureToFile(UTexture2D* InTexture, const FString& InFilePath);

	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static UTexture2D* CompositeTextures(const TArray<UTexture2D*>& InTextures, FVector2D InTexSize);

	//////////////////////////////////////////////////////////////////////////
	// Blueprint
	static bool IsImplementedInBlueprint(const UFunction* Func);

	//////////////////////////////////////////////////////////////////////////
	// Gameplay
	template<class T>
	static T* GetObjectInExistedWorld(TFunction<UObject*(UWorld*)>&& Callback, bool bInEditor = false)
	{
		for(const FWorldContext& Context : GEngine->GetWorldContexts())
		{
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

	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static UWorld* GetWorldFromObjectExisted(const UObject* InObject)
	{
		return InObject->GetWorld();
	}

	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static UObject* GetWorldContext(bool bInEditor = false);

	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static UWorld* GetCurrentWorld(bool bInEditor = false)
	{
		return GetWorldFromObjectExisted(GetWorldContext(bInEditor));
	}

	template<class T>
	static T* GetGameInstance()
	{
		return Cast<T>(UGameplayStatics::GetGameInstance(GetWorldContext()));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetGameInstance", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static UWHGameInstance* GetGameInstance(TSubclassOf<UWHGameInstance> InClass = nullptr);

	template<class T>
	static T* GetGameMode()
	{
		return Cast<T>(UGameplayStatics::GetGameMode(GetWorldContext()));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static AWHGameMode* GetGameMode(TSubclassOf<AWHGameMode> InClass = nullptr);

	template<class T>
	static T* GetGameState()
	{
		return Cast<T>(UGameplayStatics::GetGameState(GetWorldContext()));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static AWHGameState* GetGameState(TSubclassOf<AWHGameState> InClass = nullptr);

	template<class T>
	static T* GetPlayerController(int32 InPlayerIndex = 0)
	{
		return Cast<T>(UGameplayStatics::GetPlayerController(GetWorldContext(), InPlayerIndex));
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static AWHPlayerController* GetPlayerController(TSubclassOf<AWHPlayerController> InClass = nullptr, int32 InPlayerIndex = 0);

	template<class T>
	static T* GetPlayerControllerByID(int32 InPlayerID = 0)
	{
		return Cast<T>(UGameplayStatics::GetPlayerControllerFromID(GetWorldContext(), InPlayerID));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static AWHPlayerController* GetPlayerControllerByID(TSubclassOf<AWHPlayerController> InClass = nullptr, int32 InPlayerID = 0);

	template<class T>
	static T* GetLocalPlayerController()
	{
		return Cast<T>(GetLocalPlayerController(T::StaticClass()));
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static AWHPlayerController* GetLocalPlayerController(TSubclassOf<AWHPlayerController> InClass = nullptr);

	template<class T>
	static T* GetPossessedPawn(int32 InPlayerIndex = 0)
	{
		if(AWHPlayerController* PlayerController = GetPlayerController<AWHPlayerController>(InPlayerIndex))
		{
			return PlayerController->GetPawn<T>();
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static APawn* GetPossessedPawn(TSubclassOf<APawn> InClass = nullptr, int32 InPlayerIndex = 0);

	template<class T>
	static T* GetPossessedPawnByID(int32 InPlayerID = 0)
	{
		if(AWHPlayerController* PlayerController = GetPlayerControllerByID<AWHPlayerController>(InPlayerID))
		{
			return PlayerController->GetPawn<T>();
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static APawn* GetPossessedPawnByID(TSubclassOf<APawn> InClass = nullptr, int32 InPlayerID = 0);

	template<class T>
	static T* GetLocalPossessedPawn()
	{
		if(AWHPlayerController* PlayerController = GetLocalPlayerController<AWHPlayerController>())
		{
			return PlayerController->GetPawn<T>();
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static APawn* GetLocalPossessedPawn(TSubclassOf<APawn> InClass = nullptr);

	template<class T>
	static T* GetPlayerPawn(int32 InPlayerIndex = 0)
	{
		if(AWHPlayerController* PlayerController = GetPlayerController<AWHPlayerController>(InPlayerIndex))
		{
			return PlayerController->GetPlayerPawn<T>();
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static APawn* GetPlayerPawn(TSubclassOf<APawn> InClass = nullptr, int32 InPlayerIndex = 0);

	template<class T>
	static T* GetPlayerPawnByID(int32 InPlayerID = 0)
	{
		if(AWHPlayerController* PlayerController = GetPlayerControllerByID<AWHPlayerController>(InPlayerID))
		{
			return PlayerController->GetPlayerPawn<T>();
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static APawn* GetPlayerPawnByID(TSubclassOf<APawn> InClass = nullptr, int32 InPlayerID = 0);

	template<class T>
	static T* GetLocalPlayerPawn()
	{
		if(AWHPlayerController* PlayerController = GetLocalPlayerController<AWHPlayerController>())
		{
			return PlayerController->GetPlayerPawn<T>();
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static APawn* GetLocalPlayerPawn(TSubclassOf<APawn> InClass = nullptr);
};
