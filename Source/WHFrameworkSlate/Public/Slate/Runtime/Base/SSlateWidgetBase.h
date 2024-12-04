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
	virtual void Init(UObject* InOwner, const TArray<FParameter>* InParams = nullptr, bool bForce = false) override;

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

	virtual ISubWidgetInterface* CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>* InParams = nullptr) override;

	virtual ISubWidgetInterface* CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>& InParams) override;

	virtual bool DestroySubWidget(ISubWidgetInterface* InWidget, bool bRecovery) override;

	virtual void DestroyAllSubWidget(bool bRecovery) override;

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
				
	EWidgetOpenType WidgetOpenType;

	EWidgetCloseType WidgetCloseType;
	
	EWidgetRefreshType WidgetRefreshType;

	TArray<FParameter> WidgetParams;

	EInputMode WidgetInputMode;

	EScreenWidgetState WidgetState;

	bool bConsumePointerInput;

	UObject* OwnerObject;
	
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

	virtual FName GetWidgetName() const override { return _WidgetName; }

	virtual FName GetParentName() const override { return _ParentName; }

	virtual FName GetParentSlot() const override { return ParentSlot; }

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

	virtual EWidgetCreateType GetWidgetCreateType() const override { return WidgetCreateType; }

	virtual EWidgetOpenType GetWidgetOpenType() const override { return WidgetOpenType; }

	virtual EWidgetCloseType GetWidgetCloseType() const override { return WidgetCloseType; }
	
	virtual EWidgetRefreshType GetWidgetRefreshType() const override { return WidgetRefreshType; }

	virtual EInputMode GetWidgetInputMode() const override { return WidgetInputMode; }

	virtual TArray<FParameter> GetWidgetParams() const override { return WidgetParams; }

	virtual UObject* GetOwnerObject() const override { return OwnerObject; }

	virtual IScreenWidgetInterface* GetLastTemporary() const override { return LastTemporary; }

	virtual void SetLastTemporary(IScreenWidgetInterface* InLastTemporary) override { LastTemporary = InLastTemporary; }

	virtual IScreenWidgetInterface* GetParentWidgetN() const override { return ParentWidget; }

	virtual void SetParentWidgetN(IScreenWidgetInterface* InParentWidget) override { ParentWidget = InParentWidget; }
		
	virtual IScreenWidgetInterface* GetTemporaryChild() const override { return TemporaryChild; }

	virtual void SetTemporaryChild(IScreenWidgetInterface* InTemporaryChild) override { TemporaryChild = InTemporaryChild; }
	
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
