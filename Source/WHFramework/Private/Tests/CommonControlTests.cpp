#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "UObject/StrongObjectPtr.h"
#include "NativeGameplayTags.h"
#include "Widget/Common/CommonBorderN.h"
#include "Widget/Common/CommonInput.h"
#include "Widget/Common/CommonRange.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/Common/CommonTabList.h"
#include "Widget/Common/CommonOptionSelector.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Common/CommonImageN.h"
#include "Components/PanelWidget.h"
#include "Components/NativeWidgetHost.h"
#include "Widget/Style/CommonInputStyle.h"
#include "Widget/Style/CommonRangeStyle.h"
#include "Components/HorizontalBoxSlot.h"
#include "Widget/Style/CommonScrollStyle.h"
#include "Widget/Style/SubWidgetStyle.h"
#include "Widget/Style/WidgetBrushStyle.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Widget/Common/CommonToggle.h"
#include "Widget/Common/CommonProgress.h"
#include "Widget/WidgetModuleTypes.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SBorder.h"
#include "UObject/UnrealType.h"
#include "Components/StackBox.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeResolver.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ControlTestBorder, "Style.Border.ControlTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ControlTestScroll, "Style.Scroll.ControlTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ControlTestButton, "Style.Button.ControlTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ControlTestMarker, "Style.Brush.ControlTestMarker");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ControlTestLabel, "Style.Text.ControlTestLabel");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ControlTestInput, "Style.Input.ControlTest");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonBorderSynchronizationTest, "WHFramework.WidgetTheme.Controls.BorderSynchronization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonBorderSynchronizationTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UWidgetTheme> Theme(NewObject<UWidgetTheme>());
	Theme->SetStyleClass(TAG_ControlTestBorder, EWidgetStyleKind::Border, UCommonBorderStyle::StaticClass());
	TStrongObjectPtr<UCommonBorderN> Border(NewObject<UCommonBorderN>(Theme.Get()));
	FWidgetThemeResolver::SetPreviewOverride(Theme.Get(), Theme.Get());
	Border->SetStyleTag(TAG_ControlTestBorder);
	const TSharedRef<SWidget> BorderWidget = Border->TakeWidget();
	Border->SynchronizeProperties();
	TestEqual(TEXT("Border resolves the preview style after Slate construction"), Border->Style.Get(), UCommonBorderStyle::StaticClass());
	FWidgetThemeResolver::NotifyThemeChanged();
	Border->SetStyle(UCommonBorderStyle::StaticClass());
	Border->SetInheritedStyleTag(TAG_ControlTestBorder);
	Border->ClearStyleOverride();
	TestEqual(TEXT("Native style updates and theme notifications preserve the resolved style"), Border->Style.Get(), UCommonBorderStyle::StaticClass());
	FWidgetThemeResolver::ClearPreviewOverride(Theme.Get());
	static_cast<UWidget*>(Border.Get())->ReleaseSlateResources(true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonButtonStyleStateTest, "WHFramework.WidgetTheme.Controls.ButtonStyleState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonButtonStyleStateTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UWidgetTheme> Theme(NewObject<UWidgetTheme>());
	Theme->SetStyleClass(TAG_ControlTestButton, EWidgetStyleKind::Button, USubWidgetStyle::StaticClass());
	Theme->SetStyleClass(WidgetStyleTags::CommonDefault(EWidgetStyleKind::Button), EWidgetStyleKind::Button, USubWidgetStyle::StaticClass());
	Theme->SetStyleClass(TAG_ControlTestMarker, EWidgetStyleKind::Brush, UWidgetBrushStyle::StaticClass());
	USubWidgetStyle* ButtonStyle = GetMutableDefault<USubWidgetStyle>();
	UWidgetBrushStyle* BrushStyle = GetMutableDefault<UWidgetBrushStyle>();
	TGuardValue<FGameplayTag> MarkerGuard(ButtonStyle->SelectionMarkerStyleTag, TAG_ControlTestMarker);
	TGuardValue<FGameplayTag> IconGuard(ButtonStyle->IconStyleTag, TAG_ControlTestMarker);
	TGuardValue<FSlateBrush> BrushGuard(BrushStyle->Brush, BrushStyle->Brush);
	TStrongObjectPtr<UTexture2D> Texture(NewObject<UTexture2D>());
	BrushStyle->Brush.SetResourceObject(Texture.Get());
	TStrongObjectPtr<UCommonButton> Button(NewObject<UCommonButton>(Theme.Get()));
	FWidgetThemeResolver::SetPreviewOverride(Theme.Get(), Theme.Get());
	ON_SCOPE_EXIT
	{
		FWidgetThemeResolver::ClearPreviewOverride(Theme.Get());
	};
	Button->SetStyleTag(TAG_ControlTestButton);
	Button->Initialize();
	const TSharedRef<SWidget> ButtonWidget = Button->TakeWidget();
	UCommonButtonInternalBase* Internal = Cast<UCommonButtonInternalBase>(Button->WidgetTree->RootWidget);
	if (!TestNotNull(TEXT("CommonUI wraps the native layout during initialization"), Internal))
	{
		return false;
	}
	int32 ClickCount = 0;
	const FDelegateHandle ClickHandle = Button->OnClicked().AddLambda([&ClickCount]() { ++ClickCount; });
	Internal->OnClicked.Broadcast();
	Button->OnClicked().Remove(ClickHandle);
	TestEqual(TEXT("The native root forwards a click exactly once"), ClickCount, 1);
	UImage* Marker = Cast<UImage>(Button->WidgetTree->FindWidget(TEXT("Img_SelectionMarker")));
	UImage* Icon = Cast<UImage>(Button->WidgetTree->FindWidget(TEXT("Img_Icon")));
	if (!TestNotNull(TEXT("Native button has a selection marker"), Marker) || !TestNotNull(TEXT("Native button has an icon"), Icon))
	{
		return false;
	}
	TestEqual(TEXT("Unselected marker reserves its stable layout space"), Marker->GetVisibility(), ESlateVisibility::Hidden);
	Button->SetIsSelectable(true);
	Button->SetIsToggleable(true);
	Button->SetIsSelected(true, false);
	TestTrue(TEXT("Selection updates the CommonUI button state"), Button->GetSelected());
	TestEqual(TEXT("Selection reveals the themed marker"), Marker->GetVisibility(), ESlateVisibility::HitTestInvisible);
	Button->SetIsSelected(false, false);
	TestFalse(TEXT("Deselection updates the CommonUI button state"), Button->GetSelected());
	TestEqual(TEXT("Deselection preserves marker layout space"), Marker->GetVisibility(), ESlateVisibility::Hidden);
	TestTrue(TEXT("Icon resolves its brush Tag"), Icon->GetBrush().HasUObject());
	ButtonStyle->IconStyleTag = FGameplayTag();
	Button->SetStyleTag(TAG_ControlTestButton);
	TestEqual(TEXT("Removing an icon from the active style hides the old glyph"), Icon->GetVisibility(), ESlateVisibility::Collapsed);
	TestFalse(TEXT("Removing an icon releases its old brush"), Icon->GetBrush().HasUObject());
	Button->SetIconStyleTag(TAG_ControlTestMarker);
	TestTrue(TEXT("An instance glyph Tag resolves independently of the button style"), Icon->GetBrush().HasUObject());
	Button->SetIconStyleTag(FGameplayTag());
	TestFalse(TEXT("Clearing the instance glyph releases its old brush"), Icon->GetBrush().HasUObject());
	UStackBox* Content = Cast<UStackBox>(Button->WidgetTree->FindWidget(TEXT("ButtonLayout")));
	if (!TestNotNull(TEXT("Native button supports both content orientations"), Content))
	{
		return false;
	}
	Button->SetContentOrientation(Orient_Vertical);
	TestEqual(TEXT("Vertical icon and caption layout applies to the existing control"), Content->GetOrientation(), Orient_Vertical);
	Button->SetContentOrientation(Orient_Horizontal);
	TestEqual(TEXT("Horizontal layout can be restored"), Content->GetOrientation(), Orient_Horizontal);
	Button->SetInheritedStyleTag(TAG_ControlTestButton);
	Button->SetStyleTag(WidgetStyleTags::CommonDefault(EWidgetStyleKind::Button));
	TestEqual(TEXT("Explicit style overrides its inherited slot style"), Button->GetStyleTag(), WidgetStyleTags::CommonDefault(EWidgetStyleKind::Button));
	Button->ClearStyleOverride();
	TestEqual(TEXT("Clearing explicit style restores its inherited slot style"), Button->GetStyleTag(), FGameplayTag(TAG_ControlTestButton));
	Button->SetInheritedStyleTag(FGameplayTag());
	TestEqual(TEXT("A button without a configured Tag resolves the framework default"), Button->GetStyleTag(), WidgetStyleTags::CommonDefault(EWidgetStyleKind::Button));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonInputTypedParsingTest, "WHFramework.WidgetTheme.Controls.TypedInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonInputTypedParsingTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UCommonInput> Input(NewObject<UCommonInput>());
	Input->SetValue(FParameter(TNumericLimits<int64>::Max()));
	TestEqual(TEXT("Int64 maximum keeps its exact type and value"), Input->GetValue().Get<int64>(), TNumericLimits<int64>::Max());
	FParameter Parsed;
	for (const TCHAR* Invalid : {TEXT("9223372036854775808"), TEXT("12suffix"), TEXT("1.5"), TEXT(""), TEXT("--1"), TEXT("+-1"), TEXT("++1")})
	{
		Input->SetText(FText::FromString(Invalid));
		TestFalse(TEXT("Invalid/overflow integer input is rejected"), Input->TryGetValue(Parsed));
		TestFalse(TEXT("Rejected text never becomes a valid zero"), Parsed.HasValue());
	}
	Input->SetValue(FParameter(0.5));
	for (const TCHAR* Invalid : {TEXT("1.5suffix"), TEXT("nan"), TEXT("inf"), TEXT("1e999")})
	{
		Input->SetText(FText::FromString(Invalid));
		TestFalse(TEXT("Floating input must be finite and fully consumed"), Input->TryGetValue(Parsed));
	}
	const FText Localized = NSLOCTEXT("CommonControlTests", "History", "Localized text history");
	Input->SetValue(FParameter(Localized));
	TestTrue(TEXT("FText history survives a model refresh"), Input->GetValue().Get<FText>().IdenticalTo(Localized));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonRangeExactInputTest, "WHFramework.WidgetTheme.Controls.ExactRangeInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonRangeExactInputTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UCommonRange> Range(NewObject<UCommonRange>());
	Range->SetValue(FParameter(TNumericLimits<int64>::Max()));
	Range->Initialize();
	const TSharedRef<SWidget> RangeWidget = Range->TakeWidget();
	IObjectPoolInterface::Execute_OnSpawn(Range.Get(), FParameter());
	TestEqual(TEXT("Range stores an exact typed integer"), Range->GetValue().Get<int64>(), TNumericLimits<int64>::Max());
	UCommonInput* Input = Range->WidgetTree ? Cast<UCommonInput>(Range->WidgetTree->FindWidget(TEXT("Input_Value"))) : nullptr;
	if (!TestNotNull(TEXT("Range owns the shared input control"), Input))
	{
		return false;
	}
	UEditableTextBox* TextBox = Input->WidgetTree ? Cast<UEditableTextBox>(Input->WidgetTree->FindWidget(TEXT("TxtBox_Value"))) : nullptr;
	if (!TestNotNull(TEXT("Input owns the native editable text box"), TextBox))
	{
		return false;
	}
	const FText ExactText = FText::FromString(TEXT("9223372036854775806"));
	TextBox->SetText(ExactText);
	TextBox->OnTextCommitted.Broadcast(ExactText, ETextCommit::OnEnter);
	TestEqual(TEXT("Committed int64 text never passes through double"), Range->GetValue().Get<int64>(), TNumericLimits<int64>::Max() - 1);
	const FText Overflow = FText::FromString(TEXT("9223372036854775808"));
	TextBox->SetText(Overflow);
	TextBox->OnTextCommitted.Broadcast(Overflow, ETextCommit::OnEnter);
	TestEqual(TEXT("Overflow preserves the last accepted range value"), Range->GetValue().Get<int64>(), TNumericLimits<int64>::Max() - 1);
	TestEqual(TEXT("Invalid text is restored from the accepted value"), Input->GetText().ToString(), ExactText.ToString());
	IObjectPoolInterface::Execute_OnDespawn(Range.Get(), EObjectDespawnMode::Recovery);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonRangeStyleRefreshTest, "WHFramework.WidgetTheme.Controls.RangeStyleRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonRangeStyleRefreshTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UCommonRange> Range(NewObject<UCommonRange>());
	Range->SetValue(FParameter(1.5));
	FCommonRangeConfig Config;
	Config.bUseSlider = false;
	Config.bShowStepButtons = true;
	Range->Configure(Config);
	Range->Initialize();
	const TSharedRef<SWidget> RangeSlate = Range->TakeWidget();
	TStrongObjectPtr<UWidgetTheme> Theme(NewObject<UWidgetTheme>());
	Theme->SetStyleClass(TAG_ControlTestInput, EWidgetStyleKind::Input, UCommonInputStyle::StaticClass());
	Theme->SetStyleClass(TAG_ControlTestMarker, EWidgetStyleKind::Brush, UWidgetBrushStyle::StaticClass());
	FWidgetThemeResolver::SetPreviewOverride(Range.Get(), Theme.Get());
	ON_SCOPE_EXIT
	{
		FWidgetThemeResolver::ClearPreviewOverride(Range.Get());
	};
	UCommonInputStyle* InputStyle = GetMutableDefault<UCommonInputStyle>();
	TGuardValue<FEditableTextBoxStyle> InputGuard(InputStyle->InputStyle, InputStyle->InputStyle);
	InputStyle->InputStyle.BackgroundImageNormal.DrawAs = ESlateBrushDrawType::Box;
	UWidgetBrushStyle* BackgroundStyle = GetMutableDefault<UWidgetBrushStyle>();
	TGuardValue<FSlateBrush> BrushGuard(BackgroundStyle->Brush, BackgroundStyle->Brush);
	BackgroundStyle->Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	TStrongObjectPtr<UCommonRangeStyle> Style(NewObject<UCommonRangeStyle>());
	Style->ValueInputStyleTag = TAG_ControlTestInput;
	Style->ValueBackgroundBrushStyleTag = TAG_ControlTestMarker;
	Range->ApplyResolvedStyle(Style.Get());
	UCommonInput* Input = CastChecked<UCommonInput>(Range->WidgetTree->FindWidget(TEXT("Input_Value")));
	UEditableTextBox* TextBox = CastChecked<UEditableTextBox>(Input->GetWidgetFromName(TEXT("TxtBox_Value")));
	UCommonImageN* Background = CastChecked<UCommonImageN>(Range->WidgetTree->FindWidget(TEXT("ValueBackground")));
	UHorizontalBoxSlot* ValueSlot = CastChecked<UHorizontalBoxSlot>(Range->WidgetTree->FindWidget(TEXT("ValueArea"))->Slot);
	TestEqual(TEXT("Grouped numeric controls use the theme brush"), Background->GetBrush().DrawAs.GetValue(), ESlateBrushDrawType::RoundedBox);
	TestEqual(TEXT("The numeric background does not intercept input"), Background->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("Grouped numeric controls suppress the duplicate input frame"), TextBox->GetWidgetStyle().BackgroundImageNormal.DrawAs.GetValue(), ESlateBrushDrawType::NoDrawType);
	TestEqual(TEXT("Standalone numeric controls fill their row"), ValueSlot->GetSize().SizeRule.GetValue(), ESlateSizeRule::Fill);
	TestEqual(TEXT("Standalone controls have no phantom slider gap"), ValueSlot->GetPadding(), FMargin(0.f));
	Style->ValueBackgroundBrushStyleTag = FGameplayTag();
	Range->ApplyResolvedStyle(Style.Get());
	TestEqual(TEXT("Removing a group binding restores the input brush"), TextBox->GetWidgetStyle().BackgroundImageNormal.DrawAs.GetValue(), ESlateBrushDrawType::Box);
	TestEqual(TEXT("An unbound numeric background is collapsed"), Background->GetVisibility(), ESlateVisibility::Collapsed);
	Config.bUseSlider = true;
	Config.bHasMin = true;
	Config.bHasMax = true;
	Config.Max = 100;
	Range->Configure(Config);
	TestEqual(TEXT("Slider layout lets the value group size to its content"), ValueSlot->GetSize().SizeRule.GetValue(), ESlateSizeRule::Automatic);
	TestEqual(TEXT("Slider layout retains its separation"), ValueSlot->GetPadding(), FMargin(12.f, 0.f, 0.f, 0.f));
	TestEqual(TEXT("Style and layout refresh preserve typed values"), Range->GetValue().Get<double>(), 1.5);
	IObjectPoolInterface::Execute_OnDespawn(Range.Get(), EObjectDespawnMode::Recovery);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonTabListSelectionTest, "WHFramework.WidgetTheme.Controls.TabListSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonTabListSelectionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("Preview world is available"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
	};
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Tab lifecycle has a player controller"), Controller))
	{
		return false;
	}
	TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
	Controller->Player = Player.Get();
	Player->PlayerController = Controller;
	World->AddController(Controller);
	const FLocalPlayerContext PlayerContext(Player.Get(), World);
	if (!TestTrue(TEXT("Preview controller is discoverable through the local player context"), PlayerContext.IsValid()))
	{
		return false;
	}
	TStrongObjectPtr<UCommonTabList> Tabs(NewObject<UCommonTabList>(World));
	Tabs->SetPlayerContext(PlayerContext);
	if (!TestTrue(TEXT("Tabs initialize with their local player context"), Tabs->Initialize()))
	{
		return false;
	}
	const TSharedRef<SWidget> TabsWidget = Tabs->TakeWidget();
	if (!TestTrue(TEXT("First tab registers"), Tabs->RegisterTab(TEXT("First"), UCommonButton::StaticClass(), nullptr)) ||
		!TestTrue(TEXT("Second tab registers"), Tabs->RegisterTab(TEXT("Second"), UCommonButton::StaticClass(), nullptr)))
	{
		return false;
	}
	UPanelWidget* Content = Cast<UPanelWidget>(Tabs->WidgetTree->FindWidget(TEXT("TabContent")));
	if (!TestNotNull(TEXT("Native tabs attach to their themed widget tree"), Content))
	{
		return false;
	}
	TestEqual(TEXT("Both registered buttons are visible children"), Content->GetChildrenCount(), 2);
	TestTrue(TEXT("Second tab can be selected"), Tabs->SelectTabByID(TEXT("Second")));
	TestEqual(TEXT("Group owns the selected identifier"), Tabs->GetSelectedTabId(), FName(TEXT("Second")));
	TestFalse(TEXT("Selecting second clears first selection"), Tabs->GetTabButtonBaseByID(TEXT("First"))->GetSelected());
	Tabs->SetOrientation(Orient_Vertical);
	UStackBox* Stack = Cast<UStackBox>(Content);
	if (!TestNotNull(TEXT("Native tabs support either layout direction"), Stack))
	{
		return false;
	}
	TestEqual(TEXT("Changing direction updates the existing layout"), Stack->GetOrientation(), Orient_Vertical);
	TestEqual(TEXT("Changing direction preserves selection"), Tabs->GetSelectedTabId(), FName(TEXT("Second")));
	TestTrue(TEXT("Removing a tab succeeds"), Tabs->RemoveTab(TEXT("First")));
	TestEqual(TEXT("Removal detaches its layout child"), Content->GetChildrenCount(), 1);
	TestEqual(TEXT("Removing another tab preserves current selection"), Tabs->GetSelectedTabId(), FName(TEXT("Second")));
	IObjectPoolInterface::Execute_OnDespawn(Tabs.Get(), EObjectDespawnMode::Recovery);
	TestEqual(TEXT("Pool release clears registration"), Tabs->GetTabCount(), 0);
	TestEqual(TEXT("Pool release clears layout children"), Content->GetChildrenCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonOptionLabelLifetimeTest, "WHFramework.WidgetTheme.Controls.OptionLabelLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonOptionLabelLifetimeTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UCommonOptionSelector> Selector(NewObject<UCommonOptionSelector>());
	Selector->SetOptionNames({TEXT("Same"), TEXT("Same"), TEXT("Other")});
	Selector->Initialize();
	const TSharedRef<SWidget> SelectorWidget = Selector->TakeWidget();
	UNativeWidgetHost* Combo = Cast<UNativeWidgetHost>(Selector->WidgetTree->FindWidget(TEXT("Combo_Value")));
	if (!TestNotNull(TEXT("Selector hosts its native dropdown"), Combo) || !TestTrue(TEXT("Dropdown is constructed"), Selector->Dropdown.IsValid()))
	{
		return false;
	}
	UWidget* FirstItem = Selector->GenerateOption(TEXT("Option_0"));
	UWidget* Content = Selector->GenerateSelectedOption(TEXT("Option_0"));
	TestTrue(TEXT("Repeated row generation reuses the same label"), FirstItem == Selector->GenerateOption(TEXT("Option_0")));
	TestTrue(TEXT("Popup row and selected content have distinct widgets"), FirstItem != Content);
	for (int32 Index = 0; Index < 100; ++Index)
	{
		TestTrue(TEXT("Content label is bounded across selection changes"), Content == Selector->GenerateSelectedOption(FName(*FString::Printf(TEXT("Option_%d"), Index % 3))));
	}
	Selector->SetOptionIndex(1);
	TestEqual(TEXT("Equal labels retain distinct option indices"), Selector->GetOptionIndex(), 1);
	Selector->SetOptionNames({TEXT("Replacement")});
	TestTrue(TEXT("Changing options releases obsolete row cache"), FirstItem != Selector->GenerateOption(TEXT("Option_0")));
	IObjectPoolInterface::Execute_OnDespawn(Selector.Get(), EObjectDespawnMode::Recovery);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonOptionStyleRefreshTest, "WHFramework.WidgetTheme.Controls.OptionStyleRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonOptionStyleRefreshTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UCommonOptionSelector> Selector(NewObject<UCommonOptionSelector>());
	Selector->SetLabel(FText::FromString(TEXT("Skin")));
	Selector->SetLabelWidth(138.f);
	Selector->SetOptionNames({TEXT("First"), TEXT("Second")});
	Selector->SetOptionIndex(1);
	Selector->Initialize();
	const TSharedRef<SWidget> SelectorWidget = Selector->TakeWidget();
	const TSharedPtr<SComboBox<FName>> OriginalDropdown = Selector->Dropdown;
	const TSharedPtr<SScrollBar> OriginalScrollbar = Selector->DropdownScrollbar;
	TStrongObjectPtr<UWidgetTheme> Theme(NewObject<UWidgetTheme>());
	Theme->SetStyleClass(TAG_ControlTestScroll, EWidgetStyleKind::Scroll, UCommonScrollStyle::StaticClass());
	Theme->SetStyleClass(TAG_ControlTestLabel, EWidgetStyleKind::Text, UCommonTextStyle::StaticClass());
	Theme->SetStyleClass(TAG_ControlTestMarker, EWidgetStyleKind::Brush, UWidgetBrushStyle::StaticClass());
	FWidgetThemeResolver::SetPreviewOverride(Selector.Get(), Theme.Get());
	UCommonScrollStyle* ScrollStyle = GetMutableDefault<UCommonScrollStyle>();
	TGuardValue<FScrollBarStyle> ScrollStyleGuard(ScrollStyle->ScrollBarStyle, ScrollStyle->ScrollBarStyle);
	UWidgetBrushStyle* BackgroundStyle = GetMutableDefault<UWidgetBrushStyle>();
	TGuardValue<FSlateBrush> BackgroundGuard(BackgroundStyle->Brush, BackgroundStyle->Brush);
	BackgroundStyle->Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	BackgroundStyle->Brush.TintColor = FSlateColor(FLinearColor::Green);
	ON_SCOPE_EXIT
	{
		FWidgetThemeResolver::ClearPreviewOverride(Selector.Get());
	};
	TStrongObjectPtr<UCommonInputStyle> Style(NewObject<UCommonInputStyle>());
	Style->ComboItemStyle.TextColor = FSlateColor(FLinearColor::Yellow);
	Style->OptionStyles.PopupScrollStyleTag = TAG_ControlTestScroll;
	Style->OptionStyles.LabelTextStyleTag = TAG_ControlTestLabel;
	Style->OptionStyles.ValueBackgroundBrushStyleTag = TAG_ControlTestMarker;
	Style->InputStyle.BackgroundImageNormal.DrawAs = ESlateBrushDrawType::Box;
	ScrollStyle->ScrollBarStyle.NormalThumbImage.TintColor = FSlateColor(FLinearColor::Red);
	ScrollStyle->ScrollBarStyle.HoveredThumbImage.TintColor = FSlateColor(FLinearColor::Red);
	ScrollStyle->ScrollBarStyle.DraggedThumbImage.TintColor = FSlateColor(FLinearColor::Red);
	Selector->ApplyResolvedStyle(Style.Get());
	TestEqual(TEXT("Selector caption displays its field label"), Selector->NativeLabel->GetText().ToString(), FString(TEXT("Skin")));
	TestEqual(TEXT("Selector caption resolves its own text style"), Selector->NativeLabel->GetStyleTag(), TAG_ControlTestLabel.GetTag());
	TestEqual(TEXT("Grouped selector background uses the themed brush"), Selector->NativeValueBackground->GetBrush().DrawAs.GetValue(), ESlateBrushDrawType::RoundedBox);
	TestEqual(TEXT("Grouped background does not intercept input"), Selector->NativeValueBackground->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("A shared background removes the duplicate input frame"), Selector->TxtBox_Value->GetWidgetStyle().BackgroundImageNormal.DrawAs.GetValue(), ESlateBrushDrawType::NoDrawType);
	Selector->SetUseDropdown(true);
	TestEqual(TEXT("Dropdown uses its own background"), Selector->NativeValueBackground->GetVisibility(), ESlateVisibility::Collapsed);
	Selector->SetUseDropdown(false);
	Selector->DropdownScrollbar->SetState(0.f, .5f);
	Selector->DropdownScrollbar->SlatePrepass();
	TFunction<const FSlateBrush*(const TSharedRef<SWidget>&)> FindThumb = [&](const TSharedRef<SWidget>& Widget) -> const FSlateBrush*
	{
		if (Widget->GetType() == FName(TEXT("SBorder")))
		{
			const FSlateBrush* Brush = StaticCastSharedRef<SBorder>(Widget)->GetBorderImage();
			if (Brush == &Selector->DropdownScrollStyle.NormalThumbImage || Brush == &Selector->DropdownScrollStyle.HoveredThumbImage || Brush == &Selector->DropdownScrollStyle.DraggedThumbImage)
			{
				return Brush;
			}
		}
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Index < Children->Num(); ++Index)
		{
			if (const FSlateBrush* Brush = FindThumb(Children->GetChildAt(Index)))
			{
				return Brush;
			}
		}
		return nullptr;
	};
	const FSlateBrush* FirstThumb = FindThumb(Selector->DropdownScrollbar.ToSharedRef());
	if (!TestNotNull(TEXT("Live scrollbar exposes its painted thumb"), FirstThumb))
	{
		return false;
	}
	TestEqual(TEXT("First theme updates the live scrollbar brush"), FirstThumb->TintColor.GetSpecifiedColor(), FLinearColor::Red);
	ScrollStyle->ScrollBarStyle.NormalThumbImage.TintColor = FSlateColor(FLinearColor::Blue);
	ScrollStyle->ScrollBarStyle.HoveredThumbImage.TintColor = FSlateColor(FLinearColor::Blue);
	ScrollStyle->ScrollBarStyle.DraggedThumbImage.TintColor = FSlateColor(FLinearColor::Blue);
	Selector->ApplyResolvedStyle(Style.Get());
	Selector->DropdownScrollbar->SlatePrepass();
	const FSlateBrush* SecondThumb = FindThumb(Selector->DropdownScrollbar.ToSharedRef());
	if (!TestNotNull(TEXT("Restyled scrollbar keeps its painted thumb"), SecondThumb))
	{
		return false;
	}
	TestEqual(TEXT("Second theme updates the same scrollbar brush"), SecondThumb->TintColor.GetSpecifiedColor(), FLinearColor::Blue);
	TestTrue(TEXT("Theme refresh keeps the live dropdown"), Selector->Dropdown == OriginalDropdown);
	TestTrue(TEXT("Theme refresh updates the existing scrollbar"), Selector->DropdownScrollbar == OriginalScrollbar);
	TestEqual(TEXT("Theme refresh preserves the selected index"), Selector->GetOptionIndex(), 1);
	TestEqual(TEXT("Theme refresh preserves the Slate selection"), Selector->Dropdown->GetSelectedItem(), FName(TEXT("Option_1")));
	Style->OptionStyles.ValueBackgroundBrushStyleTag = FGameplayTag();
	Selector->ApplyResolvedStyle(Style.Get());
	TestEqual(TEXT("Removing the grouped background restores the input style"), Selector->TxtBox_Value->GetWidgetStyle().BackgroundImageNormal.DrawAs.GetValue(), ESlateBrushDrawType::Box);
	TestEqual(TEXT("An unbound grouped background is collapsed"), Selector->NativeValueBackground->GetVisibility(), ESlateVisibility::Collapsed);
	Selector->bEditable = true;
	UEditableTextBox* Input = Cast<UEditableTextBox>(Selector->WidgetTree->FindWidget(TEXT("TxtBox_Value")));
	if (!TestNotNull(TEXT("Editable selector owns its input"), Input))
	{
		return false;
	}
	Input->SetText(FText::FromString(TEXT("Custom")));
	Input->OnTextChanged.Broadcast(Input->GetText());
	TestEqual(TEXT("Editable options retain values outside the list"), Selector->GetText().ToString(), FString(TEXT("Custom")));
	TestEqual(TEXT("Custom values do not select a different option"), Selector->GetOptionIndex(), INDEX_NONE);
	Input->SetText(FText::FromString(TEXT("First")));
	Input->OnTextChanged.Broadcast(Input->GetText());
	TestEqual(TEXT("Typing a known option updates its index"), Selector->GetOptionIndex(), 0);
	IObjectPoolInterface::Execute_OnDespawn(Selector.Get(), EObjectDespawnMode::Recovery);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonControlPoolResetTest, "WHFramework.WidgetTheme.Controls.PoolReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonControlPoolResetTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UCommonOptionSelector> Selector(NewObject<UCommonOptionSelector>());
	Selector->Initialize();
	const TSharedRef<SWidget> SelectorWidget = Selector->TakeWidget();
	FCommonOptionSelectorSpawnParameter SelectorParameters(FText::FromString(TEXT("Pool skin")), {TEXT("First"), TEXT("Second")}, false);
	IObjectPoolInterface::Execute_OnSpawn(Selector.Get(), FParameter(SelectorParameters));
	Selector->SetLabelWidth(138.f);
	TestEqual(TEXT("Spawn parameters produce a visible selector caption"), Selector->NativeLabel->GetText().ToString(), FString(TEXT("Pool skin")));
	IObjectPoolInterface::Execute_OnDespawn(Selector.Get(), EObjectDespawnMode::Recovery);
	TestTrue(TEXT("Pool restores the template selector caption"), Selector->Label.EqualTo(CastChecked<UCommonOptionSelector>(Selector->GetArchetype())->Label));
	TestEqual(TEXT("Pool restores the template caption width"), Selector->LabelWidth, CastChecked<UCommonOptionSelector>(Selector->GetArchetype())->LabelWidth);
	TestEqual(TEXT("An empty caption releases its layout space"), static_cast<UWidget*>(Selector->NativeLabel.Get())->GetParent()->GetVisibility(), ESlateVisibility::Collapsed);
	TStrongObjectPtr<UCommonInput> Input(NewObject<UCommonInput>());
	Input->SetIsPassword(true);
	Input->SetIsReadOnly(true);
	Input->SetHintText(FText::FromString(TEXT("Changed hint")));
	Input->SetUseStyleBackground(false);
	Input->SetText(FText::FromString(TEXT("Changed value")));
	Input->SetIsEnabled(false);
	Input->SetVisibility(ESlateVisibility::Collapsed);
	IObjectPoolInterface::Execute_OnDespawn(Input.Get(), EObjectDespawnMode::Recovery);
	for (FName Name : {FName(TEXT("bPassword")), FName(TEXT("bReadOnly")), FName(TEXT("bUseStyleBackground")), FName(TEXT("HintText")), FName(TEXT("Text")), FName(TEXT("bIsEnabled")), FName(TEXT("Visibility"))})
	{
		const FProperty* Property = FindFProperty<FProperty>(Input->GetClass(), Name);
		if (TestNotNull(TEXT("Reset property exists"), Property))
		{
			TestTrue(FString::Printf(TEXT("Pool restores template %s"), *Name.ToString()), Property->Identical_InContainer(Input.Get(), Input->GetArchetype()));
		}
	}
	TStrongObjectPtr<UCommonRange> Range(NewObject<UCommonRange>());
	FCommonRangeConfig Config;
	Config.bHasMin = true;
	Config.Min = 19;
	Config.DisplayScale = 100;
	Range->Configure(Config);
	Range->SetValue(FParameter(int64(123456789)));
	IObjectPoolInterface::Execute_OnDespawn(Range.Get(), EObjectDespawnMode::Recovery);
	for (FName Name : {FName(TEXT("Config")), FName(TEXT("Value"))})
	{
		const FProperty* Property = FindFProperty<FProperty>(Range->GetClass(), Name);
		if (TestNotNull(TEXT("Range reset property exists"), Property))
		{
			TestTrue(FString::Printf(TEXT("Pool restores range %s"), *Name.ToString()), Property->Identical_InContainer(Range.Get(), Range->GetArchetype()));
		}
	}
	TStrongObjectPtr<UCommonToggle> Toggle(NewObject<UCommonToggle>());
	Toggle->SetIsChecked(true);
	IObjectPoolInterface::Execute_OnDespawn(Toggle.Get(), EObjectDespawnMode::Recovery);
	TestEqual(TEXT("Pool restores template check state"), Toggle->GetCheckedState(), CastChecked<UCommonToggle>(Toggle->GetArchetype())->GetCheckedState());
	TStrongObjectPtr<UCommonProgress> Progress(NewObject<UCommonProgress>());
	Progress->SetPercent(.75f);
	IObjectPoolInterface::Execute_OnDespawn(Progress.Get(), EObjectDespawnMode::Recovery);
	TestEqual(TEXT("Pool restores template progress"), Progress->GetPercent(), CastChecked<UCommonProgress>(Progress->GetArchetype())->GetPercent());
	return true;
}
#endif
