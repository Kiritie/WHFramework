#pragma once

#include "PropertyEditorModule.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"

class FWidgetStylePropertyEditing
{
public:
	explicit FWidgetStylePropertyEditing(UObject* Defaults)
	{
		Generator = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
		Generator->OnRowsRefreshed().AddLambda([] {});
		Generator->SetObjects({Defaults});
	}

	TSharedPtr<IPropertyHandle> Find(FProperty* Property) const
	{
		return Find(Generator->GetRootTreeNodes(), Property);
	}

private:
	static TSharedPtr<IPropertyHandle> Find(const TArray<TSharedRef<IDetailTreeNode>>& Nodes, FProperty* Property)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
		{
			TSharedPtr<IPropertyHandle> Handle = Node->CreatePropertyHandle();
			if (Handle && Handle->GetProperty() == Property)
			{
				return Handle;
			}
			TArray<TSharedRef<IDetailTreeNode>> Children;
			Node->GetChildren(Children, true);
			if (TSharedPtr<IPropertyHandle> Child = Find(Children, Property))
			{
				return Child;
			}
		}
		return nullptr;
	}

	TSharedPtr<IPropertyRowGenerator> Generator;
};
