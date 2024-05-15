// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonVisualAttachmentN.h"
#include "SVisualAttachmentBox.h"
#include "Components/SizeBoxSlot.h"

UCommonVisualAttachmentN::UCommonVisualAttachmentN(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UCommonVisualAttachmentN::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyAttachmentBox.Reset();
}

TSharedRef<SWidget> UCommonVisualAttachmentN::RebuildWidget()
{
	MyAttachmentBox = SNew(SVisualAttachmentBox);
	MySizeBox = MyAttachmentBox;

	if (GetChildrenCount() > 0)
	{
		Cast<USizeBoxSlot>(GetContentSlot())->BuildSlot(MySizeBox.ToSharedRef());
	}

	return MySizeBox.ToSharedRef();
}

void UCommonVisualAttachmentN::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyAttachmentBox->SetContentAnchor(ContentAnchor);
}

FVector2D UCommonVisualAttachmentN::GetContentAnchor() const
{
	return ContentAnchor;
}

void UCommonVisualAttachmentN::SetContentAnchor(FVector2D InContentAnchor)
{
	ContentAnchor = InContentAnchor;

	if(MyAttachmentBox)
	{
		MyAttachmentBox->SetContentAnchor(ContentAnchor);
	}
}
