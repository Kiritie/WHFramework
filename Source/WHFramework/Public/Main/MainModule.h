// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Base/Module.h"
#include "MainModuleTypes.h"

#include "MainModule.generated.h"

class AIVRealGameState;
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

public:
	/// 模块类
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "MainModule")
	TArray<TSubclassOf<AModuleBase>> ModuleClasses;
	/// 模块列表
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Replicated, Category = "MainModule")
	TArray<TScriptInterface<IModule>> ModuleRefs;

public:
	/**
	 * 通过类型获取模块
	 */
	template<class T>
	T* GetModuleByClass()
	{
		for(int32 i = 0; i < ModuleRefs.Num(); i++)
		{
			if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel() && ModuleRefs[i].GetObject()->IsA(T::StaticClass()))
			{
				return Cast<T>(ModuleRefs[i].GetObject());
			}
		}
		return nullptr;
	}
	/**
	* 通过类型获取模块
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "Get Module By Class", DeterminesOutputType = "InModuleClass"))
	AModuleBase* K2_GetModuleByClass(TSubclassOf<AModuleBase> InModuleClass);
	/**
	* 通过名称获取模块
	*/
	template<class T>
	T* GetModuleByName(const FName InModuleName)
	{
		for(int32 i = 0; i < ModuleRefs.Num(); i++)
		{
			if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel() && ModuleRefs[i]->GetModuleName() == InModuleName)
			{
				return Cast<T>(ModuleRefs[i].GetObject());
			}
		}
		return nullptr;
	}
	/**
	* 通过名称获取模块
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "Get Module By Name"))
	TScriptInterface<IModule> K2_GetModuleByName(const FName InModuleName);

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
