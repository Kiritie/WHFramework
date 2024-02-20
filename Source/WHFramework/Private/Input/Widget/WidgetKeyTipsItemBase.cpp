// Fill out your copyright notice in the Description page of Project Settings.

#include "Input/Widget/WidgetKeyTipsItemBase.h"

#include "CommonInputBaseTypes.h"
#include "Components/Border.h"
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
	Border_KeyCode = nullptr;
	Txt_KeyCode = nullptr;
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

	const UImage* Img_KeyIcon = Cast<UImage>(Box_KeyIcon->GetChildAt(0));

	while(Box_KeyIcon->GetChildrenCount() > 1)
	{
		Box_KeyIcon->RemoveChildAt(1);
	}
	
	TArray<FString> KeyMappingNames;
	KeyMappingName.ParseIntoArray(KeyMappingNames, TEXT(","));
	
	#define EXPRESSION1(Key, Code) \
	FSlateBrush ImageBrush; \
	const UCommonInputPlatformSettings* Settings = UPlatformSettingsManager::Get().GetSettingsForPlatform<UCommonInputPlatformSettings>(); \
	if(Settings->TryGetInputBrush(ImageBrush, Key, ECommonInputType::MouseAndKeyboard, FName("XSX"))) \
	{ \
		ImageBrush.ImageSize = Img_KeyIcon->GetBrush().ImageSize; \
		UImage* Image = NewObject<UImage>(GetWorld()); \
		Image->SetBrush(ImageBrush); \
		if(const auto ImageSlot = Box_KeyIcon->AddChildToHorizontalBox(Image)) \
		{ \
			ImageSlot->SetPadding(Cast<UHorizontalBoxSlot>(Img_KeyIcon->Slot)->GetPadding()); \
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
			}
		}
		else
		{
			EXPRESSION1(FKey(*Iter1), Iter1)
		}
		KeyCode.RemoveFromEnd(TEXT("/"));
		KeyCode.Append(TEXT("."));
	}
	KeyCode.RemoveFromEnd(TEXT("."));

	if(Box_KeyIcon->GetChildrenCount() > 1)
	{
		Cast<UHorizontalBoxSlot>(Box_KeyIcon->GetChildAt(Box_KeyIcon->GetChildrenCount() - 1)->Slot)->SetPadding(FMargin(0.f));
	}

	Txt_KeyCode->SetText(FText::FromString(KeyCode));

	Box_KeyIcon->SetVisibility(Box_KeyIcon->GetChildrenCount() > 1 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	Border_KeyCode->SetVisibility(Box_KeyIcon->GetChildrenCount() == 1 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	Super::OnRefresh();
}

void UWidgetKeyTipsItemBase::OnDestroy(bool bRecovery)
{
	Super::OnDestroy(bRecovery);
}
