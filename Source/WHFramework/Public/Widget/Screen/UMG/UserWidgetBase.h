// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Blueprint/UserWidget.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Widget/Interfaces/ScreenWidgetInterface.h"

#include "UserWidgetBase.generated.h"

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

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnCreate(AActor* InOwner = nullptr) override;

	UFUNCTION(BlueprintNativeEvent)
	void OnInitialize(AActor* InOwner = nullptr) override;

	UFUNCTION(BlueprintNativeEvent)
	void OnOpen(const TArray<FParameter>& InParams, bool bInstant = false) override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnClose(bool bInstant = false) override;

	UFUNCTION(BlueprintNativeEvent)
	void OnReset() override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnRefresh() override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnDestroy(bool bRecovery = false) override;

	UFUNCTION(BlueprintNativeEvent)
	void OnStateChanged(EScreenWidgetState InWidgetState) override;

public:
	bool Initialize() override;

	UFUNCTION(BlueprintCallable)
	void Initialize(AActor* InOwner) override;
	
	void Open(const TArray<FParameter>* InParams = nullptr, bool bInstant = false) override;
	
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	void Open(const TArray<FParameter>& InParams, bool bInstant = false) override;

	UFUNCTION(BlueprintCallable)
	void Close(bool bInstant = false) override;

	UFUNCTION(BlueprintCallable)
	void Toggle(bool bInstant = false) override;

	UFUNCTION(BlueprintCallable)
	void Reset() override;

	UFUNCTION(BlueprintCallable)
	void Refresh() override;

	UFUNCTION(BlueprintCallable)
	void Destroy(bool bRecovery = false) override;
	
protected:
	UFUNCTION(BlueprintCallable)
	void FinishOpen(bool bInstant) override;

	UFUNCTION(BlueprintCallable)
	void FinishClose(bool bInstant) override;

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
		return Cast<UUserWidgetBase>(GetChild(InIndex));
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
		
	UPROPERTY(EditDefaultsOnly)
	TArray<FName> ChildNames;
		
	UPROPERTY(EditDefaultsOnly)
	int32 WidgetZOrder;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "ParentName != NAME_None)"))
	FAnchors WidgetAnchors;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "ParentName != NAME_None)"))
	bool bWidgetAutoSize;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "ParentName != NAME_None) && bWidgetAutoSize == false"))
	FVector2D WidgetDrawSize;
	
	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "ParentName != NAME_None) && bWidgetAutoSize == false"))
	FMargin WidgetOffsets;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "ParentName != NAME_None)"))
	FVector2D WidgetAlignment;

	UPROPERTY(EditDefaultsOnly)
	EWidgetCreateType WidgetCreateType;
	
	UPROPERTY(EditDefaultsOnly)
	EWidgetOpenType WidgetOpenType;
	
	UPROPERTY(EditDefaultsOnly)
	EWidgetOpenFinishType WidgetOpenFinishType;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "WidgetOpenFinishType == EWidgetOpenFinishType::Delay"))
	float WidgetOpenFinishTime;

	UPROPERTY(EditDefaultsOnly)
	EWidgetCloseType WidgetCloseType;
	
	UPROPERTY(EditDefaultsOnly)
	EWidgetCloseFinishType WidgetCloseFinishType;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "WidgetCloseFinishType == EWidgetCloseFinishType::Delay"))
	float WidgetCloseFinishTime;

	UPROPERTY(EditDefaultsOnly)
	EWidgetRefreshType WidgetRefreshType;

	UPROPERTY(EditDefaultsOnly, meta = (EditConditionHides, EditCondition = "WidgetRefreshType == EWidgetRefreshType::Timer"))
	float WidgetRefreshTime;

	UPROPERTY(VisibleAnywhere)
	TArray<FParameter> WidgetParams;

	UPROPERTY(EditDefaultsOnly)
	EInputMode InputMode;

	UPROPERTY(Transient)
	EScreenWidgetState WidgetState;

	UPROPERTY(Transient)
	AActor* OwnerActor;

	IScreenWidgetInterface* LastTemporary;
	
	IScreenWidgetInterface* ParentWidget;
		
	IScreenWidgetInterface* TemporaryChild;

	TArray<IScreenWidgetInterface*> ChildWidgets;

	UPROPERTY(BlueprintAssignable)
	FOnWidgetStateChanged OnWidgetStateChanged;

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
	virtual EScreenWidgetState GetWidgetState() const override
	{
		if(ParentWidget && ParentWidget->GetWidgetState() == EScreenWidgetState::Closed)
		{
			return EScreenWidgetState::Closed;
		}
		return WidgetState;
	}
	
	UFUNCTION(BlueprintPure)
	TArray<FParameter> GetWidgetParams() const { return WidgetParams; }
	
	UFUNCTION(BlueprintPure)
	virtual EInputMode GetInputMode() const override { return InputMode; }

	UFUNCTION(BlueprintPure)
	virtual AActor* GetOwnerActor() const override { return OwnerActor; }

	virtual IScreenWidgetInterface* GetLastTemporary() const override { return LastTemporary; }

	virtual void SetLastTemporary(IScreenWidgetInterface* InLastTemporary) override { LastTemporary = InLastTemporary; }

	virtual IScreenWidgetInterface* GetParentWidgetN() const override { return ParentWidget; }

	virtual void SetParentWidgetN(IScreenWidgetInterface* InParentWidget) override { ParentWidget = InParentWidget; }
	
	virtual IScreenWidgetInterface* GetTemporaryChild() const override { return TemporaryChild; }

	virtual void SetTemporaryChild(IScreenWidgetInterface* InTemporaryChild) override { TemporaryChild = InTemporaryChild; }

	UFUNCTION(BlueprintPure)
	virtual int32 GetChildNum() const override { return ChildWidgets.Num(); }

	virtual TArray<IScreenWidgetInterface*>& GetChildWidgets() override { return ChildWidgets; }

	FOnWidgetStateChanged& GetOnWidgetStateChanged() { return OnWidgetStateChanged; }

	UFUNCTION(BlueprintPure)
	virtual class UPanelWidget* GetRootPanelWidget() const override;
};
