// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Item/Skill/AbilitySkillBase.h"
#include "AbilitySkillMeleeBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * 近战技能基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilitySkillMeleeBase : public AAbilitySkillBase
{
	GENERATED_BODY()
	
public:	
	AAbilitySkillMeleeBase();
};
