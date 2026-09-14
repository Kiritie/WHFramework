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
	virtual void OnCreate(const FParameter& InParam) override;
	
	virtual void OnInitialize(const FParameter& InParam) override;

	virtual void OnOpen(const FParameter& InParam, bool bInstant = false) override;
	
	virtual void OnClose(bool bInstant = false) override;

	virtual void OnReset(bool bForce = false) override;
	
	virtual void OnRefresh() override;

	virtual void OnDestroy(EObjectDespawnMode InMode) override;

	virtual void OnStateChanged(EScreenWidgetState InWidgetChange) override;

public:
	virtual void Init(const FParameter& InParam = FParameter(), bool bForce = false) override;
	
	virtual void Open(const FParameter& InParam = FParameter(), bool bInstant = false, bool bForce = false) override;

	virtual void Close(bool bInstant = false) override;

	virtual void Toggle(bool bInstant) override;

	virtual void Reset(bool bForce) override;

	virtual void Refresh() override;

	virtual void Destroy(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy) override;

public:
	virtual bool CanOpen() const override;

protected:
	virtual void FinishOpen(bool bInstant) override;

	virtual void FinishClose(bool bInstant) override;

public:
	template<class T>
	T* CreateSubWidget(const FParameter& InParam = FParameter(), TSubclassOf<UUserWidget> InClass = T::StaticClass())
	{
		return Cast<T>(CreateSubWidget(InClass, InParam));
	}

	virtual ISubWidgetInterface* CreateSubWidget(TSubclassOf<UUserWidget> InClass, const FParameter& InParam = FParameter()) override;

	virtual bool DestroySubWidget(ISubWidgetInterface* InWidget, EObjectDespawnMode InMode) override;

	virtual void DestroyAllSubWidget(EObjectDespawnMode InMode) override;

	virtual void AddChildWidget(IScreenWidgetInterface* InWidget) override;

	virtual void RemoveChildWidget(IScreenWidgetInterface* InWidget) override;

	virtual void RemoveAllChildWidget() override;

protected:
	FName _WidgetName;
	
	FName _ParentName;

	EWidgetType WidgetType;

	FName ParentSlot;

	int32 WidgetZOrder;

	FAnchors WidgetAnchors;

	bool bWidgetAutoSize;
	
	FMargin WidgetOffsets;

	FVector2D WidgetAlignment;
								
	EWidgetCreateType WidgetCreateType;
				
	EWidgetRefreshType WidgetRefreshType;

	FParameter WidgetParams;

	EInputMode WidgetInputMode;

	EScreenWidgetState WidgetState;

	bool bConsumePointerInput;

	bool bInitialized;

	IScreenWidgetInterface* LastTemporary;
	
	IScreenWidgetInterface* ParentWidget;
			
	IScreenWidgetInterface* TemporaryChild;

	TArray<ISubWidgetInterface*> SubWidgets;

	TArray<IScreenWidgetInterface*> ChildWidgets;

public:
	virtual EWidgetType GetWidgetType(bool bInheritParent = true) const override
	{
		if(bInheritParent && ParentWidget) return ParentWidget->GetWidgetType();
		return WidgetType;
	}

	FName GetWidgetName() const { return _WidgetName; }

	FName GetParentName() const { return _ParentName; }

	FName GetParentSlot() const { return ParentSlot; }

	virtual int32 GetWidgetZOrder() const override { return WidgetZOrder; }

	virtual FAnchors GetWidgetAnchors() const override { return WidgetAnchors; }

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
	
	virtual bool IsWidgetOpened(bool bCheckOpening = true, bool bInheritParent = false) const override
	{
		return GetWidgetState(bInheritParent) == EScreenWidgetState::Opened || (bCheckOpening && GetWidgetState(bInheritParent) == EScreenWidgetState::Opening);
	}
	
	virtual bool IsWidgetClosed(bool bCheckClosing = true, bool bInheritParent = false) const override
	{
		return GetWidgetState(bInheritParent) == EScreenWidgetState::Closed || (bCheckClosing && GetWidgetState(bInheritParent) == EScreenWidgetState::Closing);
	}

	EWidgetCreateType GetWidgetCreateType() const { return WidgetCreateType; }

	virtual EWidgetRefreshType GetWidgetRefreshType() const override { return WidgetRefreshType; }

	EInputMode GetWidgetInputMode() const { return WidgetInputMode; }

	virtual FParameter GetWidgetParams() const override { return WidgetParams; }

	virtual UObject* GetOwnerObject() const override { return nullptr; }

	IScreenWidgetInterface* GetLastTemporary() const { return LastTemporary; }

	void SetLastTemporary(IScreenWidgetInterface* InLastTemporary) { LastTemporary = InLastTemporary; }

	virtual IScreenWidgetInterface* GetParentWidgetN() const override { return ParentWidget; }

	virtual void SetParentWidgetN(IScreenWidgetInterface* InParentWidget) override { ParentWidget = InParentWidget; }
		
	IScreenWidgetInterface* GetTemporaryChild() const { return TemporaryChild; }

	void SetTemporaryChild(IScreenWidgetInterface* InTemporaryChild) { TemporaryChild = InTemporaryChild; }
	
	TArray<UWidget*> GetPoolWidgets() const;

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

	template<class T>
	T* GetSubWidget(int32 InIndex) const
	{
		return Cast<T>(GetSubWidget(InIndex));
	}

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

	template<class T>
	T* GetChildWidget(int32 InIndex) const
	{
		return Cast<T>(GetChildWidget(InIndex));
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

	virtual TSharedPtr<SPanel> GetRootPanelWidget() const;

	virtual TSharedPtr<SPanel> GetParentPanelWidget() const;
};
