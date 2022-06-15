// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "AudioModuleTypes.generated.h"

class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSoundsPlayFinishedDelegate);
DECLARE_DYNAMIC_DELEGATE(FOnSoundsPlayFinishedSingleDelegate);
DECLARE_DYNAMIC_DELEGATE(FOnSoundPlayFinishedDelegate);

UENUM(BlueprintType)
enum class ESoundState : uint8
{
	None,
	Playing,
	Pausing,
	Stopped
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FBGSoundInfo
{
	GENERATED_USTRUCT_BODY()

public:
	FBGSoundInfo()
	{
		Sound = nullptr;
		Volume = 1.0f;
		VolumeScale = 1.0f;
		bUISound = false;
		bLoopSound = true;
		SoundState = ESoundState::None;
	}
	
	FBGSoundInfo(USoundBase* InSound, ESoundState InInitSoundState = ESoundState::None, float InVolume = 1.0f, bool bInLoopSound = true, bool bInUISound = false)
	{
		Sound = InSound;
		Volume = InVolume;
		VolumeScale = 1.0f;
		bLoopSound = bInLoopSound;
		bUISound = bInUISound;
		SoundState = InInitSoundState;
	}
	
public:
	// 声音
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundBase* Sound;
	
	// 音量大小
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Volume;
	
	// 音量缩放
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float VolumeScale;

	// 是否是2D声音
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bUISound;
		
	// 是否是循环声音
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bLoopSound;
	
	// 声音播放状态
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	ESoundState SoundState;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSingleSoundInfo
{
	GENERATED_USTRUCT_BODY()

public:
	FSingleSoundInfo()
	{
		Sound = nullptr;
		Volume = 1.0f;
		VolumeScale = 1.0f;
		bUISound = false;
		Location = FVector::ZeroVector;
	}
	
	FSingleSoundInfo(USoundBase* InSound, float InVolume = 1.0f, bool bInUISound = false, FVector InLocation = FVector::ZeroVector)
	{
		Sound = InSound;
		Volume = InVolume;
		VolumeScale = 1.0f;
		bUISound = bInUISound;
		Location = InLocation;
	}
	
public:
	// 声音
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	USoundBase* Sound;
	
	// 音量大小
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	float Volume;
	
	// 音量缩放
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	float VolumeScale;

	// 是否是2D声音
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	bool bUISound;

	// 位置
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	FVector Location;
};
