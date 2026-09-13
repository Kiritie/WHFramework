// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Customization/WidgetModuleCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "GameplayTagsManager.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleTypes.h"

#define LOCTEXT_NAMESPACE "WidgetModuleCustomization"

namespace
{
	DECLARE_DELEGATE_OneParam(FOnWidgetTagPicked, FGameplayTag)

	class SWidgetConfigTagPicker : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWidgetConfigTagPicker)
		{
		}
			SLATE_ARGUMENT(FGameplayTag, RootTag)
			SLATE_ARGUMENT(TSet<FGameplayTag>, ExcludedTags)
			SLATE_EVENT(FOnWidgetTagPicked, OnTagPicked)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			RootTag = InArgs._RootTag;
			ExcludedTags = InArgs._ExcludedTags;
			OnTagPicked = InArgs._OnTagPicked;
			RootNode = UGameplayTagsManager::Get().FindTagNode(RootTag);
			RefreshRootItems();

			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.f)
				[
					SNew(SSearchBox)
					.HintText(LOCTEXT("SearchWidgetTags", "Search widget tags"))
					.OnTextChanged(this, &SWidgetConfigTagPicker::OnSearchTextChanged)
				]
				+ SVerticalBox::Slot()
				.MaxHeight(360.f)
				[
					SAssignNew(TagTreeView, STreeView<TSharedPtr<FGameplayTagNode>>)
					.TreeItemsSource(&RootItems)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SWidgetConfigTagPicker::OnGenerateRow)
					.OnGetChildren(this, &SWidgetConfigTagPicker::OnGetChildren)
					.OnSelectionChanged(this, &SWidgetConfigTagPicker::OnSelectionChanged)
				]
			];
		}

	private:
		bool ShouldShowNode(const TSharedPtr<FGameplayTagNode>& InNode) const
		{
			if(!InNode.IsValid())
			{
				return false;
			}

			bool bHasVisibleChild = false;
			for(const TSharedPtr<FGameplayTagNode>& ChildNode : InNode->GetChildTagNodes())
			{
				if(ShouldShowNode(ChildNode))
				{
					bHasVisibleChild = true;
					break;
				}
			}

			const FGameplayTag NodeTag = InNode->GetCompleteTag();
			if(!SearchText.IsEmpty())
			{
				return (!ExcludedTags.Contains(NodeTag)
						&& NodeTag.ToString().Contains(SearchText, ESearchCase::IgnoreCase))
					|| bHasVisibleChild;
			}
			return !ExcludedTags.Contains(NodeTag) || bHasVisibleChild;
		}

		void RefreshRootItems()
		{
			RootItems.Reset();
			if(RootNode.IsValid())
			{
				for(const TSharedPtr<FGameplayTagNode>& ChildNode : RootNode->GetChildTagNodes())
				{
					if(ShouldShowNode(ChildNode))
					{
						RootItems.Add(ChildNode);
					}
				}
			}
		}

		void OnSearchTextChanged(const FText& InText)
		{
			SearchText = InText.ToString();
			RefreshRootItems();
			if(TagTreeView.IsValid())
			{
				TagTreeView->RequestTreeRefresh();
				if(!SearchText.IsEmpty())
				{
					for(const TSharedPtr<FGameplayTagNode>& RootItem : RootItems)
					{
						TagTreeView->SetItemExpansion(RootItem, true);
					}
				}
			}
		}

		TSharedRef<ITableRow> OnGenerateRow(
			TSharedPtr<FGameplayTagNode> InNode,
			const TSharedRef<STableViewBase>& InOwnerTable) const
		{
			const FGameplayTag NodeTag = InNode->GetCompleteTag();
			const bool bAlreadyAdded = ExcludedTags.Contains(NodeTag);
			return SNew(STableRow<TSharedPtr<FGameplayTagNode>>, InOwnerTable)
				.ToolTipText(FText::FromString(NodeTag.ToString()))
				[
					SNew(STextBlock)
					.Text(FText::FromName(InNode->GetSimpleTagName()))
					.ColorAndOpacity(bAlreadyAdded
						? FSlateColor::UseSubduedForeground()
						: FSlateColor::UseForeground())
				];
		}

		void OnGetChildren(
			TSharedPtr<FGameplayTagNode> InNode,
			TArray<TSharedPtr<FGameplayTagNode>>& OutChildren) const
		{
			for(const TSharedPtr<FGameplayTagNode>& ChildNode : InNode->GetChildTagNodes())
			{
				if(ShouldShowNode(ChildNode))
				{
					OutChildren.Add(ChildNode);
				}
			}
		}

		void OnSelectionChanged(
			TSharedPtr<FGameplayTagNode> InNode,
			ESelectInfo::Type InSelectInfo)
		{
			if(!InNode.IsValid() || InSelectInfo == ESelectInfo::Direct)
			{
				return;
			}

			const FGameplayTag NodeTag = InNode->GetCompleteTag();
			if(ExcludedTags.Contains(NodeTag))
			{
				TagTreeView->SetItemExpansion(InNode, !TagTreeView->IsItemExpanded(InNode));
				TagTreeView->ClearSelection();
				return;
			}

			OnTagPicked.ExecuteIfBound(NodeTag);
		}

	private:
		FGameplayTag RootTag;
		TSet<FGameplayTag> ExcludedTags;
		FOnWidgetTagPicked OnTagPicked;
		TSharedPtr<FGameplayTagNode> RootNode;
		TArray<TSharedPtr<FGameplayTagNode>> RootItems;
		TSharedPtr<STreeView<TSharedPtr<FGameplayTagNode>>> TagTreeView;
		FString SearchText;
	};

	FString GetTagSuffix(const FGameplayTag InTag, const FString& InRootTag)
	{
		const FString TagString = InTag.ToString();
		const FString Prefix = InRootTag + TEXT(".");
		return TagString.StartsWith(Prefix) ? TagString.RightChop(Prefix.Len()) : TagString;
	}

	void SetTagHandleValue(const TSharedPtr<IPropertyHandle>& InHandle, const FGameplayTag InTag)
	{
		if(!InHandle.IsValid())
		{
			return;
		}

		TArray<void*> RawData;
		InHandle->AccessRawData(RawData);
		InHandle->NotifyPreChange();
		for(void* Data : RawData)
		{
			if(Data)
			{
				*static_cast<FGameplayTag*>(Data) = InTag;
			}
		}
		InHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		InHandle->NotifyFinishedChangingProperties();
	}
}

FWidgetModuleCustomization::FWidgetModuleCustomization()
	: DetailBuilder(nullptr)
{
}

TSharedRef<IDetailCustomization> FWidgetModuleCustomization::MakeInstance()
{
	return MakeShared<FWidgetModuleCustomization>();
}

void FWidgetModuleCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FClassCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
	DetailBuilder = &DetailLayoutBuilder;

	BuildConfigCategory(
		DetailLayoutBuilder,
		TEXT("UserWidgetConfigs"),
		TEXT("ScreenWidget"),
		LOCTEXT("ScreenWidgetCategory", "Screen Widget"),
		false);
	BuildConfigCategory(
		DetailLayoutBuilder,
		TEXT("WorldWidgetConfigs"),
		TEXT("WorldWidget"),
		LOCTEXT("WorldWidgetCategory", "World Widget"),
		true);
}

void FWidgetModuleCustomization::BuildConfigCategory(
	IDetailLayoutBuilder& DetailLayoutBuilder,
	const FName InPropertyName,
	const FName InCategoryName,
	const FText& InCategoryLabel,
	bool bInWorldWidget)
{
	const TSharedRef<IPropertyHandle> ConfigsProperty = DetailLayoutBuilder.GetProperty(
		InPropertyName,
		UWidgetModule::StaticClass());
	const TSharedPtr<IPropertyHandleArray> ConfigsHandle = ConfigsProperty->AsArray();
	if(bInWorldWidget)
	{
		WorldWidgetConfigsHandle = ConfigsHandle;
	}
	else
	{
		UserWidgetConfigsHandle = ConfigsHandle;
	}
	DetailLayoutBuilder.HideProperty(ConfigsProperty);

	IDetailCategoryBuilder& Category = DetailLayoutBuilder.EditCategory(
		InCategoryName,
		InCategoryLabel,
		ECategoryPriority::Important);

	TSharedPtr<SComboButton>& AddButton = bInWorldWidget ? WorldAddButton : ScreenAddButton;
	Category.AddCustomRow(LOCTEXT("WidgetConfigActions", "Widget Config Actions"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 0.f, 6.f, 0.f)
		[
			SAssignNew(AddButton, SComboButton)
			.OnGetMenuContent(this, &FWidgetModuleCustomization::MakeTagPicker, bInWorldWidget)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(bInWorldWidget
					? LOCTEXT("AddWorldWidget", "+ Add World Widget")
					: LOCTEXT("AddScreenWidget", "+ Add Screen Widget"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("ValidateWidgets", "Validate"))
			.OnClicked(this, &FWidgetModuleCustomization::OnValidateWidgets)
		]
	];

	uint32 ConfigCount = 0;
	if(!ConfigsHandle.IsValid() || ConfigsHandle->GetNumElements(ConfigCount) != FPropertyAccess::Success)
	{
		return;
	}

	struct FConfigRow
	{
		uint32 Index = 0;
		FGameplayTag WidgetTag;
		FString ClassName;
		bool bHasParent = false;
	};

	TArray<FConfigRow> Rows;
	for(uint32 Index = 0; Index < ConfigCount; ++Index)
	{
		const TSharedRef<IPropertyHandle> ElementHandle = ConfigsHandle->GetElement(Index);
		TArray<void*> RawData;
		ElementHandle->AccessRawData(RawData);

		FConfigRow& Row = Rows.AddDefaulted_GetRef();
		Row.Index = Index;
		if(bInWorldWidget)
		{
			const FWorldWidgetConfig* Config = RawData.IsEmpty()
				? nullptr
				: static_cast<FWorldWidgetConfig*>(RawData[0]);
			if(Config)
			{
				Row.WidgetTag = Config->ResolveWidgetTag();
				Row.ClassName = GetNameSafe(Config->WidgetClass);
			}
		}
		else
		{
			const FScreenWidgetConfig* Config = RawData.IsEmpty()
				? nullptr
				: static_cast<FScreenWidgetConfig*>(RawData[0]);
			if(Config)
			{
				Row.WidgetTag = Config->ResolveWidgetTag();
				Row.ClassName = GetNameSafe(Config->WidgetClass);
				Row.bHasParent = Config->ResolveParentWidgetTag().IsValid();
			}
		}
	}

	Rows.StableSort([](const FConfigRow& Left, const FConfigRow& Right)
	{
		return Left.WidgetTag.ToString() < Right.WidgetTag.ToString();
	});

	const FString RootTag = bInWorldWidget ? TEXT("Widget.World") : TEXT("Widget.Screen");
	IDetailGroup& RootGroup = Category.AddGroup(
		FName(*FString::Printf(TEXT("%s.Root"), *RootTag)),
		FText::FromString(bInWorldWidget ? TEXT("WorldWidgetConfigs") : TEXT("ScreenWidgetConfigs")),
		false,
		false);
	TMap<FString, IDetailGroup*> GroupsByPath;
	GroupsByPath.Add(RootTag, &RootGroup);
	TMap<FString, IDetailGroup*> ChildGroupsByPath;

	for(const FConfigRow& Row : Rows)
	{
		const TSharedRef<IPropertyHandle> ElementHandle = ConfigsHandle->GetElement(Row.Index);
		FString TagSuffix = Row.WidgetTag.IsValid()
			? GetTagSuffix(Row.WidgetTag, RootTag)
			: FString::Printf(TEXT("MissingTag_%d"), Row.Index);
		TArray<FString> Segments;
		TagSuffix.ParseIntoArray(Segments, TEXT("."), true);
		if(Segments.IsEmpty())
		{
			Segments.Add(TagSuffix);
		}

		FString CurrentPath = RootTag;
		IDetailGroup* ParentGroup = &RootGroup;
		for(const FString& Segment : Segments)
		{
			const FString ParentPath = CurrentPath;
			CurrentPath += TEXT(".") + Segment;
			IDetailGroup** ExistingGroup = GroupsByPath.Find(CurrentPath);
			if(ExistingGroup)
			{
				ParentGroup = *ExistingGroup;
				continue;
			}

			IDetailGroup* ConfigContainer = ParentGroup;
			if(ParentPath != RootTag)
			{
				if(IDetailGroup** ExistingChildGroup = ChildGroupsByPath.Find(ParentPath))
				{
					ConfigContainer = *ExistingChildGroup;
				}
				else
				{
					IDetailGroup& ChildWidgetsGroup = ParentGroup->AddGroup(
						FName(*FString::Printf(TEXT("%s.ChildWidgets"), *ParentPath)),
						LOCTEXT("ChildWidgets", "Child Widgets"),
						false);
					ChildGroupsByPath.Add(ParentPath, &ChildWidgetsGroup);
					ConfigContainer = &ChildWidgetsGroup;
				}
			}

			IDetailGroup& ChildGroup = ConfigContainer->AddGroup(
				FName(*FString::Printf(TEXT("%s.%s"), bInWorldWidget ? TEXT("World") : TEXT("Screen"), *CurrentPath)),
				FText::FromString(Segment),
				false);
			GroupsByPath.Add(CurrentPath, &ChildGroup);
			ParentGroup = &ChildGroup;
		}

		const FString DisplayTag = Row.WidgetTag.IsValid()
			? Row.WidgetTag.ToString()
			: TEXT("<Missing Tag>");
		ParentGroup->HeaderRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Segments.Last()))
			.ToolTipText(FText::FromString(DisplayTag))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(360.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Row.ClassName.IsEmpty() ? TEXT("None") : Row.ClassName))
				.ToolTipText(FText::FromString(DisplayTag))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RemoveWidget", "Remove"))
				.OnClicked(this, &FWidgetModuleCustomization::OnRemoveWidget, bInWorldWidget, Row.Index)
			]
		];

		uint32 ChildCount = 0;
		ElementHandle->GetNumChildren(ChildCount);
		for(uint32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			const TSharedPtr<IPropertyHandle> ChildHandle = ElementHandle->GetChildHandle(ChildIndex);
			if(!ChildHandle.IsValid() || !ChildHandle->GetProperty())
			{
				continue;
			}

			const FName PropertyName = ChildHandle->GetProperty()->GetFName();
			if(PropertyName != TEXT("WidgetTagOverride")
				&& (PropertyName != TEXT("SlotTag") || Row.bHasParent))
			{
				ParentGroup->AddPropertyRow(ChildHandle.ToSharedRef());
			}
		}
	}
}

TSharedRef<SWidget> FWidgetModuleCustomization::MakeTagPicker(bool bInWorldWidget)
{
	TSet<FGameplayTag> ExcludedTags;
	const TSharedPtr<IPropertyHandleArray> ConfigsHandle = bInWorldWidget
		? WorldWidgetConfigsHandle
		: UserWidgetConfigsHandle;
	uint32 ConfigCount = 0;
	if(ConfigsHandle.IsValid()
		&& ConfigsHandle->GetNumElements(ConfigCount) == FPropertyAccess::Success)
	{
		for(uint32 Index = 0; Index < ConfigCount; ++Index)
		{
			const TSharedRef<IPropertyHandle> ElementHandle = ConfigsHandle->GetElement(Index);
			TArray<void*> RawData;
			ElementHandle->AccessRawData(RawData);
			if(RawData.IsEmpty() || !RawData[0])
			{
				continue;
			}

			const FGameplayTag WidgetTag = bInWorldWidget
				? static_cast<FWorldWidgetConfig*>(RawData[0])->ResolveWidgetTag()
				: static_cast<FScreenWidgetConfig*>(RawData[0])->ResolveWidgetTag();
			if(WidgetTag.IsValid())
			{
				ExcludedTags.Add(WidgetTag);
			}
		}
	}

	const FGameplayTag RootTag = FGameplayTag::RequestGameplayTag(
		bInWorldWidget ? TEXT("Widget.World") : TEXT("Widget.Screen"),
		false);
	return SNew(SWidgetConfigTagPicker)
		.RootTag(RootTag)
		.ExcludedTags(MoveTemp(ExcludedTags))
		.OnTagPicked(this, &FWidgetModuleCustomization::OnTagPicked, bInWorldWidget);
}

void FWidgetModuleCustomization::OnTagPicked(FGameplayTag InWidgetTag, bool bInWorldWidget)
{
	const FGameplayTag RootTag = FGameplayTag::RequestGameplayTag(
		bInWorldWidget ? TEXT("Widget.World") : TEXT("Widget.Screen"),
		false);
	if(!RootTag.IsValid() || InWidgetTag == RootTag || !InWidgetTag.MatchesTag(RootTag))
	{
		return;
	}

	TSharedPtr<IPropertyHandleArray> ConfigsHandle = bInWorldWidget
		? WorldWidgetConfigsHandle
		: UserWidgetConfigsHandle;
	uint32 ConfigCount = 0;
	if(!ConfigsHandle.IsValid() || ConfigsHandle->GetNumElements(ConfigCount) != FPropertyAccess::Success)
	{
		return;
	}

	for(uint32 Index = 0; Index < ConfigCount; ++Index)
	{
		const TSharedPtr<IPropertyHandle> TagHandle = ConfigsHandle->GetElement(Index)->GetChildHandle(TEXT("WidgetTagOverride"));
		TArray<void*> RawData;
		if(TagHandle.IsValid())
		{
			TagHandle->AccessRawData(RawData);
		}
		if(!RawData.IsEmpty() && RawData[0] && *static_cast<FGameplayTag*>(RawData[0]) == InWidgetTag)
		{
			return;
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("AddWidgetTransaction", "Add Widget Config"));
	ConfigsHandle->AddItem();
	if(ConfigsHandle->GetNumElements(ConfigCount) == FPropertyAccess::Success && ConfigCount > 0)
	{
		SetTagHandleValue(
			ConfigsHandle->GetElement(ConfigCount - 1)->GetChildHandle(TEXT("WidgetTagOverride")),
			InWidgetTag);
	}

	TSharedPtr<SComboButton> AddButton = bInWorldWidget ? WorldAddButton : ScreenAddButton;
	if(AddButton.IsValid())
	{
		AddButton->SetIsOpen(false);
	}
	if(DetailBuilder)
	{
		DetailBuilder->ForceRefreshDetails();
	}
}

FReply FWidgetModuleCustomization::OnRemoveWidget(bool bInWorldWidget, uint32 InIndex)
{
	TSharedPtr<IPropertyHandleArray> ConfigsHandle = bInWorldWidget
		? WorldWidgetConfigsHandle
		: UserWidgetConfigsHandle;
	if(ConfigsHandle.IsValid())
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveWidgetTransaction", "Remove Widget Config"));
		ConfigsHandle->DeleteItem(InIndex);
		if(DetailBuilder)
		{
			DetailBuilder->ForceRefreshDetails();
		}
	}
	return FReply::Handled();
}

FReply FWidgetModuleCustomization::OnValidateWidgets()
{
	if(SelectedObjectsList.IsValidIndex(0))
	{
		if(UWidgetModule* WidgetModule = Cast<UWidgetModule>(SelectedObjectsList[0]))
		{
			WidgetModule->OnGenerate();
		}
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
