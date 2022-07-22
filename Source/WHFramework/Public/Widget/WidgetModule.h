// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Main/Base/ModuleBase.h"
#include "GameFramework/Actor.h"
#include "Input/InputManager.h"
#include "Input/InputModule.h"
#include "Main/MainModule.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Widget/WidgetModuleTypes.h"
#include "Widget/Screen/Slate/SSlateWidgetBase.h"
#include "Widget/Screen/UMG/UserWidgetBase.h"
#include "World/WorldWidgetBase.h"
#include "WidgetModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AWidgetModule : public AModuleBase, public IInputManager
{
	GENERATED_BODY()
	
public:	
	AWidgetModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif
	
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

	////////////////////////////////////////////////////
	// UserWidget
protected:
	UPROPERTY(EditAnywhere, Category = "UserWidget")
	TArray<TSubclassOf<UUserWidgetBase>> UserWidgetClasses;

private:
	UPROPERTY(Transient)
	UUserWidgetBase* MainUserWidget;

	UPROPERTY(Transient)
	UUserWidgetBase* TemporaryUserWidget;

	UPROPERTY()
	TMap<FName, UUserWidgetBase*> AllUserWidgets;

	UPROPERTY(Transient)
	TMap<FName, TSubclassOf<UUserWidgetBase>> UserWidgetClassMap;

public:
	template<class T>
	T* GetMainUserWidget() const { return Cast<T>(MainUserWidget); }
	
	UFUNCTION(BlueprintPure)
	UUserWidgetBase* GetMainUserWidget() const { return MainUserWidget; }

	template<class T>
	T* GetTemporaryUserWidget() const { return Cast<T>(TemporaryUserWidget); }
	
	UFUNCTION(BlueprintPure)
	UUserWidgetBase* GetTemporaryUserWidget() const { return TemporaryUserWidget; }

	template<class T>
	bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass()) const
	{
		if(!InWidgetClass) return false;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		return HasUserWidgetClassByName(WidgetName);
	}

	UFUNCTION(BlueprintPure)
	bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InWidgetClass) const;

	UFUNCTION(BlueprintPure)
	bool HasUserWidgetClassByName(FName InWidgetName) const
	{
		return UserWidgetClassMap.Contains(InWidgetName);
	}

	UFUNCTION(BlueprintPure)
	TSubclassOf<UUserWidgetBase> GetUserWidgetClassByName(FName InWidgetName) const
	{
		if(HasUserWidgetClassByName(InWidgetName))
		{
			return UserWidgetClassMap[InWidgetName];
		}
		return nullptr;
	}

	template<class T>
	bool HasUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass()) const
	{
		if(!InWidgetClass) return false;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		return HasUserWidgetByName(WidgetName);
	}

	UFUNCTION(BlueprintPure)
	bool HasUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass) const;

	UFUNCTION(BlueprintPure)
	bool HasUserWidgetByName(FName InWidgetName) const
	{
		return AllUserWidgets.Contains(InWidgetName);
	}

	template<class T>
	T* GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass()) const
	{
		if(!InWidgetClass) return nullptr;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		return GetUserWidgetByName<T>(WidgetName);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InWidgetClass"))
	UUserWidgetBase* GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass) const;

	template<class T>
	T* GetUserWidgetByName(FName InWidgetName) const
	{
		if(AllUserWidgets.Contains(InWidgetName))
		{
			return Cast<T>(AllUserWidgets[InWidgetName]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InWidgetClass"))
	UUserWidgetBase* GetUserWidgetByName(FName InWidgetName, TSubclassOf<UUserWidgetBase> InWidgetClass) const;

	template<class T>
	T* CreateUserWidget(AActor* InOwner = nullptr, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return nullptr;
		
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		
		return CreateUserWidgetByName<T>(WidgetName, InOwner);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InWidgetClass"))
	UUserWidgetBase* CreateUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner = nullptr);

	template<class T>
	T* CreateUserWidgetByName(FName InWidgetName, AActor* InOwner = nullptr)
	{
		if(InWidgetName.IsNone()) return nullptr;
		
		if(!UserWidgetClassMap.Contains(InWidgetName))
		{
			ensureEditor(true);
			WHLog(WH_Widget, Warning, TEXT("Failed to create user widget. Module does not contain this type: %s"), *InWidgetName.ToString());
			return nullptr;
		}
		
		if(!AllUserWidgets.Contains(InWidgetName))
		{
			if(UUserWidgetBase* UserWidget = UObjectPoolModuleBPLibrary::SpawnObject<UUserWidgetBase>(nullptr, UserWidgetClassMap[InWidgetName]))
			{
				AllUserWidgets.Add(InWidgetName, UserWidget);
				UserWidget->OnCreate();
				UserWidget->OnInitialize(InOwner);
				return Cast<T>(UserWidget);
			}
		}
		else
		{
			return Cast<T>(AllUserWidgets[InWidgetName]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable)
	UUserWidgetBase* CreateUserWidgetByName(FName InWidgetName, AActor* InOwner = nullptr);

	template<class T>
	bool InitializeUserWidget(AActor* InOwner, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();

		return InitializeUserWidgetByName(WidgetName, InOwner);
	}

	UFUNCTION(BlueprintCallable)
	bool InitializeUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner = nullptr);

	UFUNCTION(BlueprintCallable)
	bool InitializeUserWidgetByName(FName InWidgetName, AActor* InOwner)
	{
		if(UUserWidgetBase* UserWidget = GetUserWidgetByName<UUserWidgetBase>(InWidgetName))
		{
			UserWidget->OnInitialize(InOwner);
			return true;
		}
		return false;
	}

	template<class T>
	bool OpenUserWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();

		return OpenUserWidgetByName(WidgetName, InParams, bInstant);
	}

	template<class T>
	bool OpenUserWidget(const TArray<FParameter>& InParams, bool bInstant = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();

		return OpenUserWidgetByName(WidgetName, InParams, bInstant);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool OpenUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, const TArray<FParameter>& InParams, bool bInstant = false);

	bool OpenUserWidgetByName(FName InWidgetName, const TArray<FParameter>* InParams = nullptr, bool bInstant = false)
	{
		if(UUserWidgetBase* UserWidget = HasUserWidgetByName(InWidgetName) ? GetUserWidgetByName<UUserWidgetBase>(InWidgetName) : CreateUserWidgetByName<UUserWidgetBase>(InWidgetName))
		{
			if(UserWidget->GetWidgetState() != EScreenWidgetState::Opening && UserWidget->GetWidgetState() != EScreenWidgetState::Opened)
			{
				if(UserWidget->GetWidgetType() == EWidgetType::Main)
				{
					MainUserWidget = UserWidget;
				}
				if(!UserWidget->GetParentWidgetN() && UserWidget->GetWidgetCategory() == EWidgetCategory::Temporary)
				{
					if(TemporaryUserWidget)
					{
						TemporaryUserWidget->OnClose(true);
					}
					UserWidget->SetLastWidget(TemporaryUserWidget);
					TemporaryUserWidget = UserWidget;
				}
				UserWidget->OnOpen(InParams ? *InParams : TArray<FParameter>(), bInstant);
				return true;
			}
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool OpenUserWidgetByName(FName InWidgetName, const TArray<FParameter>& InParams, bool bInstant = false);

	template<class T>
	bool CloseUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();

		return CloseUserWidgetByName(WidgetName, bInstant);
	}

	UFUNCTION(BlueprintCallable)
	bool CloseUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	bool CloseUserWidgetByName(FName InWidgetName, bool bInstant = false)
	{
		if(UUserWidgetBase* UserWidget = GetUserWidgetByName<UUserWidgetBase>(InWidgetName))
		{
			if(UserWidget->GetWidgetState() != EScreenWidgetState::Closing && UserWidget->GetWidgetState() != EScreenWidgetState::Closed)
			{
				if(UserWidget->GetWidgetType() == EWidgetType::Main)
				{
					MainUserWidget = nullptr;
				}
				if(!UserWidget->GetParentWidgetN() && UserWidget->GetWidgetCategory() == EWidgetCategory::Temporary)
				{
					TemporaryUserWidget = nullptr;
				}
				UserWidget->OnClose(bInstant);
			}
			return true;
		}
		return false;
	}

	template<class T>
	bool ToggleUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();

		return ToggleUserWidgetByName(WidgetName, bInstant);
	}

	UFUNCTION(BlueprintCallable)
	bool ToggleUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	bool ToggleUserWidgetByName(FName InWidgetName, bool bInstant = false)
	{
		if(UUserWidgetBase* UserWidget = GetUserWidgetByName<UUserWidgetBase>(InWidgetName))
		{
			UserWidget->Toggle(bInstant);
			return true;
		}
		return false;
	}

	template<class T>
	bool DestroyUserWidget(bool bRecovery = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();

		return DestroyUserWidgetByName(WidgetName, bRecovery);
	}

	UFUNCTION(BlueprintCallable)
	bool DestroyUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass,  bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	bool DestroyUserWidgetByName(FName InWidgetName,  bool bRecovery = false)
	{
		if(AllUserWidgets.Contains(InWidgetName))
		{
			if(UUserWidgetBase* UserWidget = AllUserWidgets[InWidgetName])
			{
				AllUserWidgets.Remove(InWidgetName);
				if(TemporaryUserWidget == UserWidget)
				{
					TemporaryUserWidget = nullptr;
				}
				UserWidget->OnDestroy(bRecovery);
			}
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable)
	void CloseAllUserWidget(bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	void ClearAllUserWidget();

	////////////////////////////////////////////////////
	// SlateWidget
protected:
	TMap<FName, TSharedPtr<class SSlateWidgetBase>> AllSlateWidgets;

	TSharedPtr<class SSlateWidgetBase> TemporarySlateWidget;

public:
	template<class T>
	bool HasSlateWidget() const
	{
		// const FName WidgetName = typeid(T).name();
		// return AllSlateWidgets.Contains(WidgetName);
		return false;
	}

	template<class T>
	TSharedPtr<T> GetSlateWidget() const
	{
		// const FName WidgetName = typeid(T).name();
		// if(AllSlateWidgets.Contains(WidgetName))
		// {
		// 	return AllSlateWidgets[WidgetName];
		// }
		return nullptr;
	}

	template<class T>
	TSharedPtr<T> CreateSlateWidget(AActor* InOwner)
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = &SNew(T))
		{
			SlateWidget->OnCreate();
			SlateWidget->OnInitialize(InOwner);
			// const FName WidgetName = typeid(SlateWidget).name();
			// if(!AllSlateWidgets.Contains(WidgetName))
			// {
			// 	AllSlateWidgets.Add(WidgetName, SlateWidget);
			// }
			return Cast<T>(SlateWidget);
		}
		return nullptr;
	}

	template<class T>
	bool InitializeSlateWidget(AActor* InOwner)
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = (TSharedPtr<SSlateWidgetBase>)(HasSlateWidget<T>() ? GetSlateWidget<T>() : CreateSlateWidget<T>(InOwner)))
		{
			SlateWidget->OnInitialize(InOwner);
			return true;
		}
		return false;
	}

	template<class T>
	bool OpenSlateWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false)
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = (TSharedPtr<SSlateWidgetBase>)(HasSlateWidget<T>() ? GetSlateWidget<T>() : CreateSlateWidget<T>(nullptr)))
		{
			if(!SlateWidget->GetParentWidgetN() && SlateWidget->GetWidgetCategory() == EWidgetCategory::Temporary)
			{
				if(TemporarySlateWidget)
				{
					TemporarySlateWidget->OnClose(true);
				}
				//SlateWidget->SetLastWidget(TemporarySlateWidget);
				TemporarySlateWidget = SlateWidget;
			}
			SlateWidget->OnOpen(*InParams, bInstant);
			return true;
		}
		return false;
	}
	
	template<class T>
	bool CloseSlateWidget(bool bInstant = false)
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = GetSlateWidget<T>())
		{
			if(!SlateWidget->GetParentWidgetN() && SlateWidget->GetWidgetCategory() == EWidgetCategory::Temporary)
			{
				TemporarySlateWidget = nullptr;
			}
			SlateWidget->OnClose(bInstant);
			return true;
		}
		return false;
	}
	
	template<class T>
	bool ToggleSlateWidget(bool bInstant = false)
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = GetSlateWidget<T>())
		{
			SlateWidget->Toggle(bInstant);
			return true;
		}
		return false;
	}

	template<class T>
	bool DestroySlateWidget()
	{
		// const FName WidgetName = typeid(T).name();
		// if(AllSlateWidgets.Contains(WidgetName))
		// {
		// 	if(TSharedPtr<SSlateWidgetBase> SlateWidget = AllSlateWidgets[WidgetName])
		// 	{
		// 		AllSlateWidgets.Remove(WidgetName);
		// 		if(TemporarySlateWidget == SlateWidget)
		// 		{
		// 			TemporarySlateWidget = nullptr;
		// 		}
		// 		SlateWidget->OnDestroy();
		// 		SlateWidget = nullptr;
		// 	}
		// 	return true;
		// }
		return false;
	}

	void CloseAllSlateWidget(bool bInstant = false);
	
	void ClearAllSlateWidget();

	////////////////////////////////////////////////////
	// Widget
protected:
	UPROPERTY(EditAnywhere, Category = "WorldWidget")
	TArray<TSubclassOf<UWorldWidgetBase>> WorldWidgetClasses;

	UPROPERTY(EditAnywhere, Category = "WorldWidget")
	TSubclassOf<class UWorldWidgetContainer> WorldWidgetContainerClass;
	
	UPROPERTY(EditAnywhere, Category = "WorldWidget")
	int32 WorldWidgetContainerZOrder;
	
private:
	UPROPERTY(Transient)
	TMap<FName, FWorldWidgets> AllWorldWidgets;

	UPROPERTY(Transient)
	class UWorldWidgetContainer* WorldWidgetContainer;

	UPROPERTY(Transient)
	TMap<FName, TSubclassOf<UWorldWidgetBase>> WorldWidgetClassMap;

public:
	UFUNCTION(BlueprintPure)
	UWorldWidgetContainer* GetWorldWidgetContainer() const { return WorldWidgetContainer; }

	template<class T>
	bool HasWorldWidget(int32 InWidgetIndex, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass()) const
	{
		if(!InWidgetClass) return false;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		
		return HasWorldWidgetByName(WidgetName, InWidgetIndex);
	}

	UFUNCTION(BlueprintPure)
	bool HasWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex) const;

	UFUNCTION(BlueprintPure)
	bool HasWorldWidgetByName(FName InWidgetName, int32 InWidgetIndex) const
	{
		return AllWorldWidgets.Contains(InWidgetName) && AllWorldWidgets[InWidgetName].WorldWidgets.IsValidIndex(InWidgetIndex);
	}

	template<class T>
	T* GetWorldWidget(int32 InWidgetIndex, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass()) const
	{
		if(!InWidgetClass) return nullptr;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		return GetWorldWidgetByName<T>(WidgetName, InWidgetIndex);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InWidgetClass"))
	UWorldWidgetBase* GetWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex) const;

	template<class T>
	T* GetWorldWidgetByName(FName InWidgetName, int32 InWidgetIndex) const
	{
		if(AllWorldWidgets.Contains(InWidgetName) && AllWorldWidgets[InWidgetName].WorldWidgets.IsValidIndex(InWidgetIndex))
		{
			return Cast<T>(AllWorldWidgets[InWidgetName].WorldWidgets[InWidgetIndex]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InWidgetClass"))
	UWorldWidgetBase* GetWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex) const;

	template<class T>
	TArray<T*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass()) const
	{
		if(!InWidgetClass) return TArray<T*>();

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		return GetWorldWidgetsByName<T>(WidgetName);
	}

	UFUNCTION(BlueprintPure)
	TArray<UWorldWidgetBase*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass) const;

	template<class T>
	TArray<T*> GetWorldWidgetsByName(FName InWidgetName) const
	{
		if(InWidgetName.IsNone()) return TArray<T*>();

		TArray<T*> Widgets;
		if(AllWorldWidgets.Contains(InWidgetName))
		{
			for(auto Iter : AllWorldWidgets[InWidgetName].WorldWidgets)
			{
				Widgets.Add(Cast<T>(Iter));
			}
		}
		return Widgets;
	}

	UFUNCTION(BlueprintPure)
	TArray<UWorldWidgetBase*> GetWorldWidgetsByName(FName InWidgetName) const;

	template<class T>
	T* CreateWorldWidget(AActor* InOwner, FVector InLocation = FVector::ZeroVector, class USceneComponent* InSceneComp = nullptr, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return nullptr;
		
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		
		return CreateWorldWidgetByName<T>(WidgetName, InOwner, InLocation, InSceneComp, InParams, InWidgetClass);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InWidgetClass", AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, class USceneComponent* InSceneComp, const TArray<FParameter>& InParams);

	template<class T>
	T* CreateWorldWidgetByName(FName InWidgetName, AActor* InOwner, FVector InLocation = FVector::ZeroVector, class USceneComponent* InSceneComp = nullptr, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!WorldWidgetClassMap.Contains(InWidgetName))
		{
			ensureEditor(true);
			WHLog(WH_Widget, Warning, TEXT("Failed to create world widget. Module does not contain this type: %s"), *InWidgetName.ToString());
			return nullptr;
		}
		
		if(UWorldWidgetBase* WorldWidget = UObjectPoolModuleBPLibrary::SpawnObject<UWorldWidgetBase>(nullptr, WorldWidgetClassMap[InWidgetName]))
		{
			if(!AllWorldWidgets.Contains(InWidgetName))
			{
				AllWorldWidgets.Add(InWidgetName);
			}
			WorldWidget->SetWidgetIndex(AllWorldWidgets[InWidgetName].WorldWidgets.Add(WorldWidget));
			WorldWidget->OnCreate(InOwner, InLocation, InSceneComp, *InParams);
			return Cast<T>(WorldWidget);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InWidgetClass", AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* CreateWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, class USceneComponent* InSceneComp, const TArray<FParameter>& InParams);

	template<class T>
	bool DestroyWorldWidget(int32 InWidgetIndex, bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return false;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		
		return DestroyWorldWidgetByName(WidgetName, InWidgetIndex, bRecovery);
	}

	UFUNCTION(BlueprintCallable)
	bool DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	bool DestroyWorldWidgetByName(FName InWidgetName, int32 InWidgetIndex, bool bRecovery = false);

	template<class T>
	void DestroyWorldWidgets(bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		
		DestroyWorldWidgetsByName(WidgetName);
	}

	UFUNCTION(BlueprintCallable)
	void DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	void DestroyWorldWidgetsByName(FName InWidgetName, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	void ClearAllWorldWidget();

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	virtual EInputMode GetNativeInputMode() const override;
};
