#include "Widget/Common/CommonWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Widget/Theme/WidgetThemeResolver.h"

UCommonWidget::UCommonWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UCommonWidget::OnSpawn_Implementation(const FParameter& InParam)
{
	EnsureThemeSubscription();
	BindControlEvents();
	RefreshStyle();
}

void UCommonWidget::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	UnbindControlEvents();
	ReleaseThemeSubscription();
	InheritedStyleTag = FGameplayTag();
	const UCommonWidget* Defaults = CastChecked<UCommonWidget>(GetArchetype());
	StyleTag = Defaults->StyleTag;
	SetIsEnabled(Defaults->GetIsEnabled());
	SetVisibility(Defaults->GetVisibility());
	bSynchronizingValue = false;
}

void UCommonWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	EnsureThemeSubscription();
	RefreshStyle();
}

TSharedRef<SWidget> UCommonWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		BuildNativeLayout();
	}
	return Super::RebuildWidget();
}

void UCommonWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureThemeSubscription();
	BindControlEvents();
	RefreshStyle();
}

void UCommonWidget::NativeDestruct()
{
	UnbindControlEvents();
	ReleaseThemeSubscription();
	Super::NativeDestruct();
}

void UCommonWidget::BeginDestroy()
{
	ReleaseThemeSubscription();
	Super::BeginDestroy();
}

EWidgetStyleKind UCommonWidget::GetStyleKind() const
{
	return EWidgetStyleKind::None;
}

void UCommonWidget::BuildNativeLayout()
{
}

void UCommonWidget::ApplyResolvedStyle(const UObject* InStyle)
{
}

void UCommonWidget::BindControlEvents()
{
}

void UCommonWidget::UnbindControlEvents()
{
}

FGameplayTag UCommonWidget::GetStyleTag() const
{
	if (StyleTag.IsValid())
	{
		return StyleTag;
	}
	if (InheritedStyleTag.IsValid())
	{
		return InheritedStyleTag;
	}
	return WidgetStyleTags::CommonDefault(GetStyleKind());
}

void UCommonWidget::SetStyleTag(FGameplayTag InStyleTag)
{
	StyleTag = InStyleTag;
	RefreshStyle();
}

void UCommonWidget::SetInheritedStyleTag(FGameplayTag InStyleTag)
{
	InheritedStyleTag = InStyleTag;
	RefreshStyle();
}

void UCommonWidget::RefreshStyle()
{
	if (bApplyingStyle || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}
	TGuardValue<bool> Guard(bApplyingStyle, true);
	UClass* Class = FWidgetThemeResolver::ResolveClass(this, GetStyleTag(), GetStyleKind());
	if (Class)
	{
		ApplyResolvedStyle(Class->GetDefaultObject());
	}
}

void UCommonWidget::EnsureThemeSubscription()
{
	if (!ThemeChangedHandle.IsValid() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		ThemeChangedHandle = FWidgetThemeResolver::OnThemeChanged().AddUObject(this, &ThisClass::RefreshStyle);
	}
}

void UCommonWidget::ReleaseThemeSubscription()
{
	FWidgetThemeResolver::OnThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
}
