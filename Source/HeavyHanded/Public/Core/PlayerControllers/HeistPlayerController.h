#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HeistPlayerController.generated.h"

/**
 * 작업 레벨(저택 · 박물관 · 은행)의 PlayerController.
 *
 * **[임시 클래스다]** 지금 하는 일은 결과 화면의 '확인' 버튼을 서버로 넘기는 것 하나뿐이다.
 *   세션 · 코어 루프 파트가 작업 레벨용 컨트롤러를 정식으로 만들면 이 클래스는
 *   거기로 흡수되거나 부모가 바뀐다. 그때까지 자리를 잡아 두는 껍데기다.
 *
 * [왜 컨트롤러가 필요한가] AHeistGameState 는 소유자가 없어서 클라이언트의 Server RPC 를
 *   받지 못한다. 클라이언트에서 서버로 무언가를 보내려면 그 클라이언트가 소유한 액터가
 *   필요하고, PlayerController 가 그 자리다.
 *   (AHeistGameState::SetResultConfirmed 주석이 이 경로를 UI 파트 몫으로 남겨 두었다.)
 *
 * [여기에 무엇을 더 넣지 말 것] 입력 · 카메라 · 어빌리티는 ABaseCharacter 와
 *   APlayerSessionState 가 이미 맡고 있다. 이 클래스가 커지면 정식 컨트롤러가 생길 때
 *   옮겨야 할 짐만 늘어난다.
 */
UCLASS()
class HEAVYHANDED_API AHeistPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/**
	 * 결과 화면을 확인했다고 서버에 알린다.
	 *
	 * 남아 있는 사람이 전부 확인하면 체류 시간을 기다리지 않고 매치가 끝난다.
	 * 결과 페이즈가 아니면 조용히 무시한다 — 정상 범위의 잘못된 호출이라
	 * 접속을 끊을 일이 아니다 (문서 02 — _Validate 는 악의적 입력만 막는다).
	 */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Heist|Result")
	void Server_ConfirmResult();
};
