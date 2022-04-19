// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameEngine.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"

#include "GlobalBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UGlobalBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	//////////////////////////////////////////////////////////////////////////
	// States
public:
	/*
	 * 当前是否为播放状态
	 */
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static bool IsPlaying() { return GWorld->HasBegunPlay(); }
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

	//////////////////////////////////////////////////////////////////////////
	// Enum
public:
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

	//////////////////////////////////////////////////////////////////////////
	// Serialize
public:
	/*
	* 将目标对象数据保存到内存
	* @param InObject 目标对象
	* @param OutObjectData 保存到的字节数组
	*/
	UFUNCTION(BlueprintCallable, Category = "GlobalBPLibrary")
	static void SaveObjectToMemory(UObject* InObject, TArray<uint8>& OutObjectData);

	/*
	* 从内存加载目标对象数据
	* @param InObject 目标对象
	* @param InObjectData 目标对象字节数组
	*/
	UFUNCTION(BlueprintCallable, Category = "GlobalBPLibrary")
	static void LoadObjectFromMemory(UObject* InObject, const TArray<uint8>& InObjectData);

	/*
	* 解析目标对象带有 ExposeOnSpawn 标签的参数
	* @param InObject 目标对象
	* @param InParams 符合导出格式的参数键值对
	* @param bParamHavePropertyType Param的key是否含有属性类的前缀表示：<int> <Array>...
	*/
	UFUNCTION(BlueprintCallable,Category = "GlobalBPLibrary")
	static void SerializeExposedParam(UObject* InObject, const TMap<FString,FString>& InParams, bool bParamHavePropertyType = true);
	/*
	* 导出目标类带有 ExposeOnSpawn 标签的参数
	* @param InClass 目标类
	* @param OutParams 导出参数的存放键值对
	* @param bDisplayPropertyType Param的key是否显示属性类的前缀表示：<int> <Array>...
	*/
	UFUNCTION(BlueprintCallable,Category = "GlobalBPLibrary")
	static void ExportExposedParam(UClass* InClass, TMap<FString,FString>& OutParams, bool bDisplayPropertyType = true);

	//////////////////////////////////////////////////////////////////////////
	// Regex
public:
	/*
	* 正则表达式匹配字串，匹配到返回true，没匹配到返回false
	* @param InSourceStr 源字符串
	* @param InPattern 正则表达式匹配规则
	* @param OutResult 匹配到的字符串
	*/
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static bool RegexMatch(const FString& InSourceStr, const FString& InPattern, TArray<FString>& OutResult);
		
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
	// Input
public:
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static FText GetInputActionKeyCodeByName(const FString& InInputActionName);

	//////////////////////////////////////////////////////////////////////////
	/// Execute Object Func
public:
	static bool ExecuteObjectFunc(UObject* InObject, const FName& InFuncName, void* Params);

	//////////////////////////////////////////////////////////////////////////
	// Gameplay
public:
	UFUNCTION(BlueprintPure, Category = "GlobalBPLibrary")
	static UWorld* GetCurrentWorld()
	{
		UWorld* CurrentWorld = nullptr;
		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			CurrentWorld = GameEngine->GetGameWorld();
		}
		return CurrentWorld;
	}

	template<class T>
	static T* GetGameInstance(const UObject* InWorldContext)
	{
		return Cast<T>(UGameplayStatics::GetGameInstance(InWorldContext));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetGameInstance", WorldContext = "InWorldContext", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static UGameInstance* K2_GetGameInstance(const UObject* InWorldContext, TSubclassOf<UGameInstance> InClass);

	template<class T>
	static T* GetGameMode(const UObject* InWorldContext)
	{
		return Cast<T>(UGameplayStatics::GetGameMode(InWorldContext));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetGameMode", WorldContext = "InWorldContext", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static AGameModeBase* K2_GetGameMode(const UObject* InWorldContext, TSubclassOf<AGameModeBase> InClass);

	template<class T>
	static T* GetGameState(const UObject* InWorldContext)
	{
		return Cast<T>(UGameplayStatics::GetGameState(InWorldContext));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetGameState", WorldContext = "InWorldContext", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static AGameStateBase* K2_GetGameState(const UObject* InWorldContext, TSubclassOf<AGameStateBase> InClass);

	template<class T>
	static T* GetPlayerController(const UObject* InWorldContext, int32 InPlayerIndex = 0)
	{
		return Cast<T>(UGameplayStatics::GetPlayerController(InWorldContext, InPlayerIndex));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetPlayerController", WorldContext = "InWorldContext", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static APlayerController* K2_GetPlayerController(const UObject* InWorldContext, TSubclassOf<APlayerController> InClass, int32 InPlayerIndex = 0);

	template<class T>
	static T* GetPlayerControllerByID(const UObject* InWorldContext, int32 InPlayerID = 0)
	{
		return Cast<T>(UGameplayStatics::GetPlayerControllerFromID(InWorldContext, InPlayerID));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetPlayerControllerByID", WorldContext = "InWorldContext", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static APlayerController* K2_GetPlayerControllerByID(const UObject* InWorldContext, TSubclassOf<APlayerController> InClass, int32 InPlayerID = 0);

	template<class T>
	static T* GetLocalPlayerController(const UObject* InWorldContext)
	{
		return Cast<T>(K2_GetLocalPlayerController(InWorldContext, T::StaticClass()));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetLocalPlayerController", WorldContext = "InWorldContext", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static APlayerController* K2_GetLocalPlayerController(const UObject* InWorldContext, TSubclassOf<APlayerController> InClass);

	template<class T>
	static T* GetPlayerPawn(const UObject* InWorldContext, int32 InPlayerIndex = 0)
	{
		return Cast<T>(UGameplayStatics::GetPlayerPawn(InWorldContext, InPlayerIndex));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetPlayerPawn", WorldContext = "InWorldContext", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static APawn* K2_GetPlayerPawn(const UObject* InWorldContext, TSubclassOf<APawn> InClass, int32 InPlayerIndex = 0);

	template<class T>
	static T* GetPlayerPawnByID(const UObject* InWorldContext, int32 InPlayerID = 0)
	{
		return Cast<T>(K2_GetPlayerPawnByID(InWorldContext, T::StaticClass(), InPlayerID));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetPlayerPawnByID", WorldContext = "InWorldContext", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static APawn* K2_GetPlayerPawnByID(const UObject* InWorldContext, TSubclassOf<APawn> InClass, int32 InPlayerID = 0);

	template<class T>
	static T* GetLocalPlayerPawn(const UObject* InWorldContext)
	{
		return Cast<T>(K2_GetLocalPlayerPawn(InWorldContext, T::StaticClass()));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetLocalPlayerPawn", WorldContext = "InWorldContext", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static APawn* K2_GetLocalPlayerPawn(const UObject* InWorldContext, TSubclassOf<APawn> InClass);

	template<class T>
	static T* GetPlayerCharacter(const UObject* InWorldContext, int32 InPlayerIndex = 0)
	{
		return Cast<T>(UGameplayStatics::GetPlayerCharacter(InWorldContext, InPlayerIndex));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetPlayerCharacter", WorldContext = "InWorldContext", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static ACharacter* K2_GetPlayerCharacter(const UObject* InWorldContext, TSubclassOf<ACharacter> InClass, int32 InPlayerIndex = 0);

	template<class T>
	static T* GetPlayerCharacterByID(const UObject* InWorldContext, int32 InPlayerID = 0)
	{
		return Cast<T>(K2_GetPlayerCharacterByID(InWorldContext, T::StaticClass(), InPlayerID));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetPlayerCharacterByID", WorldContext = "InWorldContext", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static ACharacter* K2_GetPlayerCharacterByID(const UObject* InWorldContext, TSubclassOf<ACharacter> InClass, int32 InPlayerID = 0);

	template<class T>
	static T* GetLocalPlayerCharacter(const UObject* InWorldContext)
	{
		return Cast<T>(K2_GetLocalPlayerCharacter(InWorldContext, T::StaticClass()));
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetLocalPlayerCharacter", WorldContext = "InWorldContext", DeterminesOutputType = "InClass"), Category = "GlobalBPLibrary")
	static ACharacter* K2_GetLocalPlayerCharacter(const UObject* InWorldContext, TSubclassOf<ACharacter> InClass);
};
