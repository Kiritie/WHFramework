// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure_PlayMediaPlayerMovie.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API AProcedure_PlayMediaPlayerMovie : public AProcedureBase
{
	GENERATED_BODY()

	
public:
	AProcedure_PlayMediaPlayerMovie();

	//////////////////////////////////////////////////////////////////////////
	/// Procedure
public:
	virtual void ServerOnInitialize_Implementation() override;
	
	virtual void ServerOnEnter_Implementation(AProcedureBase* InLastProcedure) override;

	virtual void ServerOnLeave_Implementation(AProcedureBase* InNextProcedure) override;

protected:
	UFUNCTION()
	void ServerOnMoviePlayFinished(const FName& InMovieName);

	//////////////////////////////////////////////////////////////////////////
	/// Character
public:
	/// 角色名称
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Media")
	FName PlayerName;
	
	/// 视频播放名称
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Media")
	FName MovieName;
};
