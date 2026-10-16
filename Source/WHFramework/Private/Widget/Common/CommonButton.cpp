// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonButton.h"

#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "Components/StackBox.h"
#include "Components/StackBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Input/Base/InputActionBase.h"
#include "Input/InputModule.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Widget/Style/SubWidgetStyle.h"
#include "Widget/Style/WidgetBrushStyle.h"

UCommonButton::UCommonButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Img_Icon = nullptr;
	Txt_Title = nullptr;

	bStandalone = false;
	WidgetParams = FParameter();
}

void UCommonButton::OnSpawn_Implementation(const FParameter& InParam)
{
	EnsureThemeSubscription();
	ApplyStyleTag();
	WidgetParams = InParam;
}

void UCommonButton::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	InheritedStyleTag = FGameplayTag();
	const UCommonButton* Defaults = CastChecked<UCommonButton>(GetArchetype());
	StyleTag = Defaults->StyleTag;
	bOverrideStyleTag = Defaults->bOverrideStyleTag;
	IconStyleTag = Defaults->IconStyleTag;
	ContentOrientation = Defaults->ContentOrientation;
	ContentHorizontalAlignment = Defaults->ContentHorizontalAlignment;
	ContentSpacing = Defaults->ContentSpacing;
	ApplyContentLayout();
	IconOverride = FSlateBrush();
	bHasIconOverride = false;
	if (Img_Icon)
	{
		Img_Icon->SetBrush(FSlateBrush());
		Img_Icon->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetTitle(Defaults->Title);
	SetIsEnabled(Defaults->GetIsEnabled());
	SetVisibility(Defaults->GetVisibility());

	OnSelectedChangedBase.Clear();
	OnButtonBaseClicked.Clear();
	OnButtonBaseDoubleClicked.Clear();
	OnButtonBaseHovered.Clear();
	OnButtonBaseUnhovered.Clear();

	if (GetSelected())
	{
		SetSelectedInternal(false, false, false);
	}

	HoldReset();
	WidgetParams.Reset();
}

void UCommonButton::SynchronizeProperties()
{
	EnsureThemeSubscription();
	ApplyStyleTag();
	Super::SynchronizeProperties();
	ApplyContentLayout();
}

void UCommonButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyStyleTag();
	ApplyTriggeringActionTag();
	SetTitle(Title);
}

void UCommonButton::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyStyleTag();
}

UCommonButtonInternalBase* UCommonButton::ConstructInternalButton()
{
	if (!WidgetTree->RootWidget)
	{
		BuildNativeLayout();
	}
	return Super::ConstructInternalButton();
}

void UCommonButton::BuildNativeLayout()
{
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ButtonLayers"));
	WidgetTree->RootWidget = Layers;
	NativeContent = WidgetTree->ConstructWidget<UStackBox>(UStackBox::StaticClass(), TEXT("ButtonLayout"));
	UOverlaySlot* ContentSlot = Layers->AddChildToOverlay(NativeContent);
	ContentSlot->SetHorizontalAlignment(HAlign_Center);
	ContentSlot->SetVerticalAlignment(VAlign_Center);
	Img_Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_Icon"));
	UStackBoxSlot* IconSlot = NativeContent->AddChildToStackBox(Img_Icon);
	IconSlot->SetHorizontalAlignment(HAlign_Center);
	IconSlot->SetVerticalAlignment(VAlign_Center);
	Txt_Title = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass(), TEXT("Txt_Title"));
	Txt_Title->SetJustification(ETextJustify::Center);
	UStackBoxSlot* TitleSlot = NativeContent->AddChildToStackBox(Txt_Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	ApplyContentLayout();
	Img_Icon->SetVisibility(ESlateVisibility::Collapsed);
	Img_SelectionMarker = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_SelectionMarker"));
	UOverlaySlot* MarkerSlot = Layers->AddChildToOverlay(Img_SelectionMarker);
	MarkerSlot->SetHorizontalAlignment(HAlign_Center);
	MarkerSlot->SetVerticalAlignment(VAlign_Bottom);
	Img_SelectionMarker->SetVisibility(ESlateVisibility::Collapsed);
}

TSharedRef<SWidget> UCommonButton::RebuildWidget()
{
	return Super::RebuildWidget();
}

void UCommonButton::OnResolvedStyleChanged(const USubWidgetStyle* InStyle)
{
}

void UCommonButton::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	if (!InDragDropEvent.IsTouchEvent())
	{
		if (GetIsEnabled() && IsInteractionEnabled())
		{
			NativeOnHovered();
		}
	}
}

void UCommonButton::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	if (!InDragDropEvent.IsTouchEvent())
	{
		if (GetIsEnabled() && IsInteractionEnabled())
		{
			NativeOnUnhovered();
		}
	}
}

void UCommonButton::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	ApplyTitleStyle();
}

void UCommonButton::NativeOnClicked()
{
	Super::NativeOnClicked();
}

void UCommonButton::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	ApplySelectionMarker();
}

void UCommonButton::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	ApplySelectionMarker();
}

void UCommonButton::SetTitle(const FText InTitle)
{
	Title = InTitle;
	if (Txt_Title)
	{
		Txt_Title->SetText(Title);
		const USubWidgetStyle* ButtonStyle = Cast<USubWidgetStyle>(GetStyle());
		const bool bIconOnly = ButtonStyle && ButtonStyle->bIconOnly && Img_Icon && Img_Icon->GetBrush().HasUObject();
		Txt_Title->SetVisibility(Title.IsEmpty() || bIconOnly ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UCommonButton::SetIconBrush(const FSlateBrush& InBrush)
{
	IconOverride = InBrush;
	bHasIconOverride = true;
	if (Img_Icon)
	{
		Img_Icon->SetBrush(InBrush);
		Img_Icon->SetVisibility(!InBrush.HasUObject() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	SetTitle(Title);
}

void UCommonButton::SetIconStyleTag(FGameplayTag InStyleTag)
{
	IconStyleTag = InStyleTag;
	ApplyIconStyle(Cast<USubWidgetStyle>(GetStyle()));
}

void UCommonButton::SetContentOrientation(EOrientation InOrientation)
{
	ContentOrientation = InOrientation;
	ApplyContentLayout();
}

void UCommonButton::ApplyContentLayout()
{
	if (NativeContent)
	{
		NativeContent->SetOrientation(ContentOrientation);
		if (UOverlaySlot* ContentSlot = Cast<UOverlaySlot>(NativeContent->Slot))
		{
			ContentSlot->SetHorizontalAlignment(ContentHorizontalAlignment);
		}
		if (Img_Icon)
		{
			if (UStackBoxSlot* IconSlot = Cast<UStackBoxSlot>(Img_Icon->Slot))
			{
				IconSlot->SetPadding(ContentOrientation == Orient_Vertical ? FMargin(0.f, 0.f, 0.f, ContentSpacing) : FMargin(0.f, 0.f, ContentSpacing, 0.f));
			}
		}
	}
}

void UCommonButton::SetMinWidth(int32 InValue)
{
	MinWidth = InValue;
}

void UCommonButton::SetMinHeight(int32 InValue)
{
	MinHeight = InValue;
}

void UCommonButton::SetIsEnabledN(bool bEnable)
{
	SetRenderOpacity(bEnable ? 1.f : 0.5f);
	SetVisibility(bEnable ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::HitTestInvisible);
}

void UCommonButton::SetStyleTag(FGameplayTag InStyleTag)
{
	StyleTag = InStyleTag;
	bOverrideStyleTag = InStyleTag.IsValid();
	ApplyStyleTag();
}

void UCommonButton::SetInheritedStyleTag(FGameplayTag InStyleTag)
{
	InheritedStyleTag = InStyleTag;
	ApplyStyleTag();
}

void UCommonButton::ClearStyleOverride()
{
	bOverrideStyleTag = false;
	StyleTag = FGameplayTag();
	ApplyStyleTag();
}

void UCommonButton::SetTriggeringActionTag(FGameplayTag InActionTag)
{
	TriggeringActionTag = InActionTag;
	ApplyTriggeringActionTag();
}

FGameplayTag UCommonButton::GetStyleTag() const
{
	if (bOverrideStyleTag && StyleTag.IsValid())
	{
		return StyleTag;
	}
	if (InheritedStyleTag.IsValid())
	{
		return InheritedStyleTag;
	}
	return StyleTag.IsValid() ? StyleTag : WidgetStyleTags::CommonDefault(EWidgetStyleKind::Button);
}

void UCommonButton::ApplyStyleTag()
{
	const FGameplayTag Tag = GetStyleTag();
	if (!Tag.IsValid())
	{
		ApplyTitleStyle();
		ApplyIconStyle(Cast<USubWidgetStyle>(GetStyle()));
		ApplySelectionMarker();
		return;
	}
	EnsureThemeSubscription();
	SetStyle(FWidgetThemeResolver::ResolveClass(this, Tag, EWidgetStyleKind::Button));
	ApplyTitleStyle();
	ApplyIconStyle(Cast<USubWidgetStyle>(GetStyle()));
	ApplySelectionMarker();
	OnResolvedStyleChanged(Cast<USubWidgetStyle>(GetStyle()));
}

void UCommonButton::ApplyIconStyle(const USubWidgetStyle* InStyle)
{
	if (Img_Icon)
	{
		const FSlateBrush* Brush = bHasIconOverride ? &IconOverride : nullptr;
		const FGameplayTag GlyphTag = IconStyleTag.IsValid() ? IconStyleTag : (InStyle ? InStyle->IconStyleTag : FGameplayTag());
		if (!Brush && GlyphTag.IsValid())
		{
			UClass* Class = FWidgetThemeResolver::ResolveClass(this, GlyphTag, EWidgetStyleKind::Brush);
			const UWidgetBrushStyle* IconStyle = Class ? Class->GetDefaultObject<UWidgetBrushStyle>() : nullptr;
			Brush = IconStyle ? &IconStyle->Brush : nullptr;
		}
		if (Brush)
		{
			Img_Icon->SetBrush(*Brush);
			Img_Icon->SetVisibility(Brush->HasUObject() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		else
		{
			Img_Icon->SetBrush(FSlateBrush());
			Img_Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	SetTitle(Title);
}

void UCommonButton::ApplySelectionMarker()
{
	if (!Img_SelectionMarker)
	{
		return;
	}
	const USubWidgetStyle* ButtonStyle = Cast<USubWidgetStyle>(GetStyle());
	if (!ButtonStyle || !ButtonStyle->SelectionMarkerStyleTag.IsValid())
	{
		Img_SelectionMarker->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	UClass* Class = FWidgetThemeResolver::ResolveClass(this, ButtonStyle->SelectionMarkerStyleTag, EWidgetStyleKind::Brush);
	if (const UWidgetBrushStyle* Marker = Class ? Class->GetDefaultObject<UWidgetBrushStyle>() : nullptr)
	{
		FSlateBrush Brush = Marker->Brush;
		Brush.ImageSize = ButtonStyle->SelectionMarkerSize;
		Img_SelectionMarker->SetBrush(Brush);
		Img_SelectionMarker->SetVisibility(GetSelected() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UCommonButton::ApplyTitleStyle()
{
	if (Txt_Title)
	{
		Txt_Title->SetStyleTag(FGameplayTag());
		FGameplayTag TitleTag;
		if (const USubWidgetStyle* ButtonStyle = Cast<USubWidgetStyle>(GetStyle()))
		{
			TitleTag = !GetIsEnabled() ? ButtonStyle->DisabledTextStyleTag
				: GetSelected() ? (IsHovered() ? ButtonStyle->SelectedHoveredTextStyleTag : ButtonStyle->SelectedTextStyleTag)
				: (IsHovered() ? ButtonStyle->NormalHoveredTextStyleTag : ButtonStyle->NormalTextStyleTag);
			if (!TitleTag.IsValid())
			{
				TitleTag = ButtonStyle->NormalTextStyleTag;
			}
		}
		Txt_Title->SetStyle(TitleTag.IsValid()
			? FWidgetThemeResolver::ResolveClass(this, TitleTag, EWidgetStyleKind::Text)
			: GetCurrentTextStyleClass().Get());
	}
}

void UCommonButton::ApplyTriggeringActionTag()
{
	const UInputActionBase* InputAction = TriggeringActionTag.IsValid() ? UInputModule::Get().GetInputActionByTag(TriggeringActionTag, false) : nullptr;
	SetTriggeringEnhancedInputAction(const_cast<UInputActionBase*>(InputAction));
}

void UCommonButton::EnsureThemeSubscription()
{
	if (!ThemeChangedHandle.IsValid() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		ThemeChangedHandle = FWidgetThemeResolver::OnThemeChanged().AddUObject(this, &ThisClass::ApplyStyleTag);
	}
}
void UCommonButton::BeginDestroy()
{
	FWidgetThemeResolver::OnThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
	Super::BeginDestroy();
}
