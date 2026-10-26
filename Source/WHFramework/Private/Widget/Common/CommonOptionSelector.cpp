#include "Widget/Common/CommonOptionSelector.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/NativeWidgetHost.h"
#include "Components/ComboBoxKey.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SBox.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Common/CommonImageN.h"
#include "Widget/Style/CommonInputStyle.h"
#include "Widget/Style/CommonScrollStyle.h"
#include "Widget/Style/CommonBrushStyle.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/WidgetModuleTypes.h"
#include "CommonBorder.h"

UCommonOptionSelector::UCommonOptionSelector(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bReadOnly = true;
	DropdownStyle = GetDefault<UComboBoxKey>()->GetWidgetStyle();
	DropdownItemStyle = GetDefault<UComboBoxKey>()->GetItemStyle();
	DropdownScrollStyle = GetDefault<UComboBoxKey>()->GetScrollBarStyle();
}

void UCommonOptionSelector::BuildNativeLayout()
{
	UOverlay* ValueArea = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ValueArea"));
	WidgetTree->RootWidget = ValueArea;
	NativeValueBackground = WidgetTree->ConstructWidget<UCommonImageN>(UCommonImageN::StaticClass(), TEXT("ValueBackground"));
	UOverlaySlot* BackgroundSlot = ValueArea->AddChildToOverlay(NativeValueBackground);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
	UHorizontalBox* Controls = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ValueControls"));
	UOverlaySlot* ControlsSlot = ValueArea->AddChildToOverlay(Controls);
	ControlsSlot->SetHorizontalAlignment(HAlign_Fill);
	ControlsSlot->SetVerticalAlignment(VAlign_Fill);
	Btn_Last = WidgetTree->ConstructWidget<UCommonButton>(UCommonButton::StaticClass(), TEXT("Btn_Last"));
	Btn_Last->SetTitle(FText::FromString(TEXT("‹")));
	Controls->AddChildToHorizontalBox(Btn_Last);
	TxtBox_Value = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("TxtBox_Value"));
	TxtBox_Value->SetJustification(ETextJustify::Center);
	Controls->AddChildToHorizontalBox(TxtBox_Value)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Combo_Value = WidgetTree->ConstructWidget<UNativeWidgetHost>(UNativeWidgetHost::StaticClass(), TEXT("Combo_Value"));
	Controls->AddChildToHorizontalBox(Combo_Value)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Btn_Next = WidgetTree->ConstructWidget<UCommonButton>(UCommonButton::StaticClass(), TEXT("Btn_Next"));
	Btn_Next->SetTitle(FText::FromString(TEXT("›")));
	Controls->AddChildToHorizontalBox(Btn_Next);
	BuildDropdown();
}

void UCommonOptionSelector::BuildDropdown()
{
	if (!Combo_Value || Dropdown)
	{
		return;
	}
	DropdownScrollbar = SNew(SScrollBar).Style(&DropdownScrollStyle).Thickness(DropdownScrollBarThickness);
	SAssignNew(Dropdown, SComboBox<FName>)
		.ComboBoxStyle(&DropdownStyle)
		.ItemStyle(&DropdownItemStyle)
		.OptionsSource(&OptionKeys)
		.CustomScrollbar(DropdownScrollbar)
		.EnableGamepadNavigationMode(true)
		.OnGenerateWidget_UObject(this, &ThisClass::GenerateOptionSlate)
		.OnSelectionChanged_UObject(this, &ThisClass::HandleSelectionChanged)
		[
			SAssignNew(SelectedContent, SBox)
		];
	Combo_Value->SetContent(Dropdown.ToSharedRef());
	RefreshValue();
}

void UCommonOptionSelector::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Dropdown.Reset();
	DropdownScrollbar.Reset();
	SelectedContent.Reset();
}

void UCommonOptionSelector::RefreshSelectedLabel()
{
	if (SelectedContent)
	{
		SelectedContent->SetContent(GenerateSelectedOption(OptionKeys.IsValidIndex(OptionIndex) ? OptionKeys[OptionIndex] : NAME_None)->TakeWidget());
	}
}

TSharedRef<SWidget> UCommonOptionSelector::GenerateOptionSlate(FName InKey)
{
	return GenerateOption(InKey)->TakeWidget();
}

void UCommonOptionSelector::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	BuildDropdown();
	RefreshValue();
}

void UCommonOptionSelector::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
	if (const FCommonOptionSelectorSpawnParameter* Parameter = InParam.GetPtr<FCommonOptionSelectorSpawnParameter>())
	{
		bEditable = Parameter->bEditable;
		SetOptionNames(Parameter->Options);
	}
	RefreshValue();
}

void UCommonOptionSelector::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
	OnOptionChanged.Clear();
	const UCommonOptionSelector* Defaults = CastChecked<UCommonOptionSelector>(GetArchetype());
	bEditable = Defaults->bEditable;
	bUseDropdown = Defaults->bUseDropdown;
	SetOptions(Defaults->Options);
	SetOptionIndex(Defaults->OptionIndex);
}

void UCommonOptionSelector::BindControlEvents()
{
	Super::BindControlEvents();
	if (Btn_Last)
	{
		Btn_Last->OnClicked().RemoveAll(this);
		Btn_Last->OnClicked().AddUObject(this, &ThisClass::OnPreviousClicked);
	}
	if (Btn_Next)
	{
		Btn_Next->OnClicked().RemoveAll(this);
		Btn_Next->OnClicked().AddUObject(this, &ThisClass::OnNextClicked);
	}
}

void UCommonOptionSelector::UnbindControlEvents()
{
	if (Btn_Last)
	{
		Btn_Last->OnClicked().RemoveAll(this);
	}
	if (Btn_Next)
	{
		Btn_Next->OnClicked().RemoveAll(this);
	}
	Super::UnbindControlEvents();
}

void UCommonOptionSelector::ApplyResolvedStyle(const UObject* InStyle)
{
	const UCommonInputStyle* Style = Cast<UCommonInputStyle>(InStyle);
	bUseStyleBackground = !Style || !Style->OptionStyles.ValueBackgroundBrushStyleTag.IsValid();
	Super::ApplyResolvedStyle(InStyle);
	if (!Style)
	{
		return;
	}
	if (NativeValueBackground)
	{
		NativeValueBackground->SetStyleTag(Style->OptionStyles.ValueBackgroundBrushStyleTag);
	}
	if (Btn_Last)
	{
		Btn_Last->SetInheritedStyleTag(Style->OptionStyles.PreviousButtonStyleTag);
	}
	if (Btn_Next)
	{
		Btn_Next->SetInheritedStyleTag(Style->OptionStyles.NextButtonStyleTag);
	}
	OptionTextStyleTag = Style->OptionStyles.ItemTextStyleTag;
	for (const auto& Pair : GeneratedLabels)
	{
		Pair.Value->SetStyleTag(OptionTextStyleTag);
	}
	if (SelectedLabel)
	{
		SelectedLabel->SetStyleTag(OptionTextStyleTag);
	}
	DropdownStyle = Style->ComboBoxStyle;
	DropdownItemStyle = Style->ComboItemStyle;
	DropdownScrollStyle = GetDefault<UComboBoxKey>()->GetScrollBarStyle();
	DropdownScrollBarThickness = GetDefault<UCommonScrollStyle>()->ScrollBarThickness;
	UClass* ExpandClass = FWidgetThemeResolver::ResolveClass(this, Style->OptionStyles.ExpandButtonStyleTag, EWidgetStyleKind::Button);
	if (const UCommonButtonStyle* Expand = ExpandClass ? ExpandClass->GetDefaultObject<UCommonButtonStyle>() : nullptr)
	{
		Expand->GetNormalBaseBrush(DropdownStyle.ComboButtonStyle.ButtonStyle.Normal);
		Expand->GetNormalHoveredBrush(DropdownStyle.ComboButtonStyle.ButtonStyle.Hovered);
		Expand->GetNormalPressedBrush(DropdownStyle.ComboButtonStyle.ButtonStyle.Pressed);
		Expand->GetDisabledBrush(DropdownStyle.ComboButtonStyle.ButtonStyle.Disabled);
		Expand->GetButtonPadding(DropdownStyle.ComboButtonStyle.ContentPadding);
	}
	UClass* ArrowClass = FWidgetThemeResolver::ResolveClass(this, Style->OptionStyles.ArrowBrushStyleTag, EWidgetStyleKind::Brush);
	if (const UCommonBrushStyle* Arrow = ArrowClass ? ArrowClass->GetDefaultObject<UCommonBrushStyle>() : nullptr;
		Style->OptionStyles.ArrowBrushStyleTag.IsValid() && Arrow)
	{
		DropdownStyle.ComboButtonStyle.DownArrowImage = Arrow->Brush;
	}
	UClass* ItemClass = FWidgetThemeResolver::ResolveClass(this, Style->OptionStyles.ItemButtonStyleTag, EWidgetStyleKind::Button);
	if (const UCommonButtonStyle* Item = ItemClass ? ItemClass->GetDefaultObject<UCommonButtonStyle>() : nullptr)
	{
		Item->GetNormalBaseBrush(DropdownItemStyle.EvenRowBackgroundBrush);
		DropdownItemStyle.OddRowBackgroundBrush = DropdownItemStyle.EvenRowBackgroundBrush;
		Item->GetNormalHoveredBrush(DropdownItemStyle.EvenRowBackgroundHoveredBrush);
		DropdownItemStyle.OddRowBackgroundHoveredBrush = DropdownItemStyle.EvenRowBackgroundHoveredBrush;
		Item->GetSelectedBaseBrush(DropdownItemStyle.ActiveBrush);
		Item->GetSelectedHoveredBrush(DropdownItemStyle.ActiveHoveredBrush);
		DropdownItemStyle.InactiveBrush = DropdownItemStyle.ActiveBrush;
		DropdownItemStyle.InactiveHoveredBrush = DropdownItemStyle.ActiveHoveredBrush;
	}
	UClass* BorderClass = FWidgetThemeResolver::ResolveClass(this, Style->OptionStyles.PopupBorderStyleTag, EWidgetStyleKind::Border);
	if (const UCommonBorderStyle* Border = BorderClass ? BorderClass->GetDefaultObject<UCommonBorderStyle>() : nullptr)
	{
		DropdownStyle.ComboButtonStyle.MenuBorderBrush = Border->Background;
	}
	UClass* ScrollClass = FWidgetThemeResolver::ResolveClass(this, Style->OptionStyles.PopupScrollStyleTag, EWidgetStyleKind::Scroll);
	if (const UCommonScrollStyle* Scroll = ScrollClass ? ScrollClass->GetDefaultObject<UCommonScrollStyle>() : nullptr)
	{
		DropdownScrollStyle = Scroll->ScrollBarStyle;
		DropdownScrollBarThickness = Scroll->ScrollBarThickness;
	}
	if (Dropdown)
	{
		DropdownScrollbar->SetStyle(&DropdownScrollStyle);
		DropdownScrollbar->SetThickness(DropdownScrollBarThickness);
		Dropdown->InvalidateStyle();
		Dropdown->InvalidateItemStyle();
	}
	RefreshValue();
}

void UCommonOptionSelector::SetOptions(const TArray<FText>& InOptions)
{
	TGuardValue<bool> Guard(bSynchronizingValue, true);
	Options = InOptions;
	OptionIndex = INDEX_NONE;
	OptionKeys.Reset();
	if (Dropdown)
	{
		Dropdown->ClearSelection();
	}
	GeneratedLabels.Reset();
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		const FName Key(*FString::Printf(TEXT("Option_%d"), Index));
		OptionKeys.Add(Key);
	}
	if (Dropdown)
	{
		Dropdown->RefreshOptions();
	}
	RefreshValue();
}

void UCommonOptionSelector::SetOptionNames(const TArray<FString>& InOptionNames)
{
	TArray<FText> Labels;
	for (const FString& Option : InOptionNames)
	{
		Labels.Add(FText::FromString(Option));
	}
	SetOptions(Labels);
}

TArray<FString> UCommonOptionSelector::GetOptionNames() const
{
	TArray<FString> Labels;
	for (const FText& Option : Options)
	{
		Labels.Add(Option.ToString());
	}
	return Labels;
}

void UCommonOptionSelector::SetOptionIndex(int32 InOptionIndex, bool bNotify)
{
	const int32 NewIndex = Options.IsValidIndex(InOptionIndex) ? InOptionIndex : INDEX_NONE;
	const bool bChanged = NewIndex != OptionIndex;
	OptionIndex = NewIndex;
	RefreshValue();
	if (bNotify && bChanged)
	{
		OnOptionChanged.Broadcast(this, OptionIndex);
		OnValueChanged.Broadcast(this, GetValue());
	}
}

int32 UCommonOptionSelector::GetOptionIndex() const
{
	return OptionIndex;
}

FString UCommonOptionSelector::GetSelectedOption() const
{
	return Options.IsValidIndex(OptionIndex) ? Options[OptionIndex].ToString() : FString();
}

void UCommonOptionSelector::SetValue(const FParameter& InValue)
{
	Super::SetValue(InValue);
	const FString ValueText = GetText().ToString();
	SetOptionIndex(Options.IndexOfByPredicate([&ValueText](const FText& Option) { return Option.ToString() == ValueText; }));
}

void UCommonOptionSelector::SetUseDropdown(bool bInUseDropdown)
{
	bUseDropdown = bInUseDropdown;
	RefreshValue();
}

void UCommonOptionSelector::RefreshValue()
{
	TGuardValue<bool> Guard(bSynchronizingValue, true);
	SetIsReadOnly(!bEditable);
	SetText(Options.IsValidIndex(OptionIndex) ? Options[OptionIndex] : (bEditable ? Text : FText::GetEmpty()));
	if (NativeValueBackground)
	{
		NativeValueBackground->SetVisibility(!bUseDropdown && NativeValueBackground->GetStyleTag().IsValid()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (TxtBox_Value)
	{
		TxtBox_Value->SetVisibility(bUseDropdown ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (Combo_Value)
	{
		Combo_Value->SetVisibility(bUseDropdown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Dropdown)
	{
		if (OptionKeys.IsValidIndex(OptionIndex))
		{
			Dropdown->SetSelectedItem(OptionKeys[OptionIndex]);
		}
		else
		{
			Dropdown->ClearSelection();
		}
		RefreshSelectedLabel();
	}
	if (Btn_Last)
	{
		Btn_Last->SetVisibility(bUseDropdown ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		Btn_Last->SetIsEnabled(OptionIndex > 0);
	}
	if (Btn_Next)
	{
		Btn_Next->SetVisibility(bUseDropdown ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		Btn_Next->SetIsEnabled(Options.IsValidIndex(OptionIndex + 1));
	}
}

void UCommonOptionSelector::OnPreviousClicked()
{
	SetOptionIndex(OptionIndex - 1, true);
}

void UCommonOptionSelector::OnNextClicked()
{
	SetOptionIndex(OptionIndex + 1, true);
}

void UCommonOptionSelector::HandleTextChanged(const FText& InText)
{
	const int32 PreviousIndex = OptionIndex;
	if (!bSynchronizingValue && bEditable)
	{
		const FString Value = InText.ToString();
		OptionIndex = Options.IndexOfByPredicate([&Value](const FText& Option) { return Option.ToString() == Value; });
	}
	Super::HandleTextChanged(InText);
	if (!bSynchronizingValue && bEditable && PreviousIndex != OptionIndex)
	{
		OnOptionChanged.Broadcast(this, OptionIndex);
	}
}

void UCommonOptionSelector::HandleSelectionChanged(FName InKey, ESelectInfo::Type InSelectionType)
{
	if (!bSynchronizingValue && InSelectionType != ESelectInfo::Direct)
	{
		SetOptionIndex(OptionKeys.IndexOfByKey(InKey), true);
	}
}

UWidget* UCommonOptionSelector::GenerateOption(FName InKey)
{
	TObjectPtr<UCommonTextBlockN>& OptionLabel = GeneratedLabels.FindOrAdd(InKey);
	if (!OptionLabel)
	{
		OptionLabel = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass());
	}
	const int32 Index = OptionKeys.IndexOfByKey(InKey);
	OptionLabel->SetText(Options.IsValidIndex(Index) ? Options[Index] : FText::GetEmpty());
	OptionLabel->SetStyleTag(OptionTextStyleTag);
	return OptionLabel;
}

UWidget* UCommonOptionSelector::GenerateSelectedOption(FName InKey)
{
	if (!SelectedLabel)
	{
		SelectedLabel = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass());
	}
	const int32 Index = OptionKeys.IndexOfByKey(InKey);
	SelectedLabel->SetText(Options.IsValidIndex(Index) ? Options[Index] : FText::GetEmpty());
	SelectedLabel->SetStyleTag(OptionTextStyleTag);
	return SelectedLabel;
}
