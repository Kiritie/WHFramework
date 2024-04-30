// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonVisualAttachmentN.h"

UCommonVisualAttachmentN::UCommonVisualAttachmentN(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UCommonVisualAttachmentN::SetContentAnchor(FVector2D InContentAnchor)
{
	ContentAnchor = InContentAnchor;
}
