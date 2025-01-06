// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Slate/Editor/Base/IEditorWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKSLATE_API SEditorWidgetBase : public IEditorWidgetBase
{
	friend class FSlateWidgetManager;

public:
	SLATE_BEGIN_ARGS(SEditorWidgetBase) {}

	SLATE_END_ARGS()

	SEditorWidgetBase();

	void Construct(const FArguments& InArgs);

	static const FName WidgetName;

public:
	virtual void OnCreate() override;

	virtual void OnInitialize() override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;

	virtual void OnSave() override;
	
	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnTick(float InDeltaTime) override;

	virtual void OnDestroy() override;

protected:
	virtual void OnBindCommands(const TSharedRef<FUICommandList>& InCommands);

	virtual void OnWindowActivated();
	
	virtual void OnWindowDeactivated();
	
	virtual void OnWindowClosed(const TSharedRef<SWindow>& InOwnerWindow);

protected:
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

public:
	virtual void Open(const TArray<FParameter>* InParams = nullptr, bool bInstant = false);
	
	virtual void Open(const TArray<FParameter>& InParams, bool bInstant = false);

	virtual void Close(bool bInstant = false) override;

	virtual void Toggle(bool bInstant = false) override;

	virtual void Save() override;

	virtual void Reset() override;

	virtual void Rebuild() override;

	virtual void Refresh() override;

	virtual void Destroy() override;

protected:
	virtual void FinishOpen(bool bInstant = false) override;

	virtual void FinishClose(bool bInstant = false) override;

public:
	virtual bool CanSave() override;

public:
	virtual void AddChild(const TSharedPtr<IEditorWidgetBase>& InWidget) override;

	virtual void RemoveChild(const TSharedPtr<IEditorWidgetBase>& InWidget) override;

	virtual void RemoveAllChild() override;

protected:
	FName _WidgetName;

	EEditorWidgetType WidgetType;

	EEditorWidgetState WidgetState;

	TSharedPtr<IEditorWidgetBase> ParentWidget;

	TArray<TSharedPtr<IEditorWidgetBase>> ChildWidgets;

	TMap<FName, TSharedPtr<IEditorWidgetBase>> ChildWidgetMap;
	
	TSharedRef<FUICommandList> WidgetCommands;
	
	FCurveSequence WidgetAnimSequence;

private:
	bool bInitialized;
	
	FDelegateHandle OnWindowActivatedHandle;
	FDelegateHandle OnWindowDeactivatedHandle;
	FDelegateHandle OnWindowClosedHandle;

public:
	virtual TSharedPtr<SWindow> GetOwnerWindow() override;

	virtual TSharedRef<IEditorWidgetBase> TakeWidget() override;

	virtual TSharedPtr<IEditorWidgetBase> GetChild(int32 InIndex) const override;

	virtual FName GetWidgetName() const override { return _WidgetName; }

	virtual EEditorWidgetState GetWidgetState() const override { return WidgetState; }

	virtual int32 GetChildNum() const override { return ChildWidgets.Num(); }

	virtual TSharedPtr<IEditorWidgetBase> GetParentWidgetN() const override { return ParentWidget; }

	template<class T>
	TSharedPtr<T> GetParentWidgetN() const
	{
		return StaticCastSharedPtr<T>(GetParentWidgetN());
	}

	virtual TArray<TSharedPtr<IEditorWidgetBase>>& GetChildWidgets() override { return ChildWidgets; }
};
