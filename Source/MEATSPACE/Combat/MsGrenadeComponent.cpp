#include "Combat/MsGrenadeComponent.h"

#include "Character/MsCharacter.h"
#include "Combat/MsGrenade.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"

UMsGrenadeComponent::UMsGrenadeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	GrenadeClass = AMsGrenade::StaticClass();
}

bool UMsGrenadeComponent::IsReady() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->GetTimeSeconds() >= LastThrowTime + Cooldown;
}

float UMsGrenadeComponent::GetCooldownFraction() const
{
	const UWorld* World = GetWorld();
	if (!World || Cooldown <= 0.0f)
	{
		return 0.0f;
	}

	const float Elapsed = World->GetTimeSeconds() - LastThrowTime;
	return FMath::Clamp(1.0f - Elapsed / Cooldown, 0.0f, 1.0f);
}

void UMsGrenadeComponent::ThrowGrenade()
{
	UWorld* World = GetWorld();
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!World || !OwnerPawn || !GrenadeClass || !IsReady())
	{
		return;
	}

	LastThrowTime = World->GetTimeSeconds();

	// Throw toward whatever the reticle is over, so the grenade lands where you are looking
	// rather than wherever the character happens to be facing.
	FVector Direction = OwnerPawn->GetActorForwardVector();

	const FVector Start = OwnerPawn->GetActorLocation()
		+ OwnerPawn->GetActorRotation().RotateVector(ThrowOffset);

	if (const AMsCharacter* MsOwner = Cast<AMsCharacter>(OwnerPawn))
	{
		FVector AimPoint;
		if (MsOwner->ComputeAimPoint(AimPoint))
		{
			const FVector ToAim = AimPoint - Start;
			if (!ToAim.IsNearlyZero())
			{
				Direction = ToAim.GetSafeNormal();
			}
		}
	}

	// Add lift so it arcs instead of drilling into the floor at the player's feet.
	Direction = (Direction + FVector(0.0f, 0.0f, ThrowArc)).GetSafeNormal();

	ServerThrow(Start, Direction);
}

void UMsGrenadeComponent::ServerThrow_Implementation(const FVector_NetQuantize& Start,
	const FVector_NetQuantizeNormal& Direction)
{
	UWorld* World = GetWorld();
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!World || !OwnerPawn || !GrenadeClass)
	{
		return;
	}

	// Independent server-side cooldown, with a little tolerance for jitter.
	const float Now = World->GetTimeSeconds();
	if (Now < ServerLastThrowTime + Cooldown * 0.9f)
	{
		return;
	}
	ServerLastThrowTime = Now;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = OwnerPawn;
	SpawnParams.Instigator = OwnerPawn;

	const FVector Dir(Direction);

	if (AMsGrenade* Grenade = World->SpawnActor<AMsGrenade>(
		GrenadeClass, FVector(Start), Dir.Rotation(), SpawnParams))
	{
		if (UProjectileMovementComponent* Movement =
			Grenade->FindComponentByClass<UProjectileMovementComponent>())
		{
			Movement->Velocity = Dir * ThrowSpeed;
			Movement->MaxSpeed = ThrowSpeed * 2.0f;
		}
	}
}
