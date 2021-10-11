// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ModuleBase.h"
#include "SSlateWidgetBase.h"
#include "GameFramework/Actor.h"
#include "UserWidget.h"
#include "UserWidgetBase.h"
#include "WidgetModule.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChangeInputMode, EInputMode, InInputMode);

UCLASS()
class WHFRAMEWORK_API AWidgetModule : public AModuleBase
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
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TMap<FName, class UUserWidgetBase*> AllUserWidgets;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TMap<FName, class UUserWidgetBase*> PermanentUserWidgets;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	class UUserWidgetBase* TemporaryUserWidget;

public:
	template<class T>
	bool HasUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass()) const
	{
		if(!InWidgetClass) return false;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		return AllUserWidgets.Contains(WidgetName);
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasUserWidget"))
	bool K2_HasUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass) const;

	template<class T>
	T* GetUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass()) const
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
	class UUserWidgetBase* K2_GetUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass) const;

	template<class T>
	T* CreateUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return nullptr;
		
		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		if(UUserWidgetBase* UserWidget = CreateWidget<UUserWidgetBase>(GetWorld(), InWidgetClass))
		{
			UserWidget->OnCreate();
			switch (UserWidget->GetWidgetType())
			{
				case EWidgetType::Permanent:
				{
					if(!PermanentUserWidgets.Contains(WidgetName))
					{
						PermanentUserWidgets.Add(WidgetName, UserWidget);
					}
				}
				case EWidgetType::Temporary:
				{
					if(!AllUserWidgets.Contains(WidgetName))
					{
						AllUserWidgets.Add(WidgetName, UserWidget);
					}
					break;
				}
				default: break;
			}
			return Cast<T>(UserWidget);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateUserWidget", DeterminesOutputType = "InWidgetClass"))
	class UUserWidgetBase* K2_CreateUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass);

	template<class T>
	bool InitializeUserWidget(AActor* InOwner, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(UUserWidgetBase* UserWidget = HasUserWidget<T>(InWidgetClass) ? GetUserWidget<UUserWidgetBase>(InWidgetClass) : CreateUserWidget<UUserWidgetBase>(InWidgetClass))
		{
			UserWidget->OnInitialize(InOwner);
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "InitializeUserWidget"))
	bool K2_InitializeUserWidget(AActor* InOwner, TSubclassOf<class UUserWidgetBase> InWidgetClass);

	template<class T>
	bool OpenUserWidget(bool bInstant = false, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(UUserWidgetBase* UserWidget = HasUserWidget<T>(InWidgetClass) ? GetUserWidget<UUserWidgetBase>(InWidgetClass) : CreateUserWidget<UUserWidgetBase>(InWidgetClass))
		{
			if(UserWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				if(TemporaryUserWidget)
				{
					TemporaryUserWidget->OnClose(bInstant);
				}
				TemporaryUserWidget = UserWidget;
			}
			UserWidget->OnOpen(bInstant);
			UpdateInputMode();
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "OpenUserWidget"))
	bool K2_OpenUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bInstant = false);

	template<class T>
	bool CloseUserWidget(bool bInstant = false, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(UUserWidgetBase* UserWidget = GetUserWidget<UUserWidgetBase>(InWidgetClass))
		{
			if(UserWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				TemporaryUserWidget = nullptr;
			}
			UserWidget->OnClose(bInstant);
			UpdateInputMode();
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CloseUserWidget"))
	bool K2_CloseUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bInstant = false);

	template<class T>
	bool ToggleUserWidget(bool bInstant = false, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(UUserWidgetBase* UserWidget = GetUserWidget<UUserWidgetBase>(InWidgetClass))
		{
			UserWidget->Toggle(bInstant);
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ToggleUserWidget"))
	bool K2_ToggleUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass, bool bInstant = false);

	template<class T>
	bool DestroyUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return false;

		const FName WidgetName = InWidgetClass.GetDefaultObject()->GetWidgetName();
		if(AllUserWidgets.Contains(WidgetName))
		{
			if(UUserWidgetBase* UserWidget = AllUserWidgets[WidgetName])
			{
				UserWidget->RemoveFromParent();
				AllUserWidgets.Remove(WidgetName);
				switch (UserWidget->GetWidgetType())
				{
					case EWidgetType::Permanent:
					{
						if(PermanentUserWidgets.Contains(WidgetName))
						{
							PermanentUserWidgets.Remove(WidgetName);
						}
						break;
					}
					case EWidgetType::Temporary:
					{
						if(TemporaryUserWidget == UserWidget)
						{
							TemporaryUserWidget = nullptr;
						}
						break;
					}
					default: break;
				}
				UserWidget->OnDestroy();
				UpdateInputMode();
			}
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroyUserWidget"))
	bool K2_DestroyUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass);

	UFUNCTION(BlueprintCallable)
	void OpenAllUserWidget(EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	void CloseAllUserWidget(EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	////////////////////////////////////////////////////
	// SlateWidget
protected:
	TMap<FName, TSharedPtr<class SSlateWidgetBase>> AllSlateWidgets;

	TMap<FName, TSharedPtr<class SSlateWidgetBase>> PermanentSlateWidgets;

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
	TSharedPtr<T> CreateSlateWidget()
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = &SNew(T))
		{
			SlateWidget->OnCreate();
			const FName WidgetName = typeid(SlateWidget).name();
			switch (SlateWidget->GetWidgetType())
			{
				case EWidgetType::Permanent:
				{
					if(!PermanentSlateWidgets.Contains(WidgetName))
					{
						PermanentSlateWidgets.Add(WidgetName, SlateWidget);
					}
				}
				case EWidgetType::Temporary:
				{
					if(!AllSlateWidgets.Contains(WidgetName))
					{
						AllSlateWidgets.Add(WidgetName, SlateWidget);
					}
					break;
				}
				default: break;
			}
			return Cast<T>(SlateWidget);
		}
		return nullptr;
	}
	
	template<class T>
	bool InitializeSlateWidget(AActor* InOwner)
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = (TSharedPtr<SSlateWidgetBase>)(HasSlateWidget<T>() ? GetSlateWidget<T>() : CreateSlateWidget<T>()))
		{
			SlateWidget->OnInitialize(InOwner);
			return true;
		}
		return false;
	}

	template<class T>
	bool OpenSlateWidget(bool bInstant = false)
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = (TSharedPtr<SSlateWidgetBase>)(HasSlateWidget<T>() ? GetSlateWidget<T>() : CreateSlateWidget<T>()))
		{
			if(SlateWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				if(TemporarySlateWidget)
				{
					TemporarySlateWidget->OnClose(bInstant);
				}
				TemporarySlateWidget = SlateWidget;
			}
			SlateWidget->OnOpen(bInstant);
			UpdateInputMode();
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
			UpdateInputMode();
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
				// SlateWidget->RemoveFromParent();
				AllSlateWidgets.Remove(WidgetName);
				switch (SlateWidget->GetWidgetType())
				{
					case EWidgetType::Permanent:
					{
						if(PermanentSlateWidgets.Contains(WidgetName))
						{
							PermanentSlateWidgets.Remove(WidgetName);
						}
						break;
					}
					case EWidgetType::Temporary:
					{
						if(TemporarySlateWidget == SlateWidget)
						{
							TemporarySlateWidget = nullptr;
						}
						break;
					}
					default: break;
				}
				SlateWidget->OnDestroy();
				UpdateInputMode();
			}
			return true;
		}
		return false;
	}

	void OpenAllSlateWidget(EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	void CloseAllSlateWidget(EWidgetType InWidgetType = EWidgetType::None, bool bInstant = false);

	//////////////////////////////////////////////////////////////////////////
	// InputMode
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EInputMode InputMode;

public:
	UPROPERTY(BlueprintCallable)
	FOnChangeInputMode OnChangeInputMode;

public:
	UFUNCTION(BlueprintCallable)
	void UpdateInputMode();

	UFUNCTION(BlueprintCallable)
	void SetInputMode(EInputMode InInputMode);

	UFUNCTION(BlueprintPure)
	EInputMode GetInputMode() const { return InputMode; }
};
