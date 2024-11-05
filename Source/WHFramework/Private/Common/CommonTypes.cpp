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
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_InteractSelect, "Input.Shortcut.InteractSelect", "Camera Pan Move");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_CameraPanMove, "Input.Shortcut.CameraPanMove", "Camera Pan Move");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_CameraRotate, "Input.Shortcut.CameraRotate", "Camera Rotate");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_CameraZoom, "Input.Shortcut.CameraZoom", "Camera Zoom");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_CameraSprint, "Input.Shortcut.CameraSprint", "Camera Sprint");

	////////////////////////////////////////////////////
	// Input_Camera
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_TurnCamera, "Input.Camera.Turn", "Turn Camera");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_LookUpCamera, "Input.Camera.LookUp", "Look Up Camera");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_PanHCamera, "Input.Camera.PanH", "Pan H Camera");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_PanVCamera, "Input.Camera.PanV", "Pan V Camera");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_ZoomCamera, "Input.Camera.Zoom", "Zoom Camera");

	////////////////////////////////////////////////////
	// Input_Player
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_TurnPlayer, "Input.Player.Turn", "Move Forward Player");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_MoveForwardPlayer, "Input.Player.MoveForward", "Move Forward Player");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_MoveRightPlayer, "Input.Player.MoveRight", "Move Right Player");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_MoveUpPlayer, "Input.Player.MoveUp", "Move Up Player");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_JumpPlayer, "Input.Player.Jump", "Jump Player");

	////////////////////////////////////////////////////
	// Input_System
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_SystemOperation, "Input.System.Operation", "System Operation");

	////////////////////////////////////////////////////
	// State_Vitality
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Vitality_Active, "State.Vitality.Active", "Vitality Active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Vitality_Dead, "State.Vitality.Dead", "Vitality Dead");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Vitality_Dying, "State.Vitality.Dying", "Vitality Dying");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Vitality_Walking, "State.Vitality.Walking", "Vitality Walking");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Vitality_Interrupting, "State.Vitality.Interrupting", "Vitality Interrupting");

	////////////////////////////////////////////////////
	// State_Pawn
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Pawn_Moving, "State.Pawn.Moving", "Pawn Moving");

	////////////////////////////////////////////////////
	// State_Character
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Character_FreeToAnim, "State.Character.FreeToAnim", "Character FreeToAnim");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Character_Animating, "State.Character.Animating", "Character Animating");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Character_Falling, "State.Character.Falling", "Character Falling");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Character_Jumping, "State.Character.Jumping", "Character Jumping");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Character_Crouching, "State.Character.Crouching", "Character Crouching");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Character_Swimming, "State.Character.Swimming", "Character Swimming");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Character_Floating, "State.Character.Floating", "Character Floating");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Character_Climbing, "State.Character.Climbing", "Character Climbing");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Character_Flying, "State.Character.Flying", "Character Flying");

	////////////////////////////////////////////////////
	// Ability_Vitality_Action
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Vitality_Action_Death, "Ability.Vitality.Action.Death", "Vitality Action Death");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Vitality_Action_Spawn, "Ability.Vitality.Action.Spawn", "Vitality Action Spawn");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Vitality_Action_Static, "Ability.Vitality.Action.Static", "Vitality Action Static");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Vitality_Action_Walk, "Ability.Vitality.Action.Walk", "Vitality Action Walk");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Vitality_Action_Interrupt, "Ability.Vitality.Action.Interrupt", "Vitality Action Interrupt");

	////////////////////////////////////////////////////
	// Ability_Character_Action
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Character_Action_Jump, "Ability.Character.Action.Jump", "Character Action Jump");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Character_Action_Fall, "Ability.Character.Action.Fall", "Character Action Fall");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Character_Action_Crouch, "Ability.Character.Action.Crouch", "Character Action Crouch");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Character_Action_Swim, "Ability.Character.Action.Swim", "Character Action Swim");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Character_Action_Float, "Ability.Character.Action.Float", "Character Action Float");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Character_Action_Climb, "Ability.Character.Action.Climb", "Character Action Climb");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Character_Action_Fly, "Ability.Character.Action.Fly", "Character Action Fly");

	////////////////////////////////////////////////////
	// Event_Hit
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit_Attack, "Event.Hit.Attack", "Hit Attack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit_Skill, "Event.Hit.Skill", "Hit Skill");
}
