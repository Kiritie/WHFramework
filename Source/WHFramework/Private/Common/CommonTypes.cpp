// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/CommonTypes.h"

#include "Common/CommonStatics.h"
#include "Debug/DebugModuleTypes.h"

#if WITH_EDITOR
bool GIsPlaying = false;
#else
bool GIsPlaying = true;
#endif

bool GIsSimulating = false;

const UObject* GetWorldContext(bool bInEditor)
{
	return UCommonStatics::GetWorldContext(bInEditor);
}

UObject* GetMutableWorldContext(bool bInEditor)
{
	return UCommonStatics::GetMutableWorldContext(bInEditor);
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

	////////////////////////////////////////////////////
	// Input_Shortcut
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_InteractSelect, "Input.Shortcut.InteractSelect", "Camera Pan Move");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_CameraPanMove, "Input.Shortcut.CameraPanMove", "Camera Pan Move");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_CameraRotate, "Input.Shortcut.CameraRotate", "Camera Rotate");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_CameraZoom, "Input.Shortcut.CameraZoom", "Camera Zoom");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_CameraSprint, "Input.Shortcut.CameraSprint", "Camera Sprint");

	////////////////////////////////////////////////////
	// Input_Camera
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_TurnCamera, "Input.Camera.Turn", "Turn Camera");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_LookUpCamera, "Input.Camera.LookUp", "Look Up Camera");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_PanHCamera, "Input.Camera.PanH", "Pan H Camera");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_PanVCamera, "Input.Camera.PanV", "Pan V Camera");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_ZoomCamera, "Input.Camera.Zoom", "Zoom Camera");

	////////////////////////////////////////////////////
	// Input_Player
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_TurnPlayer, "Input.Player.Turn", "Move Forward Player");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_MoveForwardPlayer, "Input.Player.MoveForward", "Move Forward Player");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_MoveRightPlayer, "Input.Player.MoveRight", "Move Right Player");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_MoveUpPlayer, "Input.Player.MoveUp", "Move Up Player");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_JumpPlayer, "Input.Player.Jump", "Jump Player");

	////////////////////////////////////////////////////
	// Input_System
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_SystemOperation, "Input.System.Operation", "System Operation");

	////////////////////////////////////////////////////
	// State_Vitality
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateTag_Vitality_Dead, "State.Vitality.Dead", "Vitality Dead");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateTag_Vitality_Dying, "State.Vitality.Dying", "Vitality Dying");

	////////////////////////////////////////////////////
	// State_Character
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateTag_Character_Active, "State.Character.Active", "Character Active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateTag_Character_Moving, "State.Character.Moving", "Character Moving");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateTag_Character_Falling, "State.Character.Falling", "Character Falling");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateTag_Character_Walking, "State.Character.Walking", "Character Walking");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateTag_Character_Jumping, "State.Character.Jumping", "Character Jumping");

	////////////////////////////////////////////////////
	// Event_Hit
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventTag_Hit_Attack, "Event.Hit.Attack", "Hit Attack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventTag_Hit_Skill, "Event.Hit.Skill", "Hit Skill");
}
