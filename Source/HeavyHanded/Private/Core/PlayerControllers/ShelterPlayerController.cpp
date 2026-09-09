#include "Core/PlayerControllers/ShelterPlayerController.h"
#include "Core/GameStates/ShelterGameState.h"
#include "Core/PlayerStates/ShelterPlayerState.h"
#include "Core/GameInstances/NetGameInstanceSubsystem.h"
#include "Core/HeistLog.h"   // 직업 확정 → 역할 기록 → 폰 스폰을 한 카테고리로 따라간다
#include "Core/RunProgressSubsystem.h"

#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"

#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Kismet/GameplayStatics.h"

#include "Character/CharacterSettings.h"

#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "UI/Shelter/ShelterHUD.h"

#include "Core/GameModes/ShelterGameMode.h"

void AShelterPlayerController::BeginPlay()
{
    Super::BeginPlay();

	AddShelterMappingContext();

	UNetGameInstanceSubsystem* NetSubsystem =
		GetGameInstance()->GetSubsystem<UNetGameInstanceSubsystem>();

	if (!NetSubsystem)
	{
		return;
	}

	const FString& RoomName =
		NetSubsystem->JoinedRoomName;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Lobby Room Name = %s"),
		*RoomName
	);


	UE_LOG(LogTemp, Warning, TEXT("=== AFTER BEGIN PLAY ==="));
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("=== AFTER BEGIN PLAY ==="));



}

void AShelterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		UE_LOG(LogHeist, Warning,
		       TEXT("[은신처] InputComponent 가 UEnhancedInputComponent 가 아닙니다. "
			       "Project Settings > Input > Default Player Input Class 를 확인하세요."));
		return;
	}

	if (!ChatAction)
	{
		UE_LOG(LogHeist, Warning,
		       TEXT("[은신처] ChatAction 이 비어 있어 채팅 키를 붙이지 못했습니다. "
			       "BP_ShelterPlayerController 의 Class Defaults 에서 IA_Chat 을 지정하세요."));
		return;
	}

	EIC->BindAction(ChatAction, ETriggerEvent::Started, this, &AShelterPlayerController::HandleChatKey);
}

void AShelterPlayerController::AddShelterMappingContext()
{
	if (!IsLocalController())
	{
		return;
	}

	ULocalPlayer* LP = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Sub =
		LP ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP) : nullptr;

	if (!Sub || !ShelterMappingContext)
	{
		UE_LOG(LogHeist, Warning,
					  TEXT("[은신처] 입력 컨텍스트를 붙이지 못했습니다 (Subsystem=%d, IMC=%d). "
							   "BP_ShelterPlayerController 의 Class Defaults 에서 IMC_Shelter 를 지정하세요."),
					  Sub != nullptr, ShelterMappingContext != nullptr);
		return;
	}

	Sub->AddMappingContext(ShelterMappingContext, ShelterInputPriority);
}

void AShelterPlayerController::HandleChatKey()
{
	AShelterHUD* HUD = Cast<AShelterHUD>(GetHUD());
	if (!HUD)
	{
		UE_LOG(LogHeist, Warning,
		       TEXT("[은신처] HUD 가 AShelterHUD 가 아닙니다. "
			       "GM_ShelterGameMode 의 HUDClass 를 확인하세요."));
		return;
	}

	HUD->SetChatFocused(true);
}

/**
 * 내 PlayerState. **아직 안 왔으면 nullptr 이다** — 호출부가 반드시 검사할 것.
 *
 * [여기 있던 화면 디버그 출력을 지웠다]
 *   `GetPlayerState<>()->GetName()` 을 검사 없이 불러서, 방에 막 참가한 클라이언트가
 *   접속하자마자 터졌다 (EXCEPTION_ACCESS_VIOLATION, 0x18 = UObject 의 NamePrivate 오프셋).
 *
 *   PlayerState 는 PlayerController 보다 늦게 도착한다. 접속 직후 한동안 nullptr 인 것이
 *   정상이고, 그 창을 밟는 것은 예외가 아니라 규칙이다 (문서 02 8장 — "폰은 있는데 ASC 가
 *   nullptr" 과 같은 상황이다).
 *
 *   덧붙여 이 함수는 `BlueprintPure` 다. BP 의 pure 노드는 **연결된 곳마다 다시 실행되므로**
 *   여기 부수효과(화면 출력)를 두면 프레임마다 여러 번 돈다 (문서 03 4장).
 *   조회 함수는 조회만 한다 — 디버그가 필요하면 별도의 non-pure 함수로 뺄 것.
 */
AShelterPlayerState* AShelterPlayerController::GetMyPlayerState() const
{
	AShelterPlayerState* PS = GetPlayerState<AShelterPlayerState>();

	//FString Message = FString::Printf(TEXT("[PC GetMyPS] PC=%p / PS=%p / %s"), this, PS, PS ? *PS->GetName() : TEXT("NULL"));

	//if (GEngine)
	//{
	//	//GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Cyan, Message);
	//}

	return PS;
	//return GetPlayerState<AShelterPlayerState>();
}

void AShelterPlayerController::LeaveRoom(FName MapName)
{
	// 완료 콜백에서 사용할 맵 이름 저장
	LeaveMapName = MapName;


	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();

	if (!Subsystem)
	{
		UGameplayStatics::OpenLevel(this, MapName);
		return;
	}

	IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();

	if (!SessionInterface.IsValid())
	{
		UGameplayStatics::OpenLevel(this, MapName);
		return;
	}

	LeaveSessionCompleteHandle =
		SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(
				this,
				&AShelterPlayerController::OnLeaveSessionComplete
			)
		);

	// 세션 삭제 요청
	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
			LeaveSessionCompleteHandle
		);

		UE_LOG(LogTemp, Warning, TEXT("DestroySession request failed."));
		return;
	}


}

void AShelterPlayerController::OnLeaveSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();

	if (Subsystem)
	{
		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();

		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
				LeaveSessionCompleteHandle
			);
		}
	}

	if (!bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("DestroySession failed."));
		return;
	}

	// 로비 나가기 성공
	OnLeaveRoomComplete.Broadcast();

	UE_LOG(LogTemp, Warning, TEXT("LeaveMapName = %s"), *LeaveMapName.ToString());
	UGameplayStatics::OpenLevel(this, LeaveMapName);
}





//void AShelterPlayerController::Client_ReceiveChatMessage(const FString& PlayerName, const FString& Message)
//{
//}



void AShelterPlayerController::Client_ReceiveChatMessage_Implementation(const FString& PlayerName, const FString& Message)
{
	OnChatMessageReceived.Broadcast(PlayerName, Message);
}

bool AShelterPlayerController::Server_SendChatMessage_Validate(const FString& Message)
{
	// 검증에 실패하면 엔진이 해당 클라이언트의 접속을 끊는다.
	// 그래서 "빈 문자열" 같은 정상 범위의 잘못된 입력은 여기서 막지 않는다 —
	// 그건 아래 _Implementation 에서 조용히 무시한다.
	// 여기서 막는 것은 악의적인 입력뿐이다.
	return Message.Len() <= MaxChatMessageLength;
}

void AShelterPlayerController::Server_SendChatMessage_Implementation(const FString& Message)
{
	if (Message.TrimStartAndEnd().IsEmpty())
	{
		return;
	}

	AShelterPlayerState* PS = GetPlayerState<AShelterPlayerState>();
	if (!PS)
	{
		return;
	}

	FString PlayerName = PS->GetPlayerName();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AShelterPlayerController* PC =
			Cast<AShelterPlayerController>(It->Get());

		if (PC)
		{
			PC->Client_ReceiveChatMessage(PlayerName, Message);
		}
	}

}

// -----------------------------------------------------------------


void AShelterPlayerController::ServerSelectJob_Implementation(EJobType NewJob)
{

	AShelterGameState* GameState =
		GetWorld()->GetGameState<AShelterGameState>();

	AShelterPlayerState* MyPlayerState =
		GetPlayerState<AShelterPlayerState>();

	if (GEngine)
	{
		FString Message = FString::Printf(
			TEXT("[1 PC] PS=%p / %s / Authority=%s"),
			MyPlayerState,
			MyPlayerState ? *MyPlayerState->GetName() : TEXT("NULL"),
			HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT")
		);

		//GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow, Message);
	}

	if (!GameState || !MyPlayerState)
	{
		return;
	}

	const bool bSuccess =
		GameState->SelectJob(MyPlayerState, NewJob);

	if (GEngine)
	{
		FString Message = FString::Printf(
			TEXT("[1 PC] SelectJob 결과: %s"),
			bSuccess ? TEXT("SUCCESS") : TEXT("FAIL")
		);

		GEngine->AddOnScreenDebugMessage(-1, 10.f,bSuccess ? FColor::Green : FColor::Red,Message);
	}


}

void AShelterPlayerController::ServerClearJob_Implementation()
{
	AShelterGameState* GameState = GetWorld()->GetGameState<AShelterGameState>();
	if (!GameState)
	{
		return;
	}

	AShelterPlayerState* MyPlayerState = GetPlayerState<AShelterPlayerState>();
	if (!MyPlayerState)
	{
		return;
	}

	GameState->ClearJob(MyPlayerState);
}

bool AShelterPlayerController::serverConfirmedJob_Validate(const FString& Nickname)
{
	return Nickname.Len() <= AShelterGameState::MaxNicknameLength * 4;
}

void AShelterPlayerController::serverConfirmedJob_Implementation(const FString& Nickname)
{
	/*
	URunProgressSubsystem* Subsystem = GetGameInstance()->GetSubsystem<URunProgressSubsystem>();

	// config/Role.ini 참고할 것

	//	"Role.Brute"
	//	"Role.Ghost"
	//	"Role.Oracle"
	//	"Role.Mimic"


	FUniqueNetIdRepl PlayerId = GetMyPlayerState()->GetUniqueId();
	FGameplayTag RoleTag = RoleTagFromJobType(GetMyPlayerState()->SelectedJob);

	if (Subsystem)
	{
		Subsystem->TrySelectRole(PlayerId, RoleTag);
	}


	// --------------------------------------------------------

	AShelterPlayerState* ShelterPS = GetPlayerState<AShelterPlayerState>();

	if (!ShelterPS)
	{
		return;
	}

	// pawn 스폰
	SpawnJobPawn(RoleTag);

	*/

	// 0903 디버그용

	DebugMessage(TEXT("1. serverConfirmedJob 시작"));

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		DebugMessage(TEXT("ERROR: GameInstance == nullptr"), true);
		return;
	}

	URunProgressSubsystem* Subsystem = GameInstance->GetSubsystem<URunProgressSubsystem>();
	if (!Subsystem)
	{
		DebugMessage(TEXT("ERROR: Subsystem == nullptr"), true);
		return;
	}

	DebugMessage(TEXT("2. Subsystem 가져옴"));

	AShelterPlayerState* ShelterPS = GetPlayerState<AShelterPlayerState>();
	if (!ShelterPS)
	{
		DebugMessage(TEXT("ERROR: ShelterPS == nullptr"), true);
		return;
	}

	DebugMessage(TEXT("3. ShelterPS 가져옴"));

	FUniqueNetIdRepl PlayerId = ShelterPS->GetUniqueId();

	if (!PlayerId.IsValid())
	{
		DebugMessage(TEXT("ERROR: PlayerId가 유효하지 않음"), true);
		return;
	}

	DebugMessage(FString::Printf(TEXT("4. PlayerId 가져옴 — %s"), *PlayerId.ToDebugString()));

	AShelterGameMode* GM = GetWorld()->GetAuthGameMode<AShelterGameMode>();
	if (!GM)
	{
		DebugMessage(TEXT("ERROR: ShelterGameMode 를 찾지 못함"), true);
		return;
	}

	ENicknameError NickError = ENicknameError::None;
	if (!GM->TryApplyNickname(this, Nickname, NickError))
	{
		DebugMessage(FString::Printf(TEXT("닉네임 거절 — %s"), *UEnum::GetValueAsString(NickError)), true);
		ShelterPS->ReportNicknameError(NickError);
		return;
	}

	FGameplayTag RoleTag = RoleTagFromJobType(ShelterPS->SelectedJob);

	if (!RoleTag.IsValid())
	{
		DebugMessage(TEXT("ERROR: RoleTag가 유효하지 않음"), true);
		return;
	}

	DebugMessage(FString::Printf(TEXT("5. RoleTag 생성 완료 — %s (직업 %s)"),
		*RoleTag.ToString(), *UEnum::GetValueAsString(ShelterPS->SelectedJob)));

	DebugMessage(TEXT("6. TrySelectRole 호출 전"));

	Subsystem->TrySelectRole(PlayerId, RoleTag);

	DebugMessage(TEXT("7. TrySelectRole 호출 완료"));

	DebugMessage(TEXT("8. SpawnJobPawn 호출 전"));

	SpawnJobPawn(RoleTag);

	DebugMessage(TEXT("9. SpawnJobPawn 호출 완료"));

	// 여기까지 왔다는 건 위 가드를 다 통과했다는 뜻이다. 이 시점에서만 "확정"이 선다.
	//
	// RPC 가 아니라 복제 프로퍼티인 이유 — 확정 여부는 연출이 아니라 상태다.
	// RPC 는 그 순간 받은 사람에게만 도달하므로, 접속 타이밍이나 재입장에서
	// 클라이언트만 선택 화면에 남는 사고가 난다 (문서 02 / CLAUDE.md 3절)
	ShelterPS->SetJobConfirmed(true);

	// 팝업 닫기 연출용. 페이지 전환 자체는 위 bJobConfirmed 복제가 담당한다
	ClientHideJobSelect();

	DebugMessage(TEXT("10. ClientHideJobSelect 호출 완료"));
}


void AShelterPlayerController::SpawnJobPawn(FGameplayTag JobTag)
{

	// 1. 권한 체크
	if (!HasAuthority())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,TEXT("1.[SpawnJobPawn] HasAuthority() == FALSE"));
		}

		return;
	}


	// 2. GameplayTag에 해당하는 Pawn 클래스 찾기

	//TSubclassOf<APawn>* FoundPawnClass = JobPawnMap.Find(JobTag);
	//
	//if (!FoundPawnClass || !(*FoundPawnClass))
	//{
	//	if (GEngine)
	//	{
	//		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,FString::Printf(TEXT("Error : [SpawnJobPawn] No Pawn class for JobTag [%s]"), *JobTag.ToString()));
	//	}
	//
	//	return;
	//}
	//
	//TSubclassOf<APawn> PawnClass = *FoundPawnClass;

	//0903 수정
	const UCharacterSettings* CharacterSettings =
		GetDefault<UCharacterSettings>();

	UClass* PawnClass =
		CharacterSettings->ResolveRolePawnClass(JobTag);

	if (!PawnClass)
	{
		UE_LOG(LogTemp, Error, TEXT("RoleTag [%s]에 해당하는 PawnClass가 없습니다."),*JobTag.ToString());

		return;
	}




	// 3. 기존 Pawn의 위치/회전 저장
	FVector JobSpawnLocation = FVector::ZeroVector;
	FRotator JobSpawnRotation = FRotator::ZeroRotator;
	bool bFoundPlayerStart = false;

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* PlayerStart = *It;

		if (PlayerStart && PlayerStart->Tags.Contains(JobTag.GetTagName()))
		{
			JobSpawnLocation = PlayerStart->GetActorLocation();
			JobSpawnRotation = PlayerStart->GetActorRotation();
			bFoundPlayerStart = true;
			break;
		}
	}

	if (!bFoundPlayerStart)
	{
		// 화면에만 남기면 Shipping 에서 사라지고, 4인 테스트에서는 누구 것인지도 알 수 없다
		DebugMessage(FString::Printf(
			TEXT("SpawnJobPawn 중단 — 태그 %s 가 붙은 PlayerStart 가 레벨에 없습니다."),
			*JobTag.ToString()), true);

		return;
	}


	// 4. 새로운 Pawn Spawn
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = nullptr;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* NewPawn = GetWorld()->SpawnActor<APawn>(PawnClass, JobSpawnLocation, JobSpawnRotation, SpawnParams);

	if (!NewPawn)
	{
		DebugMessage(FString::Printf(
			TEXT("SpawnJobPawn 중단 — 폰 스폰 실패. 클래스 %s / 위치 %s"),
			*GetNameSafe(PawnClass), *JobSpawnLocation.ToCompactString()), true);

		return;
	}


	// 5. PlayerController가 새로운 Pawn 빙의
	Possess(NewPawn);

	// 성공도 로그에 남긴다. "저 사람만 폰이 없다" 를 확인하려면 성공한 쪽도 세어야 한다
	DebugMessage(FString::Printf(TEXT("SpawnJobPawn 성공 — 직업 %s / 폰 %s"),
		*JobTag.ToString(), *NewPawn->GetName()));


}

void AShelterPlayerController::ClientShowJobSelect_Implementation()
{
	BP_ShowJobSelect();
}

void AShelterPlayerController::ClientHideJobSelect_Implementation()
{
	BP_HideJobSelect();
}


//-------------------------------------------------------------------------------


void AShelterPlayerController::ServerSetEntryTag_Implementation(EEntryTag NewTag)
{
	// 서버의 GameState 가져오기
	AShelterGameState* GameState = GetWorld()->GetGameState<AShelterGameState>();
	if (!GameState)
	{
		return;
	}

	URunProgressSubsystem* Subsystem = GetGameInstance()->GetSubsystem<URunProgressSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	// 서버 GameState의 Entry 변경
	GameState->SetEntryTag(NewTag);

	FGameplayTag EntryTag;

	// config/Phase.ini 참고할 것
	switch (NewTag)
	{
	case EEntryTag::Front:
		// GameplayTagList = (Tag = "Entry.Mansion.Front",DevComment="저택 정문 — 시야 노출 높음, 도주로 많음")
		EntryTag = FGameplayTag::RequestGameplayTag(FName("Entry.Mansion.Front"));
		break;


	case EEntryTag::Garage:
		// GameplayTagList = (Tag = "Entry.Mansion.Garage", DevComment = "저택 지하 주차장 — 은폐 좋음, 내부 동선 김")
		EntryTag = FGameplayTag::RequestGameplayTag(FName("Entry.Mansion.Garage"));
		break;

	case EEntryTag::Alley:
		// GameplayTagList = (Tag = "Entry.Mansion.Alley", DevComment = "저택 뒷골목 — 경비 적음, 진입 후 좁은 통로")
		EntryTag = FGameplayTag::RequestGameplayTag(FName("Entry.Mansion.Alley"));
		break;

	default:
		EntryTag = FGameplayTag();
		break;
	}


	// Entry 정보도 Subsystem에 먼저 전달
	Subsystem->TrySelectEntry(EntryTag);

}

void AShelterPlayerController::ServerSetSiteTag_Implementation(ESiteTag NewTag)
{
	// 서버의 GameState 가져오기
	AShelterGameState* GameState = GetWorld()->GetGameState<AShelterGameState>();
	if (!GameState)
	{
		return;
	}


	 // 서버 GameState의 Site 변경
	GameState->SetSiteTag(NewTag);
}



void AShelterPlayerController::ClientShowStartGameWindow_Implementation()
{
	BP_ShowStartGameWindow();
}


void AShelterPlayerController::IngameTravel()
{


	AShelterGameState* GameState = GetWorld()->GetGameState<AShelterGameState>();
	if (!GameState)
	{
		return;
	}


	URunProgressSubsystem* Subsystem = GetGameInstance()->GetSubsystem<URunProgressSubsystem>();
	if (!Subsystem)
	{
		return;
	}


	// 로딩창 띄우기
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AShelterPlayerController* PC = Cast<AShelterPlayerController>(It->Get());

		if (PC)
		{
			PC->ClientShowStartGameWindow();
		}
	}



	ESiteTag CurrentSite = GameState->SiteTag;
	FGameplayTag SiteGT;

	// config/Phase.ini 참고할 것


	// 맵이 클리어 형식이라면 지금처럼 액터를 나눌 이유가 없음
	/*
	switch (CurrentSite)
	{
	case ESiteTag::Mansion:
		// GameplayTagList=(Tag="Site.Mansion",DevComment="저택 — 목표 $50,000 / 7분. 경비견, 삐걱거리는 마루")
		SiteGT = FGameplayTag::RequestGameplayTag(FName("Site.Mansion"));;
		break;

	case ESiteTag::Museum:
		//GameplayTagList = (Tag = "Site.Museum", DevComment = "박물관 — 목표 $120,000 / 8분. 레이저 센서, 감시 카메라")
		SiteGT = FGameplayTag::RequestGameplayTag(FName("Site.Museum"));;
		break;

	case ESiteTag::Bank:
		//GameplayTagList = (Tag = "Site.Bank", DevComment = "은행 — 목표 $250,000 / 9분. 압력판, 자동 셔터, 무장 경비")
		SiteGT = FGameplayTag::RequestGameplayTag(FName("Site.Bank"));;
		break;

	default:
		SiteGT = FGameplayTag();
		break;
	}
	*/

	SiteGT = FGameplayTag::RequestGameplayTag(FName("Site.Mansion"));;

	// TryDepartToSite 내부에서 서버 권한 검사 후 ServerTravel 실행
	Subsystem->TryDepartToSite(SiteGT);

}

void AShelterPlayerController::DebugMessage(const FString& Message, bool bError)
{
	// 화면 메시지만으로는 원인을 못 잡는다 — Shipping 에서는 아예 안 뜨고,
	// 여러 명이 동시에 확정하면 누가 보낸 단계인지도 구분되지 않는다.
	// 그래서 로그에도 남기고, 컨트롤러 이름을 같이 찍는다.
	//
	// LogHeist 를 쓰는 것은 이 흐름이 역할 확정 → RunProgress → 폰 스폰으로 이어져서,
	// "저 사람은 왜 폰이 없나" 를 한 필터로 따라갈 수 있어야 하기 때문이다
	if (bError)
	{
		UE_LOG(LogHeist, Warning, TEXT("[직업확정] %s — %s"), *GetName(), *Message);
	}
	else
	{
		UE_LOG(LogHeist, Log, TEXT("[직업확정] %s — %s"), *GetName(), *Message);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, bError ? FColor::Red : FColor::Yellow, Message);
	}
}



