// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Widget/EventHandle_OpenUserWidget.h"

#include "Widget/Screen/UMG/UserWidgetBase.h"

UEventHandle_OpenUserWidget::UEventHandle_OpenUserWidget()
{
	WidgetClass = nullptr;
	WidgetParams = TArray<FParameter>();
	bInstant = false;
}

void UEventHandle_OpenUserWidget::OnDespawn_Implementation(bool bRecovery)
{
	WidgetClass = nullptr;
	WidgetParams = TArray<FParameter>();
	bInstant = false;
}

void UEventHandle_OpenUserWidget::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		WidgetClass = InParams[0].GetClassValue();
	}
	if(InParams.IsValidIndex(1))
	{
		WidgetParams = InParams[1].GetPointerValueRef<TArray<FParameter>>();
	}
	if(InParams.IsValidIndex(2))
	{
		bInstant = InParams[2].GetBooleanValue();
	}
}

TArray<FParameter> UEventHandle_OpenUserWidget::Pack_Implementation()
{
	return { WidgetClass.Get(), &WidgetParams, bInstant };
}

#if WITH_EDITOR
void UEventHandle_OpenUserWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const FName PropertyName = Property->GetFName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UEventHandle_OpenUserWidget, WidgetClass))
		{
			if(WidgetClass)
			{
				WidgetParams = WidgetClass->GetDefaultObject<UUserWidgetBase>()->GetWidgetParams();
			}
			else
			{
				WidgetParams.Empty();
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
