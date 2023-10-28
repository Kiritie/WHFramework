// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/CommonTypes.h"

#include "Common/CommonBPLibrary.h"
#include "Debug/DebugModuleTypes.h"

#if WITH_EDITOR
bool GIsPlaying = false;
#else
bool GIsPlaying = true;
#endif

bool GIsSimulating = false;

UObject* GetWorldContext(bool bInEditor)
{
	return UCommonBPLibrary::GetWorldContext(bInEditor);
}

namespace GameplayTags
{
	FGameplayTag FindTagByString(const FString& TagString, bool bMatchPartialString)
	{
		const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagString), false);

		if (!Tag.IsValid() && bMatchPartialString)
		{
			FGameplayTagContainer AllTags;
			Manager.RequestAllGameplayTags(AllTags, true);

			for (const FGameplayTag& TestTag : AllTags)
			{
				if (TestTag.ToString().Contains(TagString))
				{
					WHLog(FString::Printf(TEXT("Could not find exact match for tag [%s] but found partial match on tag [%s]."), *TagString, *TestTag.ToString()), EDC_Default, EDV_Warning);
					Tag = TestTag;
					break;
				}
			}
		}

		return Tag;
	}

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_TurnCamera, "Input.Camera.Turn", "Turn Camera");
}
