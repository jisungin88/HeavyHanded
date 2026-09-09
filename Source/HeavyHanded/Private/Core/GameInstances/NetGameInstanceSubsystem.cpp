// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameInstances/NetGameInstanceSubsystem.h"
//#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"


FString UNetGameInstanceSubsystem::GetJoinedRoomName() const
{
	return JoinedRoomName;
}

FString UNetGameInstanceSubsystem::GetJoinedRoomCode() const
{
	return JoinedRoomCode;
}

int32 UNetGameInstanceSubsystem::GetRoomMaxPlayer() const
{
	return MaxPlayers;
}




// ------------------------------ 디버그 로그 ------------------------------

// 디버그 메시지를 로그 배열에 저장하고 Output Log에 출력
// bError가 true면 Error 로그, false면 Warning 로그로 출력
void UNetGameInstanceSubsystem::DebugInstMessage(const FString& Message, bool bError)
{
	DebugLogs.Add(Message);

	if (bError)
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
	}
}

void UNetGameInstanceSubsystem::SetDebugErrorWidgetClass(TSubclassOf<UUserWidget> WidgetClass)
{
	DebugErrorWidgetClass = WidgetClass;
}

// 디버그 에러 UI를 생성하고 저장된 로그를 TextBlock에 표시
void UNetGameInstanceSubsystem::ShowDebugErrorUI(const TArray<FString>& Logs)
{
	// 디버그 UI 클래스가 지정되어 있는지 확인
	if (!DebugErrorWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[DebugUI] DebugErrorWidgetClass가 설정되지 않았습니다."));
		return;
	}

	// 현재 월드가 존재하는지 확인
	UWorld* World = GetWorld();

	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[DebugUI] World를 찾을 수 없습니다."));
		return;
	}

	// 디버그 에러 위젯 생성
	UUserWidget* DebugWidget = CreateWidget<UUserWidget>(World, DebugErrorWidgetClass);

	if (!DebugWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[DebugUI] DebugErrorWidget 생성에 실패했습니다."));
		return;
	}

	// 로그 배열을 하나의 문자열로 합침
	FString CombinedLogs;

	for (const FString& Log : Logs)
	{
		CombinedLogs += Log;
		CombinedLogs += TEXT("\n");
	}

	// 위젯 내부에서 이름이 DebugLogText인 TextBlock 찾기
	UTextBlock* DebugLogText = Cast<UTextBlock>(
		DebugWidget->GetWidgetFromName(TEXT("DebugLogText"))
	);

	if (!DebugLogText)
	{
		UE_LOG(LogTemp, Error, TEXT("[DebugUI] DebugLogText를 찾을 수 없습니다."));
		DebugWidget->RemoveFromParent();
		return;
	}

	// TextBlock에 로그 출력
	DebugLogText->SetText(FText::FromString(CombinedLogs));

	// 최상위 레이어에 UI 표시
	DebugWidget->AddToViewport(9999);

	UE_LOG(LogTemp, Warning, TEXT("[DebugUI] 디버그 에러 UI를 표시했습니다."));
}



// 에러 발생 정보를 저장하고 Output Log에 에러 내용 출력
// ErrorCode : 에러 종류
// Context   : 에러가 발생한 함수 또는 시스템
// Message   : 구체적인 에러 원인
void UNetGameInstanceSubsystem::RecordError(const FString& ErrorCode)
{
	// DataTable이 없으면 종료
	if (!ErrorCodeTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[RunError] ErrorCodeTable이 설정되지 않았습니다."));
		return;
	}

	// ErrorCode에 해당하는 DataTable Row 찾기
	const FDebugErrorInfo* ErrorData = ErrorCodeTable->FindRow<FDebugErrorInfo>(
		FName(*ErrorCode),
		TEXT("RecordError")
	);

	// 해당 에러 코드가 없으면 종료
	if (!ErrorData)
	{
		UE_LOG(LogTemp, Error, TEXT("[RunError] 존재하지 않는 에러 코드: %s"), *ErrorCode);
		return;
	}

	// DataTable의 정보를 마지막 에러로 저장
	LastError = *ErrorData;

	// 로그 생성
	const FString FullMessage = FString::Printf(
		TEXT("[%s] [%s] %s | Developer: %s"),
		*ErrorData->ErrorCode,
		*ErrorData->Context,
		*ErrorData->Message,
		*ErrorData->DeveloperMessage.ToString()
	);

	// 로그 저장
	DebugLogs.Add(FullMessage);

	// Output Log 출력
	UE_LOG(LogTemp, Error, TEXT("[RunError] %s"), *FullMessage);
}
