// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Base/Module.h"
#include "MainModuleTypes.h"
#include "Base/ModuleBase.h"
#include "Base/ModuleNetworkComponent.h"
#include "Kismet/GameplayStatics.h"

#include "MainModule.generated.h"

class AModuleBase;

UCLASS()
class WHFRAMEWORK_API AMainModule : public AActor
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	AMainModule();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//////////////////////////////////////////////////////////////////////////
	/// Instance
protected:
	static AMainModule* Current;

public:
	static AMainModule* Get();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:	
#if WITH_EDITOR
	UFUNCTION(CallInEditor, BlueprintNativeEvent, Category = "MainModule")
	void GenerateModules();
    
	UFUNCTION(CallInEditor, BlueprintNativeEvent, Category = "MainModule")
	void DestroyModules();
#endif

	UFUNCTION(BlueprintNativeEvent, Category = "MainModule")
	void InitializeModules();
    
	UFUNCTION(BlueprintNativeEvent, Category = "MainModule")
    void RefreshModules(float DeltaSeconds);
    
	UFUNCTION(BlueprintNativeEvent, Category = "MainModule")
    void PauseModules();
    
	UFUNCTION(BlueprintNativeEvent, Category = "MainModule")
    void UnPauseModules();

	UFUNCTION(BlueprintNativeEvent, Category = "MainModule")
	void TerminationModules();

protected:
	/// 模块类
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "MainModule")
	TArray<TSubclassOf<AModuleBase>> ModuleClasses;
	/// 模块列表
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated, Category = "MainModule")
	TArray<TScriptInterface<IModule>> ModuleRefs;

	UPROPERTY()
	TMap<FName, TScriptInterface<IModule>> ModuleMap;

public:
	/**
	* 获取所有模块
	*/
	static TArray<TScriptInterface<IModule>> GetAllModules()
	{
		if(Current && Current->IsValidLowLevel())
		{
			return Current->ModuleRefs;
		}
		return TArray<TScriptInterface<IModule>>();
	}
	/**
	 * 通过类型获取模块
	 */
	template<class T>
	static T* GetModuleByClass(TSubclassOf<AModuleBase> InModuleClass = T::StaticClass())
	{
		if(Current && Current->IsValidLowLevel())
		{
			AModuleBase* ModuleBase = InModuleClass.GetDefaultObject();
			if(Current->ModuleMap.Contains(ModuleBase->Execute_GetModuleName(ModuleBase)))
			{
				return Cast<T>(Current->ModuleMap[ModuleBase->Execute_GetModuleName(ModuleBase)].GetObject());
			}
		}
		return nullptr;
	}
	/**
	* 通过名称获取模块
	*/
	template<class T>
	static T* GetModuleByName(const FName InModuleName)
	{
		if(Current && Current->IsValidLowLevel())
		{
			if(Current->ModuleMap.Contains(InModuleName))
			{
				return Cast<T>(Current->ModuleMap[InModuleName].GetObject());
			}
		}
		return nullptr;
	}
	/**
	* 通过类型获取模块网络组件
	*/
	template<class T>
	static T* GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponent> InModuleClass = T::StaticClass())
	{
		if(Current && Current->IsValidLowLevel())
		{
			if(APlayerController* PlayerController = UGameplayStatics::GetPlayerController(Current, 0))
			{
				return Cast<T>(PlayerController->GetComponentByClass(InModuleClass));
			}
		}
		return nullptr;
	}

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
