// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Base/ModuleBase.h"
#include "GameFramework/Actor.h"
#include "Input/InputManager.h"
#include "Input/InputModule.h"
#include "Main/MainModule.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Widget/WidgetModuleTypes.h"
#include "Widget/Slate/SSlateWidgetBase.h"
#include "Widget/User/UserWidgetBase.h"
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

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	////////////////////////////////////////////////////
	// UserWidget
protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<TSubclassOf<UUserWidgetBase>> UserWidgetClasses;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TMap<FName, UUserWidgetBase*> AllUserWidgets;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	UUserWidgetBase* TemporaryUserWidget;

	TMap<FName, TSubclassOf<UUserWidgetBase>> UserWidgetClassMap;

public:
	template<class T>
	TSubclassOf<UUserWidgetBase> GetUserWidgetClass(FName InName = NAME_None) const
	{
		if(InName.IsNone()) InName = Cast<UUserWidgetBase>(T::StaticClass()->GetDefaultObject())->GetWidgetName();
		if(UserWidgetClassMap.Contains(InName))
		{
			return UserWidgetClassMap[InName];
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintPure)
	TSubclassOf<UUserWidgetBase> K2_GetUserWidgetClass(FName InName) const
	{
		if(UserWidgetClassMap.Contains(InName))
		{
			return UserWidgetClassMap[InName];
		}
		return nullptr;
	}
	
	template<class T>
	bool HasUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass()) const
	{
		if(!InWidgetClass) return false;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		return AllUserWidgets.Contains(WidgetName);
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasUserWidget"))
	bool K2_HasUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass) const;

	template<class T>
	T* GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass()) const
	{
		if(!InWidgetClass) return nullptr;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		if(AllUserWidgets.Contains(WidgetName))
		{
			return Cast<T>(AllUserWidgets[WidgetName]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetUserWidget", DeterminesOutputType = "InWidgetClass"))
	UUserWidgetBase* K2_GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass) const;

	template<class T>
	T* CreateUserWidget(AActor* InOwner = nullptr, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return nullptr;
		
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		
		if(!UserWidgetClassMap.Contains(WidgetName)) return nullptr;
		
		if(UUserWidgetBase* UserWidget = CreateWidget<UUserWidgetBase>(GetWorld(), UserWidgetClassMap[WidgetName]))
		{
			UserWidget->OnCreate();
			UserWidget->OnInitialize(InOwner);
			if(!AllUserWidgets.Contains(WidgetName))
			{
				AllUserWidgets.Add(WidgetName, UserWidget);
			}
			return Cast<T>(UserWidget);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateUserWidget", DeterminesOutputType = "InWidgetClass"))
	UUserWidgetBase* K2_CreateUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner = nullptr);

	template<class T>
	bool InitializeUserWidget(AActor* InOwner, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(UUserWidgetBase* UserWidget = HasUserWidget<T>(InWidgetClass) ? GetUserWidget<UUserWidgetBase>(InWidgetClass) : CreateUserWidget<UUserWidgetBase>(InOwner, InWidgetClass))
		{
			UserWidget->OnInitialize(InOwner);
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "InitializeUserWidget"))
	bool K2_InitializeUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner = nullptr);

	template<class T>
	bool OpenUserWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(UUserWidgetBase* UserWidget = HasUserWidget<T>(InWidgetClass) ? GetUserWidget<UUserWidgetBase>(InWidgetClass) : CreateUserWidget<UUserWidgetBase>(nullptr, InWidgetClass))
		{
			if(UserWidget->GetWidgetType() == EWidgetType::Temporary)
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
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "OpenUserWidget", AutoCreateRefTerm = "InParams"))
	bool K2_OpenUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, const TArray<FParameter>& InParams, bool bInstant = false);

	template<class T>
	bool CloseUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(UUserWidgetBase* UserWidget = GetUserWidget<UUserWidgetBase>(InWidgetClass))
		{
			if(UserWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				TemporaryUserWidget = nullptr;
			}
			UserWidget->OnClose(bInstant);
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CloseUserWidget"))
	bool K2_CloseUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant = false);

	template<class T>
	bool ToggleUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(UUserWidgetBase* UserWidget = GetUserWidget<UUserWidgetBase>(InWidgetClass))
		{
			UserWidget->Toggle(bInstant);
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ToggleUserWidget"))
	bool K2_ToggleUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant = false);

	template<class T>
	bool DestroyUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return false;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		if(AllUserWidgets.Contains(WidgetName))
		{
			if(UUserWidgetBase* UserWidget = AllUserWidgets[WidgetName])
			{
				UserWidget->RemoveFromParent();
				AllUserWidgets.Remove(WidgetName);
				if(TemporaryUserWidget == UserWidget)
				{
					TemporaryUserWidget = nullptr;
				}
				UserWidget->OnDestroy();
				UserWidget->ConditionalBeginDestroy();
				if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
				{
					InputModule->UpdateInputMode();
				}
			}
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyUserWidget"))
	bool K2_DestroyUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass);

	UFUNCTION(BlueprintCallable)
	void CloseAllUserWidget(EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	////////////////////////////////////////////////////
	// SlateWidget
protected:
	TMap<FName, TSharedPtr<class SSlateWidgetBase>> AllSlateWidgets;

	TSharedPtr<class SSlateWidgetBase> TemporarySlateWidget;

public:
	template<class T>
	bool HasSlateWidget() const
	{
		const FName WidgetName = typeid(T).name();
		return AllSlateWidgets.Contains(WidgetName);
	}

	template<class T>
	TSharedPtr<T> GetSlateWidget() const
	{
		const FName WidgetName = typeid(T).name();
		if(AllSlateWidgets.Contains(WidgetName))
		{
			return AllSlateWidgets[WidgetName];
		}
		return nullptr;
	}

	template<class T>
	TSharedPtr<T> CreateSlateWidget(AActor* InOwner)
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = &SNew(T))
		{
			SlateWidget->OnCreate();
			SlateWidget->OnInitialize(InOwner);
			const FName WidgetName = typeid(SlateWidget).name();
			if(!AllSlateWidgets.Contains(WidgetName))
			{
				AllSlateWidgets.Add(WidgetName, SlateWidget);
			}
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
			if(SlateWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				if(TemporarySlateWidget)
				{
					TemporarySlateWidget->OnClose(true);
				}
				//SlateWidget->SetLastWidget(TemporarySlateWidget);
				TemporarySlateWidget = SlateWidget;
			}
			SlateWidget->OnOpen(*InParams, bInstant);
			if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
			{
				InputModule->UpdateInputMode();
			}
			return true;
		}
		return false;
	}
	
	template<class T>
	bool CloseSlateWidget(bool bInstant = false)
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = GetSlateWidget<T>())
		{
			if(SlateWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				TemporarySlateWidget = nullptr;
			}
			SlateWidget->OnClose(bInstant);
			if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
			{
				InputModule->UpdateInputMode();
			}
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
		const FName WidgetName = typeid(T).name();
		if(AllSlateWidgets.Contains(WidgetName))
		{
			if(TSharedPtr<SSlateWidgetBase> SlateWidget = AllSlateWidgets[WidgetName])
			{
				AllSlateWidgets.Remove(WidgetName);
				if(TemporarySlateWidget == SlateWidget)
				{
					TemporarySlateWidget = nullptr;
				}
				SlateWidget->OnDestroy();
				SlateWidget = nullptr;
				if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
				{
					InputModule->UpdateInputMode();
				}
			}
			return true;
		}
		return false;
	}

	void CloseAllSlateWidget(EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	virtual EInputMode GetNativeInputMode() const override;
};
