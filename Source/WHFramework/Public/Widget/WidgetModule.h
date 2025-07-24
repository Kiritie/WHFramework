// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/RichTextBlockDecorator.h"
#include "Main/Base/ModuleBase.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/WidgetModuleTypes.h"
#include "Widget/Screen/UserWidgetBase.h"
#include "World/WorldWidgetBase.h"
#include "Debug/DebugModuleTypes.h"
#include "Input/InputManagerInterface.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "WidgetModule.generated.h"

class UWorldWidgetContainer;
class UEventHandle_SetWorldWidgetVisible;
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
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

public:
	virtual FString GetModuleDebugMessage() override;

protected:
	UFUNCTION()
	void OnOpenUserWidget(UObject* InSender, UEventHandle_OpenUserWidget* InEventHandle);

	UFUNCTION()
	void OnCloseUserWidget(UObject* InSender, UEventHandle_CloseUserWidget* InEventHandle);

	////////////////////////////////////////////////////
	// GlobalSettings
protected:
	UPROPERTY(EditAnywhere, Category = "GlobalSettings")
	TArray<FLanguageType> LanguageTypes;

	UPROPERTY(EditAnywhere, Category = "GlobalSettings")
	int32 LanguageType;

	UPROPERTY(EditAnywhere, Category = "GlobalSettings")
	float GlobalScale;

public:
	UFUNCTION(BlueprintPure)
	TArray<FLanguageType> GetLanguageTypes() const { return LanguageTypes; }

	UFUNCTION(BlueprintCallable)
	void SetLanguageTypes(const TArray<FLanguageType>& InLanguageTypes) { LanguageTypes = InLanguageTypes; }

	UFUNCTION(BlueprintPure)
	int32 GetLanguageType() const { return LanguageType; }

	UFUNCTION(BlueprintCallable)
	void SetLanguageType(int32 InLanguageType)
	{
		LanguageType = InLanguageType;
		if(LanguageTypes.IsValidIndex(InLanguageType))
		{
			UKismetInternationalizationLibrary::SetCurrentCulture(LanguageTypes[LanguageType].LocalCulture);
		}
	}

	UFUNCTION(BlueprintPure)
	float GetGlobalScale() const { return GlobalScale; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalScale(float InGlobalScale) { GlobalScale = InGlobalScale; }

	////////////////////////////////////////////////////
	// CommonWidget
protected:
	UPROPERTY(EditAnywhere, Category = "CommonWidget|RichText")
	UDataTable* CommonRichTextStyle;

	UPROPERTY(EditAnywhere, Category = "CommonWidget|RichText")
	TArray<TSubclassOf<URichTextBlockDecorator>> CommonRichTextDecorators;

public:
	UFUNCTION(BlueprintPure)
	UDataTable* GetCommonRichTextStyle() const { return CommonRichTextStyle; }

	UFUNCTION(BlueprintCallable)
	void SetCommonRichTextStyle(UDataTable* InCommonRichTextStyle) { CommonRichTextStyle = InCommonRichTextStyle; }

	UFUNCTION(BlueprintPure)
	TArray<TSubclassOf<URichTextBlockDecorator>> GetCommonRichTextDecorators() const { return CommonRichTextDecorators; }

	UFUNCTION(BlueprintCallable)
	void SetCommonRichTextDecorators(const TArray<TSubclassOf<URichTextBlockDecorator>>& InCommonRichTextDecorators) { CommonRichTextDecorators = InCommonRichTextDecorators; }

	UFUNCTION(BlueprintCallable)
	void AddCommonRichTextDecorator(const TSubclassOf<URichTextBlockDecorator>& InCommonRichTextDecorator) { CommonRichTextDecorators.Add(InCommonRichTextDecorator); }

	UFUNCTION(BlueprintCallable)
	void RemoveCommonRichTextDecorator(const TSubclassOf<URichTextBlockDecorator>& InCommonRichTextDecorator) { CommonRichTextDecorators.Remove(InCommonRichTextDecorator); }

	////////////////////////////////////////////////////
	// ScreenWidget
protected:
	UPROPERTY(EditAnywhere, Category = "ScreenWidget")
	TArray<TSubclassOf<UUserWidgetBase>> UserWidgetClasses;

	UPROPERTY(EditAnywhere, Category = "ScreenWidget")
	TMap<FName, TSubclassOf<UUserWidgetBase>> UserWidgetClassMap;

private:
	UPROPERTY(VisibleAnywhere, Transient, Category = "ScreenWidget")
	UUserWidgetBase* TemporaryUserWidget;

	UPROPERTY(VisibleAnywhere, Transient, Category = "ScreenWidget")
	TMap<FName, UUserWidgetBase*> AllUserWidget;

private:
	UFUNCTION(CallInEditor, Category = "ScreenWidget")
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

	template<class T>
	TSubclassOf<UUserWidgetBase> GetUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass()) const
	{
		if(!InClass) return nullptr;

		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		return GetUserWidgetClassByName(WidgetName);
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
	TArray<FName> GetUserWidgetChildren(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass()) const
	{
		if(!InClass) return TArray<FName>();

		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		return GetUserWidgetChildrenByName(WidgetName);
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
	FName GetUserWidgetParent(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass()) const
	{
		if(!InClass) return NAME_None;

		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		return GetUserWidgetParentByName(WidgetName);
	}

	UFUNCTION(BlueprintPure)
	FName GetUserWidgetParentByName(FName InName) const
	{
		if(!HasUserWidgetClassByName(InName)) return NAME_None;
		
		const FName ParentName = GetUserWidgetClassByName(InName).GetDefaultObject()->GetParentName();
		return ParentName;
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
			ensureEditorMsgf(false, FString::Printf(TEXT("Failed to create user widget. Module does not contain this type: %s"), *InName.ToString()), EDC_Widget, EDV_Error);
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
			if(!UserWidget->CanOpen() || UserWidget->GetWidgetState() == EScreenWidgetState::Closing) return false;
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
		if(UUserWidgetBase* UserWidget = HasUserWidgetByName(InName) ? GetUserWidgetByName<UUserWidgetBase>(InName) : CreateUserWidgetByName<UUserWidgetBase>(InName))
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
	bool DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bRecovery = false);

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
	void ClearAllUserWidget(bool bRecovery = false);

	////////////////////////////////////////////////////
	// WorldWidget
protected:
	UPROPERTY(EditAnywhere, Category = "WorldWidget")
	TArray<TSubclassOf<UWorldWidgetBase>> WorldWidgetClasses;

	UPROPERTY(EditAnywhere, Category = "WorldWidget")
	TMap<FName, TSubclassOf<UWorldWidgetBase>> WorldWidgetClassMap;
	
	UPROPERTY(EditAnywhere, Category = "WorldWidget")
	TSubclassOf<UWorldWidgetContainer> WorldWidgetContainerClass;

	UPROPERTY(EditAnywhere, Category = "WorldWidget")
	int32 WorldWidgetContainerZOrder;

private:
	UPROPERTY(VisibleAnywhere, Transient, Category = "WorldWidget")
	UWorldWidgetContainer* WorldWidgetContainer;

	UPROPERTY(VisibleAnywhere, Transient, Category = "WorldWidget")
	TMap<FName, FWorldWidgets> AllWorldWidget;

protected:
	UFUNCTION(CallInEditor, Category = "WorldWidget")
	void SortWorldWidgetClasses();

	UFUNCTION()
	void OnSetWorldWidgetVisible(UObject* InSender, UEventHandle_SetWorldWidgetVisible* InEventHandle);

public:
	UFUNCTION(BlueprintPure)
	UWorldWidgetContainer* GetWorldWidgetContainer() const { return WorldWidgetContainer; }

	UFUNCTION(BlueprintPure)
	bool GetWorldWidgetVisible(TSubclassOf<UWorldWidgetBase> InClass = nullptr);

	UFUNCTION(BlueprintCallable)
	void SetWorldWidgetVisible(bool bVisible, TSubclassOf<UWorldWidgetBase> InClass = nullptr);

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
		return AllWorldWidget.Contains(InName) && AllWorldWidget[InName].WorldWidgets.IsValidIndex(InIndex);
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
		if(AllWorldWidget.Contains(InName) && AllWorldWidget[InName].WorldWidgets.IsValidIndex(InIndex))
		{
			return Cast<T>(AllWorldWidget[InName].WorldWidgets[InIndex]);
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
		if(AllWorldWidget.Contains(InName))
		{
			for(auto Iter : AllWorldWidget[InName].WorldWidgets)
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
		
		return CreateWorldWidgetByName<T>(WidgetName, InOwner, InMapping, InParams);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams);

	template<class T>
	T* CreateWorldWidgetByName(FName InName, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>* InParams = nullptr)
	{
		WorldWidgetClassMap.Empty();
		for(auto& Iter : WorldWidgetClasses)
		{
			if(!Iter) continue;
			const FName WidgetName = Iter->GetDefaultObject<UWorldWidgetBase>()->GetWidgetName();
			if(!WorldWidgetClassMap.Contains(WidgetName))
			{
				WorldWidgetClassMap.Add(WidgetName, Iter);
			}
		}
		
		if(!WorldWidgetClassMap.Contains(InName))
		{
			ensureEditorMsgf(false, FString::Printf(TEXT("Failed to create world widget. Module does not contain this type: %s"), *InName.ToString()), EDC_Widget, EDV_Error);
			return nullptr;
		}
		
		if(UWorldWidgetBase* WorldWidget = UObjectPoolModuleStatics::SpawnObject<UWorldWidgetBase>(nullptr, nullptr, WorldWidgetClassMap[InName]))
		{
			if(!AllWorldWidget.Contains(InName))
			{
				AllWorldWidget.Add(InName);
			}
			WorldWidget->WidgetIndex = AllWorldWidget[InName].WorldWidgets.Add(WorldWidget);
			WorldWidget->OnCreate(InOwner, InMapping, InParams ? *InParams : TArray<FParameter>());
			return Cast<T>(WorldWidget);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* CreateWorldWidgetByName(FName InName, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams);

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
	void ClearAllWorldWidget(bool bRecovery = false);

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	virtual int32 GetNativeInputPriority() const override { return 1; }
	
	virtual EInputMode GetNativeInputMode() const override;
};
