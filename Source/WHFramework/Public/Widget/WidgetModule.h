// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Main/Base/ModuleBase.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Widget/WidgetModuleTypes.h"
#include "Widget/Screen/Slate/SSlateWidgetBase.h"
#include "Widget/Screen/UMG/UserWidgetBase.h"
#include "World/WorldWidgetBase.h"
#include "Debug/DebugModuleTypes.h"
#include "Input/Manager/InputManagerInterface.h"
#include "WidgetModule.generated.h"

class UEventHandle_CloseUserWidget;
class UEventHandle_OpenUserWidget;

UCLASS()
class WHFRAMEWORK_API UWidgetModule : public UModuleBase, public IInputManagerInterface
{
	GENERATED_BODY()
			
	GENERATED_MODULE(UWidgetModule)

public:	
	UWidgetModule();

	~UWidgetModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	UFUNCTION()
	void OnOpenUserWidget(UObject* InSender, UEventHandle_OpenUserWidget* InEventHandle);

	UFUNCTION()
	void OnCloseUserWidget(UObject* InSender, UEventHandle_CloseUserWidget* InEventHandle);

	////////////////////////////////////////////////////
	// UserWidget
protected:
	UPROPERTY(EditAnywhere, Category = "UserWidget")
	TArray<TSubclassOf<UUserWidgetBase>> UserWidgetClasses;

	UPROPERTY(EditAnywhere, Category = "UserWidget")
	TMap<FName, TSubclassOf<UUserWidgetBase>> UserWidgetClassMap;

private:
	UPROPERTY(Transient)
	UUserWidgetBase* TemporaryUserWidget;

	UPROPERTY()
	TMap<FName, UUserWidgetBase*> AllUserWidget;

private:
	UFUNCTION(CallInEditor, Category = "UserWidget")
	void SortUserWidgetClasses();

public:
	template<class T>
	T* GetTemporaryUserWidget() const { return Cast<T>(TemporaryUserWidget); }
	
	UFUNCTION(BlueprintPure)
	UUserWidgetBase* GetTemporaryUserWidget() const { return TemporaryUserWidget; }
	
	UFUNCTION(BlueprintPure)
	TMap<FName, UUserWidgetBase*> GetAllUserWidget() const { return AllUserWidget; }

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

	UFUNCTION(BlueprintPure)
	TArray<FName> GetUserWidgetChildrenByName(FName InName) const
	{
		TArray<FName> ReturnValues;
		for(const auto Iter : UserWidgetClassMap)
		{
			const UUserWidgetBase* DefaultObject = Iter.Value->GetDefaultObject<UUserWidgetBase>();
			if(DefaultObject->GetParentName() == InName)
			{
				ReturnValues.Add(DefaultObject->GetWidgetName());
			}
		}
		return ReturnValues;
	}

	template<class T>
	void AddUserWidgetClassMapping(FName InName, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass) return;

		if(!UserWidgetClassMap.Contains(InName))
		{
			UserWidgetClassMap.Add(InName, InClass);
		}
	}

	UFUNCTION(Blueprintable)
	void AddUserWidgetClassMapping(FName InName, TSubclassOf<UUserWidgetBase> InClass);

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
		return AllUserWidget.Contains(InName);
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
		if(AllUserWidget.Contains(InName))
		{
			return Cast<T>(AllUserWidget[InName]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UUserWidgetBase* GetUserWidgetByName(FName InName, TSubclassOf<UUserWidgetBase> InClass = nullptr) const;

	template<class T>
	T* CreateUserWidget(UObject* InOwner = nullptr, const TArray<FParameter>* InParams = nullptr, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass) return nullptr;
		
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		
		return CreateUserWidgetByName<T>(WidgetName, InOwner, InParams, bForce);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"))
	UUserWidgetBase* CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false);

	template<class T>
	T* CreateUserWidgetByName(FName InName, UObject* InOwner = nullptr, const TArray<FParameter>* InParams = nullptr, bool bForce = false)
	{
		if(InName.IsNone()) return nullptr;
		
		if(!UserWidgetClassMap.Contains(InName))
		{
			ensureEditorMsgf(false, FString::Printf(TEXT("Failed to create user widget. Module does not contain this type: %s"), *InName.ToString()), EDC_Widget, EDV_Warning);
			return nullptr;
		}

		UUserWidgetBase* UserWidget;
		
		if(!HasUserWidgetByName(InName))
		{
			UserWidget = UObjectPoolModuleStatics::SpawnObject<UUserWidgetBase>(nullptr, nullptr, UserWidgetClassMap[InName]);
			if(UserWidget)
			{
				AllUserWidget.Add(InName, UserWidget);
				UserWidget->WidgetName = InName;
				UserWidget->OnCreate(InOwner, InParams ? * InParams : TArray<FParameter>());
			}
		}
		else
		{
			UserWidget = GetUserWidgetByName(InName);
			if(UserWidget && bForce)
			{
				UserWidget->OnCreate(InOwner, InParams ? * InParams : TArray<FParameter>());
			}
		}
		
		if(UserWidget)
		{
			UserWidget->Init(InOwner, InParams, bForce);
		}
		return Cast<T>(UserWidget);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	UUserWidgetBase* CreateUserWidgetByName(FName InName, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false);

	template<class T>
	bool OpenUserWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();

		return OpenUserWidgetByName(WidgetName, InParams, bInstant, bForce);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false);

	bool OpenUserWidgetByName(FName InName, const TArray<FParameter>* InParams = nullptr, bool bInstant = false, bool bForce = false)
	{
		if(UUserWidgetBase* UserWidget = HasUserWidgetByName(InName) ? GetUserWidgetByName<UUserWidgetBase>(InName) : CreateUserWidgetByName<UUserWidgetBase>(InName))
		{
			if(!UserWidget->CanOpen()) return false;
			if(bForce) CloseUserWidgetByName(InName, true);
			if(UserWidget->GetWidgetState() != EScreenWidgetState::Opening && UserWidget->GetWidgetState() != EScreenWidgetState::Opened)
			{
				if(!UserWidget->GetParentWidgetN())
				{
					if(UserWidget->GetWidgetType() == EWidgetType::Temporary)
					{
						if(TemporaryUserWidget)
						{
							UserWidget->SetLastTemporary(TemporaryUserWidget);
							TemporaryUserWidget->SetLastTemporary(nullptr);
							TemporaryUserWidget->Close(true);
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
	bool OpenUserWidgetByName(FName InName, const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false);

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
			if(UserWidget->GetWidgetState() != EScreenWidgetState::None && UserWidget->GetWidgetState() != EScreenWidgetState::Closing && UserWidget->GetWidgetState() != EScreenWidgetState::Closed)
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
		if(AllUserWidget.Contains(InName))
		{
			if(UUserWidgetBase* UserWidget = AllUserWidget[InName])
			{
				AllUserWidget.Remove(InName);
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
	TSharedPtr<T> CreateSlateWidget(UObject* InOwner = nullptr, const TArray<FParameter>* InParams = nullptr)
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = &SNew(T))
		{
			SlateWidget->OnCreate(InOwner, InParams ? *InParams : TArray<FParameter>());
			SlateWidget->Init(InOwner, InParams);
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
	
private:
	UFUNCTION(CallInEditor, Category = "WorldWidget")
	void SortWorldWidgetClasses();

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
	T* CreateWorldWidget(UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass) return nullptr;
		
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		
		return CreateWorldWidgetByName<T>(WidgetName, InOwner, InMapping, InParams, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams);

	template<class T>
	T* CreateWorldWidgetByName(FName InName, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(!WorldWidgetClassMap.Contains(InName))
		{
			ensureEditorMsgf(false, FString::Printf(TEXT("Failed to create world widget. Module does not contain this type: %s"), *InName.ToString()), EDC_Widget, EDV_Warning);
			return nullptr;
		}
		
		if(UWorldWidgetBase* WorldWidget = UObjectPoolModuleStatics::SpawnObject<UWorldWidgetBase>(nullptr, nullptr, WorldWidgetClassMap[InName]))
		{
			if(!AllWorldWidgets.Contains(InName))
			{
				AllWorldWidgets.Add(InName);
			}
			WorldWidget->SetWidgetIndex(AllWorldWidgets[InName].WorldWidgets.Add(WorldWidget));
			WorldWidget->OnCreate(InOwner, InMapping, *InParams);
			return Cast<T>(WorldWidget);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* CreateWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams);

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
	virtual int32 GetNativeInputPriority() const override { return 1; }
	
	virtual EInputMode GetNativeInputMode() const override;
};
