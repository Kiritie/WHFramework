// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Slate/Runtime/Interfaces/ScreenWidgetInterface.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKSLATE_API SSlateWidgetBase : public SCompoundWidget, public IScreenWidgetInterface
{
	friend class FSlateWidgetManager;
	
public:
	SLATE_BEGIN_ARGS(SSlateWidgetBase)
	{}
	SLATE_END_ARGS()

	SSlateWidgetBase();

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	
	static const FName WidgetName;

	static const FName ParentName;

protected:
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
	virtual FReply OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent) override;
	
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent) override;
	
	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent) override;
	
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent) override;

public:
	virtual void OnCreate(UObject* InOwner, const TArray<FParameter>& InParams) override;
	
	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant = false) override;
	
	virtual void OnClose(bool bInstant = false) override;

	virtual void OnReset(bool bForce = false) override;
	
	virtual void OnRefresh() override;

	virtual void OnDestroy(bool bRecovery) override;

	virtual void OnStateChanged(EScreenWidgetState InWidgetChange) override;

public:
	virtual void Init(UObject* InOwner, const TArray<FParameter>* InParams, bool bForce = false) override;

	virtual void Init(UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false) override;
	
	virtual void Open(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, bool bForce = false) override;
	
	virtual void Open(const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false) override;

	virtual void Close(bool bInstant = false) override;

	virtual void Toggle(bool bInstant) override;

	virtual void Reset(bool bForce) override;

	virtual void Refresh() override;

	virtual void Destroy(bool bRecovery = false) override;

public:
	virtual bool CanOpen() const override;

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

	virtual int32 FindChild(IScreenWidgetInterface* InChildWidget) const override
	{
		return ChildWidgets.Find(InChildWidget);
	}

protected:
	FName _WidgetName;
	
	FName _ParentName;

	EWidgetType WidgetType;

	FName ParentSlot;

	int32 WidgetZOrder;

	bool bWidgetPenetrable;

	FAnchors WidgetAnchors;

	bool bWidgetAutoSize;
	
	FMargin WidgetOffsets;

	FVector2D WidgetAlignment;
								
	EWidgetCreateType WidgetCreateType;
				
	EWidgetOpenType WidgetOpenType;

	EWidgetCloseType WidgetCloseType;
	
	EWidgetRefreshType WidgetRefreshType;

	TArray<FParameter> WidgetParams;

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

	virtual FName GetWidgetName() const override { return _WidgetName; }

	virtual FName GetParentName() const override { return _ParentName; }

	virtual FName GetParentSlot() const override { return ParentSlot; }

	virtual int32 GetWidgetZOrder() const override { return WidgetZOrder; }

	virtual FAnchors GetWidgetAnchors() const override { return WidgetAnchors; }

	virtual bool IsWidgetPenetrable() const override { return bWidgetPenetrable; }

	virtual bool IsWidgetAutoSize() const override { return bWidgetAutoSize; }

	virtual FVector2D GetWidgetDrawSize() const override { return FVector2D(WidgetOffsets.Right, WidgetOffsets.Bottom); }

	virtual FMargin GetWidgetOffsets() const override { return WidgetOffsets; }

	virtual FVector2D GetWidgetAlignment() const override { return WidgetAlignment; }

	virtual EScreenWidgetState GetWidgetState(bool bInheritParent = false) const override
	{
		if(bInheritParent && ParentWidget && ParentWidget->GetWidgetState() == EScreenWidgetState::Closed)
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

	virtual TSharedPtr<SPanel> GetRootPanelWidget() const;

	virtual TSharedPtr<SPanel> GetParentPanelWidget() const;
};
