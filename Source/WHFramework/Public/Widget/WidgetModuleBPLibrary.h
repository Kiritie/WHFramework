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

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext", DisplayName = "GetUserWidget", DeterminesOutputType = "InWidgetClass"), Category = "WidgetModuleBPLibrary")
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

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DisplayName = "InitializeUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_InitializeUserWidget(UObject* InWorldContext, AActor* InOwner, TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->K2_InitializeUserWidget(InOwner, InWidgetClass);
		}
		return false;
	}

	template<class T>
	static bool OpenUserWidget(UObject* InWorldContext, bool bInstant = true, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->OpenUserWidget<T>(bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DisplayName = "OpenUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_OpenUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bInstant = true)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->K2_OpenUserWidget(InWidgetClass, bInstant);
		}
		return false;
	}

	template<class T>
	static bool CloseUserWidget(UObject* InWorldContext, bool bInstant = true, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->CloseUserWidget<T>(bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DisplayName = "CloseUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_CloseUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bInstant = true)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->K2_CloseUserWidget(InWidgetClass, bInstant);
		}
		return false;
	}
		
	template<class T>
	static bool ToggleUserWidget(UObject* InWorldContext, bool bInstant = true, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->ToggleUserWidget<T>(bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DisplayName = "ToggleUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_ToggleUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bInstant = true)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->K2_ToggleUserWidget(InWidgetClass, bInstant);
		}
		return false;
	}

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
	static bool K2_DestroyUserWidget(UObject* InWorldContext, TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->K2_DestroyUserWidget(InWidgetClass);
		}
		return false;
	}
		
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "WidgetModuleBPLibrary")
	static void OpenAllUserWidget(UObject* InWorldContext, EWidgetType InWidgetType = EWidgetType::None, bool bInstant = true)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			WidgetModule->OpenAllUserWidget(InWidgetType, bInstant);
		}
	}
	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "WidgetModuleBPLibrary")
	static void CloseAllUserWidget(UObject* InWorldContext, EWidgetType InWidgetType = EWidgetType::None, bool bInstant = true)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			WidgetModule->CloseAllUserWidget(InWidgetType, bInstant);
		}
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
	static bool OpenSlateWidget(UObject* InWorldContext, bool bInstant = true)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->OpenSlateWidget<T>(bInstant);
		}
		return false;
	}
	
	template<class T>
	static bool CloseSlateWidget(UObject* InWorldContext, bool bInstant = true)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			return WidgetModule->CloseSlateWidget<T>(bInstant);
		}
		return false;
	}
	
	template<class T>
	static bool ToggleSlateWidget(UObject* InWorldContext, bool bInstant = true)
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
		
	static void OpenAllSlateWidget(UObject* InWorldContext, EWidgetType InWidgetType = EWidgetType::None, bool bInstant = true)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			WidgetModule->OpenAllSlateWidget(InWidgetType, bInstant);
		}
	}
	
	static void CloseAllSlateWidget(UObject* InWorldContext, EWidgetType InWidgetType = EWidgetType::None, bool bInstant = true)
	{
		if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
		{
			WidgetModule->CloseAllSlateWidget(InWidgetType, bInstant);
		}
	}
};