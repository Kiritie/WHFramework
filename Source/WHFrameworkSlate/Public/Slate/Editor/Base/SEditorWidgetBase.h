// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Slate/Editor/Interfaces/EditorWidgetInterface.h"

#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKSLATE_API SEditorWidgetBase : public SCompoundWidget, public IEditorWidgetInterface
{
	friend class FSlateWidgetManager;

public:
	SEditorWidgetBase();

	SLATE_BEGIN_ARGS(SEditorWidgetBase) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static const FName WidgetName;

public:
	virtual void OnCreate();

	virtual void OnInitialize();

	virtual void OnOpen(bool bInstant = false);

	virtual void OnClose(bool bInstant = false);

	virtual void OnSave();
	
	virtual void OnReset();

	virtual void OnRefresh();

	virtual void OnDestroy();

protected:
	virtual void OnBindCommands(const TSharedRef<FUICommandList>& InCommands);

	virtual void OnWindowActivated();
	
	virtual void OnWindowDeactivated();
	
	virtual void OnWindowClosed(const TSharedRef<SWindow>& InOwnerWindow);

protected:
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

public:
	virtual void Open(bool bInstant = false);

	virtual void Close(bool bInstant = false);

	virtual void Toggle(bool bInstant = false);

	virtual void Save();

	virtual void Reset();

	virtual void Rebuild();

	virtual void Refresh();

	virtual void Destroy();

protected:
	virtual void FinishOpen(bool bInstant = false);

	virtual void FinishClose(bool bInstant = false);

public:
	virtual bool CanSave();

public:
	virtual void AddChild(const TSharedPtr<SEditorWidgetBase>& InChildWidget);

	virtual void RemoveChild(const TSharedPtr<SEditorWidgetBase>& InChildWidget);

	virtual void RemoveAllChild();

protected:
	FName _WidgetName;

	EEditorWidgetType WidgetType;

	EEditorWidgetState WidgetState;

	TSharedPtr<SEditorWidgetBase> ParentWidget;

	TArray<TSharedPtr<SEditorWidgetBase>> ChildWidgets;

	TMap<FName, TSharedPtr<SEditorWidgetBase>> ChildWidgetMap;
	
	TSharedRef<FUICommandList> WidgetCommands;
	
	FCurveSequence WidgetAnimSequence;

private:
	bool bInitialized;
	
	FDelegateHandle OnWindowActivatedHandle;
	FDelegateHandle OnWindowDeactivatedHandle;
	FDelegateHandle OnWindowClosedHandle;

public:
	TSharedPtr<SWindow> GetOwnerWindow();

	TSharedRef<SEditorWidgetBase> TakeWidget();

	TSharedPtr<SEditorWidgetBase> GetChild(int32 InIndex) const;

	FName GetWidgetName() const { return _WidgetName; }

	EEditorWidgetState GetWidgetState() const { return WidgetState; }

	int32 GetChildNum() const { return ChildWidgets.Num(); }

	TSharedPtr<SEditorWidgetBase> GetParentWidgetN() const { return ParentWidget; }

	template<class T>
	TSharedPtr<T> GetParentWidgetN() const
	{
		return StaticCastSharedPtr<T>(ParentWidget);
	}

	TArray<TSharedPtr<SEditorWidgetBase>>& GetChildWidgets() { return ChildWidgets; }
};
