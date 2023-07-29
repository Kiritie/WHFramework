// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

enum class EEditorWidgetType : uint8
{
	Main,
	Child
};

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SEditorWidgetBase : public SCompoundWidget
{
public:
	SEditorWidgetBase();

	SLATE_BEGIN_ARGS(SEditorWidgetBase) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	virtual void OnWindowActivated();
	
	virtual void OnWindowDeactivated();
	
	virtual void OnWindowClosed(const TSharedRef<SWindow>& InOwnerWindow);

private:
	FDelegateHandle OnWindowActivatedHandle;
	FDelegateHandle OnWindowDeactivatedHandle;
	FDelegateHandle OnWindowClosedHandle;

public:
	virtual void OnCreate();

	virtual void OnReset();

	virtual void OnRefresh();

	virtual void OnDestroy();

public:
	virtual void Reset();

	virtual void Refresh();

	virtual void Destroy();

public:
	virtual void AddChild(const TSharedPtr<SEditorWidgetBase>& InChildWidget);

	virtual void RemoveChild(const TSharedPtr<SEditorWidgetBase>& InChildWidget);

	virtual void RemoveAllChild();

protected:
	FName WidgetName;

	EEditorWidgetType WidgetType;

	TSharedPtr<SEditorWidgetBase> ParentWidget;

	TArray<TSharedPtr<SEditorWidgetBase>> ChildWidgets;

	TMap<FName, TSharedPtr<SEditorWidgetBase>> ChildWidgetMap;

public:
	TSharedPtr<SEditorWidgetBase> GetChild(int32 InIndex) const;

	TSharedPtr<SWindow> GetOwnerWindow();

	FName GetWidgetName() const { return WidgetName; }

	int32 GetChildNum() const { return ChildWidgets.Num(); }

	TSharedPtr<SEditorWidgetBase> GetParentWidgetN() const { return ParentWidget; }

	TArray<TSharedPtr<SEditorWidgetBase>>& GetChildWidgets() { return ChildWidgets; }
};
