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
	case EWidgetStyleKind::Input:
		return UCommonInputStyle::StaticClass();
	case EWidgetStyleKind::Toggle:
		return UCommonToggleStyle::StaticClass();
	case EWidgetStyleKind::Range:
		return UCommonRangeStyle::StaticClass();
	case EWidgetStyleKind::Progress:
		return UCommonProgressStyle::StaticClass();
	case EWidgetStyleKind::Scroll:
		return UCommonScrollStyle::StaticClass();
	case EWidgetStyleKind::Border:
		return UCommonBorderStyle::StaticClass();
	case EWidgetStyleKind::TextScroll:
		return UCommonTextScrollStyle::StaticClass();

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
	for (EWidgetStyleKind Kind : WidgetStyleTags::Kinds())
	{
		UClass* Base = DefaultStyle(Kind);
		if (!Base || !Base->IsChildOf(NativeBase(Kind)) || Base->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			Error = TEXT("Project settings contain an invalid style base class.");
			return false;
		}
	}
	TSet<const UWidgetTheme*> Visited;
	for (const UWidgetTheme* Layer = Theme; Layer; Layer = Layer->ParentTheme)
	{
		if (Visited.Contains(Layer))
		{
			Error = TEXT("Theme parent chain contains a cycle.");
			return false;
		}
		Visited.Add(Layer);
		for (EWidgetStyleKind Kind : WidgetStyleTags::Kinds())
		{
			for (FGameplayTag Tag : Layer->GetStyleTags(Kind))
			{
				EWidgetStyleKind TagKind;
				UClass* Class = Layer->GetStyleClass(Tag, Kind);
				if (!WidgetStyleTags::Kind(Tag, TagKind) || TagKind != Kind || WidgetStyleTags::IsRoot(Tag) || !Class ||
					!Class->IsChildOf(NativeBase(Kind)) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
				{
					Error = FString::Printf(TEXT("Invalid style in %s: %s"), *Layer->GetPathName(), *Tag.ToString());
					return false;
				}
			}
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
	UClass* Base = NativeBase(Kind);
	if (!Base)
	{
		return nullptr;
	}
	const UWidgetTheme* Theme = ResolveTheme(Context);
	EWidgetStyleKind TagKind;
	UClass* Class = Theme ? Theme->ResolveStyleClass(Tag, Kind) : nullptr;
	if (WidgetStyleTags::Kind(Tag, TagKind) && TagKind == Kind && Class && Class->IsChildOf(Base) &&
		!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return Class;
	}
	Class = Theme ? Theme->ResolveStyleClass(WidgetStyleTags::CommonDefault(Kind), Kind) : nullptr;
	if (Class && Class->IsChildOf(Base) && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
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
