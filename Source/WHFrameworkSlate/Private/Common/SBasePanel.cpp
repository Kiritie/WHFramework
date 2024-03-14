// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/SBasePanel.h"

#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SBasePanel::SBasePanel()
{
	PanelState = EPanelState::Opened;
}

void SBasePanel::Construct(const FArguments& InArgs)
{
}

void SBasePanel::OnOpen()
{
	PanelState = EPanelState::Opened;
	SetVisibility(EVisibility::SelfHitTestInvisible);
}

void SBasePanel::OnClose()
{
	PanelState = EPanelState::Closed;
	SetVisibility(EVisibility::Collapsed);
}

void SBasePanel::OnRefresh()
{
}

void SBasePanel::Open()
{
	if(PanelState != EPanelState::Opened)
	{
		OnOpen();
	}
}

void SBasePanel::Close()
{
	if(PanelState != EPanelState::Closed)
	{
		OnClose();
	}
}

void SBasePanel::Toggle()
{
	if(PanelState == EPanelState::Opened)
	{
		Close();
	}
	else
	{
		Open();
	}
}

void SBasePanel::Refresh()
{
	OnRefresh();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
