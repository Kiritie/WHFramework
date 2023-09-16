// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Widget/Interfaces/ScreenWidgetInterface.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORK_API SSlateWidgetBase : public SCompoundWidget, public IScreenWidgetInterface
{
public:
	SLATE_BEGIN_ARGS(SSlateWidgetBase)
	{}
	SLATE_END_ARGS()

	SSlateWidgetBase();

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

public:
	virtual void OnCreate(UObject* InOwner = nullptr) override;
	
	virtual void OnInitialize(UObject* InOwner = nullptr) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant = false) override;
	
	virtual void OnClose(bool bInstant = false) override;

	virtual void OnReset() override;
	
	virtual void OnRefresh() override;

	virtual void OnDestroy(bool bRecovery = false) override;

	virtual void OnStateChanged(EScreenWidgetState InWidgetChange) override;

public:
	virtual void Open(const TArray<FParameter>* InParams = nullptr, bool bInstant = false) override;
	
	virtual void Open(const TArray<FParameter>& InParams, bool bInstant = false) override;
	
	virtual void Close(bool bInstant = false) override;

	virtual void Toggle(bool bInstant) override;

	virtual void Reset() override;

	virtual void Refresh() override;

	virtual void Destroy(bool bRecovery = false) override;

protected:
	virtual void FinishOpen(bool bInstant) override;

	virtual void FinishClose(bool bInstant) override;

public:
	virtual void AddChild(IScreenWidgetInterface* InChildWidget) override;

	virtual void RemoveChild(IScreenWidgetInterface* InChildWidget) override;

	virtual void RemoveAllChild() override;
	
	virtual IScreenWidgetInterface* GetChild(int32 InIndex) const override
	{
		if(ChildWidgets.IsValidIndex(InIndex))
		{
			return ChildWidgets[InIndex];
		}
		return nullptr;
	}

protected:
	EWidgetType WidgetType;

	FName WidgetName;

	FName ParentName;

	TArray<FName> ChildNames;

	int32 WidgetZOrder;

	FAnchors WidgetAnchors;

	bool bWidgetAutoSize;

	FVector2D WidgetDrawSize;
	
	FMargin WidgetOffsets;

	FVector2D WidgetAlignment;
								
	EWidgetCreateType WidgetCreateType;
				
	EWidgetOpenType WidgetOpenType;

	EWidgetCloseType WidgetCloseType;
	
	EWidgetRefreshType WidgetRefreshType;

	EInputMode WidgetInputMode;

	EScreenWidgetState WidgetState;

	UObject* OwnerObject;
	
	IScreenWidgetInterface* LastTemporary;
	
	IScreenWidgetInterface* ParentWidget;
			
	IScreenWidgetInterface* TemporaryChild;

	TArray<IScreenWidgetInterface*> ChildWidgets;

public:
	virtual EWidgetType GetWidgetType(bool bInheritParent = true) const override
	{
		if(bInheritParent && ParentWidget) return ParentWidget->GetWidgetType();
		return WidgetType;
	}

	virtual FName GetWidgetName() const override { return WidgetName; }

	virtual FName GetParentName() const override { return ParentName; }

	virtual TArray<FName> GetChildNames() const override { return ChildNames; }

	virtual int32 GetWidgetZOrder() const override { return WidgetZOrder; }

	virtual FAnchors GetWidgetAnchors() const override { return WidgetAnchors; }

	virtual bool IsWidgetAutoSize() const override { return bWidgetAutoSize; }

	virtual FVector2D GetWidgetDrawSize() const override { return WidgetDrawSize; }

	virtual FMargin GetWidgetOffsets() const override { return WidgetOffsets; }

	virtual FVector2D GetWidgetAlignment() const override { return WidgetAlignment; }

	virtual EScreenWidgetState GetWidgetState() const override
	{
		if(ParentWidget && ParentWidget->GetWidgetState() == EScreenWidgetState::Closed)
		{
			return EScreenWidgetState::Closed;
		}
		return WidgetState;
	}

	virtual EWidgetCreateType GetWidgetCreateType() const override { return WidgetCreateType; }

	virtual EWidgetOpenType GetWidgetOpenType() const override { return WidgetOpenType; }

	virtual EWidgetCloseType GetWidgetCloseType() const override { return WidgetCloseType; }
	
	virtual EWidgetRefreshType GetWidgetRefreshType() const override { return WidgetRefreshType; }

	virtual EInputMode GetWidgetInputMode() const override { return WidgetInputMode; }

	virtual UObject* GetOwnerObject() const override { return OwnerObject; }

	virtual IScreenWidgetInterface* GetLastTemporary() const override { return LastTemporary; }

	virtual void SetLastTemporary(IScreenWidgetInterface* InLastTemporary) override { LastTemporary = InLastTemporary; }

	virtual IScreenWidgetInterface* GetParentWidgetN() const override { return ParentWidget; }

	virtual void SetParentWidgetN(IScreenWidgetInterface* InParentWidget) override { ParentWidget = InParentWidget; }
		
	virtual IScreenWidgetInterface* GetTemporaryChild() const override { return TemporaryChild; }

	virtual void SetTemporaryChild(IScreenWidgetInterface* InTemporaryChild) override { TemporaryChild = InTemporaryChild; }

	virtual int32 GetChildNum() const override { return ChildWidgets.Num(); }

	virtual TArray<IScreenWidgetInterface*>& GetChildWidgets() override { return ChildWidgets; }
};
