// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "WidgetModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WidgetModuleStatics.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	////////////////////////////////////////////////////
	// UserWidget
public:
	template<class T>
	static T* GetTemporaryUserWidget()
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->GetTemporaryUserWidget<T>();
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static UUserWidgetBase* GetTemporaryUserWidget();

	template<class T>
	static bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->HasUserWidgetClass<T>(InClass);
		}
		return false;
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidgetClassByName(FName InName)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->HasUserWidgetClassByName(InName);
		}
		return false;
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TSubclassOf<UUserWidgetBase> GetUserWidgetClassByName(FName InName)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->GetUserWidgetClassByName(InName);
		}
		return nullptr;
	}

	template<class T>
	static bool HasUserWidget(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->HasUserWidget<T>(InClass);
		}
		return false;
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidget(TSubclassOf<UUserWidgetBase> InClass);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidgetByName(FName InName)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->HasUserWidgetByName(InName);
		}
		return false;
	}

	template<class T>
	static T* GetUserWidget(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->GetUserWidget<T>(InClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* GetUserWidget(TSubclassOf<UUserWidgetBase> InClass);

	template<class T>
	static T* GetUserWidgetByName(FName InName)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->GetUserWidgetByName<T>(InName);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	static UUserWidgetBase* GetUserWidgetByName(FName InName, TSubclassOf<UUserWidgetBase> InClass = nullptr);

	template<class T>
	static T* CreateUserWidget(UObject* InOwner = nullptr, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->CreateUserWidget<T>(InOwner, InClass);
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, UObject* InOwner = nullptr);

	template<class T>
	static T* CreateUserWidgetByName(FName InName, UObject* InOwner = nullptr)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->CreateUserWidgetByName<T>(InName, InOwner);
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static UUserWidgetBase* CreateUserWidgetByName(FName InName, UObject* InOwner = nullptr);

	template<class T>
	static bool OpenUserWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->OpenUserWidget<T>(InParams, bInstant, InClass);
		}
		return false;
	}

	template<class T>
	static bool OpenUserWidget(const TArray<FParameter>& InParams, bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return OpenUserWidget<T>(&InParams, bInstant, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static bool OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams, bool bInstant = false);

	static bool OpenUserWidgetByName(FName InName, const TArray<FParameter>* InParams = nullptr, bool bInstant = false)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->OpenUserWidgetByName(InName, InParams, bInstant);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static bool OpenUserWidgetByName(FName InName, const TArray<FParameter>& InParams, bool bInstant = false);

	template<class T>
	static bool CloseUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->CloseUserWidget<T>(bInstant, InClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool CloseUserWidgetByName(FName InName, bool bInstant = false)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->CloseUserWidgetByName(InName, bInstant);
		}
		return false;
	}

	template<class T>
	static bool ToggleUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->ToggleUserWidget<T>(bInstant, InClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool ToggleUserWidgetByName(FName InName, bool bInstant = false)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->ToggleUserWidgetByName(InName, bInstant);
		}
		return false;
	}

	template<class T>
	static bool DestroyUserWidget(bool bRecovery = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->DestroyUserWidget<T>(bRecovery, InClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyUserWidgetByName(FName InName, bool bRecovery = false)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->DestroyUserWidgetByName(InName, bRecovery);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void CloseAllUserWidget(bool bInstant = false);

	////////////////////////////////////////////////////
	// SlateWidget
public:
	template<class T>
	static TSharedPtr<T> CreateSlateWidget()
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->CreateSlateWidget<T>();
		}
		return nullptr;
	}

	template<class T>
	static TSharedPtr<T> GetSlateWidget()
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->GetSlateWidget<T>();
		}
		return nullptr;
	}
	
	template<class T>
	static bool InitializeSlateWidget(UObject* InOwner)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->InitializeSlateWidget<T>(InOwner);
		}
		return false;
	}

	template<class T>
	static bool OpenSlateWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->OpenSlateWidget<T>(InParams, bInstant);
		}
		return false;
	}
	
	template<class T>
	static bool CloseSlateWidget(bool bInstant = false)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->CloseSlateWidget<T>(bInstant);
		}
		return false;
	}
	
	template<class T>
	static bool ToggleSlateWidget(bool bInstant = false)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->ToggleSlateWidget<T>(bInstant);
		}
		return false;
	}

	template<class T>
	static bool DestroySlateWidget()
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->DestroySlateWidget<T>();
		}
		return false;
	}

	static void CloseAllSlateWidget(bool bInstant = false);
	
	////////////////////////////////////////////////////
	// Widget
public:
	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static UWorldWidgetContainer* GetWorldWidgetContainer();

	template<class T>
	static bool HasWorldWidget(int32 InIndex, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->HasWorldWidget<T>(InIndex, InClass);
		}
		return false;
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex);

	UFUNCTION(BlueprintPure)
	bool HasWorldWidgetByName(FName InName, int32 InIndex) const
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->HasWorldWidgetByName(InName, InIndex);
		}
		return false;
	}

	template<class T>
	static T* GetWorldWidget(int32 InIndex, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->GetWorldWidget<T>(InIndex, InClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* GetWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex);

	template<class T>
	static T* GetWorldWidgetByName(FName InName, int32 InIndex)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->GetWorldWidgetByName<T>(InName, InIndex);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* GetWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex);

	template<class T>
	static TArray<T*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->GetWorldWidgets<T>(InClass);
		}
		return TArray<T*>();
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<UWorldWidgetBase*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass);

	template<class T>
	static TArray<T*> GetWorldWidgetsByName(FName InName)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->GetWorldWidgetsByName<T>(InName);
		}
		return TArray<T*>();
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<UWorldWidgetBase*> GetWorldWidgetsByName(FName InName);

	template<class T>
	static T* CreateWorldWidget(UObject* InOwner, FWorldWidgetBindInfo InBindInfo, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->CreateWorldWidget<T>(InOwner, InBindInfo, InParams, InClass);
		}
		return nullptr;
	}

	template<class T>
	static T* CreateWorldWidget(UObject* InOwner, FWorldWidgetBindInfo InBindInfo, const TArray<FParameter>& InParams, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return CreateWorldWidget<T>(InOwner, InBindInfo, &InParams, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetBindInfo InBindInfo, const TArray<FParameter>& InParams);

	template<class T>
	static T* CreateWorldWidgetByName(FName InName, UObject* InOwner, FWorldWidgetBindInfo InBindInfo, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->CreateWorldWidgetByName<T>(InName, InOwner, InBindInfo, InParams, InClass);
		}
		return nullptr;
	}

	template<class T>
	static T* CreateWorldWidgetByName(FName InName, UObject* InOwner, FWorldWidgetBindInfo InBindInfo, const TArray<FParameter>& InParams, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return CreateWorldWidgetByName<T>(InName, InOwner, InBindInfo, &InParams, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* CreateWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetBindInfo InBindInfo, const TArray<FParameter>& InParams);

	static bool DestroyWorldWidget(UWorldWidgetBase* InWidget, bool bRecovery = false)
	{
		if(!InWidget) return false;

		const FName WidgetName = InWidget->GetWidgetName();
		
		return DestroyWorldWidgetByName(WidgetName, InWidget->GetWidgetIndex(), bRecovery);
	}

	template<class T>
	static bool DestroyWorldWidget(int32 InIndex, bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->DestroyWorldWidget<T>(InIndex, bRecovery, InClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyWorldWidgetByName(FName InName, int32 InIndex, bool bRecovery = false)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			return WidgetModule->DestroyWorldWidgetByName(InName, InIndex, bRecovery);
		}
		return false;
	}

	template<class T>
	static void DestroyWorldWidgets(bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			WidgetModule->DestroyWorldWidgets<T>(bRecovery, InClass);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	void DestroyWorldWidgetsByName(FName InName, bool bRecovery = false)
	{
		if(UWidgetModule* WidgetModule = UWidgetModule::Get())
		{
			WidgetModule->DestroyWorldWidgetsByName(InName, bRecovery);
		}
	}
};
