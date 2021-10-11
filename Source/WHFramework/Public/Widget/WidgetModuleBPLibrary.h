// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MainModule.h"
#include "WidgetModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WidgetModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	////////////////////////////////////////////////////
	// UserWidget
public:
	template<class T>
	static T* CreateUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CreateUserWidget<T>(InWidgetClass);
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateUserWidget", DeterminesOutputType = "InWidgetClass"), Category = "WidgetModuleBPLibrary")
	static class UUserWidgetBase* K2_CreateUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass);

	template<class T>
	static T* GetUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetUserWidget<T>(InWidgetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetUserWidget", DeterminesOutputType = "InWidgetClass"), Category = "WidgetModuleBPLibrary")
	static class UUserWidgetBase* K2_GetUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass);

	template<class T>
	static bool InitializeUserWidget(AActor* InOwner, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->InitializeUserWidget<T>(InOwner, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "InitializeUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_InitializeUserWidget(AActor* InOwner, TSubclassOf<class UUserWidgetBase> InWidgetClass);

	template<class T>
	static bool OpenUserWidget(bool bInstant = false, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->OpenUserWidget<T>(bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "OpenUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_OpenUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bInstant = false);

	template<class T>
	static bool CloseUserWidget(bool bInstant = false, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CloseUserWidget<T>(bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CloseUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_CloseUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bInstant = false);

	template<class T>
	static bool ToggleUserWidget(bool bInstant = false, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->ToggleUserWidget<T>(bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ToggleUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_ToggleUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bInstant = false);

	template<class T>
	static bool DestroyUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->DestroyUserWidget<T>(InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_DestroyUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static void OpenAllUserWidget(EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static void CloseAllUserWidget(EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	////////////////////////////////////////////////////
	// SlateWidget
public:
	template<class T>
	static TSharedPtr<T> CreateSlateWidget()
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CreateSlateWidget<T>();
		}
		return nullptr;
	}

	template<class T>
	static TSharedPtr<T> GetSlateWidget()
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetSlateWidget<T>();
		}
		return nullptr;
	}
	
	template<class T>
	static bool InitializeSlateWidget(AActor* InOwner)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->InitializeSlateWidget<T>(InOwner);
		}
		return false;
	}

	template<class T>
	static bool OpenSlateWidget(bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->OpenSlateWidget<T>(bInstant);
		}
		return false;
	}
	
	template<class T>
	static bool CloseSlateWidget(bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CloseSlateWidget<T>(bInstant);
		}
		return false;
	}
	
	template<class T>
	static bool ToggleSlateWidget(bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->ToggleSlateWidget<T>(bInstant);
		}
		return false;
	}

	template<class T>
	static bool RemoveSlateWidget()
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->DestroySlateWidget<T>();
		}
		return false;
	}

	static void OpenAllSlateWidget(EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	static void CloseAllSlateWidget(EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static void SetInputMode(EInputMode InInputMode);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleBPLibrary")
	static EInputMode GetInputMode();
};