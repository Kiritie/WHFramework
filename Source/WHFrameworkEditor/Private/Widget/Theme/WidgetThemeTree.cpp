#include "Widget/Theme/WidgetThemeTree.h"
#include "Widget/Theme/WidgetThemeEditorService.h"
#include "Widget/Theme/WidgetStyleTags.h"

TArray<TSharedPtr<FThemeTreeItem>> FWidgetThemeTree::Build(UWidgetTheme* Theme, EWidgetStyleKind TypeFilter, const FString& Search)
{
	TArray<TSharedPtr<FThemeTreeItem>> Roots;
	TMap<FGameplayTag, TSharedPtr<FThemeTreeItem>> Items;
	for (EWidgetStyleKind Type : WidgetStyleTags::Kinds())
	{
		if (TypeFilter != EWidgetStyleKind::None && TypeFilter != Type)
		{
			continue;
		}
		TSharedPtr<FThemeTreeItem> Root = MakeShared<FThemeTreeItem>();
		Root->Tag = WidgetStyleTags::Root(Type);
		Root->bRoot = true;
		Root->Tooltip = TEXT("Built-in style type root");
		Roots.Add(Root);
		Items.Add(Root->Tag, Root);
	}
	TSet<FGameplayTag> Matches;
	TArray<FGameplayTag> AllTags;
	for (EWidgetStyleKind StyleKind : WidgetStyleTags::Kinds())
	{
		AllTags.Append(Theme->GetStyleTags(StyleKind, true));
	}
	for (FGameplayTag Tag : AllTags)
	{
		EWidgetStyleKind Type;
		if (!FWidgetThemeEditorService::Kind(Tag, Type) || (TypeFilter != EWidgetStyleKind::None && TypeFilter != Type))
		{
			continue;
		}
		TSharedPtr<FThemeTreeItem> Item = MakeShared<FThemeTreeItem>();
		Item->Tag = Tag;
		Item->Tooltip = FWidgetThemeEditorService::DescribeTag(Theme, Tag);
		const FWidgetStyleBinding* Binding = Theme->Bindings.Find(Tag);
		Item->bShared = !Binding || !Binding->bManaged;
		Items.Add(Tag, Item);
		if (Search.IsEmpty() || Tag.ToString().Contains(Search))
		{
			Matches.Add(Tag);
		}
	}
	TMap<FGameplayTag, FGameplayTag> Parents;
	for (const auto& Entry : Items)
	{
		if (Entry.Value->bRoot)
		{
			continue;
		}
		EWidgetStyleKind Type;
		FWidgetThemeEditorService::Kind(Entry.Key, Type);
		FGameplayTag Parent = WidgetStyleTags::Root(Type);
		UClass* StyleClass = Theme->ResolveStyleClass(Entry.Key, Type);
		TArray<FGameplayTag> Candidates;
		for (const auto& Candidate : Items)
		{
			if (!Candidate.Value->bRoot && Candidate.Key != Entry.Key && StyleClass && Theme->ResolveStyleClass(Candidate.Key, Type) == StyleClass->GetSuperClass())
			{
				Candidates.Add(Candidate.Key);
			}
		}
		Candidates.Sort([](const FGameplayTag& Left, const FGameplayTag& Right) { return Left.ToString() < Right.ToString(); });
		if (!Candidates.IsEmpty())
		{
			Parent = Candidates[0];
		}
		Parents.Add(Entry.Key, Parent);
	}
	TSet<FGameplayTag> Visible = Matches;
	for (FGameplayTag Match : Matches)
	{
		while (const FGameplayTag* Parent = Parents.Find(Match))
		{
			Visible.Add(*Parent);
			Match = *Parent;
		}
	}
	for (const auto& Entry : Parents)
	{
		if (Visible.Contains(Entry.Key))
		{
			Items.FindChecked(Entry.Value)->Children.Add(Items.FindChecked(Entry.Key));
		}
	}
	for (const auto& Entry : Items)
	{
		Entry.Value->Children.Sort([](const auto& Left, const auto& Right) { return Left->Tag.ToString() < Right->Tag.ToString(); });
	}
	return Roots;
}
