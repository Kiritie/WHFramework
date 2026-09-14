// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CommonActivatableWidget.h"
#include "Common/CommonModuleTypes.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Slate/Runtime/Interfaces/ScreenWidgetInterface.h"
#include "Slate/Runtime/Interfaces/SubWidgetInterface.h"
#include "Widget/WidgetModuleTypes.h"

#include "UserWidgetBase.generated.h"

class USubWidgetBase;
class UWidgetAnimatorBase;
class UWidgetMountSlot;
struct FUIActionBindingHandle;
/**
 * 
 */
UCLASS(BlueprintType, meta = (DisableNativeTick))
class WHFRAMEWORK_API UUserWidgetBase : public UCommonActivatableWidget, public IScreenWidgetInterface, public IObjectPoolInterface
{
	friend class UWidgetModule;
	
	GENERATED_BODY()

public:
	UUserWidgetBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

protected:
	UPROPERTY(EditDefaultsOnly)
	bool bWidgetTickAble;

public:
	virtual bool IsTickAble_Implementation() const override { return bWidgetTickAble; }

	virtual void OnTick_Implementation(float DeltaSeconds) override;

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	FUIActionBindingHandle RegisterUIActionByTag(
		FGameplayTag InActionTag,
		bool bInDisplayInActionBar = true);

	UFUNCTION(BlueprintNativeEvent, Category = "Input")
	void OnUIActionTriggered(FGameplayTag InActionTag);

private:
	void HandleUIActionTriggered(FGameplayTag InActionTag);

public:
	UFUNCTION(BlueprintImplementableEvent, meta = (AutoCreateRefTerm = "InParam"), DisplayName = "OnCreate")
	void K2_OnCreate(const FParameter& InParam);
	UFUNCTION()
	virtual void OnCreate(const FParameter& InParam) override;

	UFUNCTION(BlueprintImplementableEvent, meta = (AutoCreateRefTerm = "InParam"), DisplayName = "OnInitialize")
	void K2_OnInitialize(const FParameter& InParam);
	UFUNCTION()
	virtual void OnInitialize(const FParameter& InParam) override;
	
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnReset")
	void K2_OnReset(bool bForce = false);
	UFUNCTION()
	virtual void OnReset(bool bForce = false) override;

	UFUNCTION(BlueprintImplementableEvent, meta = (AutoCreateRefTerm = "InParam"), DisplayName = "OnOpen")
	void K2_OnOpen(const FParameter& InParam, bool bInstant = false);
	UFUNCTION()
	virtual void OnOpen(const FParameter& InParam, bool bInstant = false) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnClose")
	void K2_OnClose(bool bInstant = false);
	UFUNCTION()
	virtual void OnClose(bool bInstant = false) override;
	
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	UFUNCTION()
	virtual void OnRefresh() override;
	
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnDestroy")
	void K2_OnDestroy(EObjectDespawnMode InMode);
	UFUNCTION()
	virtual void OnDestroy(EObjectDespawnMode InMode) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnStateChanged")
	void K2_OnStateChanged(EScreenWidgetState InWidgetState);
	UFUNCTION()
	virtual void OnStateChanged(EScreenWidgetState InWidgetState) override;

public:
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParam"))
	virtual void Init(const FParameter& InParam = FParameter(), bool bForce = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Reset(bool bForce = false) override;

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParam"))
	virtual void Open(const FParameter& InParam = FParameter(), bool bInstant = false, bool bForce = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Close(bool bInstant = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Toggle(bool bInstant = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Refresh() override;

	UFUNCTION(BlueprintCallable)
	virtual void Destroy(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy) override;
	
public:
	UFUNCTION(BlueprintNativeEvent)
	bool CanOpen() const override;

protected:
	UFUNCTION(BlueprintCallable)
	virtual void FinishOpen(bool bInstant) override;

	UFUNCTION(BlueprintCallable)
	virtual void FinishClose(bool bInstant) override;

public:
	template<class T>
	T* CreateSubWidget(const FParameter& InParam = FParameter(), TSubclassOf<UUserWidget> InClass = T::StaticClass())
	{
		return Cast<T>(CreateSubWidget(InClass, InParam));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParam"), DisplayName = "CreateSubWidget")
	UUserWidget* K2_CreateSubWidget(TSubclassOf<UUserWidget> InClass, const FParameter& InParam = FParameter());

	virtual ISubWidgetInterface* CreateSubWidget(TSubclassOf<UUserWidget> InClass, const FParameter& InParam = FParameter()) override;

	UFUNCTION(BlueprintCallable, DisplayName = "DestroySubWidget")
	bool K2_DestroySubWidget(UUserWidget* InWidget, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

	virtual bool DestroySubWidget(ISubWidgetInterface* InWidget, EObjectDespawnMode InMode) override;

	UFUNCTION(BlueprintCallable)
	virtual void DestroyAllSubWidget(EObjectDespawnMode InMode) override;

	virtual void AddChildWidget(IScreenWidgetInterface* InWidget) override;

	virtual void RemoveChildWidget(IScreenWidgetInterface* InWidget) override;

	virtual void RemoveAllChildWidget() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget", meta = (Categories = "Widget.Screen"))
	FGameplayTag DefaultWidgetTag;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Widget")
	FGameplayTag WidgetTag;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UWidgetMountSlot>> WidgetMountSlotMap;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EWidgetOpenFinishType WidgetOpenFinishType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "WidgetOpenFinishType == EWidgetOpenFinishType::Delay"))
	float WidgetOpenFinishTime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Instanced, meta = (EditConditionHides, EditCondition = "WidgetOpenFinishType == EWidgetOpenFinishType::Animator"))
	UWidgetAnimatorBase* WidgetOpenAnimator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EWidgetCloseFinishType WidgetCloseFinishType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "WidgetCloseFinishType == EWidgetCloseFinishType::Delay"))
	float WidgetCloseFinishTime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Instanced, meta = (EditConditionHides, EditCondition = "WidgetCloseFinishType == EWidgetCloseFinishType::Animator"))
	UWidgetAnimatorBase* WidgetCloseAnimator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EWidgetRefreshType WidgetRefreshType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "WidgetRefreshType == EWidgetRefreshType::Timer"))
	float WidgetRefreshTime;

	UPROPERTY(Transient)
	FParameter InitializeParameter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	EWidgetInputConfig InputConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	bool bWidgetActivatable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bWidgetAutoFocus;

	UPROPERTY(Transient)
	EScreenWidgetState WidgetState;

	UPROPERTY(Transient)
	UObject* OwnerObject;

	UPROPERTY(Transient)
	bool bInitialized;

	UPROPERTY(Transient)
	FParameter CurrentOpenParameter;

	IScreenWidgetInterface* ParentWidget;

	UPROPERTY(Transient)
	TArray<FSubWidgetRuntimeEntry> SubWidgetEntries;

	TArray<IScreenWidgetInterface*> ChildWidgets;

	UPROPERTY(BlueprintAssignable)
	FOnWidgetStateChanged OnWidgetStateChanged;

public:
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnOpened"))
	FK2_OnWidgetOpened K2_OnOpened;

	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnClosed"))
	FK2_OnWidgetClosed K2_OnClosed;

	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnClosed"))
	FK2_OnWidgetDestroyed K2_OnDestroyed;

	FOnWidgetOpened OnOpened;

	FOnWidgetClosed OnClosed;
	
	FOnWidgetDestroyed OnDestroyed;

private:
	void AbortOpenTransition();

	void AbortCloseTransition();

	FTimerHandle WidgetFinishOpenTimerHandle;
	FTimerHandle WidgetFinishCloseTimerHandle;
	FTimerHandle WidgetRefreshTimerHandle;

public:
	UFUNCTION(BlueprintPure)
	FGameplayTag GetDefaultWidgetTag() const { return DefaultWidgetTag; }

	UFUNCTION(BlueprintPure)
	FGameplayTag GetWidgetTag() const { return WidgetTag; }

	UFUNCTION(BlueprintPure)
	UWidgetMountSlot* GetWidgetMountSlot(const FGameplayTag& InSlotTag) const;

	UFUNCTION(BlueprintPure)
	bool IsWidgetActiveInHierarchy() const;

	void RebuildWidgetMountSlotCache();

	const FScreenWidgetConfig* GetWidgetConfig() const;

	UFUNCTION(BlueprintPure)
	virtual EWidgetType GetWidgetType(bool bInheritParent = true) const override;

	UFUNCTION(BlueprintPure)
	virtual int32 GetWidgetZOrder() const override;

	UFUNCTION(BlueprintPure)
	virtual FAnchors GetWidgetAnchors() const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsWidgetAutoSize() const override;

	UFUNCTION(BlueprintPure)
	virtual FVector2D GetWidgetDrawSize() const override;

	UFUNCTION(BlueprintPure)
	virtual FMargin GetWidgetOffsets() const override;

	UFUNCTION(BlueprintPure)
	virtual FVector2D GetWidgetAlignment() const override;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual UWidgetAnimatorBase* GetWidgetOpenAnimator(TSubclassOf<UWidgetAnimatorBase> InClass = nullptr) const;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual UWidgetAnimatorBase* GetWidgetCloseAnimator(TSubclassOf<UWidgetAnimatorBase> InClass = nullptr) const;

	UFUNCTION(BlueprintPure)
	virtual EWidgetRefreshType GetWidgetRefreshType() const override { return WidgetRefreshType; }

	UFUNCTION(BlueprintPure)
	virtual EScreenWidgetState GetWidgetState(bool bInheritParent = false) const override
	{
		if(bInheritParent && ParentWidget && ParentWidget->GetWidgetState() == EScreenWidgetState::Closed)
		{
			return EScreenWidgetState::Closed;
		}
		return WidgetState;
	}
	
	UFUNCTION(BlueprintPure)
	virtual bool IsWidgetOpened(bool bCheckOpening = true, bool bInheritParent = false) const override
	{
		return GetWidgetState(bInheritParent) == EScreenWidgetState::Opened || (bCheckOpening && GetWidgetState(bInheritParent) == EScreenWidgetState::Opening);
	}
	
	UFUNCTION(BlueprintPure)
	virtual bool IsWidgetClosed(bool bCheckClosing = true, bool bInheritParent = false) const override
	{
		return GetWidgetState(bInheritParent) == EScreenWidgetState::Closed || (bCheckClosing && GetWidgetState(bInheritParent) == EScreenWidgetState::Closing);
	}

	UFUNCTION(BlueprintPure)
	virtual FParameter GetWidgetParams() const override
	{
		return CurrentOpenParameter.HasValue() ? CurrentOpenParameter : InitializeParameter;
	}
	
	template<class T>
	T* GetOwnerObject() const
	{
		return Cast<T>(GetOwnerObject());
	}
	virtual UObject* GetOwnerObject() const override { return OwnerObject; }

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual UObject* GetOwnerObject(TSubclassOf<UObject> InClass) const { return GetDeterminesOutputObject(OwnerObject, InClass); }

	template<class T>
	T* GetParentWidgetN() const
	{
		return Cast<T>(GetParentWidgetN());
	}
	virtual IScreenWidgetInterface* GetParentWidgetN() const override { return ParentWidget; }

	virtual void SetParentWidgetN(IScreenWidgetInterface* InParentWidget) override { ParentWidget = InParentWidget; }
	
	UFUNCTION(BlueprintPure)
	TArray<UWidget*> GetPoolWidgets() const;

	UFUNCTION(BlueprintPure)
	virtual int32 GetSubWidgetNum() const override { return SubWidgetEntries.Num(); }

	template<class T>
	TArray<T*> GetSubWidgets()
	{
		TArray<T*> ReturnValues;
		for(auto Iter : GetSubWidgets())
		{
			ReturnValues.Add(Cast<T>(Iter));
		}
		return ReturnValues;
	}
	
	virtual TArray<ISubWidgetInterface*> GetSubWidgets() override;
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), DisplayName = "GetSubWidgets")
	virtual TArray<UUserWidget*> K2_GetSubWidgets(TSubclassOf<UUserWidget> InClass);

	template<class T>
	T* GetSubWidget(int32 InIndex) const
	{
		return Cast<T>(GetSubWidget(InIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UUserWidget* GetSubWidget(int32 InIndex, TSubclassOf<UUserWidget> InClass) const;

	UFUNCTION(BlueprintPure)
	int32 FindSubWidget(UUserWidget* InWidget) const;

	virtual ISubWidgetInterface* GetSubWidget(int32 InIndex) const override
	{
		if(SubWidgetEntries.IsValidIndex(InIndex))
		{
			return Cast<ISubWidgetInterface>(SubWidgetEntries[InIndex].Widget);
		}
		return nullptr;
	}

	virtual int32 FindSubWidget(ISubWidgetInterface* InWidget) const override
	{
		for(int32 Index = 0; Index < SubWidgetEntries.Num(); ++Index)
		{
			if(Cast<ISubWidgetInterface>(SubWidgetEntries[Index].Widget) == InWidget)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	UFUNCTION(BlueprintPure)
	virtual int32 GetChildWidgetNum() const override { return ChildWidgets.Num(); }

	template<class T>
	TArray<T*> GetChildWidgets()
	{
		TArray<T*> ReturnValues;
		for(auto Iter : GetChildWidgets())
		{
			ReturnValues.Add(Cast<T>(Iter));
		}
		return ReturnValues;
	}
	
	virtual TArray<IScreenWidgetInterface*> GetChildWidgets() override { return ChildWidgets; }
		
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), DisplayName = "GetChildWidgets")
	virtual TArray<UUserWidgetBase*> K2_GetChildWidgets(TSubclassOf<UUserWidgetBase> InClass);

	template<class T>
	T* GetChildWidget(int32 InIndex) const
	{
		return Cast<T>(GetChildWidget(InIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UUserWidgetBase* GetChildWidget(int32 InIndex, TSubclassOf<UUserWidgetBase> InClass) const
	{
		return GetDeterminesOutputObject(Cast<UUserWidgetBase>(GetChildWidget(InIndex)), InClass);
	}

	UFUNCTION(BlueprintPure)
	int32 FindChildWidget(UUserWidgetBase* InWidget) const
	{
		return FindChildWidget(Cast<IScreenWidgetInterface>(InWidget));
	}

	virtual IScreenWidgetInterface* GetChildWidget(int32 InIndex) const override
	{
		if(ChildWidgets.IsValidIndex(InIndex))
		{
			return ChildWidgets[InIndex];
		}
		return nullptr;
	}

	virtual int32 FindChildWidget(IScreenWidgetInterface* InWidget) const override
	{
		return ChildWidgets.Find(InWidget);
	}

	FOnWidgetStateChanged& GetOnWidgetStateChanged() { return OnWidgetStateChanged; }

};
