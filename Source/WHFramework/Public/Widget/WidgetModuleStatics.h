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
		return UWidgetModule::Get().GetTemporaryUserWidget<T>();
	}
	
	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static UUserWidgetBase* GetTemporaryUserWidget();

	template<class T>
	static bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().HasUserWidgetClass<T>(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidgetClassByName(FName InName)
	{
		return UWidgetModule::Get().HasUserWidgetClassByName(InName);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TSubclassOf<UUserWidgetBase> GetUserWidgetClassByName(FName InName)
	{
		return UWidgetModule::Get().GetUserWidgetClassByName(InName);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<FName> GetUserWidgetChildrenByName(FName InName)
	{
		return UWidgetModule::Get().GetUserWidgetChildrenByName(InName);
	}

	template<class T>
	static void AddUserWidgetClassMapping(FName InName, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		UWidgetModule::Get().AddUserWidgetClassMapping<T>(InName, InClass);
	}

	UFUNCTION(Blueprintable, Category = "WidgetModuleStatics")
	static void AddUserWidgetClassMapping(FName InName, TSubclassOf<UUserWidgetBase> InClass);

	template<class T>
	static bool HasUserWidget(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().HasUserWidget<T>(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidget(TSubclassOf<UUserWidgetBase> InClass);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidgetByName(FName InName)
	{
		return UWidgetModule::Get().HasUserWidgetByName(InName);
	}

	template<class T>
	static T* GetUserWidget(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().GetUserWidget<T>(InClass);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* GetUserWidget(TSubclassOf<UUserWidgetBase> InClass);

	template<class T>
	static T* GetUserWidgetByName(FName InName)
	{
		return UWidgetModule::Get().GetUserWidgetByName<T>(InName);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	static UUserWidgetBase* GetUserWidgetByName(FName InName, TSubclassOf<UUserWidgetBase> InClass = nullptr);
	
	template<class T>
	static T* CreateUserWidget(UObject* InOwner = nullptr, const TArray<FParameter>* InParams = nullptr, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().CreateUserWidget<T>(InOwner, InParams, bForce, InClass);
	}

	template<class T>
	static T* CreateUserWidget(UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return CreateUserWidget<T>(InOwner, &InParams, bForce, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false);

	template<class T>
	static T* CreateUserWidgetByName(FName InName, UObject* InOwner = nullptr, const TArray<FParameter>* InParams = nullptr, bool bForce = false)
	{
		return UWidgetModule::Get().CreateUserWidgetByName<T>(InName, InOwner, InParams, bForce);
	}
	
	template<class T>
	static T* CreateUserWidgetByName(FName InName, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false)
	{
		return CreateUserWidgetByName<T>(InName, InOwner, &InParams, bForce);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* CreateUserWidgetByName(FName InName, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false);

	template<class T>
	static bool OpenUserWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().OpenUserWidget<T>(InParams, bInstant, bForce, InClass);
	}

	template<class T>
	static bool OpenUserWidget(const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return OpenUserWidget<T>(&InParams, bInstant, bForce, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static bool OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false);

	static bool OpenUserWidgetByName(FName InName, const TArray<FParameter>* InParams = nullptr, bool bInstant = false, bool bForce = false)
	{
		return UWidgetModule::Get().OpenUserWidgetByName(InName, InParams, bInstant, bForce);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static bool OpenUserWidgetByName(FName InName, const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false);

	template<class T>
	static bool CloseUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().CloseUserWidget<T>(bInstant, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool CloseUserWidgetByName(FName InName, bool bInstant = false)
	{
		return UWidgetModule::Get().CloseUserWidgetByName(InName, bInstant);
	}

	template<class T>
	static bool ToggleUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().ToggleUserWidget<T>(bInstant, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool ToggleUserWidgetByName(FName InName, bool bInstant = false)
	{
		return UWidgetModule::Get().ToggleUserWidgetByName(InName, bInstant);
	}

	template<class T>
	static bool DestroyUserWidget(bool bRecovery = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().DestroyUserWidget<T>(bRecovery, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyUserWidgetByName(FName InName, bool bRecovery = false)
	{
		return UWidgetModule::Get().DestroyUserWidgetByName(InName, bRecovery);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void CloseAllUserWidget(bool bInstant = false);

	////////////////////////////////////////////////////
	// SlateWidget
public:
	template<class T>
	static TSharedPtr<T> CreateSlateWidget()
	{
		return UWidgetModule::Get().CreateSlateWidget<T>();
	}

	template<class T>
	static TSharedPtr<T> GetSlateWidget()
	{
		return UWidgetModule::Get().GetSlateWidget<T>();
	}

	template<class T>
	static bool OpenSlateWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false)
	{
		return UWidgetModule::Get().OpenSlateWidget<T>(InParams, bInstant);
	}
	
	template<class T>
	static bool CloseSlateWidget(bool bInstant = false)
	{
		return UWidgetModule::Get().CloseSlateWidget<T>(bInstant);
	}
	
	template<class T>
	static bool ToggleSlateWidget(bool bInstant = false)
	{
		return UWidgetModule::Get().ToggleSlateWidget<T>(bInstant);
	}

	template<class T>
	static bool DestroySlateWidget()
	{
		return UWidgetModule::Get().DestroySlateWidget<T>();
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
		return UWidgetModule::Get().HasWorldWidget<T>(InIndex, InClass);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex);

	UFUNCTION(BlueprintPure)
	bool HasWorldWidgetByName(FName InName, int32 InIndex) const
	{
		return UWidgetModule::Get().HasWorldWidgetByName(InName, InIndex);
	}

	template<class T>
	static T* GetWorldWidget(int32 InIndex, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().GetWorldWidget<T>(InIndex, InClass);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* GetWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex);

	template<class T>
	static T* GetWorldWidgetByName(FName InName, int32 InIndex)
	{
		return UWidgetModule::Get().GetWorldWidgetByName<T>(InName, InIndex);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* GetWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex);

	template<class T>
	static TArray<T*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().GetWorldWidgets<T>(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<UWorldWidgetBase*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass);

	template<class T>
	static TArray<T*> GetWorldWidgetsByName(FName InName)
	{
		return UWidgetModule::Get().GetWorldWidgetsByName<T>(InName);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<UWorldWidgetBase*> GetWorldWidgetsByName(FName InName);

	template<class T>
	static T* CreateWorldWidget(UObject* InOwner, FWorldWidgetBindInfo InBindInfo, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().CreateWorldWidget<T>(InOwner, InBindInfo, InParams, InClass);
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
		return UWidgetModule::Get().CreateWorldWidgetByName<T>(InName, InOwner, InBindInfo, InParams, InClass);
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
		return UWidgetModule::Get().DestroyWorldWidget<T>(InIndex, bRecovery, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyWorldWidgetByName(FName InName, int32 InIndex, bool bRecovery = false)
	{
		return UWidgetModule::Get().DestroyWorldWidgetByName(InName, InIndex, bRecovery);
	}

	template<class T>
	static void DestroyWorldWidgets(bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		UWidgetModule::Get().DestroyWorldWidgets<T>(bRecovery, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	void DestroyWorldWidgetsByName(FName InName, bool bRecovery = false)
	{
		UWidgetModule::Get().DestroyWorldWidgetsByName(InName, bRecovery);
	}
};
