// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "WHFrameworkSlateTypes.h"
#include "Editor/Base/SEditorWidgetBase.h"
#include "Runtime/Base/SSlateWidgetBase.h"
#include "Input/InputManagerInterface.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"
#include "Parameter/ParameterTypes.h"

class WHFRAMEWORKSLATE_API FSlateWidgetManager : public FManagerBase, public IInputManagerInterface
{
	GENERATED_MANAGER(FSlateWidgetManager)

public:	
	// ParamSets default values for this actor's properties
	FSlateWidgetManager();

	virtual ~FSlateWidgetManager() override;
	
	static FUniqueType Type;

	////////////////////////////////////////////////////
	// SlateWidget
protected:
	TMap<FName, TSharedPtr<SSlateWidgetBase>> AllSlateWidgets;

	TSharedPtr<SSlateWidgetBase> TemporarySlateWidget;

public:
	template<class T>
	bool HasSlateWidget(FName InName = T::WidgetName) const
	{
		return AllSlateWidgets.Contains(InName);
	}

	template<class T>
	TSharedPtr<T> GetSlateWidget(FName InName = T::WidgetName) const
	{
		if(AllSlateWidgets.Contains(InName))
		{
			return StaticCastSharedPtr<T>(AllSlateWidgets[InName]);
		}
		return nullptr;
	}

	template<class T>
	TSharedPtr<T> CreateSlateWidget(UObject* InOwner = nullptr, const TArray<FParameter>* InParams = nullptr)
	{
		return CreateSlateWidget<T>(InOwner, InParams ? *InParams : TArray<FParameter>());
	}

	template<class T>
	TSharedPtr<T> CreateSlateWidget(UObject* InOwner, const TArray<FParameter>& InParams)
	{
		if(TSharedPtr<T> SlateWidget = SNew(T))
		{
			SlateWidget->_WidgetName = T::WidgetName;
			SlateWidget->OnCreate(InOwner, InParams);
			SlateWidget->Init(InOwner, InParams);
			const FName WidgetName = SlateWidget->GetWidgetName();
			if(!AllSlateWidgets.Contains(WidgetName))
			{
				AllSlateWidgets.Add(WidgetName, SlateWidget);
			}
			return StaticCastSharedPtr<T>(SlateWidget);
		}
		return nullptr;
	}

	template<class T>
	bool OpenSlateWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, FName InName = T::WidgetName)
	{
		return OpenSlateWidget<T>(InParams ? *InParams : TArray<FParameter>(), bInstant, InName);
	}
	
	template<class T>
	bool OpenSlateWidget(const TArray<FParameter>& InParams, bool bInstant = false, FName InName = T::WidgetName)
	{
		if(TSharedPtr<T> SlateWidget = HasSlateWidget<T>(InName) ? GetSlateWidget<T>(InName) : CreateSlateWidget<T>(nullptr, InParams))
		{
			if(!SlateWidget->GetParentWidgetN() && SlateWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				if(TemporarySlateWidget)
				{
					TemporarySlateWidget->OnClose(true);
				}
				//SlateWidget->SetLastTemporary(TemporarySlateWidget);
				TemporarySlateWidget = SlateWidget;
			}
			SlateWidget->OnOpen(InParams, bInstant);
			return true;
		}
		return false;
	}

	template<class T>
	bool CloseSlateWidget(bool bInstant = false, FName InName = T::WidgetName)
	{
		if(TSharedPtr<SSlateWidgetBase> SlateWidget = GetSlateWidget<T>(InName))
		{
			if(!SlateWidget->GetParentWidgetN() && SlateWidget->GetWidgetType() == EWidgetType::Temporary)
			{
				TemporarySlateWidget = nullptr;
			}
			SlateWidget->OnClose(bInstant);
			return true;
		}
		return false;
	}
	
	template<class T>
	bool ToggleSlateWidget(bool bInstant = false, FName InName = T::WidgetName)
	{
		if(TSharedPtr<T> SlateWidget = HasSlateWidget<T>(InName) ? GetSlateWidget<T>(InName) : CreateSlateWidget<T>(nullptr))
		{
			SlateWidget->Toggle(bInstant);
			return true;
		}
		return false;
	}

	template<class T>
	bool DestroySlateWidget(FName InName = T::WidgetName)
	{
		if(AllSlateWidgets.Contains(InName))
		{
			if(TSharedPtr<T> SlateWidget = AllSlateWidgets[InName])
			{
				AllSlateWidgets.Remove(InName);
				if(TemporarySlateWidget == SlateWidget)
				{
					TemporarySlateWidget = nullptr;
				}
				SlateWidget->OnDestroy();
				SlateWidget = nullptr;
			}
			return true;
		}
		return false;
	}

	void CloseAllSlateWidget(bool bInstant = false);
	
	void ClearAllSlateWidget();

	////////////////////////////////////////////////////
	// EditorWidget
protected:
	TMap<FName, TSharedPtr<SEditorWidgetBase>> AllEditorWidgets;

public:
	template<class T>
	bool HasEditorWidget(FName InName = T::WidgetName) const
	{
		return AllEditorWidgets.Contains(InName);
	}

	template<class T>
	TSharedPtr<T> GetEditorWidget(FName InName = T::WidgetName) const
	{
		if(AllEditorWidgets.Contains(InName))
		{
			return StaticCastSharedPtr<T>(AllEditorWidgets[InName]);
		}
		return nullptr;
	}

	template<class T>
	TSharedPtr<T> CreateEditorWidget(const TSharedPtr<T>& InWidget, const TSharedPtr<SEditorWidgetBase>& InParent = nullptr, bool bAutoOpen = false)
	{
		const FName WidgetName = T::WidgetName;
		if(!AllEditorWidgets.Contains(WidgetName))
		{
			AllEditorWidgets.Add(WidgetName, InWidget);
			InWidget->_WidgetName = WidgetName;
			if(InParent)
			{
				InParent->AddChild(InWidget);
			}
			InWidget->SetVisibility(EVisibility::Collapsed);
			if(bAutoOpen)
			{
				InWidget->Open();
			}
		}
		return InWidget;
	}
	
	template<class T>
	bool OpenEditorWidget(bool bInstant = false, FName InName = T::WidgetName)
	{
		if(TSharedPtr<T> EditorWidget = GetEditorWidget<T>(InName))
		{
			if(EditorWidget->GetWidgetState() != EEditorWidgetState::Opened)
			{
				EditorWidget->OnOpen(bInstant);
			}
			return true;
		}
		return false;
	}

	template<class T>
	bool CloseEditorWidget(bool bInstant = false, FName InName = T::WidgetName)
	{
		if(TSharedPtr<T> EditorWidget = GetEditorWidget<T>(InName))
		{
			if(EditorWidget->GetWidgetState() != EEditorWidgetState::Closed)
			{
				EditorWidget->OnClose(bInstant);
			}
			return true;
		}
		return false;
	}
	
	template<class T>
	bool ToggleEditorWidget(bool bInstant = false, FName InName = T::WidgetName)
	{
		if(TSharedPtr<T> EditorWidget = GetEditorWidget<T>(InName))
		{
			EditorWidget->Toggle(bInstant);
			return true;
		}
		return false;
	}

	template<class T>
	bool DestroyEditorWidget(FName InName = T::WidgetName)
	{
		if(TSharedPtr<T> EditorWidget = GetEditorWidget<T>(InName))
		{
			AllEditorWidgets.Remove(InName);
			EditorWidget->OnDestroy();
			return true;
		}
		return false;
	}

	void CloseAllEditorWidget(bool bInstant = false);
	
	void ClearAllEditorWidget();

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	virtual int32 GetNativeInputPriority() const override { return 1; }
	
	virtual EInputMode GetNativeInputMode() const override;
};
