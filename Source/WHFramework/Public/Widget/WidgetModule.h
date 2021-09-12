// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SSlateWidgetBase.h"
#include "GameFramework/Actor.h"
#include "UserWidget.h"
#include "UserWidgetBase.h"
#include "WidgetModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AWidgetModule : public AActor
{
	GENERATED_BODY()
	
public:	
	AWidgetModule();

protected:
	virtual void BeginPlay() override;

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
	T* CreateUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return nullptr;
		
		if(UUserWidgetBase* UserWidget = CreateWidget<UUserWidgetBase>(GetWorld(), InWidgetClass))
		{
			UserWidget->OnInitialize();
			const FName WidgetName = typeid(UserWidget).name();
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
			return UserWidget;
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable)
	class UUserWidgetBase* K2_CreateUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return CreateUserWidget<UUserWidgetBase>(InWidgetClass);
	}

	template<class T>
	T* GetUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return nullptr;

		const FName WidgetName = typeid(InWidgetClass->GetDefaultObject()).name();
		if(AllUserWidgets.Contains(WidgetName))
		{
			return AllUserWidgets[WidgetName];
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable)
	class UUserWidgetBase* K2_GetUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return GetUserWidget<UUserWidgetBase>(InWidgetClass);
	}
		
	template<class T>
	bool InitializeUserWidget(AActor* InOwner, TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(UUserWidgetBase* UserWidget = GetUserWidget<UUserWidgetBase>(InWidgetClass))
		{
			UserWidget->OnInitialize(InOwner);
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable)
	bool K2_InitializeUserWidget(AActor* InOwner, TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return InitializeUserWidget<UUserWidgetBase>(InOwner, InWidgetClass);
	}

	template<class T>
	bool OpenUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(UUserWidgetBase* UserWidget = GetUserWidget<UUserWidgetBase>(InWidgetClass))
		{
			if(UserWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				if(TemporaryUserWidget)
				{
					TemporaryUserWidget->OnClose();
				}
				TemporaryUserWidget = UserWidget;
			}
			UserWidget->OnOpen();
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable)
	bool K2_OpenUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return OpenUserWidget<UUserWidgetBase>(InWidgetClass);
	}

	template<class T>
	bool CloseUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(UUserWidgetBase* UserWidget = GetUserWidget<UUserWidgetBase>(InWidgetClass))
		{
			if(UserWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				TemporaryUserWidget = nullptr;
			}
			UserWidget->OnClose();
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable)
	bool K2_CloseUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return CloseUserWidget<UUserWidgetBase>(InWidgetClass);
	}
		
	template<class T>
	bool ToggleUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(UUserWidgetBase* UserWidget = GetUserWidget<UUserWidgetBase>(InWidgetClass))
		{
			UserWidget->OnToggle();
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable)
	bool K2_ToggleUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return ToggleUserWidget<UUserWidgetBase>(InWidgetClass);
	}

	template<class T>
	bool RemoveUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass = T::StaticClass())
	{
		if(!InWidgetClass) return false;

		const FName WidgetName = typeid(InWidgetClass->GetDefaultObject()).name();
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
			}
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable)
	bool K2_RemoveUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass)
	{
		return RemoveUserWidget<UUserWidgetBase>(InWidgetClass);
	}

	////////////////////////////////////////////////////
	// SlateWidget
protected:
	TMap<FName, TSharedPtr<class SSlateWidgetBase>> AllSlateWidgets;

	TMap<FName, TSharedPtr<class SSlateWidgetBase>> PermanentSlateWidgets;

	TSharedPtr<class SSlateWidgetBase> TemporarySlateWidget;

public:
	template<class T>
	TSharedPtr<T> CreateSlateWidget()
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = &SNew(T))
		{
			SlateWidget->OnInitialize();
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
			return SlateWidget;
		}
		return nullptr;
	}

	template<class T>
	TSharedPtr<T> GetSlateWidget()
	{
		const FName WidgetName = typeid(T).name();
		if(AllSlateWidgets.Contains(WidgetName))
		{
			return AllSlateWidgets[WidgetName];
		}
		return nullptr;
	}
	
	template<class T>
	bool InitializeSlateWidget(AActor* InOwner)
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = GetSlateWidget<T>())
		{
			SlateWidget->OnInitialize(InOwner);
			return true;
		}
		return false;
	}

	template<class T>
	bool OpenSlateWidget()
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = GetSlateWidget<T>())
		{
			if(SlateWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				if(TemporarySlateWidget)
				{
					TemporarySlateWidget->OnClose();
				}
				TemporarySlateWidget = SlateWidget;
			}
			SlateWidget->OnOpen();
			return true;
		}
		return false;
	}
	
	template<class T>
	bool CloseSlateWidget()
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = GetSlateWidget<T>())
		{
			if(SlateWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				TemporarySlateWidget = nullptr;
			}
			SlateWidget->OnClose();
			return true;
		}
		return false;
	}
	
	template<class T>
	bool ToggleSlateWidget()
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = GetSlateWidget<T>())
		{
			SlateWidget->OnToggle();
			return true;
		}
		return false;
	}

	template<class T>
	bool RemoveSlateWidget()
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
			}
			return true;
		}
		return false;
	}
	
	//////////////////////////////////////////////////////////////////////////
	// InputMode
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EInputMode InputMode;

public:
	UFUNCTION(BlueprintCallable)
	void UpdateInputMode();

	UFUNCTION(BlueprintCallable)
	void SetInputMode(EInputMode InInputMode);

	UFUNCTION(BlueprintPure)
	EInputMode GetInputMode() const { return InputMode; }
};
