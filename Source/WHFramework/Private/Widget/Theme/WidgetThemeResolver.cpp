#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeSubsystem.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace WidgetThemeResolverPrivate
{
	FSimpleMulticastDelegate Changed;
#if WITH_EDITOR
	TMap<TWeakObjectPtr<const UObject>, TStrongObjectPtr<UWidgetTheme>> Previews;
	TStrongObjectPtr<UWidgetTheme> DefaultTheme;
	FSoftObjectPath CachedThemePath;
	bool bLoaded = false;
	void LoadEditorDefault()
	{
		const auto* S = GetDefault<UWidgetThemeSettings>();
		if (bLoaded && CachedThemePath == S->DefaultTheme.ToSoftObjectPath())
		{
			return;
		}
		bLoaded = true;
		CachedThemePath = S->DefaultTheme.ToSoftObjectPath();
		DefaultTheme.Reset(S->DefaultTheme.LoadSynchronous());
	}
#endif
	UWidgetThemeSubsystem* Subsystem(const UObject* Context)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		UGameInstance* GI = World && World->IsGameWorld() ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UWidgetThemeSubsystem>() : nullptr;
	}
} // namespace WidgetThemeResolverPrivate
FSimpleMulticastDelegate& FWidgetThemeResolver::OnThemeChanged()
{
	return WidgetThemeResolverPrivate::Changed;
}
void FWidgetThemeResolver::NotifyThemeChanged()
{
	check(IsInGameThread());
	OnThemeChanged().Broadcast();
}
UClass* FWidgetThemeResolver::NativeBase(EWidgetStyleKind Kind)
{
	switch (Kind)
	{
	case EWidgetStyleKind::Button:
		return UCommonButtonStyle::StaticClass();
	case EWidgetStyleKind::Text:
		return UCommonTextStyle::StaticClass();
	case EWidgetStyleKind::Brush:
		return UWidgetBrushStyle::StaticClass();
	}
	return nullptr;
}
UClass* FWidgetThemeResolver::DefaultStyle(EWidgetStyleKind Kind)
{
	return GetDefault<UWidgetThemeSettings>()->GetStyleBase(Kind);
}

bool FWidgetThemeResolver::Validate(const UWidgetTheme* Theme, FString& Error)
{
	Error.Reset();
	if (!Theme)
	{
		Error = TEXT("Theme failed to load.");
		return false;
	}
	for (EWidgetStyleKind Kind : {EWidgetStyleKind::Button, EWidgetStyleKind::Text, EWidgetStyleKind::Brush})
	{
		UClass* Base = DefaultStyle(Kind);
		if (!Base || !Base->IsChildOf(NativeBase(Kind)) || Base->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			Error = TEXT("Project settings contain an invalid style base class.");
			return false;
		}
	}
	TSet<FGameplayTag> Seen;
	auto CheckEntry = [&](FGameplayTag Tag, EWidgetStyleKind Kind) -> bool
	{
		EWidgetStyleKind TagKind;
		UClass* Class = Theme->GetStyleClass(Tag, Kind);
		if (!WidgetStyleTags::Kind(Tag, TagKind) || TagKind != Kind || Seen.Contains(Tag) || !Class || !Class->IsChildOf(NativeBase(Kind)) ||
			Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			Error = FString::Printf(TEXT("Invalid, duplicate or incompatible style: %s"), *Tag.ToString());
			return false;
		}
		Seen.Add(Tag);
		return true;
	};
	for (const auto& Entry : Theme->GetButtonStyles())
	{
		if (!CheckEntry(Entry.Key, EWidgetStyleKind::Button))
			return false;
	}
	for (const auto& Entry : Theme->GetTextStyles())
	{
		if (!CheckEntry(Entry.Key, EWidgetStyleKind::Text))
			return false;
	}
	for (const auto& Entry : Theme->GetBrushStyles())
	{
		if (!CheckEntry(Entry.Key, EWidgetStyleKind::Brush))
			return false;
	}
	return true;
}
const UWidgetTheme* FWidgetThemeResolver::ResolveTheme(const UObject* Context)
{
	check(IsInGameThread());

	if (Context && Context->GetWorld() && Context->GetWorld()->IsGameWorld())
	{
		const auto* Sub = WidgetThemeResolverPrivate::Subsystem(Context);
		return Sub ? Sub->GetActiveTheme() : nullptr;
	}
#if WITH_EDITOR
	using namespace WidgetThemeResolverPrivate;
	for (auto It = Previews.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (const UObject* Outer = Context; Outer && !Outer->IsA<UPackage>(); Outer = Outer->GetOuter())
	{
		if (const auto* Theme = Previews.Find(TWeakObjectPtr<const UObject>(Outer)))
		{
			return Theme->Get();
		}
	}
	LoadEditorDefault();
	return DefaultTheme.Get();
#else
	return nullptr;
#endif
}
UClass* FWidgetThemeResolver::ResolveClass(const UObject* Context, FGameplayTag Tag, EWidgetStyleKind Kind)
{
	const UWidgetTheme* Theme = ResolveTheme(Context);
	EWidgetStyleKind TagKind;
	UClass* Class = Theme ? Theme->GetStyleClass(Tag, Kind) : nullptr;
	if (WidgetStyleTags::Kind(Tag, TagKind) && TagKind == Kind && Class && Class->IsChildOf(NativeBase(Kind)) &&
		!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return Class;
	}
	return DefaultStyle(Kind);
}
#if WITH_EDITOR
void FWidgetThemeResolver::SetPreviewOverride(const UObject* Root, UWidgetTheme* Theme)
{
	check(IsInGameThread());
	if (!Root || Root->IsA<UPackage>() || (Root->GetWorld() && Root->GetWorld()->IsGameWorld()))
	{
		return;
	}
	WidgetThemeResolverPrivate::Previews.Add(TWeakObjectPtr<const UObject>(Root), TStrongObjectPtr<UWidgetTheme>(Theme));
	NotifyThemeChanged();
}
void FWidgetThemeResolver::ClearPreviewOverride(const UObject* Root)
{
	WidgetThemeResolverPrivate::Previews.Remove(TWeakObjectPtr<const UObject>(Root));
	NotifyThemeChanged();
}
void FWidgetThemeResolver::InvalidateEditorDefault()
{
	WidgetThemeResolverPrivate::bLoaded = false;
	WidgetThemeResolverPrivate::DefaultTheme.Reset();
	NotifyThemeChanged();
}
#endif
