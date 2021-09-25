// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

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

public:
	static AWidgetModule* WidgetModuleInst;

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "WidgetModuleBPLibrary")
	static AWidgetModule* GetWidgetModule(UObject* InWorldContext);

	////////////////////////////////////////////////////
	// UserWidget
public:
	template<class T>
	static T* CreateUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->CreateUserWidget<T>(InWidgetClass);
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DisplayName = "CreateUserWidget", DeterminesOutputType = "InWidgetClass"), Category = "WidgetModuleBPLibrary")
	static class UUserWidgetBase* K2_CreateUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass);

	template<class T>
	static T* GetUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->GetUserWidget<T>(InWidgetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext", DisplayName = "GetUserWidget", DeterminesOutputType = "InWidgetClass"), Category = "WidgetModuleBPLibrary")
	static class UUserWidgetBase* K2_GetUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass);

	template<class T>
	static bool InitializeUserWidget(UObject* InWorldContext, AActor* InOwner, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->InitializeUserWidget<T>(InOwner, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DisplayName = "InitializeUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_InitializeUserWidget(UObject* InWorldContext, AActor* InOwner, TSubclassOf<class UUserWidgetBase> InWidgetClass);

	template<class T>
	static bool OpenUserWidget(UObject* InWorldContext, bool bInstant = false, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->OpenUserWidget<T>(bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DisplayName = "OpenUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_OpenUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bInstant = false);

	template<class T>
	static bool CloseUserWidget(UObject* InWorldContext, bool bInstant = false, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->CloseUserWidget<T>(bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DisplayName = "CloseUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_CloseUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bInstant = false);

	template<class T>
	static bool ToggleUserWidget(UObject* InWorldContext, bool bInstant = false, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->ToggleUserWidget<T>(bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DisplayName = "ToggleUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_ToggleUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bInstant = false);

	template<class T>
	static bool DestroyUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->DestroyUserWidget<T>(InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DisplayName = "DestroyUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_DestroyUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "WidgetModuleBPLibrary")
	static void OpenAllUserWidget(UObject* InWorldContext, EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "WidgetModuleBPLibrary")
	static void CloseAllUserWidget(UObject* InWorldContext, EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	////////////////////////////////////////////////////
	// SlateWidget
public:
	template<class T>
	static TSharedPtr<T> CreateSlateWidget(UObject* InWorldContext)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->CreateSlateWidget<T>();
		}
		return nullptr;
	}

	template<class T>
	static TSharedPtr<T> GetSlateWidget(UObject* InWorldContext)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->GetSlateWidget<T>();
		}
		return nullptr;
	}
	
	template<class T>
	static bool InitializeSlateWidget(AActor* InOwner, UObject* InWorldContext)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->InitializeSlateWidget<T>(InOwner);
		}
		return false;
	}

	template<class T>
	static bool OpenSlateWidget(UObject* InWorldContext, bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->OpenSlateWidget<T>(bInstant);
		}
		return false;
	}
	
	template<class T>
	static bool CloseSlateWidget(UObject* InWorldContext, bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->CloseSlateWidget<T>(bInstant);
		}
		return false;
	}
	
	template<class T>
	static bool ToggleSlateWidget(UObject* InWorldContext, bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->ToggleSlateWidget<T>(bInstant);
		}
		return false;
	}

	template<class T>
	static bool RemoveSlateWidget(UObject* InWorldContext)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->DestroySlateWidget<T>();
		}
		return false;
	}

	static void OpenAllSlateWidget(UObject* InWorldContext, EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	static void CloseAllSlateWidget(UObject* InWorldContext, EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "WidgetModuleBPLibrary")
	static void SetInputMode(UObject* InWorldContext, EInputMode InInputMode);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "WidgetModuleBPLibrary")
	static EInputMode GetInputMode(UObject* InWorldContext);
};