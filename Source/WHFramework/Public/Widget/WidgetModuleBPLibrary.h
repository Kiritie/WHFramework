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
	
	UFUNCTION(BlueprintPure, Category = "WidgetModuleBPLibrary")
	static UUserWidgetBase* GetMainUserWidget();

	template<class T>
	static T* GetTemporaryUserWidget()
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetTemporaryUserWidget<T>();
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintPure, Category = "WidgetModuleBPLibrary")
	static UUserWidgetBase* GetTemporaryUserWidget();

	template<class T>
	static bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->HasUserWidgetClass<T>(InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleBPLibrary")
	static bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InWidgetClass);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleBPLibrary")
	static bool HasUserWidgetClassByName(FName InWidgetName)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->HasUserWidgetClassByName(InWidgetName);
		}
		return false;
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleBPLibrary")
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

	UFUNCTION(BlueprintPure, Category = "WidgetModuleBPLibrary")
	static bool HasUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleBPLibrary")
	static bool HasUserWidgetByName(FName InWidgetName)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->HasUserWidgetByName(InWidgetName);
		}
		return false;
	}

	template<class T>
	static T* GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetUserWidget<T>(InWidgetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InWidgetClass"), Category = "WidgetModuleBPLibrary")
	static UUserWidgetBase* GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass);

	template<class T>
	static T* GetUserWidgetByName(FName InWidgetName)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetUserWidgetByName<T>(InWidgetName);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InWidgetClass"))
	static UUserWidgetBase* GetUserWidgetByName(FName InWidgetName, TSubclassOf<UUserWidgetBase> InWidgetClass);

	template<class T>
	static T* CreateUserWidget(AActor* InOwner = nullptr, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CreateUserWidget<T>(InOwner, InWidgetClass);
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InWidgetClass"), Category = "WidgetModuleBPLibrary")
	static UUserWidgetBase* CreateUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner = nullptr);

	template<class T>
	static T* CreateUserWidgetByName(FName InWidgetName, AActor* InOwner = nullptr)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CreateUserWidgetByName<T>(InWidgetName, InOwner);
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static UUserWidgetBase* CreateUserWidgetByName(FName InWidgetName, AActor* InOwner = nullptr);

	template<class T>
	static bool InitializeUserWidget(AActor* InOwner, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->InitializeUserWidget<T>(InOwner, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static bool InitializeUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static bool InitializeUserWidgetByName(FName InWidgetName, AActor* InOwner)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->InitializeUserWidgetByName(InWidgetName, InOwner);
		}
		return false;
	}

	template<class T>
	static bool OpenUserWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->OpenUserWidget<T>(InParams, bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "WidgetModuleBPLibrary")
	static bool OpenUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, const TArray<FParameter>& InParams, bool bInstant = false);

	static bool OpenUserWidgetByName(FName InWidgetName, const TArray<FParameter>* InParams = nullptr, bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->OpenUserWidgetByName(InWidgetName, InParams, bInstant);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "WidgetModuleBPLibrary")
	static bool OpenUserWidgetByName(FName InWidgetName, const TArray<FParameter>& InParams, bool bInstant = false);

	template<class T>
	static bool CloseUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CloseUserWidget<T>(bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static bool CloseUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static bool CloseUserWidgetByName(FName InWidgetName, bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CloseUserWidgetByName(InWidgetName, bInstant);
		}
		return false;
	}

	template<class T>
	static bool ToggleUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->ToggleUserWidget<T>(bInstant, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static bool ToggleUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static bool ToggleUserWidgetByName(FName InWidgetName, bool bInstant = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->ToggleUserWidgetByName(InWidgetName, bInstant);
		}
		return false;
	}

	template<class T>
	static bool DestroyUserWidget(bool bRecovery = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->DestroyUserWidget<T>(bRecovery, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static bool DestroyUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
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

	UFUNCTION(BlueprintPure, Category = "WidgetModuleBPLibrary")
	static bool HasWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex);

	UFUNCTION(BlueprintPure)
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

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InWidgetClass"), Category = "WidgetModuleBPLibrary")
	static UWorldWidgetBase* GetWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex);

	template<class T>
	static T* GetWorldWidgetByName(FName InWidgetName, int32 InWidgetIndex)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetWorldWidgetByName<T>(InWidgetName, InWidgetIndex);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InWidgetClass"), Category = "WidgetModuleBPLibrary")
	static UWorldWidgetBase* GetWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex);

	template<class T>
	static TArray<T*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetWorldWidgets<T>(InWidgetClass);
		}
		return TArray<T*>();
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleBPLibrary")
	static TArray<UWorldWidgetBase*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass);

	template<class T>
	static TArray<T*> GetWorldWidgetsByName(FName InWidgetName)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->GetWorldWidgetsByName<T>(InWidgetName);
		}
		return TArray<T*>();
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleBPLibrary")
	static TArray<UWorldWidgetBase*> GetWorldWidgetsByName(FName InWidgetName);

	template<class T>
	static T* CreateWorldWidget(AActor* InOwner, FVector InLocation = FVector::ZeroVector, class USceneComponent* InSceneComp = nullptr, TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CreateWorldWidget<T>(InOwner, InLocation, InSceneComp, InParams, InWidgetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InWidgetClass", AutoCreateRefTerm = "InParams"), Category = "WidgetModuleBPLibrary")
	static UWorldWidgetBase* CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, class USceneComponent* InSceneComp, const TArray<FParameter>& InParams);

	template<class T>
	T* CreateWorldWidgetByName(FName InWidgetName, AActor* InOwner, FVector InLocation = FVector::ZeroVector, class USceneComponent* InSceneComp = nullptr, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->CreateWorldWidgetByName<T>(InWidgetName, InOwner, InLocation, InSceneComp, InParams, InWidgetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InWidgetClass", AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* CreateWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, class USceneComponent* InSceneComp, const TArray<FParameter>& InParams);

	template<class T>
	static bool DestroyWorldWidget(int32 InWidgetIndex, bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			return WidgetModule->DestroyWorldWidget<T>(InWidgetIndex, bRecovery, InWidgetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static bool DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
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

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleBPLibrary")
	static void DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	void DestroyWorldWidgetsByName(FName InWidgetName, bool bRecovery = false)
	{
		if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
		{
			WidgetModule->DestroyWorldWidgetsByName(InWidgetName);
		}
	}
};
