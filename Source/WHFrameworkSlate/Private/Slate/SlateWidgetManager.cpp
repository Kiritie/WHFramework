// Fill out your copyright notice in the Description page of Project Settings.

#include "Slate/SlateWidgetManager.h"

#include "Input/InputManager.h"
#include "Main/MainManager.h"

FUniqueType FSlateWidgetManager::Type = FUniqueType();

IMPLEMENTATION_MANAGER(FSlateWidgetManager)

// Sets default values
FSlateWidgetManager::FSlateWidgetManager() : FManagerBase(Type)
{
	AllSlateWidgets = TMap<FName, TSharedPtr<SSlateWidgetBase>>();
	TemporarySlateWidget = nullptr;

	AllEditorWidgets = TMap<FName, TSharedPtr<SEditorWidgetBase>>();
}

FSlateWidgetManager::~FSlateWidgetManager()
{
	
}

void FSlateWidgetManager::OnInitialize()
{
	FManagerBase::OnInitialize();

	FInputManager::Get().AddInputManager(this);
}

void FSlateWidgetManager::OnReset()
{
	FManagerBase::OnReset();

	ClearAllSlateWidget();
}

void FSlateWidgetManager::OnRefresh(float DeltaSeconds)
{
	FManagerBase::OnRefresh(DeltaSeconds);

	for (auto& Iter : AllSlateWidgets)
	{
		TSharedPtr<SSlateWidgetBase> SlateWidget = Iter.Value;
		if (SlateWidget && SlateWidget->GetWidgetState() == EScreenWidgetState::Opened && SlateWidget->GetWidgetRefreshType() == EWidgetRefreshType::Tick)
		{
			SlateWidget->Refresh();
		}
	}
}

void FSlateWidgetManager::OnTermination()
{
	FManagerBase::OnTermination();
}

void FSlateWidgetManager::CloseAllSlateWidget(bool bInstant)
{
	for (auto Iter : AllSlateWidgets)
	{
		if(Iter.Value)
		{
			Iter.Value->SetLastTemporary(nullptr);
			Iter.Value->Close(bInstant);
		}
	}
}

void FSlateWidgetManager::ClearAllSlateWidget(bool bRecovery)
{
	for (auto Iter : AllSlateWidgets)
	{
		if(Iter.Value)
		{
			Iter.Value->OnDestroy(bRecovery);
		}
	}
	AllSlateWidgets.Empty();
}

void FSlateWidgetManager::CloseAllEditorWidget(bool bInstant)
{
	for (auto Iter : AllEditorWidgets)
	{
		if(Iter.Value)
		{
			Iter.Value->Close(bInstant);
		}
	}
}

void FSlateWidgetManager::ClearAllEditorWidget()
{
	for (auto Iter : AllEditorWidgets)
	{
		if(Iter.Value)
		{
			Iter.Value->OnDestroy();
		}
	}
	AllEditorWidgets.Empty();
}

EInputMode FSlateWidgetManager::GetNativeInputMode() const
{
	EInputMode InputMode = EInputMode::None;
	for (const auto& Iter : AllSlateWidgets)
	{
		if (Iter.Value && (Iter.Value->GetWidgetState(true) == EScreenWidgetState::Opening || Iter.Value->GetWidgetState(true) == EScreenWidgetState::Opened) && (int32)Iter.Value->GetWidgetInputMode() > (int32)InputMode)
		{
			InputMode = Iter.Value->GetWidgetInputMode();
		}
	}
	return InputMode;
}
