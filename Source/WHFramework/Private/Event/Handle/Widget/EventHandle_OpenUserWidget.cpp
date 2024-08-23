// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Widget/EventHandle_OpenUserWidget.h"

#include "Widget/WidgetModuleStatics.h"
#include "Widget/Screen/UserWidgetBase.h"

UEventHandle_OpenUserWidget::UEventHandle_OpenUserWidget()
{
	WidgetClass = nullptr;
	WidgetName = NAME_None;
	WidgetParams = TArray<FParameter>();
	bInstant = false;
	bForce = false;
}

void UEventHandle_OpenUserWidget::OnDespawn_Implementation(bool bRecovery)
{
	WidgetClass = nullptr;
	WidgetName = NAME_None;
	WidgetParams = TArray<FParameter>();
	bInstant = false;
	bForce = false;
}

void UEventHandle_OpenUserWidget::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		if(InParams[0].GetParameterType() == EParameterType::Class)
		{
			WidgetClass = InParams[0];
			WidgetName = WidgetClass->GetDefaultObject<UUserWidgetBase>()->GetWidgetName();
		}
		else
		{
			WidgetName = InParams[0];
		}
	}
	if(InParams.IsValidIndex(1))
	{
		WidgetParams = InParams[1].GetPointerValueRef<TArray<FParameter>>();
	}
	if(InParams.IsValidIndex(2))
	{
		bInstant = InParams[2];
	}
	if(InParams.IsValidIndex(3))
	{
		bForce = InParams[3];
	}
}

TArray<FParameter> UEventHandle_OpenUserWidget::Pack_Implementation()
{
	return { WidgetClass ? FParameter(WidgetClass) : FParameter(WidgetName), &WidgetParams, bInstant, bForce };
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
				const auto DefaultObject = WidgetClass->GetDefaultObject<UUserWidgetBase>();
				WidgetName = DefaultObject->GetWidgetName();
				WidgetParams = DefaultObject->GetWidgetParams();
			}
			else
			{
				WidgetName = NAME_None;
				WidgetParams.Empty();
			}
		}
		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UEventHandle_OpenUserWidget, WidgetName))
		{
			if(const auto _WidgetClass = UWidgetModule::Get(true).GetUserWidgetClassByName(WidgetName))
			{
				WidgetParams = _WidgetClass->GetDefaultObject<UUserWidgetBase>()->GetWidgetParams();
			}
			else
			{
				WidgetParams.Empty();
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UEventHandle_OpenUserWidget::OnEditorRefresh()
{
	Super::OnEditorRefresh();

	TArray<FParameter> _WdigetParams;
	
	if(WidgetClass)
	{
		_WdigetParams = WidgetClass->GetDefaultObject<UUserWidgetBase>()->GetWidgetParams();
	}
	else if(!WidgetName.IsNone())
	{
		const auto _WidgetClass = UWidgetModule::Get(true).GetUserWidgetClassByName(WidgetName);
		_WdigetParams = _WidgetClass->GetDefaultObject<UUserWidgetBase>()->GetWidgetParams();
	}

	for(int32 i = 0; i < _WdigetParams.Num(); i++)
	{
		if(WidgetParams.IsValidIndex(i))
		{
			WidgetParams[i].SetDescription(_WdigetParams[i].GetDescription());
		}
		else
		{
			WidgetParams.EmplaceAt(i, _WdigetParams[i]);
		}
	}
}
#endif
