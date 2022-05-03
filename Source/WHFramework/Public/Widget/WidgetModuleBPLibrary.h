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
	static T* GetMainUserWidget()
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetMainUserWidget<T>();
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetMainUserWidget"), Category = "WidgetModuleBPLibrary")
	static UUserWidgetBase* K2_GetMainUserWidget();

	template<class T>
	static T* GetTemporaryUserWidget()
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetTemporaryUserWidget<T>();
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetTemporaryUserWidget"), Category = "WidgetModuleBPLibrary")
	static UUserWidgetBase* K2_GetTemporaryUserWidget();

	template<class T>
	static bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->HasUserWidgetClass<T>(InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasUserWidgetClass"), Category = "WidgetModuleBPLibrary")
	static bool K2_HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InWidgetClass);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasUserWidgetClassByName"), Category = "WidgetModuleBPLibrary")
	static bool HasUserWidgetClassByName(FName InWidgetName)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->HasUserWidgetClassByName(InWidgetName);
		}
		return false;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetUserWidgetClassByName"), Category = "WidgetModuleBPLibrary")
	static TSubclassOf<UUserWidgetBase> GetUserWidgetClassByName(FName InWidgetName)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetUserWidgetClassByName(InWidgetName);
		}
		return nullptr;
	}

	template<class T>
	static bool HasUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->HasUserWidget<T>(InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_HasUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasUserWidgetByName"), Category = "WidgetModuleBPLibrary")
	static bool HasUserWidgetByName(FName InWidgetName)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->HasUserWidgetByName(InWidgetName);
		}
		return false;
	}

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
	static T* GetUserWidgetByName(FName InWidgetName)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetUserWidgetByName<T>(InWidgetName);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetUserWidgetByName", DeterminesOutputType = "InWidgetClass"))
	static UUserWidgetBase* K2_GetUserWidgetByName(FName InWidgetName, TSubclassOf<UUserWidgetBase> InWidgetClass);

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
	static T* CreateUserWidgetByName(FName InWidgetName, AActor* InOwner = nullptr)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CreateUserWidgetByName<T>(InWidgetName, InOwner);
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateUserWidgetByName"), Category = "WidgetModuleBPLibrary")
	static class UUserWidgetBase* K2_CreateUserWidgetByName(FName InWidgetName, AActor* InOwner = nullptr);

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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "InitializeUserWidgetByName"), Category = "WidgetModuleBPLibrary")
	static bool InitializeUserWidgetByName(FName InWidgetName, AActor* InOwner)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->InitializeUserWidgetByName(InWidgetName, InOwner);
		}
		return false;
	}

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

	static bool OpenUserWidgetByName(FName InWidgetName, const TArray<FParameter>* InParams = nullptr, bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->OpenUserWidgetByName(InWidgetName, InParams, bInstant);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "OpenUserWidgetByName", AutoCreateRefTerm = "InParams"), Category = "WidgetModuleBPLibrary")
	static bool K2_OpenUserWidgetByName(FName InWidgetName, const TArray<FParameter>& InParams, bool bInstant = false);

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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CloseUserWidgetByName"), Category = "WidgetModuleBPLibrary")
	static bool CloseUserWidgetByName(FName InWidgetName, bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CloseUserWidgetByName(InWidgetName, bInstant);
		}
		return false;
	}

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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ToggleUserWidgetByName"), Category = "WidgetModuleBPLibrary")
	static bool ToggleUserWidgetByName(FName InWidgetName, bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->ToggleUserWidgetByName(InWidgetName, bInstant);
		}
		return false;
	}

	template<class T>
	static bool DestroyUserWidget(bool bRecovery = false, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->DestroyUserWidget<T>(bRecovery, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyUserWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_DestroyUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyUserWidgetByName"), Category = "WidgetModuleBPLibrary")
	static bool DestroyUserWidgetByName(FName InWidgetName, bool bRecovery = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->DestroyUserWidgetByName(InWidgetName, bRecovery);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static void CloseAllUserWidget(bool bInstant = false);

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

	static void CloseAllSlateWidget(bool bInstant = false);
	
	////////////////////////////////////////////////////
	// Widget
public:
	UFUNCTION(BlueprintPure, Category = "WidgetModuleBPLibrary")
	static UWorldWidgetContainer* GetWorldWidgetContainer();

	template<class T>
	static bool HasWorldWidget(int32 InWidgetIndex, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->HasWorldWidget<T>(InWidgetIndex, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasWorldWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_HasWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasWorldWidgetByName"))
	bool HasWorldWidgetByName(FName InWidgetName, int32 InWidgetIndex) const
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->HasWorldWidgetByName(InWidgetName, InWidgetIndex);
		}
		return false;
	}

	template<class T>
	static T* GetWorldWidget(int32 InWidgetIndex, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetWorldWidget<T>(InWidgetIndex, InWidgetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetWorldWidget", DeterminesOutputType = "InWidgetClass"), Category = "WidgetModuleBPLibrary")
	static UWorldWidgetBase* K2_GetWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex);

	template<class T>
	static T* GetWorldWidgetByName(FName InWidgetName, int32 InWidgetIndex)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetWorldWidgetByName<T>(InWidgetName, InWidgetIndex);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetWorldWidgetByName", DeterminesOutputType = "InWidgetClass"), Category = "WidgetModuleBPLibrary")
	static UWorldWidgetBase* K2_GetWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex);

	template<class T>
	static TArray<T*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetWorldWidgets<T>(InWidgetClass);
		}
		return TArray<T*>();
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetWorldWidgets"), Category = "WidgetModuleBPLibrary")
	static TArray<UWorldWidgetBase*> K2_GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass);

	template<class T>
	static TArray<T*> GetWorldWidgetsByName(FName InWidgetName)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetWorldWidgetsByName<T>(InWidgetName);
		}
		return TArray<T*>();
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetWorldWidgetsByName"), Category = "WidgetModuleBPLibrary")
	static TArray<UWorldWidgetBase*> K2_GetWorldWidgetsByName(FName InWidgetName);

	template<class T>
	static T* CreateWorldWidget(AActor* InOwner, FVector InLocation = FVector::ZeroVector, class USceneComponent* InSceneComp = nullptr, TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CreateWorldWidget<T>(InOwner, InLocation, InSceneComp, InParams, InWidgetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateWorldWidget", DeterminesOutputType = "InWidgetClass", AutoCreateRefTerm = "InParams"), Category = "WidgetModuleBPLibrary")
	static UWorldWidgetBase* K2_CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, class USceneComponent* InSceneComp, const TArray<FParameter>& InParams);

	template<class T>
	T* CreateWorldWidgetByName(FName InWidgetName, AActor* InOwner, FVector InLocation = FVector::ZeroVector, class USceneComponent* InSceneComp = nullptr, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CreateWorldWidgetByName<T>(InWidgetName, InOwner, InLocation, InSceneComp, InParams, InWidgetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateWorldWidgetByName", DeterminesOutputType = "InWidgetClass", AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* K2_CreateWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, class USceneComponent* InSceneComp, const TArray<FParameter>& InParams);

	template<class T>
	static bool DestroyWorldWidget(int32 InWidgetIndex, bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->DestroyWorldWidget<T>(InWidgetIndex, bRecovery, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyWorldWidget"), Category = "WidgetModuleBPLibrary")
	static bool K2_DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex, bool bRecovery = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyWorldWidgetByName"))
	bool DestroyWorldWidgetByName(FName InWidgetName, int32 InWidgetIndex, bool bRecovery = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->DestroyWorldWidgetByName(InWidgetName, InWidgetIndex, bRecovery);
		}
		return false;
	}

	template<class T>
	static void DestroyWorldWidgets(bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			WidgetModule->DestroyWorldWidgets<T>(bRecovery, InWidgetClass);
		}
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyWorldWidgets"), Category = "WidgetModuleBPLibrary")
	static void K2_DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyWorldWidgets"))
	void DestroyWorldWidgetsByName(FName InWidgetName, bool bRecovery = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			WidgetModule->DestroyWorldWidgetsByName(InWidgetName);
		}
	}
};
