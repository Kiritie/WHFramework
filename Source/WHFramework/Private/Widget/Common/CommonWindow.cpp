#include "Widget/Common/CommonWindow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/NamedSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Widget/Common/CommonTextBlockN.h"
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
	Txt_Title = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass(), TEXT("Txt_Title"));
	Txt_Title->SetStyleTag(FGameplayTag());
	Column->AddChildToVerticalBox(Txt_Title);
	Content = WidgetTree->ConstructWidget<UNamedSlot>(UNamedSlot::StaticClass(), TEXT("Content"));
	Column->AddChildToVerticalBox(Content)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SetTitle(Title);
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
	CastChecked<UVerticalBoxSlot>(Txt_Title->Slot)->SetPadding(FMargin(0.f, 0.f, 0.f, Style->TitleSpacing));
	SetTitle(Title);
}
