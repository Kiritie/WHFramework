// Fill out your copyright notice in the Description page of Project Settings.

#include "Input/Widget/WidgetKeyTipsItemBase.h"

#include "CommonInputBaseTypes.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Input/InputModuleStatics.h"
#include "Widget/Common/CommonTextBlockN.h"

UWidgetKeyTipsItemBase::UWidgetKeyTipsItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetParams.Add(FString());
	WidgetParams.Add(FText());

	Box_KeyIcon = nullptr;
	Txt_KeyCode = nullptr;
	Txt_KeyName = nullptr;
}

void UWidgetKeyTipsItemBase::OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);
}

void UWidgetKeyTipsItemBase::OnInitialize(const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InParams);
}

void UWidgetKeyTipsItemBase::OnRefresh()
{
	FString KeyCode;
	FString KeyName;

	Box_KeyIcon->ClearChildren();
	
	if(WidgetParams.IsValidIndex(0))
	{
		KeyCode.Append(TEXT("["));
		
		const FString KeyMappingName = WidgetParams[0].GetStringValue();
		TArray<FString> KeyMappingNames;
		KeyMappingName.ParseIntoArray(KeyMappingNames, TEXT(","));

		#define EXPRESSION1(Key, Code) \
		FSlateBrush ImageBrush; \
		const UCommonInputPlatformSettings* Settings = UPlatformSettingsManager::Get().GetSettingsForPlatform<UCommonInputPlatformSettings>(); \
		if(Settings->TryGetInputBrush(ImageBrush, Key, ECommonInputType::MouseAndKeyboard, FName("XSX"))) \
		{ \
			ImageBrush.ImageSize = FVector2D(25.f); \
			UImage* Image = NewObject<UImage>(GetWorld()); \
			Image->SetBrush(ImageBrush); \
			if(const auto ImageSlot = Box_KeyIcon->AddChildToHorizontalBox(Image)) \
			{ \
				ImageSlot->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f)); \
				ImageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); \
			} \
		} \
		else \
		{ \
			KeyCode.Append(Code); \
		}

		for(auto& Iter1 : KeyMappingNames)
		{
			auto KeyMappings = UInputModuleStatics::GetPlayerKeyMappingsByName(*Iter1);
			if(KeyMappings.Num() > 0)
			{
				for(auto& Iter2 : KeyMappings)
				{
					EXPRESSION1(Iter2.GetCurrentKey(), FString::Printf(TEXT("%s/"), *Iter2.GetCurrentKey().GetDisplayName(false).ToString()))
					KeyName = Iter2.GetDisplayName().ToString();
				}
			}
			else
			{
				EXPRESSION1(FKey(*Iter1), Iter1)
				KeyName = Iter1;
			}
			KeyCode.RemoveFromEnd(TEXT("/"));
			KeyCode.Append(TEXT("."));
		}
		KeyCode.RemoveFromEnd(TEXT("."));
		KeyCode.Append(TEXT("]"));
	}
	if(WidgetParams.IsValidIndex(1))
	{
		KeyName = !WidgetParams[1].GetTextValue().IsEmpty() ? WidgetParams[1].GetTextValue().ToString() : KeyName;
	}

	Box_KeyIcon->SetVisibility(Box_KeyIcon->GetChildrenCount() > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	
	Txt_KeyCode->SetText(FText::FromString(KeyCode));
	Txt_KeyCode->SetVisibility(Box_KeyIcon->GetChildrenCount() == 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	
	Txt_KeyName->SetText(FText::FromString(KeyName));

	Super::OnRefresh();
}

void UWidgetKeyTipsItemBase::OnDestroy()
{
	Super::OnDestroy();
}
