// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonTabList.h"

#include "Blueprint/WidgetTree.h"
#include "Components/StackBox.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/Common/CommonScroll.h"

UCommonTabList::UCommonTabList(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	
}

void UCommonTabList::OnSpawn_Implementation(const FParameter& InParam)
{
	
}

void UCommonTabList::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	SetListeningForInput(false);
	SetSelectionRequired(false);
	RemoveAllTabs();
	SetSelectionRequired(true);
	SetLinkedSwitcher(nullptr);
	OnTabSelected.Clear();
	OnTabButtonCreation.Clear();
	OnTabButtonRemoval.Clear();
	OnTabListRebuilt.Clear();
	const UCommonTabList* Defaults = CastChecked<UCommonTabList>(GetArchetype());
	SetButtonStyleTag(Defaults->ButtonStyleTag);
	SetOrientation(Defaults->Orientation);
}

TSharedRef<SWidget> UCommonTabList::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		NativeScroll = WidgetTree->ConstructWidget<UCommonScroll>(UCommonScroll::StaticClass(), TEXT("TabScroll"));
		TabContent = WidgetTree->ConstructWidget<UStackBox>(UStackBox::StaticClass(), TEXT("TabContent"));
		NativeScroll->AddChild(TabContent);
		WidgetTree->RootWidget = NativeScroll;
		SetOrientation(Orientation);
	}
	return Super::RebuildWidget();
}

void UCommonTabList::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	SetOrientation(Orientation);
}

void UCommonTabList::SetOrientation(EOrientation InOrientation)
{
	Orientation = InOrientation;
	if (NativeScroll)
	{
		NativeScroll->SetOrientation(Orientation);
		if (UStackBox* Stack = Cast<UStackBox>(TabContent))
		{
			Stack->SetOrientation(Orientation);
		}
	}
}

void UCommonTabList::HandleTabCreation_Implementation(FName TabNameID, UCommonButtonBase* TabButton)
{
	Super::HandleTabCreation_Implementation(TabNameID, TabButton);
	if (TabContent && TabButton)
	{
		TabContent->AddChild(TabButton);
	}
	if (UCommonButton* Button = Cast<UCommonButton>(TabButton))
	{
		Button->SetInheritedStyleTag(ButtonStyleTag);
	}
}

void UCommonTabList::HandleTabRemoval_Implementation(FName TabNameID, UCommonButtonBase* TabButton)
{
	if (TabContent && TabButton && TabButton->GetParent() == TabContent)
	{
		TabContent->RemoveChild(TabButton);
	}
	Super::HandleTabRemoval_Implementation(TabNameID, TabButton);
}

void UCommonTabList::SetButtonStyleTag(FGameplayTag InStyleTag)
{
	ButtonStyleTag = InStyleTag;
	for (const auto& Pair : GetRegisteredTabsByID())
	{
		if (UCommonButton* Button = Cast<UCommonButton>(Pair.Value.TabButton))
		{
			Button->SetInheritedStyleTag(ButtonStyleTag);
		}
	}
}
