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
	static AWidgetModule* WidgetModule;

	UFUNCTION(BlueprintPure, meta = (WorldContext = InWorldContext), Category = "WidgetModuleBPLibrary")
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

	UFUNCTION(BlueprintCallable, meta = (WorldContext = InWorldContext), Category = "WidgetModuleBPLibrary")
	static class UUserWidgetBase* K2_CreateUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return CreateUserWidget<UUserWidgetBase>(InWorldContext, InWidgetClass);
	}

	template<class T>
	static T* GetUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->GetUserWidget<T>(InWidgetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = InWorldContext), Category = "WidgetModuleBPLibrary")
	static class UUserWidgetBase* K2_GetUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return GetUserWidget<UUserWidgetBase>(InWorldContext, InWidgetClass);
	}
		
	template<class T>
	static bool InitializeUserWidget(UObject* InWorldContext, AActor* InOwner, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->InitializeUserWidget<T>(InOwner, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = InWorldContext), Category = "WidgetModuleBPLibrary")
	static bool K2_InitializeUserWidget(UObject* InWorldContext, AActor* InOwner, TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return InitializeUserWidget<UUserWidgetBase>(InWorldContext, InOwner, InWidgetClass);
	}

	template<class T>
	static bool OpenUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->OpenUserWidget<T>(InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = InWorldContext), Category = "WidgetModuleBPLibrary")
	static bool K2_OpenUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return OpenUserWidget<UUserWidgetBase>(InWorldContext, InWidgetClass);
	}

	template<class T>
	static bool CloseUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->CloseUserWidget<T>(InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = InWorldContext), Category = "WidgetModuleBPLibrary")
	static bool K2_CloseUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return CloseUserWidget<UUserWidgetBase>(InWorldContext, InWidgetClass);
	}
		
	template<class T>
	static bool ToggleUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->ToggleUserWidget<T>(InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = InWorldContext), Category = "WidgetModuleBPLibrary")
	static bool K2_ToggleUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return ToggleUserWidget<UUserWidgetBase>(InWorldContext, InWidgetClass);
	}

	template<class T>
	static bool RemoveUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->RemoveUserWidget<T>(InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = InWorldContext), Category = "WidgetModuleBPLibrary")
	static bool K2_RemoveUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return RemoveUserWidget<UUserWidgetBase>(InWorldContext, InWidgetClass);
	}

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
	static bool OpenSlateWidget(UObject* InWorldContext)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->OpenSlateWidget<T>();
		}
		return false;
	}
	
	template<class T>
	static bool CloseSlateWidget(UObject* InWorldContext)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->CloseSlateWidget<T>();
		}
		return false;
	}
	
	template<class T>
	static bool ToggleSlateWidget(UObject* InWorldContext)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->ToggleSlateWidget<T>();
		}
		return false;
	}

	template<class T>
	static bool RemoveSlateWidget(UObject* InWorldContext)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->RemoveSlateWidget<T>();
		}
		return false;
	}
};