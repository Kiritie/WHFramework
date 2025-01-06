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

	for(auto Iter : Buttons)
	{
		UObjectPoolModuleStatics::DespawnObject(Iter.Get(), true);
	}
	RemoveAll();
}

void UCommonButtonGroup::OnWidgetAdded(UWidget* NewWidget)
{
	if(UCommonButton* CommonButton = Cast<UCommonButton>(NewWidget))
	{
		CommonButton->OnSelectedChangedBase.AddUniqueDynamic(this, &UCommonButtonGroup::OnSelectionStateChangedBase);
		CommonButton->OnButtonBaseClicked.AddUniqueDynamic(this, &UCommonButtonGroup::OnHandleButtonBaseClicked);
		CommonButton->OnButtonBaseDoubleClicked.AddUniqueDynamic(this, &UCommonButtonGroup::OnHandleButtonBaseDoubleClicked);
		CommonButton->OnButtonBaseHovered.AddUniqueDynamic(this, &UCommonButtonGroup::OnButtonBaseHovered);
		CommonButton->OnButtonBaseUnhovered.AddUniqueDynamic(this, &UCommonButtonGroup::OnButtonBaseUnhovered);

		Buttons.Emplace(CommonButton);

		if(CommonButton->bStandalone)
		{
			CommonButton->bToggleable = true;
		}
		else
		{
			CommonButton->bToggleable = !bSelectionRequired;
			
			if(CommonButton->GetSelected())
			{
				for (auto& Iter : Buttons)
				{
					UCommonButton* Button = Cast<UCommonButton>(Iter.Get());
					if (Button && Button->GetSelected() && !Button->IsStandalone() && Button != CommonButton)
					{
						Button->SetSelectedInternal(false);
					}
				}
				SelectedButtonIndex = Buttons.Find(CommonButton);
			}
			else if (bSelectionRequired && GetSelectedButtonIndex() == INDEX_NONE)
			{
				CommonButton->SetSelectedInternal(true, false);
			}
		}
	}
}

void UCommonButtonGroup::OnSelectionStateChangedBase(UCommonButtonBase* BaseButton, bool bIsSelected)
{
	const UCommonButton* CommonButton = Cast<UCommonButton>(BaseButton);

	if (!CommonButton || CommonButton->IsStandalone()) return;

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

void UCommonButtonGroup::SetSelectionRequiredN(bool bRequireSelection)
{
	if (bSelectionRequired != bRequireSelection)
	{
		bSelectionRequired = bRequireSelection;
		
		if (bSelectionRequired && 
			GetButtonBaseAtIndexN(0) && !GetButtonBaseAtIndexN(0)->bStandalone)
		{
			// Selection is now required and nothing is selected, so select the first button
			SelectButtonAtIndex(0);
		}
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
			Button->SetSelectedInternal(false, false);
		}
	}

	if (SelectedButtonIndex != INDEX_NONE)
	{
		SelectedButtonIndex = INDEX_NONE;
		OnSelectionCleared.Broadcast();
	}
}

UCommonButton* UCommonButtonGroup::GetButtonBaseAtIndexN(int32 Index) const
{
	if (Buttons.IsValidIndex(Index))
	{
		return Cast<UCommonButton>(Buttons[Index].Get());
	}
	return nullptr;
}
