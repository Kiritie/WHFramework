// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonButtonGroup.h"

#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/Common/CommonButton.h"

UCommonButtonGroup::UCommonButtonGroup()
{
	bBroadcastOnDeselected = true;
}

void UCommonButtonGroup::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		bBroadcastOnDeselected = InParams[0].GetBooleanValue();
	}
}

void UCommonButtonGroup::OnDespawn_Implementation(bool bRecovery)
{
	bBroadcastOnDeselected = true;

	ForEach([](UCommonButtonBase& InButton, int32 InIndex)
	{
		UObjectPoolModuleStatics::DespawnObject(&InButton, true);
	});
	RemoveAll();
}

void UCommonButtonGroup::OnWidgetAdded(UWidget* NewWidget)
{
	if(const UCommonButton* CommonButton = Cast<UCommonButton>(NewWidget))
	{
		if(!CommonButton->IsSingle())
		{
			Super::OnWidgetAdded(NewWidget);
		}
	}
}

void UCommonButtonGroup::OnSelectionStateChangedBase(UCommonButtonBase* BaseButton, bool bIsSelected)
{
	if (bIsSelected)
	{
		SelectedButtonIndex = INDEX_NONE;
		for (int32 ButtonIndex = 0; ButtonIndex < Buttons.Num(); ButtonIndex++)
		{
			UCommonButton* Button = Cast<UCommonButton>(GetButtonBaseAtIndex(ButtonIndex));
			if (Button)
			{
				if (Button == BaseButton)
				{
					SelectedButtonIndex = ButtonIndex;
				}
				else if (Button->GetSelected())
				{
					// Make sure any other selected buttons are deselected
					Button->SetSelectedInternal(false, false, bBroadcastOnDeselected);
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
