// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonButtonGroup.h"

#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/Common/CommonButton.h"

UCommonButtonGroup::UCommonButtonGroup()
{
	bSelectionRequired = false;
	bBroadcastOnDeselected = true;
}

void UCommonButtonGroup::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UCommonButtonGroup::OnDespawn_Implementation(bool bRecovery)
{
	bSelectionRequired = false;
	bBroadcastOnDeselected = true;

	DeselectAllN();
	for(auto Iter : Buttons)
	{
		UObjectPoolModuleStatics::DespawnObject(Iter.Get(), true);
	}
	RemoveAll();
}

void UCommonButtonGroup::OnWidgetAdded(UWidget* NewWidget)
{
	Super::OnWidgetAdded(NewWidget);
}

void UCommonButtonGroup::OnSelectionStateChangedBase(UCommonButtonBase* BaseButton, bool bIsSelected)
{
	const UCommonButton* _BaseButton = Cast<UCommonButton>(BaseButton);

	if (!_BaseButton || _BaseButton->IsStandalone()) return;

	if (bIsSelected)
	{
		SelectedButtonIndex = INDEX_NONE;
		for (int32 ButtonIndex = 0; ButtonIndex < Buttons.Num(); ButtonIndex++)
		{
			UCommonButton* Button = Cast<UCommonButton>(GetButtonBaseAtIndex(ButtonIndex));
			if (Button && !Button->IsStandalone())
			{
				if (Button == BaseButton)
				{
					SelectedButtonIndex = ButtonIndex;
				}
				else if (Button->GetSelected())
				{
					// Make sure any other selected buttons are deselected
					Button->SetSelectedInternal(false);
				}
			}
		}

		NativeOnSelectedButtonBaseChanged.Broadcast(BaseButton, SelectedButtonIndex);
		OnSelectedButtonBaseChanged.Broadcast(BaseButton, SelectedButtonIndex);
	}
	else
	{
		for ( auto& WeakButton : Buttons )
		{
			if ( WeakButton.IsValid() && WeakButton->GetSelected() )
			{
				return; // early out here and prevent the delegate below from being triggered
			}
		}

		SelectedButtonIndex = INDEX_NONE;

		NativeOnSelectionCleared.Broadcast();
		OnSelectionCleared.Broadcast();
	}
}

void UCommonButtonGroup::DeselectAllN()
{
	if(bSelectionRequired) return;

	for (auto& Iter : Buttons)
	{
		UCommonButton* Button = Cast<UCommonButton>(Iter.Get());
		if (Button && Button->GetSelected() && !Button->IsStandalone())
		{
			Button->SetSelectedInternal(false);
		}
	}

	if (SelectedButtonIndex != INDEX_NONE)
	{
		SelectedButtonIndex = INDEX_NONE;
		OnSelectionCleared.Broadcast();
	}
}
