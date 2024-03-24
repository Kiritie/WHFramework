// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

enum class EEditorWidgetType : uint8
{
	Main,
	Child
};

enum class EEditorWidgetState : uint8
{
	None,
	Opened,
	Closed
};

/**
 * 
 */
class WHFRAMEWORKSLATE_API SEditorWidgetBase : public SCompoundWidget
{
public:
	SEditorWidgetBase();

	SLATE_BEGIN_ARGS(SEditorWidgetBase) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	virtual void OnCreate();

	virtual void OnInitialize();

	virtual void OnOpen();

	virtual void OnClose();

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
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

public:
	virtual void Open();

	virtual void Close();

	virtual void Toggle();

	virtual void Save();

	virtual void Reset();

	virtual void Rebuild();

	virtual void Refresh();

	virtual void Destroy();

public:
	virtual bool CanSave();

public:
	virtual void AddChild(const TSharedPtr<SEditorWidgetBase>& InChildWidget);

	virtual void RemoveChild(const TSharedPtr<SEditorWidgetBase>& InChildWidget);

	virtual void RemoveAllChild();

protected:
	FName WidgetName;

	EEditorWidgetType WidgetType;

	EEditorWidgetState WidgetState;

	TSharedPtr<SEditorWidgetBase> ParentWidget;

	TArray<TSharedPtr<SEditorWidgetBase>> ChildWidgets;

	TMap<FName, TSharedPtr<SEditorWidgetBase>> ChildWidgetMap;
	
	TSharedRef<FUICommandList> WidgetCommands;

private:
	bool bInitialized;
	
	FDelegateHandle OnWindowActivatedHandle;
	FDelegateHandle OnWindowDeactivatedHandle;
	FDelegateHandle OnWindowClosedHandle;

public:
	TSharedPtr<SWindow> GetOwnerWindow();

	TSharedRef<SEditorWidgetBase> TakeWidget();

	TSharedPtr<SEditorWidgetBase> GetChild(int32 InIndex) const;

	FName GetWidgetName() const { return WidgetName; }

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
