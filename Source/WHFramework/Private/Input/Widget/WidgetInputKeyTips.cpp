// Fill out your copyright notice in the Description page of Project Settings.

#include "Input/Widget/WidgetInputKeyTips.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "CoreGlobals.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Input/InputModule.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Style/CommonBrushStyle.h"

UWidgetInputKeyTips::UWidgetInputKeyTips(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Box_KeyIcon = nullptr;
	Border_KeyCode = nullptr;
	Txt_KeyCode = nullptr;
	Txt_DisplayName = nullptr;
	HorizontalBox = nullptr;
	Box_KeyCode = nullptr;
	KeyCodeSize = nullptr;
	KeyTextStyleTag = WidgetStyleTags::CommonDefault(EWidgetStyleKind::Text);
	NameTextStyleTag = WidgetStyleTags::CommonDefault(EWidgetStyleKind::Text);
	KeyBorderStyleTag = WidgetStyleTags::CommonDefault(EWidgetStyleKind::Brush);
	bNameBeforeKeys = false;
	bShowKeyBorder = true;
	KeyIconMaxSize = FVector2D(20.f, 20.f);
	KeyCodeMinSize = FVector2D::ZeroVector;
	KeyNameSpacing = 3.f;
	NameMinWidth = 0.f;
	KeyVerticalAlignment = VAlign_Center;
	KeyTextHorizontalAlignment = HAlign_Center;
	KeySpacing = 3.f;
	KeyCodePadding = FMargin(5.f, 0.f);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

TSharedRef<SWidget> UWidgetInputKeyTips::RebuildWidget()
{
	if (!HorizontalBox)
	{
		WidgetTree = NewObject<UWidgetTree>(this, NAME_None, RF_Transient);
		BuildNativeLayout();
	}
	return Super::RebuildWidget();
}

void UWidgetInputKeyTips::BuildNativeLayout()
{
	HorizontalBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox"));
	WidgetTree->RootWidget = HorizontalBox;
	Box_KeyCode = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Box_KeyCode"));
	HorizontalBox->AddChildToHorizontalBox(Box_KeyCode);
	KeyCodeSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("KeyCodeSize"));
	Box_KeyCode->AddChildToHorizontalBox(KeyCodeSize)->SetVerticalAlignment(VAlign_Center);
	Border_KeyCode = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_KeyCode"));
	USizeBoxSlot* BorderSlot = CastChecked<USizeBoxSlot>(KeyCodeSize->AddChild(Border_KeyCode));
	BorderSlot->SetHorizontalAlignment(HAlign_Fill);
	BorderSlot->SetVerticalAlignment(VAlign_Fill);
	Border_KeyCode->SetHorizontalAlignment(HAlign_Center);
	Border_KeyCode->SetVerticalAlignment(VAlign_Center);
	Txt_KeyCode = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass(), TEXT("Txt_KeyCode"));
	Txt_KeyCode->SetJustification(ETextJustify::Center);
	Border_KeyCode->AddChild(Txt_KeyCode);
	Box_KeyIcon = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Box_KeyIcon"));
	Box_KeyCode->AddChildToHorizontalBox(Box_KeyIcon)->SetVerticalAlignment(VAlign_Center);
	Txt_DisplayName = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass(), TEXT("Txt_DisplayName"));
	HorizontalBox->AddChildToHorizontalBox(Txt_DisplayName);
	WidgetTree->ForEachWidget([](UWidget* Widget)
	{
		Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	});
}

void UWidgetInputKeyTips::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	EnsureThemeSubscription();
	RefreshPresentation();
	SetKeyDisplayName(KeyDisplayName);
	RefreshData();
}

void UWidgetInputKeyTips::EnsureThemeSubscription()
{
	if (!ThemeChangedHandle.IsValid() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		ThemeChangedHandle = FWidgetThemeResolver::OnThemeChanged().AddUObject(this, &ThisClass::RefreshPresentation);
	}
}

void UWidgetInputKeyTips::ReleaseThemeSubscription()
{
	FWidgetThemeResolver::OnThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
}

void UWidgetInputKeyTips::NativePreConstruct()
{
	Super::NativePreConstruct();

	EnsureThemeSubscription();
	RefreshPresentation();
	SetKeyDisplayName(KeyDisplayName);
	RefreshData();
}

void UWidgetInputKeyTips::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureThemeSubscription();
	BindInputObservers();
	RefreshData();
}

void UWidgetInputKeyTips::NativeDestruct()
{
#if WITH_EDITOR
	CancelDeferredInputRefresh();
#endif
	UnbindInputObservers();
	ReleaseThemeSubscription();
	Super::NativeDestruct();
}

void UWidgetInputKeyTips::BeginDestroy()
{
#if WITH_EDITOR
	CancelDeferredInputRefresh();
#endif
	UnbindInputObservers();
	ReleaseThemeSubscription();
	Super::BeginDestroy();
}

void UWidgetInputKeyTips::RefreshPresentation()
{
	if (!HorizontalBox)
	{
		return;
	}
	UWidget* First = bNameBeforeKeys ? static_cast<UWidget*>(Txt_DisplayName) : static_cast<UWidget*>(Box_KeyCode);
	UWidget* Second = bNameBeforeKeys ? static_cast<UWidget*>(Box_KeyCode) : static_cast<UWidget*>(Txt_DisplayName);
	if (HorizontalBox->GetChildIndex(First) > HorizontalBox->GetChildIndex(Second))
	{
		HorizontalBox->RemoveChild(First);
		HorizontalBox->RemoveChild(Second);
		HorizontalBox->AddChildToHorizontalBox(First);
		HorizontalBox->AddChildToHorizontalBox(Second);
	}
	UHorizontalBoxSlot* NameSlot = CastChecked<UHorizontalBoxSlot>(Txt_DisplayName->Slot);
	NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	NameSlot->SetVerticalAlignment(VAlign_Center);
	NameSlot->SetPadding(FMargin(bNameBeforeKeys ? 0.f : KeyNameSpacing, 0.f, bNameBeforeKeys ? KeyNameSpacing : 0.f, 0.f));
	UHorizontalBoxSlot* CodeSlot = CastChecked<UHorizontalBoxSlot>(Box_KeyCode->Slot);
	CodeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	CodeSlot->SetVerticalAlignment(VAlign_Center);
	CodeSlot->SetPadding(FMargin(0.f));
	KeyCodeSize->SetMinDesiredWidth(KeyCodeMinSize.X);
	KeyCodeSize->SetMinDesiredHeight(KeyCodeMinSize.Y);
	CastChecked<UHorizontalBoxSlot>(KeyCodeSize->Slot)->SetVerticalAlignment(KeyVerticalAlignment);
	CastChecked<UHorizontalBoxSlot>(Box_KeyIcon->Slot)->SetVerticalAlignment(KeyVerticalAlignment);
	Txt_DisplayName->SetMinDesiredWidth(NameMinWidth);
	Txt_KeyCode->SetStyleTag(KeyTextStyleTag);
	Txt_DisplayName->SetStyleTag(NameTextStyleTag);
	UClass* BrushClass = FWidgetThemeResolver::ResolveClass(this, KeyBorderStyleTag, EWidgetStyleKind::Brush);
	const UCommonBrushStyle* BrushStyle = BrushClass ? BrushClass->GetDefaultObject<UCommonBrushStyle>() : nullptr;
	FSlateBrush BorderBrush = BrushStyle ? BrushStyle->Brush : FSlateBrush();
	if (!bShowKeyBorder)
	{
		BorderBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	}
	Border_KeyCode->SetBrush(BorderBrush);
	Border_KeyCode->SetPadding(bShowKeyBorder ? KeyCodePadding : FMargin(0.f));
	Border_KeyCode->SetHorizontalAlignment(KeyTextHorizontalAlignment);
}

void UWidgetInputKeyTips::BindInputObservers()
{
	UnbindInputObservers();
	InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	if (InputSubsystem.IsValid())
	{
		InputMethodChangedHandle = InputSubsystem->OnInputMethodChangedNative.AddUObject(this, &ThisClass::OnInputMethodChanged);
	}
	if (ULocalPlayer* Player = GetOwningLocalPlayer())
	{
		EnhancedInputSubsystem = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (EnhancedInputSubsystem.IsValid())
		{
			EnhancedInputSubsystem->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &ThisClass::RefreshData);
		}
	}
}

void UWidgetInputKeyTips::UnbindInputObservers()
{
	if (InputSubsystem.IsValid())
	{
		InputSubsystem->OnInputMethodChangedNative.Remove(InputMethodChangedHandle);
	}
	if (EnhancedInputSubsystem.IsValid())
	{
		EnhancedInputSubsystem->ControlMappingsRebuiltDelegate.RemoveDynamic(this, &ThisClass::RefreshData);
	}
	InputMethodChangedHandle.Reset();
	InputSubsystem.Reset();
	EnhancedInputSubsystem.Reset();
}

void UWidgetInputKeyTips::OnInputMethodChanged(ECommonInputType InInputType)
{
	RefreshData();
}

void UWidgetInputKeyTips::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);

	EnsureThemeSubscription();
	BindInputObservers();
	RefreshData();
}

void UWidgetInputKeyTips::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
#if WITH_EDITOR
	CancelDeferredInputRefresh();
#endif
	UnbindInputObservers();
	ReleaseThemeSubscription();
	Super::OnDespawn_Implementation(InMode);
}

void UWidgetInputKeyTips::RefreshData()
{
	if(!Box_KeyIcon)
	{
		return;
	}

#if WITH_EDITOR
	if (GCompilingBlueprint)
	{
		if (!DeferredInputRefreshHandle.IsValid())
		{
			DeferredInputRefreshHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::RefreshDeferredInput));
		}
		return;
	}
	CancelDeferredInputRefresh();
#endif

	Box_KeyIcon->ClearChildren();

	FString KeyCode;
	const UCommonInputPlatformSettings* Settings = UPlatformSettingsManager::Get().GetSettingsForPlatform<UCommonInputPlatformSettings>();
	ECommonInputType InputType;
	FName GamepadName;
	FCommonInputBase::GetCurrentPlatformDefaults(InputType, GamepadName);
	if(const UCommonInputSubsystem* LocalInputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		InputType = LocalInputSubsystem->GetCurrentInputType();
		GamepadName = LocalInputSubsystem->GetCurrentGamepadName();
	}
	const UInputModule* InputModule = nullptr;
	if(IsDesignTime())
	{
		if(UInputModule::IsValid(true))
		{
			InputModule = UInputModule::GetPtr(true);
		}
	}
	else
	{
		InputModule = UInputModule::GetPtr(GetWorld());
	}
	for(const FGameplayTag& ActionTag : ActionTags)
	{
		if(!InputModule || !ActionTag.IsValid())
		{
			continue;
		}

		for(const FKey& Key : InputModule->GetKeysByActionTag(ActionTag))
		{
			if ((InputType == ECommonInputType::MouseAndKeyboard && Key.IsGamepadKey()) ||
				(InputType == ECommonInputType::Gamepad && !Key.IsGamepadKey()))
			{
				continue;
			}
			FSlateBrush ImageBrush;
			if(Settings->TryGetInputBrush(
				ImageBrush,
				Key,
				InputType,
				GamepadName)
				&& ImageBrush.GetResourceObject())
			{
				const FVector2D IconSize = ImageBrush.ImageSize;
				const FVector2D MaxSize = KeyIconMaxSize;
				if (IconSize.X > 0.f && IconSize.Y > 0.f)
				{
					ImageBrush.ImageSize = IconSize * FMath::Min(MaxSize.X / IconSize.X, MaxSize.Y / IconSize.Y);
				}
				UImage* Image = NewObject<UImage>(this);
				Image->SetBrush(ImageBrush);
				if(UHorizontalBoxSlot* ImageSlot = Box_KeyIcon->AddChildToHorizontalBox(Image))
				{
					Image->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					ImageSlot->SetPadding(FMargin(0.f, 0.f, KeySpacing, 0.f));
					ImageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
					ImageSlot->SetVerticalAlignment(VAlign_Center);
				}
			}
			else
			{
				if(!KeyCode.IsEmpty())
				{
					KeyCode += TEXT("/");
				}
				KeyCode += Key.GetDisplayName(false).ToString();
			}
		}
	}

	if(Box_KeyIcon->GetChildrenCount() > 0)
	{
		CastChecked<UHorizontalBoxSlot>(Box_KeyIcon->GetChildAt(Box_KeyIcon->GetChildrenCount() - 1)->Slot)->SetPadding(FMargin(0.f));
	}
	Box_KeyIcon->SetVisibility(Box_KeyIcon->GetChildrenCount() > 0
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
	if(Border_KeyCode)
	{
		Border_KeyCode->SetVisibility(KeyCode.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
	if(Txt_KeyCode)
	{
		Txt_KeyCode->SetText(FText::FromString(KeyCode));
		Txt_KeyCode->SetVisibility(KeyCode.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
	KeyCodeSize->SetVisibility(KeyCode.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	CastChecked<UHorizontalBoxSlot>(KeyCodeSize->Slot)->SetPadding(FMargin(0.f, 0.f,
		!KeyCode.IsEmpty() && Box_KeyIcon->GetChildrenCount() > 0 ? KeySpacing : 0.f, 0.f));
}

#if WITH_EDITOR
bool UWidgetInputKeyTips::RefreshDeferredInput(float InDeltaTime)
{
	if (GCompilingBlueprint)
	{
		return true;
	}
	DeferredInputRefreshHandle.Reset();
	RefreshData();
	return false;
}

void UWidgetInputKeyTips::CancelDeferredInputRefresh()
{
	FTSTicker::RemoveTicker(DeferredInputRefreshHandle);
	DeferredInputRefreshHandle.Reset();
}
#endif

FText UWidgetInputKeyTips::GetKeyDisplayName() const
{
	return KeyDisplayName;
}

void UWidgetInputKeyTips::SetKeyDisplayName(const FText InKeyDisplayName)
{
	KeyDisplayName = InKeyDisplayName;

	if(Txt_DisplayName)
	{
		Txt_DisplayName->SetText(KeyDisplayName);
		Txt_DisplayName->SetVisibility(KeyDisplayName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UWidgetInputKeyTips::SetActionTags(const TArray<FGameplayTag>& InActionTags)
{
	ActionTags = InActionTags;
	RefreshData();
}
