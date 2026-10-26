#include "Widget/Common/CommonWindow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/NamedSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/Style/CommonWindowStyle.h"
#include "Widget/Theme/WidgetStyleTags.h"

UCommonWindow::UCommonWindow(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	StyleTag = WidgetStyleTags::CommonDefault(EWidgetStyleKind::Window);
}

EWidgetStyleKind UCommonWindow::GetStyleKind() const
{
	return EWidgetStyleKind::Window;
}

void UCommonWindow::InitializeNativeClassData()
{
	Super::InitializeNativeClassData();
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		BuildNativeLayout();
	}
}

void UCommonWindow::BuildNativeLayout()
{
	WindowBody = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WindowBody"));
	WidgetTree->RootWidget = WindowBody;
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WindowColumn"));
	WindowBody->AddChild(Column);
	TitleBarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TitleBarSize"));
	Column->AddChildToVerticalBox(TitleBarSize);
	TitleBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleBar"));
	TitleBarSize->AddChild(TitleBar);
	Txt_Title = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass(), TEXT("Txt_Title"));
	Txt_Title->SetStyleTag(FGameplayTag());
	UHorizontalBoxSlot* TitleSlot = TitleBar->AddChildToHorizontalBox(Txt_Title);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	Btn_Close = WidgetTree->ConstructWidget<UCommonButton>(UCommonButton::StaticClass(), TEXT("Btn_Close"));
	TitleBar->AddChildToHorizontalBox(Btn_Close)->SetVerticalAlignment(VAlign_Top);
	TitleSeparatorSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TitleSeparatorSize"));
	Column->AddChildToVerticalBox(TitleSeparatorSize);
	TitleSeparator = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TitleSeparator"));
	TitleSeparator->SetPadding(FMargin(0.f));
	TitleSeparator->SetVisibility(ESlateVisibility::HitTestInvisible);
	TitleSeparatorSize->SetContent(TitleSeparator);
	Content = WidgetTree->ConstructWidget<UNamedSlot>(UNamedSlot::StaticClass(), TEXT("Content"));
	Column->AddChildToVerticalBox(Content)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SetTitle(Title);
}

void UCommonWindow::BindControlEvents()
{
	Super::BindControlEvents();
	if (Btn_Close)
	{
		Btn_Close->OnClicked().RemoveAll(this);
		Btn_Close->OnClicked().AddUObject(this, &ThisClass::RequestClose);
	}
}

void UCommonWindow::UnbindControlEvents()
{
	if (Btn_Close) Btn_Close->OnClicked().RemoveAll(this);
	Super::UnbindControlEvents();
}

void UCommonWindow::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	OnCloseRequested.Clear();
	Super::OnDespawn_Implementation(InMode);
}

void UCommonWindow::RequestClose()
{
	if (bShowCloseButton && GetIsEnabled()) OnCloseRequested.Broadcast();
}

void UCommonWindow::SetShowCloseButton(bool bInShowCloseButton)
{
	bShowCloseButton = bInShowCloseButton;
	SetTitle(Title);
}

void UCommonWindow::SetCloseButtonStyleTag(FGameplayTag InStyleTag)
{
	CloseButtonStyleTag = InStyleTag;
	RefreshStyle();
}

void UCommonWindow::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (Txt_Title)
	{
		SetTitle(Title);
	}
}

void UCommonWindow::SetTitle(const FText& InTitle)
{
	Title = InTitle;
	if (Txt_Title)
	{
		Txt_Title->SetText(Title);
		Txt_Title->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Btn_Close) Btn_Close->SetVisibility(bShowCloseButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (TitleBar) TitleBar->SetVisibility(Title.IsEmpty() && !bShowCloseButton ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (TitleSeparatorSize) TitleSeparatorSize->SetVisibility(Title.IsEmpty() && !bShowCloseButton ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (TitleBarSize) TitleBarSize->SetVisibility(Title.IsEmpty() && !bShowCloseButton ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void UCommonWindow::GetSlotNames(TArray<FName>& SlotNames) const
{
	Super::GetSlotNames(SlotNames);
	SlotNames.AddUnique(TEXT("Content"));
}

UWidget* UCommonWindow::GetContentForSlot(FName SlotName) const
{
	return Super::GetContentForSlot(SlotName);
}

void UCommonWindow::SetContentForSlot(FName SlotName, UWidget* InContent)
{
	if (SlotName == TEXT("Content") && (!WidgetTree || !WidgetTree->RootWidget))
	{
		InitializeNativeClassData();
	}
	Super::SetContentForSlot(SlotName, InContent);
}

void UCommonWindow::ApplyResolvedStyle(const UObject* InStyle)
{
	if (!WindowBody)
	{
		return;
	}
	const UCommonWindowStyle* Style = CastChecked<UCommonWindowStyle>(InStyle);
	WindowBody->SetBrush(Style->Background);
	WindowBody->SetPadding(Style->Padding);
	Txt_Title->SetStyle(Style->TitleTextStyle);
	Txt_Title->SetWrapTextAt(Style->TextWidth);
	if (Style->TitleBarHeight > 0.f) TitleBarSize->SetHeightOverride(Style->TitleBarHeight);
	else TitleBarSize->ClearHeightOverride();
	CastChecked<UVerticalBoxSlot>(TitleBarSize->Slot)->SetPadding(FMargin(0.f));
	TitleSeparator->SetBrush(Style->TitleSeparator);
	TitleSeparatorSize->SetHeightOverride(Style->TitleSeparatorHeight);
	CastChecked<UVerticalBoxSlot>(TitleSeparatorSize->Slot)->SetPadding(FMargin(0.f, 0.f, 0.f, Style->TitleSpacing));
	Btn_Close->SetStyleTag(CloseButtonStyleTag.IsValid() ? CloseButtonStyleTag : Style->CloseButtonStyleTag);
	Btn_Close->SetTitle(Style->CloseButtonText);
	Btn_Close->SetContentMinSize(Style->CloseButtonSize);
	CastChecked<UHorizontalBoxSlot>(Btn_Close->Slot)->SetPadding(Style->CloseButtonPadding);
	SetTitle(Title);
}
