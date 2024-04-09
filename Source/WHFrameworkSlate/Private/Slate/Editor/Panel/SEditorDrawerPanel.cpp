// Fill out your copyright notice in the Description page of Project Settings.

#include "Slate/Editor/Panel/SEditorDrawerPanel.h"

#include "WHFrameworkSlateStyle.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SEditorDrawerPanel::SEditorDrawerPanel()
{
	Orientation = DrawerOrient_TopToBottom;
}

void SEditorDrawerPanel::Construct(const FArguments& InArgs)
{
	OpenSequence = FCurveSequence(0.f, InArgs._Duration, InArgs._Ease);

	Orientation = InArgs._Orientation;
	ContentWidth = InArgs._ContentWidth;
	ContentHeight = InArgs._ContentHeight;
	ContentPadding = InArgs._ContentPadding;

	SetVisibility(EVisibility::SelfHitTestInvisible);

	ChildSlot
	[
		SNew(SOverlay)

		+SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(InArgs._ContentPadding)
		[
			SNew(SBox)
			.WidthOverride(InArgs._ContentWidth)
			.HeightOverride(InArgs._ContentHeight)
			.Visibility_Lambda([this](){ return !IsClosed() ? EVisibility::SelfHitTestInvisible : EVisibility::Hidden; })
			.RenderTransform(this, &SEditorDrawerPanel::GetDrawerRenderTransform)
			[
				InArgs._Content.Widget
			]
		]

		+SOverlay::Slot()
		.HAlign(Orientation == DrawerOrient_LeftToRight ? HAlign_Left : (Orientation == DrawerOrient_RightToLeft ? HAlign_Right : HAlign_Center))
		.VAlign(Orientation == DrawerOrient_TopToBottom ? VAlign_Top : VAlign_Bottom)
		.Padding(InArgs._EdgePadding)
		[
			SNew(SBox)
			.Visibility_Lambda([this](){ return IsClosed() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			[
				InArgs._EdgeContent.Widget
			]
		]
	];

	if(InArgs._AutoOpen)
	{
		Open();
	}
}

void SEditorDrawerPanel::Open()
{
	if(IsOpened()) return;
	
	OpenSequence.Play(SharedThis(this));
}

void SEditorDrawerPanel::Close()
{
	if(IsClosed()) return;
	
	OpenSequence.Reverse();
}

void SEditorDrawerPanel::Toggle()
{
	if(!OpenSequence.IsPlaying() || !OpenSequence.IsForward())
	{
		Open();
	}
	else
	{
		Close();
	}
}

bool SEditorDrawerPanel::IsOpened() const
{
	return OpenSequence.IsAtEnd();
}

bool SEditorDrawerPanel::IsClosed() const
{
	return OpenSequence.IsAtStart();
}

FReply SEditorDrawerPanel::OnOpenButtonClicked()
{
	Open();
	return FReply::Handled();
}

TOptional<FSlateRenderTransform> SEditorDrawerPanel::GetDrawerRenderTransform() const
{
	FVector2D Location = FVector2D();
	switch(Orientation)
	{
		case DrawerOrient_LeftToRight:
		{
			Location = FVector2D(FMath::Lerp(-(ContentWidth.Get().Get() + ContentPadding.Get().Left), 0.f, OpenSequence.GetLerp()), 0.f);
			break;
		}
		case DrawerOrient_RightToLeft:
		{
			Location = FVector2D(FMath::Lerp((ContentWidth.Get().Get() + ContentPadding.Get().Right), 0.f, OpenSequence.GetLerp()), 0.f);
			break;
		}
		case DrawerOrient_BottomToTop:
		{
			Location = FVector2D(0.f, FMath::Lerp((ContentHeight.Get().Get() + ContentPadding.Get().Bottom), 0.f, OpenSequence.GetLerp()));
			break;
		}
		case DrawerOrient_TopToBottom:
		{
			Location = FVector2D(0.f, FMath::Lerp(-(ContentHeight.Get().Get() + ContentPadding.Get().Top), 0.f, OpenSequence.GetLerp()));
			break;
		}
		default: break;
	}
	return FSlateRenderTransform(Location);
}


END_SLATE_FUNCTION_BUILD_OPTIMIZATION
