// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Widget/EventHandle_CloseUserWidget.h"

#include "Widget/WidgetModule.h"
#include "Widget/Screen/UserWidgetBase.h"

UEventHandle_CloseUserWidget::UEventHandle_CloseUserWidget()
{
	WidgetClass = nullptr;
	WidgetName = NAME_None;
	bInstant = false;
}

void UEventHandle_CloseUserWidget::OnDespawn_Implementation(bool bRecovery)
{
	WidgetClass = nullptr;
	WidgetName = NAME_None;
	bInstant = false;
}

void UEventHandle_CloseUserWidget::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		if(InParams[0].GetParameterType() == EParameterType::Class)
		{
			WidgetClass = InParams[0].GetClassValue();
			WidgetName = WidgetClass->GetDefaultObject<UUserWidgetBase>()->GetWidgetName();
		}
		else
		{
			WidgetName = InParams[0].GetNameValue();
		}
	}
	if(InParams.IsValidIndex(1))
	{
		bInstant = InParams[1].GetBooleanValue();
	}
}

TArray<FParameter> UEventHandle_CloseUserWidget::Pack_Implementation()
{
	return { WidgetClass ? FParameter(WidgetClass) : FParameter(WidgetName), bInstant };
}

#if WITH_EDITOR
void UEventHandle_CloseUserWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const FName PropertyName = Property->GetFName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UEventHandle_CloseUserWidget, WidgetClass))
		{
			if(WidgetClass)
			{
				WidgetName = WidgetClass->GetDefaultObject<UUserWidgetBase>()->GetWidgetName();
			}
			else
			{
				WidgetName = NAME_None;
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
