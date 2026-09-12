// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonRichTextBlockN.h"

#include "Widget/WidgetModuleStatics.h"

UCommonRichTextBlockN::UCommonRichTextBlockN(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bUseCommonRichStyle = false;
}

void UCommonRichTextBlockN::OnSpawn_Implementation(const FParameter& InParam)
{
	RefreshRichStyle();
}

void UCommonRichTextBlockN::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	SetText(FText::GetEmpty());

	RemoveFromParent();
}

void UCommonRichTextBlockN::SetText(const FText& InText)
{
	Super::SetText(InText);

	RefreshRichStyle();
}

void UCommonRichTextBlockN::RefreshRichStyle()
{
	if(bUseCommonRichStyle)
	{
		SetTextStyleSet(UWidgetModuleStatics::GetCommonRichTextStyle());
		SetDecorators(UWidgetModuleStatics::GetCommonRichTextDecorators());
	}
}
