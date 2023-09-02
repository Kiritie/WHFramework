// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


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
#include "Debug/DebugModuleTypes.h"
#include "WidgetModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AWidgetModule : public AModuleBase, public IInputManager
{
	GENERATED_BODY()
			
	GENERATED_MODULE(AWidgetModule)

public:	
	AWidgetModule();

	~AWidgetModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

	////////////////////////////////////////////////////
	// UserWidget
protected:
	UPROPERTY(EditAnywhere, Category = "UserWidget")
	TArray<TSubclassOf<UUserWidgetBase>> UserWidgetClasses;

private:
	UPROPERTY(Transient)
	UUserWidgetBase* TemporaryUserWidget;

	UPROPERTY()
	TMap<FName, UUserWidgetBase*> AllUserWidgets;

	UPROPERTY(Transient)
	TMap<FName, TSubclassOf<UUserWidgetBase>> UserWidgetClassMap;

public:
	template<class T>
	T* GetTemporaryUserWidget() const { return Cast<T>(TemporaryUserWidget); }
	
	UFUNCTION(BlueprintPure)
	UUserWidgetBase* GetTemporaryUserWidget() const { return TemporaryUserWidget; }

	template<class T>
	bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass()) const
	{
		if(!InClass) return false;

		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		return HasUserWidgetClassByName(WidgetName);
	}

	UFUNCTION(BlueprintPure)
	bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass) const;

	UFUNCTION(BlueprintPure)
	bool HasUserWidgetClassByName(FName InName) const
	{
		return UserWidgetClassMap.Contains(InName);
	}

	UFUNCTION(BlueprintPure)
	TSubclassOf<UUserWidgetBase> GetUserWidgetClassByName(FName InName) const
	{
		if(HasUserWidgetClassByName(InName))
		{
			return UserWidgetClassMap[InName];
		}
		return nullptr;
	}

	template<class T>
	bool HasUserWidget(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass()) const
	{
		if(!InClass) return false;

		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		return HasUserWidgetByName(WidgetName);
	}

	UFUNCTION(BlueprintPure)
	bool HasUserWidget(TSubclassOf<UUserWidgetBase> InClass) const;

	UFUNCTION(BlueprintPure)
	bool HasUserWidgetByName(FName InName) const
	{
		return AllUserWidgets.Contains(InName);
	}

	template<class T>
	T* GetUserWidget(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass()) const
	{
		if(!InClass) return nullptr;

		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		return GetUserWidgetByName<T>(WidgetName);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UUserWidgetBase* GetUserWidget(TSubclassOf<UUserWidgetBase> InClass) const;

	template<class T>
	T* GetUserWidgetByName(FName InName) const
	{
		if(AllUserWidgets.Contains(InName))
		{
			return Cast<T>(AllUserWidgets[InName]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UUserWidgetBase* GetUserWidgetByName(FName InName, TSubclassOf<UUserWidgetBase> InClass) const;

	template<class T>
	T* CreateUserWidget(AActor* InOwner = nullptr, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass) return nullptr;
		
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		
		return CreateUserWidgetByName<T>(WidgetName, InOwner);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"))
	UUserWidgetBase* CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, AActor* InOwner = nullptr);

	template<class T>
	T* CreateUserWidgetByName(FName InName, AActor* InOwner = nullptr)
	{
		if(InName.IsNone()) return nullptr;
		
		if(!UserWidgetClassMap.Contains(InName))
		{
			ensureEditor(true);
			WHLog(FString::Printf(TEXT("Failed to create user widget. Module does not contain this type: %s"), *InName.ToString()), EDebugCategory::Widget, EDebugVerbosity::Warning);
			return nullptr;
		}
		
		if(!AllUserWidgets.Contains(InName))
		{
			if(UUserWidgetBase* UserWidget = UObjectPoolModuleBPLibrary::SpawnObject<UUserWidgetBase>(nullptr, UserWidgetClassMap[InName]))
			{
				AllUserWidgets.Add(InName, UserWidget);
				UserWidget->OnCreate();
				UserWidget->OnInitialize(InOwner);
				return Cast<T>(UserWidget);
			}
		}
		else
		{
			return Cast<T>(AllUserWidgets[InName]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable)
	UUserWidgetBase* CreateUserWidgetByName(FName InName, AActor* InOwner = nullptr);

	template<class T>
	bool InitializeUserWidget(AActor* InOwner, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();

		return InitializeUserWidgetByName(WidgetName, InOwner);
	}

	UFUNCTION(BlueprintCallable)
	bool InitializeUserWidget(TSubclassOf<UUserWidgetBase> InClass, AActor* InOwner = nullptr);

	UFUNCTION(BlueprintCallable)
	bool InitializeUserWidgetByName(FName InName, AActor* InOwner)
	{
		if(UUserWidgetBase* UserWidget = GetUserWidgetByName<UUserWidgetBase>(InName))
		{
			UserWidget->OnInitialize(InOwner);
			return true;
		}
		return false;
	}

	template<class T>
	bool OpenUserWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();

		return OpenUserWidgetByName(WidgetName, InParams, bInstant);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams, bool bInstant = false);

	bool OpenUserWidgetByName(FName InName, const TArray<FParameter>* InParams = nullptr, bool bInstant = false)
	{
		if(UUserWidgetBase* UserWidget = HasUserWidgetByName(InName) ? GetUserWidgetByName<UUserWidgetBase>(InName) : CreateUserWidgetByName<UUserWidgetBase>(InName))
		{
			if(UserWidget->GetWidgetState() != EScreenWidgetState::Opening && UserWidget->GetWidgetState() != EScreenWidgetState::Opened)
			{
				if(!UserWidget->GetParentWidgetN())
				{
					if(UserWidget->GetWidgetType() == EWidgetType::Temporary)
					{
						if(TemporaryUserWidget)
						{
							TemporaryUserWidget->Close(true);
							UserWidget->SetLastTemporary(TemporaryUserWidget);
						}
						TemporaryUserWidget = UserWidget;
					}
				}
				else
				{
					if(UserWidget->GetWidgetType(false) == EWidgetType::Temporary)
					{
						if(UserWidget->GetParentWidgetN()->GetTemporaryChild())
						{
							UserWidget->GetParentWidgetN()->GetTemporaryChild()->Close(true);
							UserWidget->SetLastTemporary(UserWidget->GetParentWidgetN()->GetTemporaryChild());
						}
						UserWidget->GetParentWidgetN()->SetTemporaryChild(UserWidget);
					}
				}
				UserWidget->OnOpen(InParams ? *InParams : TArray<FParameter>(), bInstant);
			}
			else
			{
				UserWidget->OnOpen(InParams ? *InParams : TArray<FParameter>(), true);
			}
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool OpenUserWidgetByName(FName InName, const TArray<FParameter>& InParams, bool bInstant = false);

	template<class T>
	bool CloseUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();

		return CloseUserWidgetByName(WidgetName, bInstant);
	}

	UFUNCTION(BlueprintCallable)
	bool CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	bool CloseUserWidgetByName(FName InName, bool bInstant = false)
	{
		if(UUserWidgetBase* UserWidget = GetUserWidgetByName<UUserWidgetBase>(InName))
		{
			if(UserWidget->GetWidgetState() != EScreenWidgetState::Closing && UserWidget->GetWidgetState() != EScreenWidgetState::Closed)
			{
				if(!UserWidget->GetParentWidgetN())
				{
					if(UserWidget->GetWidgetType() == EWidgetType::Temporary)
					{
						TemporaryUserWidget = nullptr;
					}
				}
				else
				{
					if(UserWidget->GetWidgetType(false) == EWidgetType::Temporary)
					{
						UserWidget->GetParentWidgetN()->SetTemporaryChild(nullptr);
					}
				}
				UserWidget->OnClose(bInstant);
			}
			return true;
		}
		return false;
	}

	template<class T>
	bool ToggleUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();

		return ToggleUserWidgetByName(WidgetName, bInstant);
	}

	UFUNCTION(BlueprintCallable)
	bool ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	bool ToggleUserWidgetByName(FName InName, bool bInstant = false)
	{
		if(UUserWidgetBase* UserWidget = GetUserWidgetByName<UUserWidgetBase>(InName))
		{
			UserWidget->Toggle(bInstant);
			return true;
		}
		return false;
	}

	template<class T>
	bool DestroyUserWidget(bool bRecovery = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();

		return DestroyUserWidgetByName(WidgetName, bRecovery);
	}

	UFUNCTION(BlueprintCallable)
	bool DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass,  bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	bool DestroyUserWidgetByName(FName InName,  bool bRecovery = false)
	{
		if(AllUserWidgets.Contains(InName))
		{
			if(UUserWidgetBase* UserWidget = AllUserWidgets[InName])
			{
				AllUserWidgets.Remove(InName);
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
			if(!SlateWidget->GetParentWidgetN() && SlateWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				if(TemporarySlateWidget)
				{
					TemporarySlateWidget->OnClose(true);
				}
				//SlateWidget->SetLastTemporary(TemporarySlateWidget);
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
			if(!SlateWidget->GetParentWidgetN() && SlateWidget->GetWidgetType() == EWidgetType::Temporary)
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
	bool HasWorldWidget(int32 InIndex, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass()) const
	{
		if(!InClass) return false;

		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		
		return HasWorldWidgetByName(WidgetName, InIndex);
	}

	UFUNCTION(BlueprintPure)
	bool HasWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex) const;

	UFUNCTION(BlueprintPure)
	bool HasWorldWidgetByName(FName InName, int32 InIndex) const
	{
		return AllWorldWidgets.Contains(InName) && AllWorldWidgets[InName].WorldWidgets.IsValidIndex(InIndex);
	}

	template<class T>
	T* GetWorldWidget(int32 InIndex, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass()) const
	{
		if(!InClass) return nullptr;

		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		return GetWorldWidgetByName<T>(WidgetName, InIndex);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UWorldWidgetBase* GetWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex) const;

	template<class T>
	T* GetWorldWidgetByName(FName InName, int32 InIndex) const
	{
		if(AllWorldWidgets.Contains(InName) && AllWorldWidgets[InName].WorldWidgets.IsValidIndex(InIndex))
		{
			return Cast<T>(AllWorldWidgets[InName].WorldWidgets[InIndex]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UWorldWidgetBase* GetWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex) const;

	template<class T>
	TArray<T*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass()) const
	{
		if(!InClass) return TArray<T*>();

		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		return GetWorldWidgetsByName<T>(WidgetName);
	}

	UFUNCTION(BlueprintPure)
	TArray<UWorldWidgetBase*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass) const;

	template<class T>
	TArray<T*> GetWorldWidgetsByName(FName InName) const
	{
		if(InName.IsNone()) return TArray<T*>();

		TArray<T*> Widgets;
		if(AllWorldWidgets.Contains(InName))
		{
			for(auto Iter : AllWorldWidgets[InName].WorldWidgets)
			{
				Widgets.Add(Cast<T>(Iter));
			}
		}
		return Widgets;
	}

	UFUNCTION(BlueprintPure)
	TArray<UWorldWidgetBase*> GetWorldWidgetsByName(FName InName) const;

	template<class T>
	T* CreateWorldWidget(AActor* InOwner, FVector InLocation = FVector::ZeroVector, class USceneComponent* InSceneComp = nullptr, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass) return nullptr;
		
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		
		return CreateWorldWidgetByName<T>(WidgetName, InOwner, InLocation, InSceneComp, InParams, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, AActor* InOwner, FVector InLocation, class USceneComponent* InSceneComp, const TArray<FParameter>& InParams);

	template<class T>
	T* CreateWorldWidgetByName(FName InName, AActor* InOwner, FVector InLocation = FVector::ZeroVector, class USceneComponent* InSceneComp = nullptr, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(!WorldWidgetClassMap.Contains(InName))
		{
			ensureEditor(false);
			WHLog(FString::Printf(TEXT("Failed to create world widget. Module does not contain this type: %s"), *InName.ToString()), EDebugCategory::Widget, EDebugVerbosity::Warning);
			return nullptr;
		}
		
		if(UWorldWidgetBase* WorldWidget = UObjectPoolModuleBPLibrary::SpawnObject<UWorldWidgetBase>(nullptr, WorldWidgetClassMap[InName]))
		{
			if(!AllWorldWidgets.Contains(InName))
			{
				AllWorldWidgets.Add(InName);
			}
			WorldWidget->SetWidgetIndex(AllWorldWidgets[InName].WorldWidgets.Add(WorldWidget));
			WorldWidget->OnCreate(InOwner, InLocation, InSceneComp, *InParams);
			return Cast<T>(WorldWidget);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* CreateWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, AActor* InOwner, FVector InLocation, class USceneComponent* InSceneComp, const TArray<FParameter>& InParams);

	bool DestroyWorldWidget(UWorldWidgetBase* InWidget, bool bRecovery = false)
	{
		if(!InWidget) return false;

		const FName WidgetName = InWidget->GetWidgetName();
		
		return DestroyWorldWidgetByName(WidgetName, InWidget->GetWidgetIndex(), bRecovery);
	}

	template<class T>
	bool DestroyWorldWidget(int32 InIndex, bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass) return false;

		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		
		return DestroyWorldWidgetByName(WidgetName, InIndex, bRecovery);
	}

	UFUNCTION(BlueprintCallable)
	bool DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	bool DestroyWorldWidgetByName(FName InName, int32 InIndex, bool bRecovery = false);

	template<class T>
	void DestroyWorldWidgets(bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass) return;

		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		
		DestroyWorldWidgetsByName(WidgetName, bRecovery);
	}

	UFUNCTION(BlueprintCallable)
	void DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	void DestroyWorldWidgetsByName(FName InName, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	void ClearAllWorldWidget();

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	virtual EInputMode GetNativeInputMode() const override;
};
