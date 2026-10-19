#include "Widget/Common/CommonRoundedClip.h"

#include "Components/PanelSlot.h"
#include "Layout/Clipping.h"
#include "Rendering/DrawElements.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SNullWidget.h"

class SCommonRoundedClip : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCommonRoundedClip)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		for (int32 Direction = 0; Direction < 32; ++Direction)
		{
			if (Direction % 8 == 0) continue;
			const double Angle = Direction * UE_DOUBLE_PI / 16.0;
			CornerNormals.Emplace(FMath::Cos(Angle), FMath::Sin(Angle));
		}
		ChildSlot[SNullWidget::NullWidget];
	}

	void SetContent(const TSharedRef<SWidget>& InContent)
	{
		ChildSlot[InContent];
	}

	void SetCornerRadius(float InRadius)
	{
		Radius = InRadius;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		if (Size.X <= 0.0 || Size.Y <= 0.0) return LayerId;
		const double EffectiveRadius = FMath::Min(static_cast<double>(Radius), FMath::Min(Size.X, Size.Y) * 0.5);
		if (EffectiveRadius <= UE_SMALL_NUMBER)
		{
			OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));
		}
		else
		{
			FSlateClippingState ClipState(EClippingFlags::AlwaysClip);
			if (const TOptional<FSlateClippingState> ParentClip = OutDrawElements.GetClippingState(); ParentClip.IsSet())
			{
				if (ParentClip->ScissorRect.IsSet())
				{
					ClipState.StencilQuads.Add(ParentClip->ScissorRect.GetValue());
				}
				else
				{
					ClipState.StencilQuads = ParentClip->StencilQuads;
				}
			}
			ClipState.StencilQuads.Add(FSlateClippingZone(AllottedGeometry));
			const FVector2D HalfSize = Size * 0.5;
			const double Span = Size.X + Size.Y + 1.0;
			for (const FVector2D& Normal : CornerNormals)
			{
				const FVector2D Tangent(-Normal.Y, Normal.X);
				const double Support = (HalfSize.X - EffectiveRadius) * FMath::Abs(Normal.X) +
					(HalfSize.Y - EffectiveRadius) * FMath::Abs(Normal.Y) + EffectiveRadius;
				const FVector2D Boundary = HalfSize + Normal * Support;
				const FVector2D First = Boundary - Tangent * Span;
				const FVector2D Second = Boundary + Tangent * Span;
				const FVector2D Inward = -Normal * (Span * 2.0);
				ClipState.StencilQuads.Emplace(AllottedGeometry.LocalToAbsolute(First), AllottedGeometry.LocalToAbsolute(Second),
					AllottedGeometry.LocalToAbsolute(First + Inward), AllottedGeometry.LocalToAbsolute(Second + Inward));
			}
			OutDrawElements.GetClippingManager().PushClippingState(ClipState);
		}
		const int32 Result = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
		OutDrawElements.PopClip();
		return Result;
	}

private:
	float Radius = 0.f;
	TArray<FVector2D> CornerNormals;
};

UCommonRoundedClip::UCommonRoundedClip(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UCommonRoundedClip::SetCornerRadius(float InRadius)
{
	CornerRadius = FMath::IsFinite(InRadius) ? FMath::Max(InRadius, 0.f) : 0.f;
	if (RoundedClip.IsValid()) RoundedClip->SetCornerRadius(CornerRadius);
}

void UCommonRoundedClip::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	SetCornerRadius(CornerRadius);
}

void UCommonRoundedClip::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	RoundedClip.Reset();
}

TSharedRef<SWidget> UCommonRoundedClip::RebuildWidget()
{
	RoundedClip = SNew(SCommonRoundedClip);
	RoundedClip->SetCornerRadius(CornerRadius);
	if (UWidget* Content = GetContent()) RoundedClip->SetContent(Content->TakeWidget());
	return RoundedClip.ToSharedRef();
}

void UCommonRoundedClip::OnSlotAdded(UPanelSlot* InSlot)
{
	if (RoundedClip.IsValid() && InSlot->Content) RoundedClip->SetContent(InSlot->Content->TakeWidget());
}

void UCommonRoundedClip::OnSlotRemoved(UPanelSlot* InSlot)
{
	if (RoundedClip.IsValid()) RoundedClip->SetContent(SNullWidget::NullWidget);
}
