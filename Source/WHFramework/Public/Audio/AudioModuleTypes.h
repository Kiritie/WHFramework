// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "AudioModuleTypes.generated.h"

class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSoundsPlayFinishedDelegate);
DECLARE_DYNAMIC_DELEGATE(FOnSoundsPlayFinishedSingleDelegate);
DECLARE_DYNAMIC_DELEGATE(FOnSoundPlayFinishedDelegate);

UENUM(BlueprintType)
enum class EBGSoundState : uint8
{
	None,
	Playing,
	Pausing,
	Stopped
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FBGSoundParams
{
	GENERATED_USTRUCT_BODY()

public:
	FBGSoundParams()
	{
		BGSound = nullptr;
		BGVolume = 1.0f;
		bIsUISound = false;
		bIsLoopSound = true;
		BGSoundState = EBGSoundState::None;
	}
	
	FBGSoundParams(USoundBase* InBGSound, EBGSoundState InInitSoundState = EBGSoundState::None, float InBGVolume = 1.0f, bool bInIsLoopSound = true, bool bInIsUISound = false)
	{
		BGSound = InBGSound;
		BGVolume = InBGVolume;
		bIsLoopSound = bInIsLoopSound;
		bIsUISound = bInIsUISound;
		BGSoundState = InInitSoundState;
	}
	
public:
	// 背景音乐
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundBase* BGSound;
	
	// 音量大小
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float BGVolume;

	// 是否是2D声音
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsUISound;
		
	// 是否是循环声音
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsLoopSound;
	
	// 背景声音播放状态
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EBGSoundState BGSoundState;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSingleSoundParams
{
	GENERATED_USTRUCT_BODY()

public:
	FSingleSoundParams()
	{
		Sound = nullptr;
		Volume = 1.0f;
		bIsUISound = false;
		Location = FVector::ZeroVector;
	}
	
	FSingleSoundParams(USoundBase* InSound, float InVolume = 1.0f, bool bInIsUISound = false, FVector InLocation = FVector::ZeroVector)
	{
		Sound = InSound;
		Volume = InVolume;
		bIsUISound = bInIsUISound;
		Location = InLocation;
	}
	
public:
	// 声音
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundBase* Sound;
	
	// 音量大小
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Volume;

	// 是否是2D声音
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsUISound;

	// 位置
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector Location;
};
