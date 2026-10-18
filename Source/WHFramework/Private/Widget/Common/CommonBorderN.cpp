#include "Widget/Common/CommonBorderN.h"

#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Theme/WidgetStyleTags.h"

UCommonBorderN::UCommonBorderN(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UCommonBorderN::SynchronizeProperties()
{
	if (!ThemeChangedHandle.IsValid() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		ThemeChangedHandle = FWidgetThemeResolver::OnThemeChanged().AddUObject(this, &ThisClass::ApplyStyleTag);
	}
	ApplyStyleTag();
}

void UCommonBorderN::BeginDestroy()
{
	FWidgetThemeResolver::OnThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
	Super::BeginDestroy();
}

FGameplayTag UCommonBorderN::GetStyleTag() const
{
	if (StyleTag.IsValid())
	{
		return StyleTag;
	}
	if (InheritedStyleTag.IsValid())
	{
		return InheritedStyleTag;
	}
	return WidgetStyleTags::CommonDefault(EWidgetStyleKind::Border);
}

void UCommonBorderN::SetStyleTag(FGameplayTag InStyleTag)
{
	StyleTag = InStyleTag;
	ApplyStyleTag();
}

void UCommonBorderN::SetInheritedStyleTag(FGameplayTag InStyleTag)
{
	InheritedStyleTag = InStyleTag;
	ApplyStyleTag();
}

void UCommonBorderN::ApplyStyleTag()
{
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		if (UClass* ResolvedClass = FWidgetThemeResolver::ResolveClass(this, GetStyleTag(), EWidgetStyleKind::Border))
		{
			Style = ResolvedClass;
		}
	}
	Super::SynchronizeProperties();
}
