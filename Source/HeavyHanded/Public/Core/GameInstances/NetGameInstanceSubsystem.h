// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Subsystems/GameInstanceSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataTable.h"

#include "NetGameInstanceSubsystem.generated.h"

/**
 * 
 */

class UTextBlock;

USTRUCT(BlueprintType, Category = "Debug")
struct FDebugErrorInfo : public FTableRowBase
{
	GENERATED_BODY()

	// 에러 코드
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString ErrorCode;

	// 에러가 발생한 시스템 또는 함수
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Context;

	// 사용자에게 보여줄 에러 메시지
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Message;

	// 개발자용 상세 에러 메시지
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DeveloperMessage;
};



UCLASS()
class HEAVYHANDED_API UNetGameInstanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()


public:

	// -------------------------- 방 정보 설정 -----------------------------------

	// 현재 참가한 방 제목
	UPROPERTY(BlueprintReadWrite, Category = "Room")
	FString JoinedRoomName;

	UPROPERTY(BlueprintReadWrite, Category = "Room")
	FString JoinedRoomCode;

	UPROPERTY(BlueprintReadWrite, Category = "Room")
	int32 MaxPlayers = 0;


	UFUNCTION(BlueprintPure, Category = "Room")
	FString GetJoinedRoomName() const;

	UFUNCTION(BlueprintPure, Category = "Room")
	FString GetJoinedRoomCode() const;

	UFUNCTION(BlueprintPure, Category = "Room")
	int32 GetRoomMaxPlayer() const;




	// ------------------------------ 디버그용 -----------------------------------

public:

	// 디버그 메시지를 저장하고 Output Log에 출력
	// bError가 true면 Error 로그로 출력
	// bError가 false면 Warning 로그로 출력
	void DebugInstMessage(const FString& Message, bool bError);


	// 현재까지 저장된 모든 디버그 로그 반환
	const TArray<FString>& GetDebugLogs() const
	{
		return DebugLogs;
	}


	// 저장된 디버그 로그 전체 삭제
	void ClearDebugLogs()
	{
		DebugLogs.Empty();
	}


	// ------------------------------ 디버그 UI -----------------------------------

	// 디버그 에러 UI로 사용할 위젯 클래스
	// 블루프린트에서 WBP_DebugError 같은 위젯을 지정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	TSubclassOf<UUserWidget> DebugErrorWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "Debug")
	void SetDebugErrorWidgetClass(TSubclassOf<UUserWidget> WidgetClass);


	// 저장된 디버그 로그를 UI에 표시
	void ShowDebugErrorUI(const TArray<FString>& Logs);


	// ------------------------------ 에러 정보 -----------------------------------

	// 에러 코드가 저장된 DataTable
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	UDataTable* ErrorCodeTable;

	// 에러 코드에 해당하는 정보를 DataTable에서 찾아 저장
	void RecordError(const FString& ErrorCode);


	// 마지막으로 기록된 에러 정보 반환
	const FDebugErrorInfo& GetLastError() const
	{
		return LastError;
	}


private:

	// 마지막으로 발생한 에러 정보
	UPROPERTY()
	FDebugErrorInfo LastError;


	// 지금까지 저장된 디버그 로그
	UPROPERTY()
	TArray<FString> DebugLogs;


};
