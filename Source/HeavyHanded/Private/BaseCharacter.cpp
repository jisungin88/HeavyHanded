// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseCharacter.h"
#include "PlayerSessionState.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

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
	// ★ 클라이언트든 서버든 '로컬 플레이어'인 경우에만 입력 매핑을 추가해야 합니다.
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

        // 시점 회전 바인딩
        if (IA_Look)
        {
            EnhancedInputComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ABaseCharacter::Look);
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

// Enhanced Input 방식에 맞춘 시점 회전 로직 (마우스 X/Y 이동량)
void ABaseCharacter::Look(const FInputActionValue& Value)
{
    // 2D 축 값 가져오기 (X: 좌우 시점 회전, Y: 상하 시점 회전)
    const FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        if (LookAxisVector.X != 0.0f)
        {
            AddControllerYawInput(LookAxisVector.X);
        }
        if (LookAxisVector.Y != 0.0f)
        {
            AddControllerPitchInput(LookAxisVector.Y);
        }
    }
}