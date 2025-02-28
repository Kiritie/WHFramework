// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CommonUserWidget.h"
#include "Common/CommonTypes.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Slate/Runtime/Interfaces/ScreenWidgetInterface.h"

#include "UserWidgetBase.generated.h"

class USubWidgetBase;
class UWidgetAnimatorBase;
/**
 * 
 */
UCLASS(BlueprintType, meta = (DisableNativeTick))
class WHFRAMEWORK_API UUserWidgetBase : public UCommonUserWidget, public IScreenWidgetInterface, public IObjectPoolInterface
{
	friend class UWidgetModule;
	
	GENERATED_BODY()

public:
	UUserWidgetBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	UPROPERTY(EditDefaultsOnly)
	bool bWidgetTickAble;

public:
	virtual bool IsTickAble_Implementation() const override { return bWidgetTickAble; }

	virtual void OnTick_Implementation(float DeltaSeconds) override;

public:
	UFUNCTION(BlueprintImplementableEvent, meta = (AutoCreateRefTerm = "InParams"), DisplayName = "OnCreate")
	void K2_OnCreate(UObject* InOwner, const TArray<FParameter>& InParams);
	UFUNCTION()
	virtual void OnCreate(UObject* InOwner, const TArray<FParameter>& InParams) override;

	UFUNCTION(BlueprintImplementableEvent, meta = (AutoCreateRefTerm = "InParams"), DisplayName = "OnInitialize")
	void K2_OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams);
	UFUNCTION()
	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;
	
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnReset")
	void K2_OnReset(bool bForce = false);
	UFUNCTION()
	virtual void OnReset(bool bForce = false) override;

	UFUNCTION(BlueprintImplementableEvent, meta = (AutoCreateRefTerm = "InParams"), DisplayName = "OnOpen")
	void K2_OnOpen(const TArray<FParameter>& InParams, bool bInstant = false);
	UFUNCTION()
	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant = false) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnClose")
	void K2_OnClose(bool bInstant = false);
	UFUNCTION()
	virtual void OnClose(bool bInstant = false) override;
	
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	UFUNCTION()
	virtual void OnRefresh() override;
	
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnDestroy")
	void K2_OnDestroy(bool bRecovery);
	UFUNCTION()
	virtual void OnDestroy(bool bRecovery) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnStateChanged")
	void K2_OnStateChanged(EScreenWidgetState InWidgetState);
	UFUNCTION()
	virtual void OnStateChanged(EScreenWidgetState InWidgetState) override;

public:
	virtual void Init(UObject* InOwner, const TArray<FParameter>* InParams = nullptr, bool bForce = false) override;
	
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	virtual void Init(UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Reset(bool bForce = false) override;

	virtual void Open(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, bool bForce = false) override;
	
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	virtual void Open(const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Close(bool bInstant = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Toggle(bool bInstant = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Refresh() override;

	UFUNCTION(BlueprintCallable)
	virtual void Destroy(bool bRecovery = false) override;
	
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
	T* CreateSubWidget(const TArray<FParameter>* InParams = nullptr, TSubclassOf<UUserWidget> InClass = T::StaticClass())
	{
		return Cast<T>(CreateSubWidget(InClass, InParams ? *InParams : TArray<FParameter>()));
	}

	template<class T>
	T* CreateSubWidget(const TArray<FParameter>& InParams, TSubclassOf<UUserWidget> InClass = T::StaticClass())
	{
		return Cast<T>(CreateSubWidget(InClass, InParams));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"), DisplayName = "CreateSubWidget")
	UUserWidget* K2_CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>& InParams);

	virtual ISubWidgetInterface* CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>* InParams = nullptr) override;

	virtual ISubWidgetInterface* CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>& InParams) override;

	UFUNCTION(BlueprintCallable, DisplayName = "DestroySubWidget")
	bool K2_DestroySubWidget(UUserWidget* InWidget, bool bRecovery = false);

	virtual bool DestroySubWidget(ISubWidgetInterface* InWidget, bool bRecovery) override;

	UFUNCTION(BlueprintCallable)
	virtual void DestroyAllSubWidget(bool bRecovery) override;

	virtual void AddChildWidget(IScreenWidgetInterface* InWidget) override;

	virtual void RemoveChildWidget(IScreenWidgetInterface* InWidget) override;

	virtual void RemoveAllChildWidget() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EWidgetType WidgetType;

	UPROPERTY(EditDefaultsOnly)
	FName WidgetName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FName ParentName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = EDC_ParentName))
	FName ParentSlot;
		
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 WidgetZOrder;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = EDC_ParentName))
	FAnchors WidgetAnchors;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = EDC_ParentName))
	bool bWidgetAutoSize;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = EDC_WidgetAutoSize))
	FMargin WidgetOffsets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = EDC_ParentName))
	FVector2D WidgetAlignment;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EWidgetCreateType WidgetCreateType;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EWidgetOpenType WidgetOpenType;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EWidgetOpenFinishType WidgetOpenFinishType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "WidgetOpenFinishType == EWidgetOpenFinishType::Delay"))
	float WidgetOpenFinishTime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Instanced, meta = (EditConditionHides, EditCondition = "WidgetOpenFinishType == EWidgetOpenFinishType::Animator"))
	UWidgetAnimatorBase* WidgetOpenAnimator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EWidgetCloseType WidgetCloseType;
	
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<FParameter> WidgetParams;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EInputMode WidgetInputMode;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bWidgetAutoFocus;

	UPROPERTY(Transient)
	EScreenWidgetState WidgetState;

	UPROPERTY(Transient)
	UObject* OwnerObject;

	IScreenWidgetInterface* LastTemporary;
	
	IScreenWidgetInterface* ParentWidget;
		
	IScreenWidgetInterface* TemporaryChild;

	TArray<ISubWidgetInterface*> SubWidgets;

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
	
	FOnWidgetClosed OnDestroyed;

private:
	FTimerHandle WidgetFinishOpenTimerHandle;
	FTimerHandle WidgetFinishCloseTimerHandle;
	FTimerHandle WidgetRefreshTimerHandle;

public:
	UFUNCTION(BlueprintPure)
	virtual EWidgetType GetWidgetType(bool bInheritParent = true) const override
	{
		if(bInheritParent && ParentWidget) return ParentWidget->GetWidgetType();
		return WidgetType;
	}

	UFUNCTION(BlueprintPure)
	virtual FName GetWidgetName() const override { return WidgetName; }

	UFUNCTION(BlueprintPure)
	virtual FName GetParentName() const override { return ParentName; }

	UFUNCTION(BlueprintPure)
	virtual FName GetParentSlot() const override { return ParentSlot; }

	UFUNCTION(BlueprintPure)
	virtual int32 GetWidgetZOrder() const override { return WidgetZOrder; }

	UFUNCTION(BlueprintPure)
	virtual FAnchors GetWidgetAnchors() const override { return WidgetAnchors; }

	UFUNCTION(BlueprintPure)
	virtual bool IsWidgetAutoSize() const override { return bWidgetAutoSize; }

	UFUNCTION(BlueprintPure)
	virtual FVector2D GetWidgetDrawSize() const override { return FVector2D(WidgetOffsets.Right, WidgetOffsets.Bottom); }

	UFUNCTION(BlueprintPure)
	virtual FMargin GetWidgetOffsets() const override { return WidgetOffsets; }

	UFUNCTION(BlueprintPure)
	virtual FVector2D GetWidgetAlignment() const override { return WidgetAlignment; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetCreateType GetWidgetCreateType() const override { return WidgetCreateType; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetOpenType GetWidgetOpenType() const override { return WidgetOpenType; }

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual UWidgetAnimatorBase* GetWidgetOpenAnimator(TSubclassOf<UWidgetAnimatorBase> InClass = nullptr) const;

	UFUNCTION(BlueprintPure)
	virtual EWidgetCloseType GetWidgetCloseType() const override { return WidgetCloseType; }

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
	virtual TArray<FParameter> GetWidgetParams() const override { return WidgetParams; }
	
	virtual TArray<FParameter> GetSpawnParams_Implementation() const override { return WidgetParams; }

	UFUNCTION(BlueprintPure)
	virtual EInputMode GetWidgetInputMode() const override { return WidgetInputMode; }
	
	template<class T>
	T* GetOwnerObject() const
	{
		return Cast<T>(GetOwnerObject());
	}
	virtual UObject* GetOwnerObject() const override { return OwnerObject; }

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual UObject* GetOwnerObject(TSubclassOf<UObject> InClass) const { return GetDeterminesOutputObject(OwnerObject, InClass); }

	virtual IScreenWidgetInterface* GetLastTemporary() const override { return LastTemporary; }

	virtual void SetLastTemporary(IScreenWidgetInterface* InLastTemporary) override { LastTemporary = InLastTemporary; }

	template<class T>
	T* GetParentWidgetN() const
	{
		return Cast<T>(GetParentWidgetN());
	}
	virtual IScreenWidgetInterface* GetParentWidgetN() const override { return ParentWidget; }

	virtual void SetParentWidgetN(IScreenWidgetInterface* InParentWidget) override { ParentWidget = InParentWidget; }
	
	template<class T>
	T* GetTemporaryChild() const
	{
		return Cast<T>(GetTemporaryChild());
	}
	virtual IScreenWidgetInterface* GetTemporaryChild() const override { return TemporaryChild; }

	virtual void SetTemporaryChild(IScreenWidgetInterface* InTemporaryChild) override { TemporaryChild = InTemporaryChild; }
	
	UFUNCTION(BlueprintPure)
	TArray<UWidget*> GetPoolWidgets() const;

	UFUNCTION(BlueprintPure)
	virtual int32 GetSubWidgetNum() const override { return SubWidgets.Num(); }

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
	
	virtual TArray<ISubWidgetInterface*> GetSubWidgets() override { return SubWidgets; }
	
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
		if(SubWidgets.IsValidIndex(InIndex))
		{
			return SubWidgets[InIndex];
		}
		return nullptr;
	}

	virtual int32 FindSubWidget(ISubWidgetInterface* InWidget) const override
	{
		return SubWidgets.Find(InWidget);
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

	UFUNCTION(BlueprintPure)
	UPanelWidget* GetRootPanelWidget() const;

	UFUNCTION(BlueprintPure)
	UPanelWidget* GetParentPanelWidget() const;

private:
	UFUNCTION()
	bool EDC_ParentName() const { return ParentName != NAME_None; };
	
	UFUNCTION()
	bool EDC_WidgetAutoSize() const { return ParentName != NAME_None && !bWidgetAutoSize; };
};
