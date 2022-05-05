// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SEditorSlateWidgetBase : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditorSlateWidgetBase) {}

	SLATE_END_ARGS()

	SEditorSlateWidgetBase();

	void Construct(const FArguments& InArgs);

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

protected:
	FName WidgetName;

	TArray<TSharedPtr<SEditorSlateWidgetBase>> ChildWidgets;

public:
	virtual FName GetWidgetName() const { return WidgetName; }

	virtual int32 GetChildNum() const { return ChildWidgets.Num(); }

	virtual TArray<TSharedPtr<SEditorSlateWidgetBase>>& GetChildWidgets() { return ChildWidgets; }
};
