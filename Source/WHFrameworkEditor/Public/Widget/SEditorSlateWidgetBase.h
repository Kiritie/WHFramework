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
class WHFRAMEWORKEDITOR_API SEditorSlateWidgetBase : public SCompoundWidget
{
public:
	SEditorSlateWidgetBase();

	SLATE_BEGIN_ARGS(SEditorSlateWidgetBase) {}

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
	virtual void AddChild(const TSharedPtr<SEditorSlateWidgetBase>& InChildWidget);

	virtual void RemoveChild(const TSharedPtr<SEditorSlateWidgetBase>& InChildWidget);

	virtual void RemoveAllChild();

	virtual TSharedPtr<SEditorSlateWidgetBase> GetChild(int32 InIndex) const
	{
		if(ChildWidgets.IsValidIndex(InIndex))
		{
			return ChildWidgets[InIndex];
		}
		return nullptr;
	}

	virtual TSharedPtr<SWindow> GetOwnerWindow();

protected:
	FName WidgetName;

	EEditorWidgetType WidgetType;

	TSharedPtr<SEditorSlateWidgetBase> ParentWidget;

	TArray<TSharedPtr<SEditorSlateWidgetBase>> ChildWidgets;

	TMap<FName, TSharedPtr<SEditorSlateWidgetBase>> ChildWidgetMap;

public:
	FName GetWidgetName() const { return WidgetName; }

	int32 GetChildNum() const { return ChildWidgets.Num(); }

	TSharedPtr<SEditorSlateWidgetBase> GetParentWidget() const { return ParentWidget; }

	TArray<TSharedPtr<SEditorSlateWidgetBase>>& GetChildWidgets() { return ChildWidgets; }
};
