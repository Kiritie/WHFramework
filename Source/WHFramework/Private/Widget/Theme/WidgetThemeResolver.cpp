#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeSubsystem.h"
#include "Widget/Theme/WidgetThemeFallbacks.h"
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
	TStrongObjectPtr<UWidgetStyleCatalog> DefaultCatalog;
	FSoftObjectPath CachedThemePath;
	FSoftObjectPath CachedCatalogPath;
	bool bLoaded = false;
	void LoadEditorDefault()
	{
		const auto* S = GetDefault<UWidgetThemeSettings>();
		if (bLoaded && CachedThemePath == S->DefaultTheme.ToSoftObjectPath() && CachedCatalogPath == S->StyleCatalog.ToSoftObjectPath())
		{
			return;
		}
		bLoaded = true;
		CachedThemePath = S->DefaultTheme.ToSoftObjectPath();
		CachedCatalogPath = S->StyleCatalog.ToSoftObjectPath();
		DefaultTheme.Reset(S->DefaultTheme.LoadSynchronous());
		DefaultCatalog.Reset(S->StyleCatalog.LoadSynchronous());
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
UClass* FWidgetThemeResolver::Placeholder(EWidgetStyleKind Kind)
{
	switch (Kind)
	{
	case EWidgetStyleKind::Button:
		return UWidgetFallbackButtonStyle::StaticClass();
	case EWidgetStyleKind::Text:
		return UWidgetFallbackTextStyle::StaticClass();
	case EWidgetStyleKind::Brush:
		return UWidgetFallbackBrushStyle::StaticClass();
	}
	return nullptr;
}
bool FWidgetThemeResolver::Validate(const UWidgetTheme* Theme, const UWidgetStyleCatalog* Catalog, FString& Error, bool bRequireComplete)
{
	Error.Reset();
	if (!Theme || !Catalog)
	{
		Error = TEXT("Theme or global style catalog failed to load.");
		return false;
	}
	for (const auto& Entry : Catalog->Types)
	{
		if (!Entry.Key.IsValid() || !NativeBase(Entry.Value))
		{
			Error = TEXT("Catalog contains an invalid Tag or control type.");
			return false;
		}
	}
	for (const auto& Entry : Catalog->NativeRoots)
	{
		UClass* Root = Entry.Value.Get();
		UClass* Base = NativeBase(Entry.Key);
		if (!Root || !Base || !Root->HasAnyClassFlags(CLASS_Native) || !Root->IsChildOf(Base))
		{
			Error = TEXT("Catalog has an invalid native creation root.");
			return false;
		}
	}
	TSet<FGameplayTag> Seen;
	auto CheckEntry = [&](FGameplayTag Tag, EWidgetStyleKind Kind) -> bool
	{
		const EWidgetStyleKind* Declared = Catalog->Types.Find(Tag);
		UClass* Class = Theme->GetStyleClass(Tag, Kind);
		UClass* Base = NativeBase(Kind);
		if (!Tag.IsValid() || Seen.Contains(Tag) || !Declared || *Declared != Kind || !Class || !Base || !Class->IsChildOf(Base) ||
			Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			Error = FString::Printf(TEXT("Invalid, duplicate, undeclared or incompatible style: %s"), *Tag.ToString());
			return false;
		}
		Seen.Add(Tag);
		return true;
	};
	for (const auto& E : Theme->GetButtonStyles())
	{
		if (!CheckEntry(E.Key, EWidgetStyleKind::Button))
		{
			return false;
		}
	}
	for (const auto& E : Theme->GetTextStyles())
	{
		if (!CheckEntry(E.Key, EWidgetStyleKind::Text))
		{
			return false;
		}
	}
	for (const auto& E : Theme->GetBrushStyles())
	{
		if (!CheckEntry(E.Key, EWidgetStyleKind::Brush))
		{
			return false;
		}
	}
	for (const auto& E : Catalog->Types)
	{
		if (bRequireComplete && !Seen.Contains(E.Key))
		{
			Error = FString::Printf(TEXT("Theme is missing required catalog tag: %s"), *E.Key.ToString());
			return false;
		}
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
	const UWidgetStyleCatalog* Catalog = nullptr;
	if (const auto* Sub = WidgetThemeResolverPrivate::Subsystem(Context))
	{
		Catalog = Sub->GetCatalog();
	}
#if WITH_EDITOR
	if (!(Context && Context->GetWorld() && Context->GetWorld()->IsGameWorld()))
	{
		WidgetThemeResolverPrivate::LoadEditorDefault();
		Catalog = WidgetThemeResolverPrivate::DefaultCatalog.Get();
	}
#endif
	const EWidgetStyleKind* Declared = Catalog ? Catalog->Types.Find(Tag) : nullptr;
	UClass* Class = Theme ? Theme->GetStyleClass(Tag, Kind) : nullptr;
	UClass* Base = NativeBase(Kind);
	if (Declared && *Declared == Kind && Class && Base && Class->IsChildOf(Base) &&
		!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return Class;
	}
	UE_LOG(LogTemp, Warning, TEXT("Theme lookup failed (Theme=%s Tag=%s Kind=%d); native placeholder applied."), *GetNameSafe(Theme), *Tag.ToString(),
		   static_cast<int32>(Kind));
	return Placeholder(Kind);
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
	WidgetThemeResolverPrivate::DefaultCatalog.Reset();
	NotifyThemeChanged();
}
#endif
