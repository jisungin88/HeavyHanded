#include "Core/PlayerControllers/HeistPlayerController.h"

#include "GameFramework/PlayerState.h"

#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Core/HeistLog.h"

bool AHeistPlayerController::Server_ConfirmResult_Validate()
{
	// 인자가 없어서 악의적으로 만들 수 있는 값 자체가 없다.
	// 규약상 Server RPC 에는 WithValidation 을 붙이므로 자리만 지킨다
	return true;
}

void AHeistPlayerController::Server_ConfirmResult_Implementation()
{
	AHeistGameState* GS = AHeistGameState::Get(this);
	if (!GS)
	{
		UE_LOG(LogHeist, Warning,
			TEXT("결과 확인을 받았지만 작업 레벨이 아닙니다 — AHeistGameState 가 없습니다."));
		return;
	}

	// 결과 화면이 뜨기 전에는 확인할 것이 없다. 미리 눌린 확인이 남아 있으면
	// 결과 페이즈에 들어가는 순간 아무도 화면을 못 본 채로 매치가 끝난다
	if (!GS->IsPhase(HHTags::Phase_Result))
	{
		UE_LOG(LogHeist, Warning,
			TEXT("결과 확인 무시 — 결과 페이즈가 아닙니다 (현재 %s)."),
			*GS->GetCurrentPhase().ToString());
		return;
	}

	APlayerState* PS = GetPlayerState<APlayerState>();
	if (!PS)
	{
		UE_LOG(LogHeist, Warning, TEXT("결과 확인 무시 — PlayerState 가 없습니다."));
		return;
	}

	GS->SetResultConfirmed(PS, true);
}
