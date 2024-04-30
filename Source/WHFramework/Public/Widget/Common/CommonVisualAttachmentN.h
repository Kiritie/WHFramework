// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonVisualAttachment.h"

#include "CommonVisualAttachmentN.generated.h"

/**
 * Adds a widget as a zero-size attachment to another. Think icons to the left of labels, without changing the computed size of the label.
 */
UCLASS(ClassGroup = UI, meta = (Category = "Common UI"))
class WHFRAMEWORK_API UCommonVisualAttachmentN : public UCommonVisualAttachment
{
	GENERATED_BODY()

public:
	UCommonVisualAttachmentN(const FObjectInitializer& ObjectInitializer);

public:
	UFUNCTION(BlueprintCallable, Category="Child Layout")
	void SetContentAnchor(FVector2D InContentAnchor);
};
