// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/Interface.h"
#include "WHFrameworkSlateTypes.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKSLATE_API IEditorWidgetBase : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(IEditorWidgetBase) {}

	SLATE_END_ARGS()

	IEditorWidgetBase();

	void Construct(const FArguments& InArgs);

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnCreate() = 0;

	virtual void OnInitialize() = 0;

	virtual void OnOpen(bool bInstant = false) = 0;

	virtual void OnClose(bool bInstant = false) = 0;

	virtual void OnSave() = 0;
	
	virtual void OnReset() = 0;

	virtual void OnRefresh() = 0;

	virtual void OnTick(float InDeltaTime) = 0;

	virtual void OnDestroy() = 0;
	
public:
	virtual void Open(bool bInstant = false) = 0;

	virtual void Close(bool bInstant = false) = 0;

	virtual void Toggle(bool bInstant = false) = 0;

	virtual void Save() = 0;

	virtual void Reset() = 0;

	virtual void Rebuild() = 0;

	virtual void Refresh() = 0;

	virtual void Destroy() = 0;

protected:
	virtual void FinishOpen(bool bInstant = false) = 0;

	virtual void FinishClose(bool bInstant = false) = 0;

public:
	virtual bool CanSave() = 0;
	
public:
	virtual void AddChild(const TSharedPtr<IEditorWidgetBase>& InChildWidget) = 0;

	virtual void RemoveChild(const TSharedPtr<IEditorWidgetBase>& InChildWidget) = 0;

	virtual void RemoveAllChild() = 0;

public:
	virtual TSharedPtr<SWindow> GetOwnerWindow() = 0;

	virtual TSharedRef<IEditorWidgetBase> TakeWidget() = 0;

	virtual TSharedPtr<IEditorWidgetBase> GetChild(int32 InIndex) const = 0;

	virtual FName GetWidgetName() const = 0;

	virtual EEditorWidgetState GetWidgetState() const = 0;

	virtual int32 GetChildNum() const = 0;

	virtual TSharedPtr<IEditorWidgetBase> GetParentWidgetN() const = 0;
	//
	// template<class T>
	// TSharedPtr<T> GetParentWidgetN() const
	// {
	// 	return StaticCastSharedPtr<T>(GetParentWidgetN());
	// }

	virtual TArray<TSharedPtr<IEditorWidgetBase>>& GetChildWidgets() = 0;
};
