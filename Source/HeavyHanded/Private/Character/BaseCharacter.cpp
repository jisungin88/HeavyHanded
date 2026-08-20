// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BaseCharacter.h"
#include "Character/PlayerSessionState.h"
#include "Character/BaseAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Interfaces/Carryable.h"   // 운반 상태를 노획물에게 알린다 (SetHeldActor)
#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.h"   // 노획물의 Loot.Type.* 태그를 읽는다 (IsCarryingHeavyItem)
#include "Core/HeavyHandedGameplayTags.h"

// 운반 동기화 진단용. 이 경로는 실패해도 예외가 없고 "클라에서 아이템이 그대로 있다"
// 로만 드러나서, 어디까지 도달했는지 로그 없이는 알 수 없다.
DEFINE_LOG_CATEGORY_STATIC(LogCarry, Log, All);

// Sets default values
ABaseCharacter::ABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    // --- 1인칭 카메라 설정 (스프링 암 관련 코드는 제거) ---
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(RootComponent);
    FollowCamera->bUsePawnControlRotation = true; // 마우스 회전에 따라 카메라가 같이 회전

    // 카메라 위치를 캐릭터의 눈높이(약 Z축 64cm 위)로 설정
    FollowCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));

    // --- 캐릭터 이동 및 회전 방향 설정 ---
    bUseControllerRotationYaw = true; // 1인칭은 시선과 몸통 방향을 일치시키기 위해 true로 설정
}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	// [컨트롤러가 없는 것은 정상이다] 시뮬레이티드 프록시(남의 캐릭터)에는 컨트롤러가 없고,
	// 서버의 원격 폰도 빙의 순서에 따라 이 시점에 비어 있을 수 있다.
	// 예전에는 여기서 Error 를 찍어 접속할 때마다 에러가 쌓였다.

	// 클라이언트든 서버든 '로컬 플레이어'인 경우에만 입력 매핑을 추가한다.
    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        // 이 캐릭터를 조종하는 로컬 플레이어인지 확인 (AI나 다른 플레이어 소유일 때 오류 방지)
        if (PlayerController->IsLocalController())
        {
            if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
                {
                    // 에디터에서 UPROPERTY로 IMC를 할당해 둔 경우 (예: DefaultMappingContext)
                    if (DefaultMappingContext)
                    {
                        Subsystem->AddMappingContext(DefaultMappingContext, 0);
                    }
                }
            }
        }
    }
}

void ABaseCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // 서버 권한에서 AbilityInputBindings에 등록된 모든 스킬들을 ASC에 부여합니다.
    if (HasAuthority())
    {
        APlayerSessionState* SessionState = GetPlayerState<APlayerSessionState>();
        if (SessionState)
        {
            UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
            if (ASC)
            {
                ASC->InitAbilityActorInfo(SessionState, this);

                int32 InputID = 0;
                for (const FAbilityInputBinding& Binding : AbilityInputBindings)
                {
                    if (Binding.AbilityClass)
                    {
                        // 배열 순서대로 고유 InputID 부여 (0, 1, 2, ...)
                        ASC->GiveAbility(FGameplayAbilitySpec(Binding.AbilityClass, 1, InputID, this));
                    }
                    ++InputID;
                }
            }
        }
    }

    // 서버 측에서 컨트롤러 소유가 끝났을 때 바인딩 실행
    BindAttributeDelegates();
}

// Called every frame
void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // 전후 이동 바인딩 (W, S)
        if (IA_MoveForward)
        {
            EnhancedInputComponent->BindAction(IA_MoveForward, ETriggerEvent::Triggered, this, &ABaseCharacter::MoveForward);
        }

        // 좌우 이동 바인딩 (A, D)
        if (IA_MoveRight)
        {
            EnhancedInputComponent->BindAction(IA_MoveRight, ETriggerEvent::Triggered, this, &ABaseCharacter::MoveRight);
        }

        // Turn (좌우) 바인딩
        if (IA_Turn)
        {
            EnhancedInputComponent->BindAction(IA_Turn, ETriggerEvent::Triggered, this, &ABaseCharacter::Turn);
        }

        // LookUp (상하) 바인딩
        if (IA_LookUp)
        {
            EnhancedInputComponent->BindAction(IA_LookUp, ETriggerEvent::Triggered, this, &ABaseCharacter::LookUp);
        }

        if (IA_Sprint)
        {
            EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Started, this, &ABaseCharacter::StartSprint);
            EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ABaseCharacter::StopSprint);
        }
        if (IA_Crouch)
        {
            EnhancedInputComponent->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ABaseCharacter::StartCrouch);
            EnhancedInputComponent->BindAction(IA_Crouch, ETriggerEvent::Completed, this, &ABaseCharacter::StopCrouch);
        }

        int32 InputID = 0;
        for (const FAbilityInputBinding& Binding : AbilityInputBindings)
        {
            if (Binding.InputAction && Binding.AbilityClass)
            {
                // 누를 때: 활성화 시도 + Pressed 상태 등록
                EnhancedInputComponent->BindAction(
                    Binding.InputAction,
                    ETriggerEvent::Started,
                    this,
                    &ABaseCharacter::AbilityInputPressed,
                    InputID
                );

                // 뗄 때: 해당 InputID를 가진 활성 어빌리티의 InputReleased 콜백 호출
                EnhancedInputComponent->BindAction(
                    Binding.InputAction,
                    ETriggerEvent::Completed,
                    this,
                    &ABaseCharacter::AbilityInputReleased,
                    InputID
                );
            }
            ++InputID;
        }
    }
}

UAbilitySystemComponent* ABaseCharacter::GetAbilitySystemComponent() const
{
    // --- 캐릭터 본인 대신 PlayerSessionState의 ASC를 리턴하도록 변경 ---
    if (APlayerSessionState* SessionState = GetPlayerState<APlayerSessionState>())
    {
        return SessionState->GetAbilitySystemComponent();
    }
    return nullptr;
}

// 2. 전진/후진 처리 함수 (Axis 1D 값 활용)
void ABaseCharacter::MoveForward(const FInputActionValue& Value)
{
    const float DirectionValue = Value.Get<float>();

    if (Controller != nullptr && DirectionValue != 0.0f)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

        AddMovementInput(Direction, DirectionValue);
    }
}

// 3. 좌우 이동 처리 함수 (Axis 1D 값 활용)
void ABaseCharacter::MoveRight(const FInputActionValue& Value)
{
    const float DirectionValue = Value.Get<float>();

    if (Controller != nullptr && DirectionValue != 0.0f)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        AddMovementInput(Direction, DirectionValue);
    }
}

void ABaseCharacter::Turn(const FInputActionValue& Value)
{
    float AxisValue = Value.Get<float>();
    if (AxisValue != 0.0f)
    {
        AddControllerYawInput(AxisValue);
    }
}

void ABaseCharacter::LookUp(const FInputActionValue& Value)
{
    float AxisValue = Value.Get<float>();
    if (AxisValue != 0.0f)
    {
        AddControllerPitchInput(AxisValue);
    }
}

// 예시: 캐릭터가 컨트롤러를 소유하거나 ASC가 초기화될 때 호출되는 함수 내부에서 바인딩 실행
void ABaseCharacter::BindAttributeDelegates()
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (!ASC) return;

    // ASC로부터 UBaseAttributeSet 가져오기
    const UBaseAttributeSet* BaseAttrSet = ASC->GetSet<UBaseAttributeSet>();
    if (BaseAttrSet)
    {
        // MovementSpeed 속성 변화를 감지하는 델리게이트 구독
        ASC->GetGameplayAttributeValueChangeDelegate(BaseAttrSet->GetMovementSpeedAttribute()).AddUObject(this, &ABaseCharacter::OnMovementSpeedChanged);
    }
}

// 속성이 변경될 때 자동 호출되어 실제 무브먼트 속도에 적용
void ABaseCharacter::OnMovementSpeedChanged(const FOnAttributeChangeData& Data)
{
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        // Data.NewValue는 변경된 MovementSpeed의 새로운 값입니다.
        MoveComp->MaxWalkSpeed = Data.NewValue;
    }
}

void ABaseCharacter::ApplyGameplayEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass)
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (ASC && EffectClass)
    {
        FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
        ContextHandle.AddSourceObject(this);
        ASC->ApplyGameplayEffectToSelf(EffectClass.GetDefaultObject(), 1.0f, ContextHandle);
    }
}

void ABaseCharacter::RemoveGameplayEffectFromSelf(TSubclassOf<UGameplayEffect> EffectClass)
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (ASC && EffectClass)
    {
        ASC->RemoveActiveGameplayEffectBySourceEffect(EffectClass, ASC);
    }
}

void ABaseCharacter::StartCrouch(const FInputActionValue& Value)
{
	if (IsCarryingHeavyItem())
	{
		UE_LOG(LogCarry, Log, TEXT("%s 는 중량형 노획물을 들고 있어 크라우치할 수 없다."), *GetName());
		return;
	}

	Crouch();
    Server_ApplyGameplayEffect(CrouchGameplayEffectClass, true);
}

void ABaseCharacter::StopCrouch(const FInputActionValue& Value)
{
	UnCrouch();
    Server_ApplyGameplayEffect(CrouchGameplayEffectClass, false);
}

void ABaseCharacter::StartSprint(const FInputActionValue& Value)
{
    Server_ApplyGameplayEffect(SprintGameplayEffectClass, true);
}

void ABaseCharacter::StopSprint(const FInputActionValue& Value)
{
    Server_ApplyGameplayEffect(SprintGameplayEffectClass, false);
}

// --- 서버 RPC 실제 동작 구현 ---
void ABaseCharacter::Server_ApplyGameplayEffect_Implementation(TSubclassOf<UGameplayEffect> EffectClass, bool bApply)
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (!ASC || !EffectClass) return;

    if (bApply)
    {
        FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
        ContextHandle.AddSourceObject(this);
        ASC->ApplyGameplayEffectToSelf(EffectClass.GetDefaultObject(), 1.0f, ContextHandle);
    }
    else
    {
        // 제거할 때 핸들 방식이거나 소스 이펙트 방식 사용
        ASC->RemoveActiveGameplayEffectBySourceEffect(EffectClass, ASC);
    }
}

void ABaseCharacter::AbilityInputPressed(int32 InputID)
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (!ASC) return;

    // InputID가 일치하는 어빌리티를 찾아 Pressed 상태로 등록.
    // 아직 활성화 안 된 어빌리티라면 내부적으로 TryActivateAbility까지 자동으로 처리해줌
    ASC->AbilityLocalInputPressed(InputID);
}

void ABaseCharacter::AbilityInputReleased(int32 InputID)
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (!ASC) return;

    // InputID가 일치하는 "활성 중인" 어빌리티의 InputReleased()를 호출
    ASC->AbilityLocalInputReleased(InputID);
}

void ABaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABaseCharacter, HeldActor);
	// 08.18 start
	DOREPLIFETIME(ABaseCharacter, HeavyCarryAssistant);
	DOREPLIFETIME(ABaseCharacter, AssistingPrimaryCarrier);
	DOREPLIFETIME(ABaseCharacter, AssistedHeavyItem);
	DOREPLIFETIME(ABaseCharacter, HeavyCarryState);
	// 08.18 end
	DOREPLIFETIME(ABaseCharacter, ReviveProgress);
}

bool ABaseCharacter::CanCarryActor(const AActor* Target) const
{
    if (!IsValid(Target))
    {
        return false;
    }

    const USceneComponent* Root = Target->GetRootComponent();
    if (!Root)
    {
        UE_LOG(LogCarry, Warning, TEXT("%s 에 루트 컴포넌트가 없어 운반할 수 없다."), *GetNameSafe(Target));
        return false;
    }

    // Static 모빌리티 컴포넌트는 Movable 부모에 붙일 수 없고 물리도 켤 수 없다.
    if (Root->Mobility != EComponentMobility::Movable)
    {
        UE_LOG(LogCarry, Warning,
            TEXT("%s 의 Mobility 가 Movable 이 아니라 운반할 수 없다. "
                 "레벨에서 해당 액터의 Transform > Mobility 를 Movable 로 바꿀 것."),
            *GetNameSafe(Target));
        return false;
    }

    // 남이 들고 있는 물건은 뺏을 수 없다.
    //
    // 노획물 쪽 ALootBase::CanBeCarriedBy 도 같은 것을 거부한다. 그런데 그 거부는
    // OnGrabbed 안에서 일어나고 OnGrabbed 는 void 라, SetHeldActor 까지 올라오지 않는다 —
    // 그 시점에는 어태치와 HeldActor 갱신이 이미 끝나 있다.
    //
    // 그래서 이 검사가 없으면 운반 상태가 두 곳으로 갈라진다. 물건은 내 메시에 붙고
    // 나는 들었다고 믿는데, PrimaryCarrier 는 원래 주인으로 남는다. 원래 주인이 놓는 순간
    // PrimaryCarrier 가 비워져서, 내 손에 들린 물건을 밴 적재존이 "아무도 안 들고 있다" 로
    // 보고 실어 버린다. LastCarrier 도 안 바뀌어 기여도까지 원래 주인에게 붙는다.
    //
    // 어태치 전에 막아야 그 갈라짐 자체가 생기지 않는다.
    if (const ICarryable* Carryable = Cast<ICarryable>(Target))
    {
        const APawn* Holder = Carryable->GetPrimaryCarrier();
        if (IsValid(Holder) && Holder != this)
        {
            UE_LOG(LogCarry, Log, TEXT("%s 는 %s 가 들고 있어 잡을 수 없다."),
                *GetNameSafe(Target), *GetNameSafe(Holder));
            return false;
        }
    }

    // 방금 내가 던진 물건은 잠깐 못 잡는다. 던지고 곧바로 낚아채는 반복으로
    // 중량형의 2인 필수 규칙과 착지 소음·파손 판정을 우회하는 것을 막는다.
    if (Target == RecentlyThrownActor && RecentlyThrownTime >= 0.f)
    {
        const float Elapsed = GetWorld()->GetTimeSeconds() - RecentlyThrownTime;
        if (Elapsed < RecatchBlockSeconds)
        {
            UE_LOG(LogCarry, Log,
                TEXT("%s 는 방금 던진 물건이라 %.2f초 뒤에야 다시 잡을 수 있다."),
                *GetNameSafe(Target), RecatchBlockSeconds - Elapsed);
            return false;
        }
    }

    return true;
}

void ABaseCharacter::BlockRecatch(AActor* ThrownActor)
{
    if (!HasAuthority() || !IsValid(ThrownActor))
    {
        return;
    }

    RecentlyThrownActor = ThrownActor;
    RecentlyThrownTime = GetWorld()->GetTimeSeconds();
}

void ABaseCharacter::ApplyCarryState(AActor* Target, bool bCarried)
{
    if (!IsValid(Target))
    {
        return;
    }

    // ICarryable(= 노획물)은 물리·콜리전·부착을 스스로 관리한다. 여기서 또 손대면 두 벌이 돈다.
    //
    // 특히 콜리전이 정반대였다. 아래 폴백 경로는 NoCollision 으로 통째로 끄는데,
    // ALootBase 는 CarriedLoot 프로파일(QueryOnly, 다른 캐릭터만 Block)로 바꿔 켜 둔다.
    // 그 프로파일이 '들고 있는 물건이 남을 막는다' 는 협동 방해의 근거라 꺼지면 안 되고,
    // 놓을 때의 SetCollisionEnabled(QueryAndPhysics)도 QueryOnly 프로파일을 어긋나게 남긴다.
    //
    // OnGrabbed/OnReleased 는 서버 전용이다. 클라이언트는 ALootBase 의
    // OnRep_PrimaryCarrier 가 똑같은 일을 하므로 여기서는 아무것도 하지 않는다.
    if (ICarryable* Carryable = Cast<ICarryable>(Target))
    {
        if (HasAuthority())
        {
            if (bCarried)
            {
                Carryable->OnGrabbed(this);
            }
            else
            {
                Carryable->OnReleased(this);
            }
        }

        return;
    }

    // 이하는 ICarryable 을 구현하지 않은 액터용 폴백이다.
    // (테스트 맵의 "Item" 태그 액터 등. 노획물이 전부 ALootBase 가 되면 지운다)
    UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Target->GetRootComponent());

    if (bCarried)
    {
        // 순서가 중요하다. 물리를 먼저 끄지 않으면 부착이 거부된다 —
        // 런타임에는 시뮬레이션 중인 바디를 웰드 없이 붙일 수 없다
        // (엔진 SceneComponent.cpp:2151, 이중 트랜스폼 갱신 방지).
        // 던진 직후의 아이템이 다시 집히지 않던 원인이 이것이었다.
        if (PrimComp)
        {
            PrimComp->SetSimulatePhysics(false);
            PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }

        Target->AttachToComponent(
            GetMesh(),
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            CarrySocketName);
    }
    else
    {
        Target->DetachFromActor(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));

        if (PrimComp)
        {
            // 콜리전을 먼저 켜야 물리 바디가 올바른 상태로 깨어난다
            PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            PrimComp->SetSimulatePhysics(true);
        }
    }
}

bool ABaseCharacter::SetHeldActor(AActor* NewHeldActor, bool bIsHeavyLoot)
{
    // 운반 상태의 소유권은 서버에 있다. 클라이언트가 직접 바꾸면
    // 다음 복제 때 덮어써지면서 물리 상태만 어긋난다.
    if (!HasAuthority())
    {
        return false;
    }

    if (HeldActor == NewHeldActor)
    {
        return true;
    }

    // 들고 있던 것을 먼저 놓는다.
    if (AActor* PreviousHeldActor = HeldActor)
    {
        // 노획물에게 알리는 것까지 여기서 끝난다 — ApplyCarryState 가 ICarryable 이면
        // OnReleased 를 부르고 바로 반환한다. 그쪽이 PrimaryCarrier 를 비우고
        // Loot.State.Dropped 를 붙이며, 그게 없으면 밴 적재존이 "아무도 안 들고 있다" 를
        // 영영 참으로 보고 압력판 같은 상태 기반 판정도 물건이 놓였다는 것을 모른다.
        //
        // 예전에는 여기서 OnReleased 를 한 번 더 불렀다. 코어 루프가 명시 호출을 넣은 것과
        // 노획물 파트가 ApplyCarryState 안으로 같은 호출을 옮긴 것이 각자 브랜치에서 진행돼
        // 병합 뒤 두 번 불리게 된 것이다. ALootBase::ApplyCarryState 의 멱등 가드가 막아 주고
        // 있었을 뿐, 그 가드가 없으면 ApplyDropImpulse 가 두 번 들어가 놓은 물건이 튀어 나간다.
        ApplyCarryState(PreviousHeldActor, false);

        UE_LOG(LogCarry, Log, TEXT("[서버] 운반 해제: %s"), *GetNameSafe(PreviousHeldActor));

		//08.18 Add
		RemoveHeavySoloPenalty();

		// 보조자가 있으면 AT_MonitorHeavyCarry 의 다음 틱 폴링을 기다리지 않고
		// Event.Loot.Dropped 로 즉시 통지해 보조 상태를 바로 정리시킨다.
		if (HeavyCarryAssistant)
		{
			FGameplayEventData EventData;
			EventData.Target = PreviousHeldActor;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
				HeavyCarryAssistant,
				HHTags::Event_Loot_Dropped,
				EventData);
		}
    }

    HeldActor = nullptr;
    HeavyCarryState = EHeavyCarryState::None;

    if (!NewHeldActor)
    {
        return true;
    }

    if (!CanCarryActor(NewHeldActor))
    {
        return false;
    }

    // 노획물에게 알리는 것까지 여기서 끝난다 — ApplyCarryState 가 ICarryable 이면
    // OnGrabbed 를 부르고 바로 반환한다. 그쪽이 PrimaryCarrier / LastCarrier 를 채우고
    // 소켓에 붙이며, 소지 중 콜리전 프로파일과 운반자 상호 무시까지 건다.
    //
    // **아래 두 검증이 이 호출의 결과를 본다.** 그래서 이 줄이 먼저여야 한다.
    ApplyCarryState(NewHeldActor, true);

    // 노획물은 자기가 요청을 거부할 수 있다 (이미 남이 들고 있음, 중량형 인원 부족 등).
    // 거부하면 부착 자체가 일어나지 않아 아래 검증에 걸리는데, 그쪽 경고는
    // "AttachTo 경고를 확인하라" 고 안내해서 원인을 엉뚱한 데서 찾게 만든다.
    // 거부는 정상 흐름이므로 여기서 따로 구분한다.
    if (const ICarryable* Carryable = Cast<const ICarryable>(NewHeldActor))
    {
        if (Carryable->GetPrimaryCarrier() != this)
        {
            UE_LOG(LogCarry, Log, TEXT("[서버] %s 가 운반 요청을 거부했다. (CanBeCarriedBy)"),
                *GetNameSafe(NewHeldActor));
            return false;
        }
    }

    // AActor::AttachToComponent 는 void 라 성공 여부를 돌려주지 않는다.
    // 확인 없이 넘기면 붙지도 않은 액터를 "들고 있다"고 믿게 되고,
    // 그 상태로 던지면 엉뚱한 곳에 임펄스가 들어간다.
    const USceneComponent* NewRoot = NewHeldActor->GetRootComponent();
    if (!NewRoot || NewRoot->GetAttachParent() != GetMesh())
    {
        UE_LOG(LogCarry, Warning,
            TEXT("[서버] %s 부착에 실패해 운반 상태로 넘기지 않는다. (직전 AttachTo 경고 확인)"),
            *GetNameSafe(NewHeldActor));

        // 물리를 되돌려 원래 상태로 남긴다.
        ApplyCarryState(NewHeldActor, false);
        return false;
    }

    // 여기까지 왔으면 노획물 쪽은 이미 운반 상태다. 위 ApplyCarryState 가 다 했고,
    // 두 검증은 그 결과가 실제로 반영됐는지만 확인한 것이다.
    HeldActor = NewHeldActor;

	// StartCrouch 입력 차단만으로는 "이미 웅크린 채로 줍는" 경우를 못 막는다.
	if (bIsCrouched && IsCarryingHeavyItem())
	{
		UnCrouch();
		RemoveGameplayEffectFromSelf(CrouchGameplayEffectClass);
		UE_LOG(LogCarry, Log, TEXT("[서버] %s 중량형 노획물을 집어 크라우치 강제 해제"), *GetName());
	}

	//08.18 수정 start
	if (IsCarryingHeavyItem())
	{
		ApplyHeavySoloPenalty();
		HeavyCarryState = EHeavyCarryState::Solo;
		UE_LOG(LogCarry, Log, TEXT("[서버] %s 는 무거운 물건 - 혼자 들어서 이동속도 패널티 적용"), *GetNameSafe(NewHeldActor));
	}
	//08.18 수정 end

    UE_LOG(LogCarry, Log, TEXT("[서버] 운반 시작: %s | Replicates=%s, ReplicateMovement=%s"),
        *GetNameSafe(NewHeldActor),
        NewHeldActor->GetIsReplicated() ? TEXT("O") : TEXT("X"),
        NewHeldActor->IsReplicatingMovement() ? TEXT("O") : TEXT("X"));

    // 복제되지 않는 액터는 클라이언트에서 HeldActor 참조 자체가 풀리지 않는다.
    if (!NewHeldActor->GetIsReplicated())
    {
        UE_LOG(LogCarry, Warning,
            TEXT("[서버] %s 는 복제되지 않는 액터다. 클라이언트에서는 손에 붙지 않는다 — "
                 "해당 액터의 Replicates(StaticMeshActor 는 Static Mesh Replicate Movement)를 켤 것."),
            *GetNameSafe(NewHeldActor));
    }

    return true;
}

void ABaseCharacter::OnRep_HeldActor(AActor* PreviousHeldActor)
{
    // 클라이언트도 서버와 똑같은 순서를 다시 밟는다.
    // 엔진의 AttachmentReplication 에 기대지 않는다 — 캐릭터와 아이템은 서로 다른
    // 액터 채널이라 도착 순서가 보장되지 않고, 부착이 먼저 오면 그 시점엔 아직
    // 물리가 켜져 있어 부착이 거부될 수 있다.
    ApplyCarryState(PreviousHeldActor, false);
    ApplyCarryState(HeldActor, true);

    const USceneComponent* HeldRoot = IsValid(HeldActor) ? HeldActor->GetRootComponent() : nullptr;

    UE_LOG(LogCarry, Log, TEXT("[클라] OnRep_HeldActor: %s -> %s | 부착 부모=%s"),
        *GetNameSafe(PreviousHeldActor),
        *GetNameSafe(HeldActor),
        HeldRoot ? *GetNameSafe(HeldRoot->GetAttachParent()) : TEXT("(루트 없음/참조 안 풀림)"));
}

//08.18 추가
void ABaseCharacter::OnRep_HeavyCarryAssistant()
{
	UE_LOG(LogCarry, Log, TEXT("[클라] OnRep_HeavyCarryAssistant: %s"), *GetNameSafe(HeavyCarryAssistant));
}

void ABaseCharacter::OnRep_AssistingPrimaryCarrier()
{
	UE_LOG(LogCarry, Log, TEXT("[클라] OnRep_AssistingPrimaryCarrier: %s"), *GetNameSafe(AssistingPrimaryCarrier));
}

bool ABaseCharacter::IsCarryingHeavyItem() const
{
	// "중량형인가" 는 Loot.Type.Heavy 태그로 판정한다.
	//
	// 예전에는 ICarryable::GetWeightClass() 를 봤는데, 노획물 파트가 무게 등급(EWeightClass)을
	// 없애면서 그 함수도 같이 사라졌다. 지금은 ULootHeavyComponent 가 붙어 있으면
	// ALootBase 가 이 태그를 단다 — 등급과 배율을 두 번 말하지 않으려는 정리다.
	//
	// 태그는 IGameplayTagAssetInterface 로 나온다. ICarryable 이 아니라 이쪽인 이유는
	// "무엇인가" 와 "어떻게 운반되는가" 를 가른 노획물 쪽 설계를 그대로 따르기 때문이다.
	if (const IGameplayTagAssetInterface* TagOwner = Cast<const IGameplayTagAssetInterface>(HeldActor))
	{
		return TagOwner->HasMatchingGameplayTag(HHTags::Loot_Type_Heavy);
	}

	return false;
}

bool ABaseCharacter::IsDowned() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(HHTags::State_Downed);
}

void ABaseCharacter::SetReviveProgress(float NewProgress)
{
	ReviveProgress = FMath::Clamp(NewProgress, 0.f, 1.f);
}

void ABaseCharacter::OnRep_ReviveProgress()
{
	// 지금은 빈 훅 — UI는 나중에 연결한다.
}
