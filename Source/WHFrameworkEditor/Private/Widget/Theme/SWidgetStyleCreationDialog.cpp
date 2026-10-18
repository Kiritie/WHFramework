#include "SWidgetStyleCreationDialog.h"

#include "SWidgetThemePicker.h"
#include "Widget/Theme/WidgetThemeEditorService.h"
#include "Widget/Theme/WidgetThemeEditorSettings.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Styling/AppStyle.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

void SWidgetStyleCreationDialog::Construct(const FArguments& InArgs)
{
	Theme = InArgs._Theme;
	ParentTag = InArgs._ParentTag;
	Window = InArgs._Window;
	bTypeLocked = FWidgetThemeEditorService::Kind(ParentTag, Kind);
	if (!bTypeLocked)
	{
		Kind = EWidgetStyleKind::Button;
		ParentTag = WidgetStyleTags::Root(Kind);
	}
	OnCompleted = InArgs._OnCompleted;
	bInheritParent = bTypeLocked && !WidgetStyleTags::IsRoot(ParentTag);
	for (const FString& Root : FWidgetThemeEditorService::GetCreationRoots())
	{
		TSharedPtr<FString> Item = MakeShared<FString>(Root);
		CreationRoots.Add(Item);
		if (Theme.IsValid() && Theme->GetOutermost()->GetName().StartsWith(Root.Left(Root.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1)) + TEXT("/")))
		{
			CreationRoot = Item;
		}
	}
	if (!CreationRoot)
	{
		CreationRoot = CreationRoots[0];
	}
	auto Row = [](const TCHAR* Label, TSharedRef<SWidget> Control) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox) +
			   SHorizontalBox::Slot().AutoWidth().Padding(
				   4)[SNew(SBox).WidthOverride(110)[SNew(STextBlock).Text(FText::FromString(Label)).Justification(ETextJustify::Center)]] +
			   SHorizontalBox::Slot().FillWidth(1).Padding(4)[Control];
	};
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	TSharedRef<SHorizontalBox> Modes = SNew(SHorizontalBox);
	for (EWidgetStyleOperation Mode : {EWidgetStyleOperation::Create, EWidgetStyleOperation::Associate})
	{
		Modes->AddSlot().FillWidth(1.f).Padding(1.f, 0.f)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "SegmentedCombo.ButtonOnly")
			.HAlign(HAlign_Center)
			.Padding(FMargin(4.f, 3.f))
			.IsChecked_Lambda([this, Mode] { return Operation == Mode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this, Mode](ECheckBoxState State)
			{
				if (State == ECheckBoxState::Checked)
				{
					Operation = Mode;
					Report.Reset();
				}
			})
			[
				SNew(STextBlock).Justification(ETextJustify::Center)
				.Text(Mode == EWidgetStyleOperation::Create ? NSLOCTEXT("WidgetTheme", "NativeMode", "Native")
					: NSLOCTEXT("WidgetTheme", "AssociateMode", "Associate"))
			]
		];
	}
	Body->AddSlot().AutoHeight()[Row(TEXT("Mode"), Modes)];
	Body->AddSlot().AutoHeight()[Row(TEXT("Tag"), SAssignNew(NameInput, SEditableTextBox)
													  .OnTextChanged_Lambda(
														  [this](const FText& Text)
														  {
															  FString Error;
															  FWidgetThemeEditorService::ValidateName(Text.ToString(), Error);
															  NameInput->SetError(FText::FromString(Error));
														  }))];
	TSharedPtr<EWidgetStyleKind> SelectedType;
	for (EWidgetStyleKind Type : WidgetStyleTags::Kinds())
	{
		TSharedPtr<EWidgetStyleKind> Item = MakeShared<EWidgetStyleKind>(Type);
		StyleTypes.Add(Item);
		if (Type == Kind)
		{
			SelectedType = Item;
		}
	}
	Body->AddSlot().AutoHeight()[Row(TEXT("Type"), SNew(SComboBox<TSharedPtr<EWidgetStyleKind>>)
		.IsEnabled(!bTypeLocked)
		.OptionsSource(&StyleTypes)
		.InitiallySelectedItem(SelectedType)
		.OnGenerateWidget_Lambda([](TSharedPtr<EWidgetStyleKind> Type)
		{
			return SNew(STextBlock).Text(FText::FromString(WidgetStyleTags::Label(*Type)));
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<EWidgetStyleKind> Type, ESelectInfo::Type)
		{
			if (Type)
			{
				Kind = *Type;
				ParentTag = WidgetStyleTags::Root(Kind);
				SourceTag = FGameplayTag();
				bInheritParent = false;
			}
		})[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(WidgetStyleTags::Label(Kind)); })])];

	Body->AddSlot()
		.AutoHeight()[Row(TEXT("Parent Tag"), SNew(SComboButton)
												  .IsEnabled(bTypeLocked)
												  .OnGetMenuContent(this, &SWidgetStyleCreationDialog::BuildParentMenu)
												  .ButtonContent()[SNew(STextBlock).Text_Lambda([this] { return UWidgetThemeEditorSettings::GetStyleTagDisplayName(ParentTag); })])];
	auto RootLabel = [](const FString& Root)
	{
		const int32 Separator = Root.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);
		const FString Mount = Separator == INDEX_NONE ? Root : Root.Left(Separator);
		return FText::FromString(Mount.Mid(1));
	};
	{
		TSharedRef<SVerticalBox> NativeFields = SNew(SVerticalBox)
			.Visibility_Lambda([this] { return Operation == EWidgetStyleOperation::Create ? EVisibility::Visible : EVisibility::Collapsed; });
		NativeFields->AddSlot().AutoHeight()[Row(
			TEXT("Create in"),
			SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&CreationRoots)
				.InitiallySelectedItem(CreationRoot)
				.OnGenerateWidget_Lambda(
					[RootLabel](TSharedPtr<FString> Root)
					{ return SNew(STextBlock).Text(RootLabel(*Root)); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Root, ESelectInfo::Type)
										   { CreationRoot = Root; })[SNew(STextBlock).Text_Lambda([this, RootLabel] { return RootLabel(*CreationRoot); })])];
		TSharedRef<SHorizontalBox> Inheritance = SNew(SHorizontalBox);
		for (bool Inherit : {false, true})
		{
			Inheritance->AddSlot().FillWidth(1).Padding(
				1, 0)[SNew(SCheckBox)
						  .Style(FAppStyle::Get(), "SegmentedCombo.ButtonOnly")
						  .HAlign(HAlign_Center)
						  .Padding(FMargin(4, 3))
						  .IsEnabled_Lambda([this, Inherit] { return !Inherit || !WidgetStyleTags::IsRoot(ParentTag); })
						  .IsChecked_Lambda([this, Inherit] { return bInheritParent == Inherit ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						  .OnCheckStateChanged_Lambda(
							  [this, Inherit](ECheckBoxState State)
							  {
								  if (State == ECheckBoxState::Checked)
								  {
									  bInheritParent = Inherit;
								  }
							  })[SNew(STextBlock).Text(FText::FromString(Inherit ? TEXT("Parent Tag") : TEXT("None"))).Justification(ETextJustify::Center)]];
		}
		NativeFields->AddSlot().AutoHeight()[Row(TEXT("Inheritance"), Inheritance)];
		Body->AddSlot().AutoHeight()[NativeFields];
	}
	{
		TSharedRef<SVerticalBox> AssociatedFields = SNew(SVerticalBox)
			.Visibility_Lambda([this] { return Operation == EWidgetStyleOperation::Associate ? EVisibility::Visible : EVisibility::Collapsed; });
		AssociatedFields->AddSlot().AutoHeight()[Row(TEXT("Source theme"), SNew(SWidgetThemePicker)
																   .SelectedTheme_Lambda([this] { return SourceTheme.Get(); })
																   .OnThemeSelected_Lambda(
																	   [this](UWidgetTheme* Source)
																	   {
																		   SourceTheme = Source;
																		   SourceTag = FGameplayTag();
																	   }))];
		AssociatedFields->AddSlot().AutoHeight()[Row(TEXT("Source Tag"),
										 SNew(SComboButton)
											 .IsEnabled_Lambda([this] { return SourceTheme.IsValid(); })
											 .OnGetMenuContent(this, &SWidgetStyleCreationDialog::BuildSourceMenu)
											 .ButtonContent()[SNew(STextBlock).Text_Lambda([this] { return UWidgetThemeEditorSettings::GetStyleTagDisplayName(SourceTag); })])];
		Body->AddSlot().AutoHeight()[AssociatedFields];
	}
	Body->AddSlot().FillHeight(1).Padding(4)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return FText::FromString(Report); })];
	Body->AddSlot().AutoHeight().Padding(4)[SNew(SButton)
												.HAlign(HAlign_Center)
												.Text(NSLOCTEXT("WidgetTheme", "Confirm", "Confirm"))
												.IsEnabled_Lambda(
													[this]
													{
														FString Error;
														return FWidgetThemeEditorService::ValidateName(NameInput->GetText().ToString(), Error) &&
															   (Operation == EWidgetStyleOperation::Create || (SourceTheme.IsValid() && SourceTag.IsValid()));
													})
												.OnClicked(this, &SWidgetStyleCreationDialog::Execute)];
	ChildSlot[SNew(SBox).Padding(12)[Body]];
}

TSharedRef<SWidget> SWidgetStyleCreationDialog::BuildSourceMenu()
{
	FMenuBuilder Menu(true, nullptr);
	for (FGameplayTag StyleTag : FWidgetThemeEditorService::Tags(SourceTheme.Get()))
	{
		EWidgetStyleKind Type;
		if (FWidgetThemeEditorService::Kind(StyleTag, Type) && Type == Kind)
		{
			Menu.AddMenuEntry(UWidgetThemeEditorSettings::GetStyleTagDisplayName(StyleTag), FText::FromString(FWidgetThemeEditorService::DescribeTag(SourceTheme.Get(), StyleTag)),
							  FSlateIcon(), FUIAction(FExecuteAction::CreateSPLambda(this, [this, StyleTag] { SourceTag = StyleTag; })));
		}
	}
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SWidgetStyleCreationDialog::BuildParentMenu()
{
	FMenuBuilder Menu(true, nullptr);
	TArray<FGameplayTag> Parents{WidgetStyleTags::Root(Kind)};
	if (Theme.IsValid())
	{
		Parents.Append(Theme->GetStyleTags(Kind, true));
	}
	for (FGameplayTag StyleTag : Parents)
	{
		EWidgetStyleKind Type;
		if (FWidgetThemeEditorService::Kind(StyleTag, Type) && Type == Kind)
		{
			Menu.AddMenuEntry(UWidgetThemeEditorSettings::GetStyleTagDisplayName(StyleTag), FText::FromName(StyleTag.GetTagName()), FSlateIcon(),
							  FUIAction(FExecuteAction::CreateSPLambda(this,
																	   [this, StyleTag]
																	   {
																		   ParentTag = StyleTag;
																		   if (WidgetStyleTags::IsRoot(StyleTag))
																		   {
																			   bInheritParent = false;
																		   }
																	   })));
		}
	}
	return Menu.MakeWidget();
}

FReply SWidgetStyleCreationDialog::Execute()
{
	Report.Reset();
	const FString Name = NameInput->GetText().ToString();
	const bool Good = Operation == EWidgetStyleOperation::Create
						  ? FWidgetThemeEditorService::Create(Theme.Get(), ParentTag, Name, Kind, *CreationRoot, bInheritParent, Report)
						  : FWidgetThemeEditorService::Associate(Theme.Get(), ParentTag, Name, SourceTheme.Get(), SourceTag, Report);
	if (Good)
	{
		OnCompleted.ExecuteIfBound();
		if (TSharedPtr<SWindow> Dialog = Window.Pin())
		{
			Dialog->RequestDestroyWindow();
		}
	}
	return FReply::Handled();
}
