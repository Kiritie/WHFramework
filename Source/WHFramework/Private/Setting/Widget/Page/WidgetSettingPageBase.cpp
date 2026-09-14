#include "Setting/Widget/Page/WidgetSettingPageBase.h"

#include "Blueprint/WidgetTree.h"
#include "CommonListView.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Setting/SettingEntry.h"
#include "Setting/SettingCategoryEntry.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"
#include "Setting/Widget/Entry/WidgetSettingCategoryEntry.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/WidgetModule.h"

UWidgetSettingPageBase::UWidgetSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	InputConfig = EWidgetInputConfig::Menu;
	bWidgetActivatable = true;
}

void UWidgetSettingPageBase::OnCreate(const FParameter& InParam)
{
	Super::OnCreate(InParam);

	if(!SettingList && WidgetTree)
	{
		SettingList = WidgetTree->ConstructWidget<UCommonListView>(
			UCommonListView::StaticClass(),
			TEXT("SettingList"));
		if(!WidgetTree->RootWidget)
		{
			WidgetTree->RootWidget = SettingList;
		}
		else if(UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget))
		{
			RootPanel->AddChild(SettingList);
		}
		else
		{
			UWidget* PreviousRoot = WidgetTree->RootWidget;
			UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(
				UOverlay::StaticClass(),
				TEXT("SettingRoot"));
			WidgetTree->RootWidget = RootOverlay;
			RootOverlay->AddChildToOverlay(PreviousRoot);
			RootOverlay->AddChildToOverlay(SettingList);
		}
	}
	if(!EmptyState && WidgetTree)
	{
		UTextBlock* EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EmptyState"));
		EmptyText->SetText(NSLOCTEXT("WH.Setting", "EmptyParameterPage", "No configurable parameters."));
		EmptyState = EmptyText;
		if(UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget))
		{
			RootPanel->AddChild(EmptyState);
		}
		else if(WidgetTree->RootWidget)
		{
			UWidget* PreviousRoot = WidgetTree->RootWidget;
			UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SettingEmptyRoot"));
			WidgetTree->RootWidget = RootOverlay;
			RootOverlay->AddChildToOverlay(PreviousRoot);
			RootOverlay->AddChildToOverlay(EmptyState);
		}
	}

	if(SettingList)
	{
		SettingList->OnGetEntryClassForItem().BindUObject(this, &ThisClass::ResolveEntryClass);
		USettingModule::Get().OnSettingValueChanged.AddDynamic(this, &ThisClass::OnSettingValueChanged);
		RefreshEntries();
	}
}

void UWidgetSettingPageBase::OnReset(bool bForce)
{
	Super::OnReset(bForce);
	if(PageName.IsNone())
	{
		return;
	}
	for(USettingEntry* Entry : USettingModule::Get().GetSettingEntriesByPage(PageName))
	{
		if(Entry)
		{
			Entry->SetPendingValue(Entry->GetDefaultValue());
		}
	}
	RefreshEntries();
}

void UWidgetSettingPageBase::OnDestroy(EObjectDespawnMode InMode)
{
	USettingModule::Get().OnSettingValueChanged.RemoveDynamic(this, &ThisClass::OnSettingValueChanged);
	Super::OnDestroy(InMode);
}

void UWidgetSettingPageBase::NativeOnActivated()
{
	Super::NativeOnActivated();
}

void UWidgetSettingPageBase::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
}

void UWidgetSettingPageBase::OnApply()
{
	K2_OnApply();
}

void UWidgetSettingPageBase::OnSettingValueChanged(FSettingId InSettingId)
{
	if(!SettingList)
	{
		return;
	}
	for(UUserWidget* EntryWidget : SettingList->GetDisplayedEntryWidgets())
	{
		UWidgetSettingEntryBase* SettingEntryWidget = Cast<UWidgetSettingEntryBase>(EntryWidget);
		if(SettingEntryWidget && SettingEntryWidget->GetSettingEntry()
			&& SettingEntryWidget->GetSettingEntry()->GetDefinition().SettingId != InSettingId)
		{
			SettingEntryWidget->RefreshFromModel();
		}
	}
}

bool UWidgetSettingPageBase::CanApply_Implementation() const
{
	return !PageName.IsNone() && USettingModule::Get().CanApply();
}

bool UWidgetSettingPageBase::CanReset_Implementation() const
{
	if(PageName.IsNone())
	{
		return false;
	}
	for(USettingEntry* Entry : USettingModule::Get().GetSettingEntriesByPage(PageName))
	{
		if(Entry && Entry->CanReset())
		{
			return true;
		}
	}
	return false;
}

void UWidgetSettingPageBase::Apply()
{
	OnApply();
}

void UWidgetSettingPageBase::SetPage(FName InPage)
{
	PageName = InPage;
	RefreshEntries();
}

void UWidgetSettingPageBase::SetPageDefinition(const FSettingPageDefinition& InDefinition)
{
	PageName = InDefinition.Page;
	Title = InDefinition.DisplayName.IsEmpty() ? FText::FromName(InDefinition.Page) : InDefinition.DisplayName;
	RefreshEntries();
}

void UWidgetSettingPageBase::RefreshEntries()
{
	if(!SettingList || PageName.IsNone())
	{
		return;
	}

	CategoryEntries.Reset();
	TArray<USettingEntry*> Entries = USettingModule::Get().GetSettingEntriesByPage(PageName);
	Entries.Sort([](const USettingEntry& A, const USettingEntry& B)
	{
		const FSettingDefinition& DefinitionA = A.GetDefinition();
		const FSettingDefinition& DefinitionB = B.GetDefinition();
		if(DefinitionA.CategoryOrder != DefinitionB.CategoryOrder)
		{
			return DefinitionA.CategoryOrder < DefinitionB.CategoryOrder;
		}
		if(DefinitionA.Category != DefinitionB.Category)
		{
			return DefinitionA.Category.LexicalLess(DefinitionB.Category);
		}
		return DefinitionA.Order < DefinitionB.Order;
	});

	TArray<UObject*> ListItems;
	FName LastCategory = NAME_None;
	for(USettingEntry* Entry : Entries)
	{
		if(!Entry)
		{
			continue;
		}

		const FSettingDefinition& Definition = Entry->GetDefinition();
		if(Definition.Category != LastCategory)
		{
			LastCategory = Definition.Category;
			if(!LastCategory.IsNone())
			{
				USettingCategoryEntry* CategoryEntry = NewObject<USettingCategoryEntry>(this);
				CategoryEntry->Initialize(LastCategory, FText::FromName(LastCategory), Definition.CategoryOrder);
				CategoryEntries.Add(CategoryEntry);
				ListItems.Add(CategoryEntry);
			}
		}
		ListItems.Add(Entry);
	}
	const bool bEmpty = ListItems.IsEmpty();
	if(EmptyState)
	{
		EmptyState->SetVisibility(bEmpty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	SettingList->SetVisibility(bEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	SettingList->SetListItems(ListItems);
}

TSubclassOf<UUserWidget> UWidgetSettingPageBase::ResolveEntryClass(UObject* InItem) const
{
	const UWidgetTheme* Theme = UWidgetModule::Get().GetDefaultWidgetTheme();
	if(!Theme)
	{
		return nullptr;
	}
	if(InItem && InItem->IsA<USettingCategoryEntry>())
	{
		return Theme->GetSettingCategoryEntryClass();
	}

	const USettingEntry* Entry = Cast<USettingEntry>(InItem);
	const TSubclassOf<UWidgetSettingEntryBase> EntryClass = Entry && Theme
		? Theme->FindSettingRendererClass(Entry->GetDefinition().Renderer)
		: nullptr;
	return EntryClass.Get();
}
