// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Blueprint/UserWidget.h"
#include "Common/CommonTypes.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Widget/Interfaces/ScreenWidgetInterface.h"

#include "UserWidgetBase.generated.h"

class UWidgetAnimatorBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UUserWidgetBase : public UUserWidget, public IScreenWidgetInterface, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UUserWidgetBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

protected:
	UPROPERTY(EditDefaultsOnly)
	bool bWidgetTickAble;

public:
	virtual bool IsTickAble_Implementation() const override { return bWidgetTickAble; }

	virtual void OnTick_Implementation(float DeltaSeconds) override;

public:
	virtual int32 GetLimit_Implementation() const override { return 0; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

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
	void K2_OnDestroy(bool bRecovery = false);
	UFUNCTION()
	virtual void OnDestroy(bool bRecovery = false) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnStateChanged")
	void K2_OnStateChanged(EScreenWidgetState InWidgetState);
	UFUNCTION()
	virtual void OnStateChanged(EScreenWidgetState InWidgetState) override;

public:
	virtual void Init(UObject* InOwner, const TArray<FParameter>* InParams, bool bForce = false) override;
	
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	virtual void Init(UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false) override;

	virtual void Open(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, bool bForce = false) override;
	
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	virtual void Open(const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Close(bool bInstant = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Toggle(bool bInstant = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Reset(bool bForce = false) override;

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
	T* CreateSubWidget(const TArray<FParameter>* InParams = nullptr, TSubclassOf<USubWidgetBase> InClass = T::StaticClass())
	{
		return Cast<T>(CreateSubWidget(InClass, InParams ? *InParams : TArray<FParameter>()));
	}

	template<class T>
	T* CreateSubWidget(const TArray<FParameter>& InParams, TSubclassOf<USubWidgetBase> InClass = T::StaticClass())
	{
		return Cast<T>(CreateSubWidget(InClass, InParams));
	}

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"))
	USubWidgetBase* CreateSubWidget(TSubclassOf<USubWidgetBase> InClass, const TArray<FParameter>& InParams);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool DestroySubWidget(USubWidgetBase* InWidget, bool bRecovery = false);

	UFUNCTION(BlueprintPure)
	TArray<USubWidgetBase*> GetAllSubWidgets() const;

public:
	void AddChild(IScreenWidgetInterface* InChildWidget) override;

	void RemoveChild(IScreenWidgetInterface* InChildWidget) override;

	void RemoveAllChild() override;

	template<class T>
	T* GetChild(int32 InIndex) const
	{
		return Cast<T>(GetChild(InIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UUserWidgetBase* GetChild(int32 InIndex, TSubclassOf<UUserWidgetBase> InClass) const
	{
		return GetDeterminesOutputObject(Cast<UUserWidgetBase>(GetChild(InIndex)), InClass);
	}

	IScreenWidgetInterface* GetChild(int32 InIndex) const override
	{
		if(ChildWidgets.IsValidIndex(InIndex))
		{
			return ChildWidgets[InIndex];
		}
		return nullptr;
	}

protected:
	UPROPERTY(EditDefaultsOnly)
	EWidgetType WidgetType;

	UPROPERTY(EditDefaultsOnly)
	FName WidgetName;

	UPROPERTY(EditDefaultsOnly)
	FName ParentName;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = EDC_ParentName))
	FName ParentSlot;

	UPROPERTY(EditDefaultsOnly)
	TArray<FName> ChildNames;
		
	UPROPERTY(EditDefaultsOnly)
	int32 WidgetZOrder;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = EDC_ParentName))
	FAnchors WidgetAnchors;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = EDC_ParentName))
	bool bWidgetAutoSize;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = EDC_WidgetAutoSize))
	FVector2D WidgetDrawSize;
	
	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = EDC_WidgetAutoSize))
	FMargin WidgetOffsets;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = EDC_ParentName))
	FVector2D WidgetAlignment;

	UPROPERTY(EditDefaultsOnly)
	EWidgetCreateType WidgetCreateType;
	
	UPROPERTY(EditDefaultsOnly)
	EWidgetOpenType WidgetOpenType;
	
	UPROPERTY(EditDefaultsOnly)
	EWidgetOpenFinishType WidgetOpenFinishType;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "WidgetOpenFinishType == EWidgetOpenFinishType::Delay"))
	float WidgetOpenFinishTime;

	UPROPERTY(EditDefaultsOnly, Instanced, meta = (EditConditionHides, EditCondition = "WidgetOpenFinishType == EWidgetOpenFinishType::Animator"))
	UWidgetAnimatorBase* WidgetOpenAnimator;

	UPROPERTY(EditDefaultsOnly)
	EWidgetCloseType WidgetCloseType;
	
	UPROPERTY(EditDefaultsOnly)
	EWidgetCloseFinishType WidgetCloseFinishType;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "WidgetCloseFinishType == EWidgetCloseFinishType::Delay"))
	float WidgetCloseFinishTime;

	UPROPERTY(EditDefaultsOnly, Instanced, meta = (EditConditionHides, EditCondition = "WidgetCloseFinishType == EWidgetCloseFinishType::Animator"))
	UWidgetAnimatorBase* WidgetCloseAnimator;

	UPROPERTY(EditDefaultsOnly)
	EWidgetRefreshType WidgetRefreshType;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "WidgetRefreshType == EWidgetRefreshType::Timer"))
	float WidgetRefreshTime;

	UPROPERTY(VisibleAnywhere)
	TArray<FParameter> WidgetParams;

	UPROPERTY(EditDefaultsOnly)
	EInputMode WidgetInputMode;

	UPROPERTY(Transient)
	EScreenWidgetState WidgetState;

	UPROPERTY(Transient)
	UObject* OwnerObject;

	IScreenWidgetInterface* LastTemporary;
	
	IScreenWidgetInterface* ParentWidget;
		
	IScreenWidgetInterface* TemporaryChild;

	TArray<IScreenWidgetInterface*> ChildWidgets;

	UPROPERTY(BlueprintAssignable)
	FOnWidgetStateChanged OnWidgetStateChanged;

public:
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnOpened"))
	FK2_OnWidgetOpened K2_OnOpened;

	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnClosed"))
	FK2_OnWidgetClosed K2_OnClosed;

	FOnWidgetOpened OnOpened;

	FOnWidgetClosed OnClosed;

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
	virtual TArray<FName> GetChildNames() const override { return ChildNames; }

	UFUNCTION(BlueprintPure)
	virtual int32 GetWidgetZOrder() const override { return WidgetZOrder; }

	UFUNCTION(BlueprintPure)
	virtual FAnchors GetWidgetAnchors() const override { return WidgetAnchors; }

	UFUNCTION(BlueprintPure)
	virtual bool IsWidgetAutoSize() const override { return bWidgetAutoSize; }

	UFUNCTION(BlueprintPure)
	virtual FVector2D GetWidgetDrawSize() const override { return WidgetDrawSize; }

	UFUNCTION(BlueprintPure)
	virtual FMargin GetWidgetOffsets() const override { return WidgetOffsets; }

	UFUNCTION(BlueprintPure)
	virtual FVector2D GetWidgetAlignment() const override { return WidgetAlignment; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetCreateType GetWidgetCreateType() const override { return WidgetCreateType; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetOpenType GetWidgetOpenType() const override { return WidgetOpenType; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetCloseType GetWidgetCloseType() const override { return WidgetCloseType; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetRefreshType GetWidgetRefreshType() const override { return WidgetRefreshType; }

	UFUNCTION(BlueprintPure)
	virtual EScreenWidgetState GetWidgetState() const override { return WidgetState; }
	
	UFUNCTION(BlueprintPure)
	TArray<FParameter> GetWidgetParams() const { return WidgetParams; }
	
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
	virtual int32 GetChildNum() const override { return ChildWidgets.Num(); }

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
	virtual TArray<IScreenWidgetInterface*>& GetChildWidgets() override { return ChildWidgets; }

	FOnWidgetStateChanged& GetOnWidgetStateChanged() { return OnWidgetStateChanged; }

	UFUNCTION(BlueprintPure)
	virtual UPanelWidget* GetRootPanelWidget() const override;

private:
	UFUNCTION()
	bool EDC_ParentName() const { return ParentName != NAME_None; };
	
	UFUNCTION()
	bool EDC_WidgetAutoSize() const { return ParentName != NAME_None && !bWidgetAutoSize; };
};
