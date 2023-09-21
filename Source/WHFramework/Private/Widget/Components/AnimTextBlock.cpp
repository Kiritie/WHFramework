// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Components/AnimTextBlock.h"

#include "TimerManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Font.h"
#include "Kismet/KismetStringLibrary.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"

/////////////////////////////////////////////////////
// UAnimTextBlock

UAnimTextBlock::UAnimTextBlock(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Text = FText::GetEmpty();

	AnimType = ETextAnimType::None;
	
	AnimTime = 1.f;

	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(*UWidget::GetDefaultFontName());
		Font = FSlateFontInfo(RobotoFontObj.Object, 24, FName("Bold"));
	}
	
	StrikeBrush = FSlateBrush();
	ShadowOffset = FVector2D(1.0f, 1.0f);
	ShadowColor = FLinearColor::Transparent;
	TextTransformPolicy = ETextTransformPolicy::None;
	TextHorizontalJustify = ETextHorizontalJustify::Left;
	TextVerticalJustify = ETextVerticalJustify::Center;
	
	TextOverlay = nullptr;
	TextBox = nullptr;
	AnimTextInfo.TextItems = TArray<FAnimTextItem>();
}

void UAnimTextBlock::NativePreConstruct()
{
	Super::NativePreConstruct();

	UpdatePreviewText();
}

void UAnimTextBlock::NativeConstruct()
{
	Super::NativeConstruct();

	ClearPreviewText();
	SetText(Text);
}

bool UAnimTextBlock::IsTickAble_Implementation() const
{
	return true;
}

void UAnimTextBlock::OnTick_Implementation(float DeltaSeconds)
{
	switch(AnimType)
	{
		case ETextAnimType::LerpOnly:
		{
			if(AnimTextInfo.CurrentNum != AnimTextInfo.TargetNum)
			{
				AnimTextInfo.CurrentNum = FMath::FInterpConstantTo(AnimTextInfo.CurrentNum, AnimTextInfo.TargetNum, DeltaSeconds, AnimTextInfo.AnimSpeed / AnimTime);

				TArray<FString> TextArr = UKismetStringLibrary::GetCharacterArrayFromString(AnimTextInfo.GetCurrentText().ToString());
				for(int32 i = 0; i < AnimTextInfo.TextItems.Num(); i++)
				{
					if(AnimTextInfo.TextItems[i].TextBlock)
					{
						if(i < TextArr.Num())
						{
							AnimTextInfo.TextItems[i].TextBlock->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
							AnimTextInfo.TextItems[i].TextBlock->SetText(FText::FromString(TextArr[i]));
						}
						else
						{
							AnimTextInfo.TextItems[i].TextBlock->SetVisibility(ESlateVisibility::Collapsed);
						}
					}
				}
			}
			break;
		}
		case ETextAnimType::LerpAndMove:
		{
			for(const auto& Iter : AnimTextInfo.TextItems)
			{
				if(Iter.TextBlock && Iter.TextBlock->GetVisibility() == ESlateVisibility::SelfHitTestInvisible && TextBox->GetDesiredSize().Y > 0.f)
				{
					switch(Iter.AnimDirection)
					{
						case ETextAnimDirection::Up:
						{
							Iter.TextBlock->SetRenderTranslation(FVector2D(0.f, Iter.TextBlock->GetRenderTransform().Translation.Y - TextBox->GetDesiredSize().Y / Iter.AnimTime * DeltaSeconds * 2));
							if(Iter.TextBlock->GetRenderTransform().Translation.Y <= -TextBox->GetDesiredSize().Y)
							{
								Iter.TextBlock->SetText(Iter.CurrentText);
								Iter.TextBlock->SetRenderTranslation(FVector2D(0.f, TextBox->GetDesiredSize().Y));
							}
							break;
						}
						case ETextAnimDirection::Down:
						{
							Iter.TextBlock->SetRenderTranslation(FVector2D(0.f, Iter.TextBlock->GetRenderTransform().Translation.Y + TextBox->GetDesiredSize().Y / Iter.AnimTime * DeltaSeconds * 2));
							if(Iter.TextBlock->GetRenderTransform().Translation.Y >= TextBox->GetDesiredSize().Y)
							{
								Iter.TextBlock->SetText(Iter.CurrentText);
								Iter.TextBlock->SetRenderTranslation(FVector2D(0.f, -TextBox->GetDesiredSize().Y));
							}
							break;
						}
						default: break;
					}
				}
			}
			break;
		}
		default: break;
	}
}

void UAnimTextBlock::PlayTextAnim(int32 TextIndex)
{
	if(AnimTextInfo.TextItems.IsValidIndex(TextIndex) && AnimTextInfo.TextItems[TextIndex].TextBlock)
	{
		if(AnimType == ETextAnimType::LerpAndMove)
		{
			const int32 Current = FCString::Atoi(*AnimTextInfo.TextItems[TextIndex].CurrentText.ToString());
			const int32 Target = FCString::Atoi(*AnimTextInfo.TextItems[TextIndex].TargetText.ToString());
			if(Target - Current > 0)
			{
				AnimTextInfo.TextItems[TextIndex].CurrentText = FText::FromString(FString::FromInt(Current + 1));
				AnimTextInfo.TextItems[TextIndex].AnimDirection = ETextAnimDirection::Up;
			}
			else if(Target - Current < 0)
			{
				AnimTextInfo.TextItems[TextIndex].CurrentText = FText::FromString(FString::FromInt(Current - 1));
				AnimTextInfo.TextItems[TextIndex].AnimDirection = ETextAnimDirection::Down;
			}
			else
			{
				AnimTextInfo.TextItems[TextIndex].AnimDirection = ETextAnimDirection::None;
				AnimTextInfo.TextItems[TextIndex].TextBlock->SetRenderTranslation(FVector2D(0.f, 0.f));
				GetWorld()->GetTimerManager().ClearTimer(AnimTextInfo.TextItems[TextIndex].AnimTimerHandle);
			}
		}
	}
}

void UAnimTextBlock::UpdatePreviewText()
{
	ClearPreviewText();
	
	TArray<FString> TextArr = UKismetStringLibrary::GetCharacterArrayFromString(Text.ToString());
	for(int32 i = 0; i < TextArr.Num(); i++)
	{
		if(TextBox->GetChildrenCount() <= i)
		{
			UTextBlock* TextBlock = NewObject<UTextBlock>(TextBox);
			if(UHorizontalBoxSlot* HorizontalBoxSlot = TextBox->AddChildToHorizontalBox(TextBlock))
			{
				HorizontalBoxSlot->SetPadding(GetPadding());
			}
		}
		if(TextBox->GetChildrenCount() > i)
		{
			if(UTextBlock* TextBlock = Cast<UTextBlock>(TextBox->GetChildAt(i)))
			{
				TextBlock->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				TextBlock->SetText(FText::FromString(TextArr[i]));
				TextBlock->SetColorAndOpacity(GetColorAndOpacity());
				TextBlock->SetFont(Font);
				TextBlock->SetStrikeBrush(StrikeBrush);
				TextBlock->SetShadowOffset(ShadowOffset);
				TextBlock->SetShadowColorAndOpacity(ShadowColor);
				TextBlock->SetTextTransformPolicy(TextTransformPolicy);
				TextBlock->SetJustification(ETextJustify::Center);
			}
		}
	}
	for(int32 i = 0; i < TextBox->GetChildrenCount(); i++)
	{
		if(i >= TextArr.Num())
		{
			if(UTextBlock* TextBlock = Cast<UTextBlock>(TextBox->GetChildAt(i)))
			{
				TextBlock->SetText(FText::GetEmpty());
				TextBlock->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	SetTextHorizontalJustify(TextHorizontalJustify);
	SetTextVerticalJustify(TextVerticalJustify);
}

void UAnimTextBlock::ClearPreviewText()
{
	TextBox->ClearChildren();
}

FText UAnimTextBlock::GetText() const
{
	return Text;
}

void UAnimTextBlock::SetText(const FText& InText)
{
	if(!InText.IsEmpty())
	{
		if(Text.ToString().Len() != InText.ToString().Len())
		{
			AnimTextInfo.CurrentNum = 0.f;
		}
		AnimTextInfo.SetTargetText(InText);
		TArray<FString> TextArr = UKismetStringLibrary::GetCharacterArrayFromString(InText.ToString());
		for(int32 i = 0; i < TextArr.Num(); i++)
		{
			if(AnimTextInfo.TextItems.Num() <= i)
			{
				FAnimTextItem TextInfo;
				TextInfo.TextBlock = NewObject<UTextBlock>(TextBox);
				AnimTextInfo.TextItems.Add(TextInfo);
			}
			if(AnimTextInfo.TextItems.IsValidIndex(i))
			{
				if(AnimTextInfo.TextItems[i].TextBlock)
				{
					AnimTextInfo.TextItems[i].TextBlock->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					switch(AnimType)
					{
						case ETextAnimType::None:
						{
							AnimTextInfo.TextItems[i].TextBlock->SetText(FText::FromString(TextArr[i]));
							break;
						}
						case ETextAnimType::LerpAndMove:
						{
							AnimTextInfo.TextItems[i].TargetText = FText::FromString(TextArr[i]);
							if(AnimTextInfo.TextItems[i].TextBlock->GetText().IsEmpty() || Text.ToString().Len() != InText.ToString().Len())
							{
								AnimTextInfo.TextItems[i].TextBlock->SetText(AnimTextInfo.TextItems[i].IsNumber() ? FText::FromString(TEXT("0")) : AnimTextInfo.TextItems[i].TargetText);
								AnimTextInfo.TextItems[i].CurrentText = AnimTextInfo.TextItems[i].TextBlock->GetText();
							}
							break;
						}
						default: break;
					}
					AnimTextInfo.TextItems[i].TextBlock->SetColorAndOpacity(GetColorAndOpacity());
					AnimTextInfo.TextItems[i].TextBlock->SetFont(Font);
					AnimTextInfo.TextItems[i].TextBlock->SetStrikeBrush(StrikeBrush);
					AnimTextInfo.TextItems[i].TextBlock->SetShadowOffset(ShadowOffset);
					AnimTextInfo.TextItems[i].TextBlock->SetShadowColorAndOpacity(ShadowColor);
					AnimTextInfo.TextItems[i].TextBlock->SetTextTransformPolicy(TextTransformPolicy);
					AnimTextInfo.TextItems[i].TextBlock->SetJustification(ETextJustify::Center);
					if(UHorizontalBoxSlot* HorizontalBoxSlot = TextBox->AddChildToHorizontalBox(AnimTextInfo.TextItems[i].TextBlock))
					{
						HorizontalBoxSlot->SetPadding(GetPadding());
					}
				}
			}
		}
		for(int32 i = 0; i < AnimTextInfo.TextItems.Num(); i++)
		{
			if(i < TextArr.Num())
			{
				if(AnimTextInfo.TextItems[i].TextBlock && AnimTextInfo.TextItems[i].IsNumber() && AnimType == ETextAnimType::LerpAndMove)
				{
					int32 Current = FCString::Atoi(*AnimTextInfo.TextItems[i].CurrentText.ToString());
					int32 Target = FCString::Atoi(*AnimTextInfo.TextItems[i].TargetText.ToString());
					AnimTextInfo.TextItems[i].AnimTime = AnimTime / FMath::Abs(Target - Current);
					GetWorld()->GetTimerManager().SetTimer(AnimTextInfo.TextItems[i].AnimTimerHandle, FTimerDelegate::CreateUObject(this, &UAnimTextBlock::PlayTextAnim, i), AnimTextInfo.TextItems[i].AnimTime, true);
					PlayTextAnim(i);
				}
			}
			else
			{
				if(AnimType == ETextAnimType::LerpAndMove)
				{
					if(AnimTextInfo.TextItems[i].TextBlock)
					{
						AnimTextInfo.TextItems[i].TextBlock->SetText(FText::GetEmpty());
						AnimTextInfo.TextItems[i].TextBlock->SetVisibility(ESlateVisibility::Collapsed);
					}
					AnimTextInfo.TextItems[i].TargetText = FText::GetEmpty();
					AnimTextInfo.TextItems[i].CurrentText = FText::GetEmpty();
					AnimTextInfo.TextItems[i].AnimDirection = ETextAnimDirection::None;
					AnimTextInfo.TextItems[i].TextBlock->SetRenderTranslation(FVector2D(0.f, 0.f));
					GetWorld()->GetTimerManager().ClearTimer(AnimTextInfo.TextItems[i].AnimTimerHandle);
				}
			}
		}
		SetTextHorizontalJustify(TextHorizontalJustify);
		SetTextVerticalJustify(TextVerticalJustify);
	}
	else
	{
		for(auto Iter : AnimTextInfo.TextItems)
		{
			GetWorld()->GetTimerManager().ClearTimer(Iter.AnimTimerHandle);
		}
		AnimTextInfo.TextItems.Empty();
		TextBox->ClearChildren();
	}

	Text = InText;
}

void UAnimTextBlock::SetAnimType(ETextAnimType InAnimType)
{
	AnimType = InAnimType;
}

void UAnimTextBlock::SetAnimTime(float InAnimTime)
{
	AnimTime = InAnimTime;
}

void UAnimTextBlock::SetColor(FLinearColor InColor)
{
	SetColorAndOpacity(InColor);
	for(auto& Iter : AnimTextInfo.TextItems)
	{
		if(Iter.TextBlock)
		{
			Iter.TextBlock->SetColorAndOpacity(InColor);
		}
	}
}

void UAnimTextBlock::SetFont(FSlateFontInfo InFontInfo)
{
	Font = InFontInfo;
	for(auto& Iter : AnimTextInfo.TextItems)
	{
		if(Iter.TextBlock)
		{
			Iter.TextBlock->SetFont(Font);
		}
	}
}

void UAnimTextBlock::SetOpacity(float InOpacity)
{
	FLinearColor CurrentColor = GetColorAndOpacity();
	CurrentColor.A = InOpacity;
	
	SetColor(CurrentColor);
}

void UAnimTextBlock::SetShadowColor(FLinearColor InShadowColor)
{
	ShadowColor = InShadowColor;
	for(auto& Iter : AnimTextInfo.TextItems)
	{
		if(Iter.TextBlock)
		{
			Iter.TextBlock->SetShadowColorAndOpacity(ShadowColor);
		}
	}
}

void UAnimTextBlock::SetShadowOffset(FVector2D InShadowOffset)
{
	ShadowOffset = InShadowOffset;
	for(auto& Iter : AnimTextInfo.TextItems)
	{
		if(Iter.TextBlock)
		{
			Iter.TextBlock->SetShadowOffset(ShadowOffset);
		}
	}
}

void UAnimTextBlock::SetStrikeBrush(FSlateBrush InStrikeBrush)
{
	StrikeBrush = InStrikeBrush;
	for(auto& Iter : AnimTextInfo.TextItems)
	{
		if(Iter.TextBlock)
		{
			Iter.TextBlock->SetStrikeBrush(StrikeBrush);
		}
	}
}

void UAnimTextBlock::SetTextTransformPolicy(ETextTransformPolicy InTransformPolicy)
{
	TextTransformPolicy = InTransformPolicy;
	for(auto& Iter : AnimTextInfo.TextItems)
	{
		if(Iter.TextBlock)
		{
			Iter.TextBlock->SetTextTransformPolicy(TextTransformPolicy);
		}
	}
}

void UAnimTextBlock::SetTextHorizontalJustify(ETextHorizontalJustify InTextHorizontalJustify)
{
	TextHorizontalJustify = InTextHorizontalJustify;
	if(UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(TextBox->Slot))
	{
		switch(TextHorizontalJustify)
		{
			case ETextHorizontalJustify::Left:
			{
				OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Left);
				break;
			}
			case ETextHorizontalJustify::Center:
			{
				OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
				break;
			}
			case ETextHorizontalJustify::Right:
			{
				OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Right);
				break;
			}
		}
	}
}

void UAnimTextBlock::SetTextVerticalJustify(ETextVerticalJustify InTextVerticalJustify)
{
	TextVerticalJustify = InTextVerticalJustify;
	if(UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(TextBox->Slot))
	{
		switch(TextVerticalJustify)
		{
			case ETextVerticalJustify::Top:
			{
				OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Top);
				break;
			}
			case ETextVerticalJustify::Center:
			{
				OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
				break;
			}
			case ETextVerticalJustify::Bottom:
			{
				OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Bottom);
				break;
			}
		}
	}
}
