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
	
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetMainUserWidget"))
	UUserWidgetBase* K2_GetMainUserWidget() const { return MainUserWidget; }

	template<class T>
	T* GetTemporaryUserWidget() const { return Cast<T>(TemporaryUserWidget); }
	
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetTemporaryUserWidget"))
	UUserWidgetBase* K2_GetTemporaryUserWidget() const { return TemporaryUserWidget; }

	template<class T>
	bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass()) const
	{
		if(!InWidgetClass) return false;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		return HasUserWidgetClassByName(WidgetName);
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasUserWidgetClass"))
	bool K2_HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InWidgetClass) const;

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasUserWidgetClassByName"))
	bool HasUserWidgetClassByName(FName InWidgetName) const
	{
		return UserWidgetClassMap.Contains(InWidgetName);
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetUserWidgetClassByName"))
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

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasUserWidget"))
	bool K2_HasUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass) const;

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasUserWidgetByName"))
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

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetUserWidget", DeterminesOutputType = "InWidgetClass"))
	UUserWidgetBase* K2_GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass) const;

	template<class T>
	T* GetUserWidgetByName(FName InWidgetName) const
	{
		if(AllUserWidgets.Contains(InWidgetName))
		{
			return Cast<T>(AllUserWidgets[InWidgetName]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetUserWidgetByName", DeterminesOutputType = "InWidgetClass"))
	UUserWidgetBase* K2_GetUserWidgetByName(FName InWidgetName, TSubclassOf<UUserWidgetBase> InWidgetClass) const;

	template<class T>
	T* CreateUserWidget(AActor* InOwner = nullptr, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return nullptr;
		
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		
		return CreateUserWidgetByName<T>(WidgetName, InOwner);
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateUserWidget", DeterminesOutputType = "InWidgetClass"))
	UUserWidgetBase* K2_CreateUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner = nullptr);

	template<class T>
	T* CreateUserWidgetByName(FName InWidgetName, AActor* InOwner = nullptr)
	{
		if(InWidgetName.IsNone()) return nullptr;
		
		if(!UserWidgetClassMap.Contains(InWidgetName)) return nullptr;
		
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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateUserWidgetByName"))
	UUserWidgetBase* K2_CreateUserWidgetByName(FName InWidgetName, AActor* InOwner = nullptr);

	template<class T>
	bool InitializeUserWidget(AActor* InOwner, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();

		return InitializeUserWidgetByName(WidgetName, InOwner);
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "InitializeUserWidget"))
	bool K2_InitializeUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner = nullptr);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "InitializeUserWidgetByName"))
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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "OpenUserWidget", AutoCreateRefTerm = "InParams"))
	bool K2_OpenUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, const TArray<FParameter>& InParams, bool bInstant = false);

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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "OpenUserWidgetByName", AutoCreateRefTerm = "InParams"))
	bool K2_OpenUserWidgetByName(FName InWidgetName, const TArray<FParameter>& InParams, bool bInstant = false);

	template<class T>
	bool CloseUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();

		return CloseUserWidgetByName(WidgetName, bInstant);
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CloseUserWidget"))
	bool K2_CloseUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CloseUserWidgetByName"))
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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ToggleUserWidget"))
	bool K2_ToggleUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ToggleUserWidgetByName"))
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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyUserWidget"))
	bool K2_DestroyUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass,  bool bRecovery = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyUserWidgetByName"))
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

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasWorldWidget"))
	bool K2_HasWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex) const;

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasWorldWidgetByName"))
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

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetWorldWidget", DeterminesOutputType = "InWidgetClass"))
	UWorldWidgetBase* K2_GetWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex) const;

	template<class T>
	T* GetWorldWidgetByName(FName InWidgetName, int32 InWidgetIndex) const
	{
		if(AllWorldWidgets.Contains(InWidgetName) && AllWorldWidgets[InWidgetName].WorldWidgets.IsValidIndex(InWidgetIndex))
		{
			return Cast<T>(AllWorldWidgets[InWidgetName].WorldWidgets[InWidgetIndex]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetWorldWidgetByName", DeterminesOutputType = "InWidgetClass"))
	UWorldWidgetBase* K2_GetWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex) const;

	template<class T>
	TArray<T*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass()) const
	{
		if(!InWidgetClass) return TArray<T*>();

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		return GetWorldWidgetsByName<T>(WidgetName);
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetWorldWidgets"))
	TArray<UWorldWidgetBase*> K2_GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass) const;

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

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetWorldWidgetsByName"))
	TArray<UWorldWidgetBase*> K2_GetWorldWidgetsByName(FName InWidgetName) const;

	template<class T>
	T* CreateWorldWidget(AActor* InOwner, FVector InLocation = FVector::ZeroVector, class USceneComponent* InSceneComp = nullptr, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return nullptr;
		
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		
		return CreateWorldWidgetByName<T>(WidgetName, InOwner, InLocation, InSceneComp, InParams, InWidgetClass);
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateWorldWidget", DeterminesOutputType = "InWidgetClass", AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* K2_CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, class USceneComponent* InSceneComp, const TArray<FParameter>& InParams);

	template<class T>
	T* CreateWorldWidgetByName(FName InWidgetName, AActor* InOwner, FVector InLocation = FVector::ZeroVector, class USceneComponent* InSceneComp = nullptr, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!WorldWidgetClassMap.Contains(InWidgetName)) return nullptr;

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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateWorldWidgetByName", DeterminesOutputType = "InWidgetClass", AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* K2_CreateWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, class USceneComponent* InSceneComp, const TArray<FParameter>& InParams);

	template<class T>
	bool DestroyWorldWidget(int32 InWidgetIndex, bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return false;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		
		return DestroyWorldWidgetByName(WidgetName, InWidgetIndex, bRecovery);
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyWorldWidget"))
	bool K2_DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex, bool bRecovery = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyWorldWidgetByName"))
	bool DestroyWorldWidgetByName(FName InWidgetName, int32 InWidgetIndex, bool bRecovery = false)
	{
		if(AllWorldWidgets.Contains(InWidgetName) && AllWorldWidgets[InWidgetName].WorldWidgets.IsValidIndex(InWidgetIndex))
		{
			if(UWorldWidgetBase* WorldWidget = AllWorldWidgets[InWidgetName].WorldWidgets[InWidgetIndex])
			{
				AllWorldWidgets[InWidgetName].WorldWidgets.RemoveAt(InWidgetIndex);

				if(AllWorldWidgets[InWidgetName].WorldWidgets.Num() > 0)
				{
					for(int32 i = 0; i < AllWorldWidgets[InWidgetName].WorldWidgets.Num(); i++)
					{
						AllWorldWidgets[InWidgetName].WorldWidgets[i]->SetWidgetIndex(i);
					}
				}
				else
				{
					AllWorldWidgets.Remove(InWidgetName);
				}
				WorldWidget->OnDestroy(bRecovery);
			}
			return true;
		}
		return false;
	}

	template<class T>
	void DestroyWorldWidgets(bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		
		DestroyWorldWidgetsByName(WidgetName);
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyWorldWidgets"))
	void K2_DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyWorldWidgets"))
	void DestroyWorldWidgetsByName(FName InWidgetName, bool bRecovery = false)
	{
		if(AllWorldWidgets.Contains(InWidgetName))
		{
			for(auto Iter : AllWorldWidgets[InWidgetName].WorldWidgets)
			{
				if(Iter)
				{
					Iter->OnDestroy(bRecovery);
				}
			}
			AllWorldWidgets.Remove(InWidgetName);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	virtual EInputMode GetNativeInputMode() const override;
};
