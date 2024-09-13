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
	Box_KeyIcon = nullptr;
	Border_KeyCode = nullptr;
	Txt_KeyCode = nullptr;
	Txt_DisplayName = nullptr;
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
	if(Settings->TryGetInputBrush(ImageBrush, Key, ECommonInputType::MouseAndKeyboard, FName("XSX")) && ImageBrush.GetResourceObject()) \
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
	else if(!Code.StartsWith(TEXT("None"))) \
	{ \
		KeyCode.Append(Code); \
	}

	ITER_ARRAY(KeyMappingNames, Iter1,
		auto KeyMappings = UInputModuleStatics::GetPlayerKeyMappingsByName(*Iter1);
		if(KeyMappings.Num() > 0)
		{
			ITER_ARRAY_WITHINDEX(KeyMappings, i, Iter2,
				EXPRESSION1(Iter2.GetCurrentKey(), FString::Printf(TEXT("%s%s"), *Iter2.GetCurrentKey().GetDisplayName(false).ToString(), i == KeyMappings.Num() - 1 ? TEXT(".") : TEXT("/")))
			)
		}
		else
		{
			EXPRESSION1(FKey(*Iter1), FString::Printf(TEXT("%s."), *FKey(*Iter1).GetDisplayName(false).ToString()))
		}
	)
	KeyCode.RemoveFromEnd(TEXT("."));
	KeyCode.RemoveFromEnd(TEXT("/"));

	if(Box_KeyIcon->GetChildrenCount() > 1)
	{
		Cast<UHorizontalBoxSlot>(Box_KeyIcon->GetChildAt(Box_KeyIcon->GetChildrenCount() - 1)->Slot)->SetPadding(FMargin(0.f));
	}

	Txt_KeyCode->SetText(FText::FromString(KeyCode));

	Box_KeyIcon->SetVisibility(Box_KeyIcon->GetChildrenCount() > 1 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	Border_KeyCode->SetVisibility(!KeyCode.IsEmpty() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	Super::OnRefresh();
}

void UWidgetKeyTipsItemBase::OnDestroy(bool bRecovery)
{
	Super::OnDestroy(bRecovery);
}

void UWidgetKeyTipsItemBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	if(Txt_DisplayName)
	{
		Txt_DisplayName->SetText(KeyDisplayName);
		Txt_DisplayName->SetVisibility(KeyDisplayName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}
