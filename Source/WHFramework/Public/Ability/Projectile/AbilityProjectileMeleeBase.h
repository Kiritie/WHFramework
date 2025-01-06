// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Projectile/AbilityProjectileBase.h"
#include "AbilityProjectileMeleeBase.generated.h"

/**
 * 近战技能基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityProjectileMeleeBase : public AAbilityProjectileBase
{
	GENERATED_BODY()
	
public:	
	AAbilityProjectileMeleeBase();
};
