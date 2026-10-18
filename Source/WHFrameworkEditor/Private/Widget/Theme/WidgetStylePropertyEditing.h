#pragma once

#include "PropertyEditorModule.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"
#include "IDetailCustomization.h"
#include "UObject/Class.h"

class FWidgetStylePropertyEditing
{
public:
	explicit FWidgetStylePropertyEditing(UObject* Defaults)
	{
		Generator = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
		for (UClass* StyleClass = Defaults->GetClass(); StyleClass; StyleClass = StyleClass->GetSuperClass())
		{
			Generator->RegisterInstancedCustomPropertyLayout(StyleClass, FOnGetDetailCustomizationInstance::CreateLambda([]
			{
				return MakeShared<FPropertyLayout>();
			}));
		}
		Generator->OnRowsRefreshed().AddLambda([] {});
		Generator->SetObjects({Defaults});
	}

	TSharedPtr<IPropertyHandle> Find(FProperty* Property) const
	{
		return Find(Generator->GetRootTreeNodes(), Property);
	}

private:
	class FPropertyLayout : public IDetailCustomization
	{
	public:
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder) override
		{
		}
	};

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
