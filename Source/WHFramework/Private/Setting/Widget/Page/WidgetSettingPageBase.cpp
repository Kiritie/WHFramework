#include "Setting/Widget/Page/WidgetSettingPageBase.h"

#include "Blueprint/WidgetTree.h"
#include "CommonListView.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Setting/SettingEntry.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/WidgetModule.h"

UWidgetSettingPageBase::UWidgetSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	InputConfig = EWidgetInputConfig::Menu;
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
	if(!PageName.IsNone())
	{
		USettingModule::Get().ApplyEditSession();
	}
	K2_OnApply();
}

void UWidgetSettingPageBase::OnSettingValueChanged(FSettingId InSettingId)
{
	if(SettingList)
	{
		SettingList->RequestRefresh();
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

	TArray<UObject*> ListItems;
	for(USettingEntry* Entry : USettingModule::Get().GetSettingEntriesByPage(PageName))
	{
		ListItems.Add(Entry);
	}
	SettingList->SetListItems(ListItems);
}

TSubclassOf<UUserWidget> UWidgetSettingPageBase::ResolveEntryClass(UObject* InItem) const
{
	const USettingEntry* Entry = Cast<USettingEntry>(InItem);
	const UWidgetTheme* Theme = UWidgetModule::Get().GetDefaultWidgetTheme();
	const TSubclassOf<UWidgetSettingEntryBase> EntryClass = Entry && Theme
		? Theme->FindSettingRendererClass(Entry->GetDefinition().Renderer)
		: nullptr;
	return EntryClass.Get();
}
