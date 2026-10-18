#include "Setting/Widget/Page/WidgetSettingPageBase.h"

#include "Blueprint/WidgetTree.h"
#include "CommonListView.h"
#include "Setting/SettingEntry.h"
#include "Setting/SettingCategoryEntry.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"
#include "Setting/Widget/Entry/WidgetSettingCategoryEntry.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeResolver.h"

UWidgetSettingPageBase::UWidgetSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetInputMode = EWidgetInputMode::UIOnly;
	bWidgetActivatable = true;
}

void UWidgetSettingPageBase::OnCreate(const FParameter& InParam)
{
	if (!ThemeChangedHandle.IsValid())
	{
		ThemeChangedHandle = FWidgetThemeResolver::OnThemeChanged().AddUObject(this, &ThisClass::HandleThemeChanged);
	}
	Super::OnCreate(InParam);

	if(SettingList)
	{
		SettingList->OnGetEntryClassForItem().BindUObject(this, &ThisClass::ResolveEntryClass);
		SettingList->OnEntryWidgetGenerated().RemoveAll(this);
		SettingList->OnEntryWidgetGenerated().AddUObject(this, &ThisClass::HandleEntryWidgetGenerated);
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
	FWidgetThemeResolver::OnThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
	if (SettingList)
	{
		SettingList->OnEntryWidgetGenerated().RemoveAll(this);
	}
	PendingFocusItem.Reset();
	PendingFocusWidgetName = NAME_None;
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
		if(SettingEntryWidget && SettingEntryWidget->GetSettingEntry())
		{
			if (SettingEntryWidget->GetSettingEntry()->GetDefinition().SettingId == InSettingId)
			{
				SettingEntryWidget->RefreshFromModel();
			}
			else
			{
				SettingEntryWidget->RefreshModelState();
			}
		}
	}
}

void UWidgetSettingPageBase::HandleThemeChanged()
{
	if (!SettingList)
	{
		return;
	}
	for (UObject* Item : SettingList->GetListItems())
	{
		UUserWidget* EntryWidget = SettingList->GetEntryWidgetFromItem(Item);
		if (EntryWidget && EntryWidget->GetClass() != ResolveEntryClass(Item).Get())
		{
			const float ScrollOffset = SettingList->GetScrollOffset();
			PendingFocusItem.Reset();
			PendingFocusWidgetName = NAME_None;
			if (APlayerController* Player = GetOwningPlayer())
			{
				for (UUserWidget* DisplayedEntry : SettingList->GetDisplayedEntryWidgets())
				{
					if (!DisplayedEntry->HasUserFocus(Player) && !DisplayedEntry->HasUserFocusedDescendants(Player))
					{
						continue;
					}
					PendingFocusItem = SettingList->GetListObjectFromEntry(*DisplayedEntry);
					if (DisplayedEntry->WidgetTree)
					{
						DisplayedEntry->WidgetTree->ForEachWidgetAndDescendants([this, Player](UWidget* Widget)
						{
							if (Widget->HasUserFocus(Player))
							{
								PendingFocusWidgetName = Widget->GetFName();
							}
						});
					}
					break;
				}
			}
			SettingList->RegenerateAllEntries();
			SettingList->SetScrollOffset(ScrollOffset);
			return;
		}
	}
}

void UWidgetSettingPageBase::HandleEntryWidgetGenerated(UUserWidget& InEntryWidget)
{
	if (!PendingFocusItem.IsValid() || SettingList->GetListObjectFromEntry(InEntryWidget) != PendingFocusItem.Get())
	{
		return;
	}
	UWidget* FocusTarget = &InEntryWidget;
	if (!PendingFocusWidgetName.IsNone() && InEntryWidget.WidgetTree)
	{
		InEntryWidget.WidgetTree->ForEachWidgetAndDescendants([this, &FocusTarget](UWidget* Widget)
		{
			if (Widget->GetFName() == PendingFocusWidgetName)
			{
				FocusTarget = Widget;
			}
		});
	}
	PendingFocusItem.Reset();
	PendingFocusWidgetName = NAME_None;
	if (APlayerController* Player = GetOwningPlayer())
	{
		FocusTarget->SetUserFocus(Player);
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
	PendingFocusItem.Reset();
	PendingFocusWidgetName = NAME_None;
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
    const UWidgetTheme* Theme = FWidgetThemeResolver::ResolveTheme(this);
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
