// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonVisualAttachment.h"

#include "CommonVisualAttachmentN.generated.h"

/**
 * Adds a widget as a zero-size attachment to another. Think icons to the left of labels, without changing the computed size of the label.
 */
UCLASS(ClassGroup = UI, meta = (Category = "Common UI"))
class WHFRAMEWORK_API UCommonVisualAttachmentN : public USizeBox
{
	GENERATED_BODY()

public:
	UCommonVisualAttachmentN(const FObjectInitializer& ObjectInitializer);

protected:
	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

public:
	UFUNCTION(BlueprintPure, Category="Child Layout")
	FVector2D GetContentAnchor() const;

	UFUNCTION(BlueprintCallable, Category="Child Layout")
	void SetContentAnchor(FVector2D InContentAnchor);

protected:
	/** */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Child Layout")
	FVector2D ContentAnchor;

private:
	TSharedPtr<class SVisualAttachmentBox> MyAttachmentBox;
};
