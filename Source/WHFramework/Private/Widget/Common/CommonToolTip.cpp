// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonToolTip.h"

#include "Components/Image.h"
#include "Widget/Common/CommonTextBlockN.h"

UCommonToolTip::UCommonToolTip(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Txt_Content = nullptr;
	OwnerWidget = nullptr;
}

void UCommonToolTip::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
	
	if(OwnerWidget)
	{
		OwnerWidget->SetToolTip(this);
		SetContent(OwnerWidget->GetToolTipText());
	}
}

void UCommonToolTip::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	SetContent(FText::GetEmpty());

	if(OwnerWidget)
	{
		OwnerWidget->SetToolTip(nullptr);
	}
	
	Super::OnDespawn_Implementation(InMode);
}

FText UCommonToolTip::GetContent() const
{
	if(Txt_Content)
	{
		return Txt_Content->GetText();
	}
	return FText::GetEmpty();
}

void UCommonToolTip::SetContent(const FText InContent)
{
	if(Txt_Content)
	{
		Txt_Content->SetText(InContent);
	}
}
