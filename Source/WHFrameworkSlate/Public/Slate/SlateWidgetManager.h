// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "WHFrameworkSlateTypes.h"
#include "Editor/Base/IEditorWidgetBase.h"
#include "Runtime/Base/SSlateWidgetBase.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"
#include "Parameter/ParameterTypes.h"

class WHFRAMEWORKSLATE_API FSlateWidgetManager : public FManagerBase
{
	GENERATED_MANAGER(FSlateWidgetManager)

public:	
	// ParamSets default values for this actor's properties
	FSlateWidgetManager();

	virtual ~FSlateWidgetManager() override;
	
	static FUniqueType Type;

public:
	virtual void OnInitialize() override;

	virtual void OnReset() override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnTermination() override;

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
	TSharedPtr<T> CreateSlateWidget(const FParameter& InParam = FParameter())
	{
		if(TSharedPtr<T> SlateWidget = SNew(T))
		{
			SlateWidget->_WidgetName = T::WidgetName;
			SlateWidget->OnCreate(InParam);
			SlateWidget->Init(InParam);
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
	bool OpenSlateWidget(const FParameter& InParam = FParameter(), bool bInstant = false, FName InName = T::WidgetName)
	{
		if(TSharedPtr<T> SlateWidget = HasSlateWidget<T>(InName) ? GetSlateWidget<T>(InName) : CreateSlateWidget<T>(InParam))
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
			SlateWidget->OnOpen(InParam, bInstant);
			RefreshInputModeRequest();
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
			RefreshInputModeRequest();
			return true;
		}
		return false;
	}
	
	template<class T>
	bool ToggleSlateWidget(bool bInstant = false, FName InName = T::WidgetName)
	{
		if(TSharedPtr<T> SlateWidget = HasSlateWidget<T>(InName) ? GetSlateWidget<T>(InName) : CreateSlateWidget<T>())
		{
			SlateWidget->Toggle(bInstant);
			RefreshInputModeRequest();
			return true;
		}
		return false;
	}

	template<class T>
	bool DestroySlateWidget(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy, FName InName = T::WidgetName)
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
				SlateWidget->OnDestroy(InMode);
				SlateWidget = nullptr;
			}
			RefreshInputModeRequest();
			return true;
		}
		return false;
	}

	void CloseAllSlateWidget(bool bInstant = false);
	
	void ClearAllSlateWidget(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

	////////////////////////////////////////////////////
	// EditorWidget
protected:
	TMap<FName, TSharedPtr<IEditorWidgetBase>> AllEditorWidgets;

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
	TSharedPtr<T> CreateEditorWidget(const TSharedPtr<T>& InWidget, const TSharedPtr<IEditorWidgetBase>& InParent = nullptr, bool bAutoOpen = false)
	{
		const FName WidgetName = T::WidgetName;
		if(AllEditorWidgets.Contains(WidgetName))
		{
			AllEditorWidgets[WidgetName]->Destroy();
		}
		AllEditorWidgets.Emplace(WidgetName, InWidget);
		InWidget->_WidgetName = WidgetName;
		if(InParent)
		{
			InParent->AddChild(InWidget);
		}
		if(bAutoOpen)
		{
			InWidget->Open(FParameter(), true);
		}
		return InWidget;
	}
		
	template<class T>
	bool OpenEditorWidget(const FParameter& InParam = FParameter(), bool bInstant = false, FName InName = T::WidgetName)
	{
		if(TSharedPtr<T> EditorWidget = GetEditorWidget<T>(InName))
		{
			if(EditorWidget->GetWidgetState() != EEditorWidgetState::Opened)
			{
				EditorWidget->OnOpen(InParam, bInstant);
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
			EditorWidget->OnDestroy();
			return true;
		}
		return false;
	}

	template<class T>
	bool RemoveEditorWidget(FName InName = T::WidgetName)
	{
		if(TSharedPtr<T> EditorWidget = GetEditorWidget<T>(InName))
		{
			AllEditorWidgets.Remove(InName);
			return true;
		}
		return false;
	}

	void CloseAllEditorWidget(bool bInstant = false);
	
	void ClearAllEditorWidget();

	//////////////////////////////////////////////////////////////////////////
	// InputMode
protected:
	void RefreshInputModeRequest();

	EInputMode GetDesiredInputMode() const;
};
