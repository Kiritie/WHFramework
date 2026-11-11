#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "MainModuleStatics.generated.h"

class UModuleNetworkComponentBase;
class AMainModule;
class UModuleBase;
class UWorld;
class UGameInstance;
class APlayerController;

UCLASS()
class WHFRAMEWORK_API UMainModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static UModuleBase* GetModuleByClassInWorld(UWorld* InWorld, TSubclassOf<UModuleBase> InClass);
	// 仅供明确的原生退役路径使用；蓝图与普通查询不能继承该能力。
	static UModuleBase* GetModuleByClassForCleanup(UWorld* InWorld, TSubclassOf<UModuleBase> InClass);
	template<typename T> static T* GetModuleForCleanup(UWorld* InWorld)
	{
		return Cast<T>(GetModuleByClassForCleanup(InWorld, T::StaticClass()));
	}

	UFUNCTION(BlueprintCallable, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void RunModuleByClass(const UObject* WorldContextObject, TSubclassOf<UModuleBase> InClass);
	UFUNCTION(BlueprintCallable, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void RunModuleByName(const UObject* WorldContextObject, FName InName);
	UFUNCTION(BlueprintCallable, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void ResetModuleByClass(const UObject* WorldContextObject, TSubclassOf<UModuleBase> InClass);
	UFUNCTION(BlueprintCallable, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void ResetModuleByName(const UObject* WorldContextObject, FName InName);
	UFUNCTION(BlueprintCallable, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void PauseModuleByClass(const UObject* WorldContextObject, TSubclassOf<UModuleBase> InClass);
	UFUNCTION(BlueprintCallable, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void PauseModuleByName(const UObject* WorldContextObject, FName InName);
	UFUNCTION(BlueprintCallable, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void UnPauseModuleByClass(const UObject* WorldContextObject, TSubclassOf<UModuleBase> InClass);
	UFUNCTION(BlueprintCallable, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void UnPauseModuleByName(const UObject* WorldContextObject, FName InName);
	UFUNCTION(BlueprintCallable, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void PauseAllModule(const UObject* WorldContextObject);
	UFUNCTION(BlueprintCallable, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void UnPauseAllModule(const UObject* WorldContextObject);
	UFUNCTION(BlueprintCallable, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void TerminationModuleByClass(const UObject* WorldContextObject, TSubclassOf<UModuleBase> InClass);
	UFUNCTION(BlueprintCallable, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static void TerminationModuleByName(const UObject* WorldContextObject, FName InName);

	UFUNCTION(BlueprintPure, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static AMainModule* GetMainModule(const UObject* WorldContextObject);
	UFUNCTION(BlueprintPure, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static TArray<UModuleBase*> GetAllModule(const UObject* WorldContextObject);
	UFUNCTION(BlueprintPure, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static bool IsExistModuleByClass(const UObject* WorldContextObject, TSubclassOf<UModuleBase> InClass);
	UFUNCTION(BlueprintPure, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static bool IsExistModuleByName(const UObject* WorldContextObject, FName InName);
	UFUNCTION(BlueprintPure, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject", DeterminesOutputType="InClass"))
	static UModuleBase* GetModuleByClass(const UObject* WorldContextObject, TSubclassOf<UModuleBase> InClass);
	UFUNCTION(BlueprintPure, Category="MainModuleStatics", meta=(DeterminesOutputType="InClass"))
	static UModuleBase* GetGameInstanceModuleByClass(const UGameInstance* InGameInstance, TSubclassOf<UModuleBase> InClass);
	UFUNCTION(BlueprintPure, Category="MainModuleStatics", meta=(WorldContext="WorldContextObject"))
	static UModuleBase* GetModuleByName(const UObject* WorldContextObject, FName InName);

	UFUNCTION(BlueprintPure, meta=(DeterminesOutputType="InClass"), Category="MainModuleStatics")
	static UModuleNetworkComponentBase* GetModuleNetworkComponentByClass(APlayerController* InController, TSubclassOf<UModuleNetworkComponentBase> InClass);
};
