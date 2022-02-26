// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Main/MainModule.h"
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
	static T* GetTemporaryUserWidget()
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetTemporaryUserWidget<T>();
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetTemporaryUserWidget"))
	static UUserWidgetBase* K2_GetTemporaryUserWidget()
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->K2_GetTemporaryUserWidget();
		}
		return nullptr;
	}
	
	template<class T>
	static TSubclassOf<UUserWidgetBase> GetUserWidgetClass(FName InName = NAME_None)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetUserWidgetClass<T>(InName);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "CreateUserWidget"), Category = "WidgetModuleBPLibrary")
	static TSubclassOf<UUserWidgetBase> K2_GetUserWidgetClass(FName InName)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->K2_GetUserWidgetClass(InName);
		}
		return nullptr;
	}

	template<class T>
	static T* CreateUserWidget(AActor* InOwner = nullptr, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CreateUserWidget<T>(InOwner, InWidgetClass);
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateUserWidget", DeterminesOutputType = "InWidgetClass"), Category = "WidgetModuleBPLibrary")
	static class UUserWidgetBase* K2_CreateUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass, AActor* InOwner = nullptr);

	template<class T>
	static T* GetUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetUserWidget<T>(InWidgetClass);
		}
		return nullptr;
	}

	template<class T>
	static T* GetUserWidgetByName(FName InName)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetUserWidgetByName<T>(InName);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetUserWidgetByName", DeterminesOutputType = "InWidgetClass"))
	static UUserWidgetBase* K2_GetUserWidgetByName(FName InName, TSubclassOf<UUserWidgetBase> InWidgetClass);

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
	static bool K2_InitializeUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner);

	template<class T>
	static bool OpenUserWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->OpenUserWidget<T>(InParams, bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "OpenUserWidget", AutoCreateRefTerm = "InParams"), Category = "WidgetModuleBPLibrary")
	static bool K2_OpenUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, const TArray<FParameter>& InParams, bool bInstant = false);

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
	static bool OpenSlateWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->OpenSlateWidget<T>(InParams, bInstant);
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
	static bool DestroySlateWidget()
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->DestroySlateWidget<T>();
		}
		return false;
	}

	static void CloseAllSlateWidget(EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);
};