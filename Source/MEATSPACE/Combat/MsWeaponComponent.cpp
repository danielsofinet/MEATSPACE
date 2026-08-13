#include "Combat/MsWeaponComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

UMsWeaponComponent::UMsWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Required for the Server/Multicast RPCs below to route at all.
	SetIsReplicatedByDefault(true);
}

void UMsWeaponComponent::StartFire()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Interval = 1.0f / FMath::Max(RoundsPerSecond, 0.01f);
	const float TimeSinceLast = World->GetTimeSeconds() - LastFireTime;

	if (TimeSinceLast >= Interval)
	{
		FireOnce();

		if (bAutomatic)
		{
			World->GetTimerManager().SetTimer(FireTimer, this, &UMsWeaponComponent::FireOnce, Interval, true);
		}
	}
	else if (bAutomatic)
	{
		// Still cooling down - start the loop so the next shot lands exactly on cadence.
		World->GetTimerManager().SetTimer(FireTimer, this, &UMsWeaponComponent::FireOnce, Interval, true, Interval - TimeSinceLast);
	}
}

void UMsWeaponComponent::StopFire()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
	}
}

void UMsWeaponComponent::FireOnce()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!OwnerPawn || !World)
	{
		return;
	}

	AController* OwnerController = OwnerPawn->GetController();
	if (!OwnerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	OwnerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	LastFireTime = World->GetTimeSeconds();

	FVector AimDir = ViewRotation.Vector();
	if (SpreadDegrees > 0.0f)
	{
		AimDir = FMath::VRandCone(AimDir, FMath::DegreesToRadians(SpreadDegrees));
	}

	// Immediate local feedback for the shooter. Cosmetic - the server still decides the truth.
	if (OwnerPawn->IsLocallyControlled())
	{
		FHitResult LocalHit;
		const bool bLocalHit = TraceShot(ViewLocation, AimDir, LocalHit);
		PlayFireFX(bLocalHit ? LocalHit.ImpactPoint : ViewLocation + AimDir * Range, bLocalHit);
	}

	// On the listen-server host this executes inline; on a client it goes over the wire.
	ServerFire(ViewLocation, AimDir);
}

void UMsWeaponComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceStart, const FVector_NetQuantizeNormal& AimDir)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Independent server-side rate limit. 10% tolerance absorbs network jitter without
	// letting a modified client fire faster than the weapon allows.
	const float Interval = 1.0f / FMath::Max(RoundsPerSecond, 0.01f);
	const float Now = World->GetTimeSeconds();
	if (Now < ServerLastFireTime + Interval * 0.9f)
	{
		return;
	}
	ServerLastFireTime = Now;

	const FVector Start(TraceStart);
	const FVector Dir(AimDir);

	FHitResult Hit;
	const bool bHit = TraceShot(Start, Dir, Hit);

	if (bHit)
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			APawn* OwnerPawn = Cast<APawn>(GetOwner());
			FPointDamageEvent DamageEvent(Damage, Hit, Dir, nullptr);
			HitActor->TakeDamage(Damage, DamageEvent, OwnerPawn ? OwnerPawn->GetController() : nullptr, GetOwner());
		}
	}

	MulticastFireFX(bHit ? Hit.ImpactPoint : Start + Dir * Range, bHit);
}

void UMsWeaponComponent::MulticastFireFX_Implementation(const FVector_NetQuantize& ImpactPoint, bool bHit)
{
	// The shooter already played this locally the instant they clicked - don't double it.
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (OwnerPawn->IsLocallyControlled())
		{
			return;
		}
	}

	PlayFireFX(FVector(ImpactPoint), bHit);
}

bool UMsWeaponComponent::TraceShot(const FVector& Start, const FVector& Dir, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector End = Start + Dir * Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MsWeaponTrace), /*bTraceComplex=*/true);
	Params.AddIgnoredActor(GetOwner());

	return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
}

void UMsWeaponComponent::PlayFireFX(const FVector& ImpactPoint, bool bHit)
{
#if ENABLE_DRAW_DEBUG
	if (!bDrawDebugShots)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Muzzle = GetMuzzleLocation();
	DrawDebugLine(World, Muzzle, ImpactPoint, bHit ? FColor::Red : FColor::Yellow, false, 0.12f, 0, 1.5f);

	if (bHit)
	{
		DrawDebugPoint(World, ImpactPoint, 12.0f, FColor::Red, false, 0.35f);
	}
#endif
}

FVector UMsWeaponComponent::GetMuzzleLocation() const
{
	// Placeholder. Once Daniel delivers a gun mesh we read a "Muzzle" socket off it instead.
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		return OwnerPawn->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
	}
	return FVector::ZeroVector;
}
