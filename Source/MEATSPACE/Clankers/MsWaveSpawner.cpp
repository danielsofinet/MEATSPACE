#include "Clankers/MsWaveSpawner.h"

#include "Clankers/MsClankerBase.h"
#include "Clankers/MsClankerFlyer.h"
#include "Clankers/MsClankerSmall.h"
#include "Combat/MsHealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

AMsWaveSpawner::AMsWaveSpawner()
{
	PrimaryActorTick.bCanEverTick = true;

	// Works out of the box with no editor setup. The flyer is rarer and arrives from wave 2,
	// so the first wave teaches the sword before the gun becomes necessary.
	FMsSpawnEntry Small;
	Small.ClankerClass = AMsClankerSmall::StaticClass();
	Small.Weight = 3.0f;
	Small.FirstWave = 1;

	FMsSpawnEntry Flyer;
	Flyer.ClankerClass = AMsClankerFlyer::StaticClass();
	Flyer.Weight = 1.0f;
	Flyer.FirstWave = 2;

	SpawnTable = { Small, Flyer };
}

void AMsWaveSpawner::BeginPlay()
{
	Super::BeginPlay();

	// Spawning is a server decision; clankers replicate down from there.
	if (!HasAuthority())
	{
		SetActorTickEnabled(false);
		return;
	}

	if (bAutoStart)
	{
		StartWaves();
	}
}

void AMsWaveSpawner::StartWaves()
{
	CurrentWave = 0;
	TimeUntilNextWave = FirstWaveDelay;
	bRunning = true;
}

void AMsWaveSpawner::StopWaves()
{
	bRunning = false;
}

void AMsWaveSpawner::ForceNextWave()
{
	TimeUntilNextWave = 0.0f;
}

void AMsWaveSpawner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !bRunning)
	{
		return;
	}

	PruneAliveList();

	TimeUntilNextWave -= DeltaSeconds;
	if (TimeUntilNextWave <= 0.0f)
	{
		SpawnWave();
		TimeUntilNextWave = TimeBetweenWaves;
	}

#if ENABLE_DRAW_DEBUG
	if (bShowDebugReadout && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9100, 0.0f, FColor::Orange,
			FString::Printf(TEXT("WAVE %d    alive %d / %d    next in %.0fs"),
				CurrentWave, AliveClankers.Num(), MaxAliveClankers, FMath::Max(TimeUntilNextWave, 0.0f)));
	}
#endif
}

void AMsWaveSpawner::PruneAliveList()
{
	AliveClankers.RemoveAll([](const TWeakObjectPtr<AMsClankerBase>& Entry)
	{
		const AMsClankerBase* Clanker = Entry.Get();
		if (!Clanker)
		{
			return true;
		}

		// Dead-but-not-yet-destroyed clankers should not count against the live cap.
		const UMsHealthComponent* Health = Clanker->GetHealth();
		return Health && Health->IsDead();
	});
}

APawn* AMsWaveSpawner::FindPlayer() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

TSubclassOf<AMsClankerBase> AMsWaveSpawner::PickClankerClass() const
{
	// Weighted pick among entries unlocked by the current wave.
	float TotalWeight = 0.0f;
	for (const FMsSpawnEntry& Entry : SpawnTable)
	{
		if (Entry.ClankerClass && Entry.Weight > 0.0f && CurrentWave >= Entry.FirstWave)
		{
			TotalWeight += Entry.Weight;
		}
	}

	if (TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const FMsSpawnEntry& Entry : SpawnTable)
	{
		if (!Entry.ClankerClass || Entry.Weight <= 0.0f || CurrentWave < Entry.FirstWave)
		{
			continue;
		}

		Roll -= Entry.Weight;
		if (Roll <= 0.0f)
		{
			return Entry.ClankerClass;
		}
	}

	return nullptr;
}

bool AMsWaveSpawner::FindSpawnLocation(const FVector& Around, FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MsSpawnGround), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(this);

	for (int32 Attempt = 0; Attempt < PlacementAttempts; ++Attempt)
	{
		// Random point on a ring around the player, so waves arrive from every direction
		// rather than always the same doorway.
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Radius = FMath::FRandRange(FMath::Min(MinSpawnRadius, MaxSpawnRadius),
			FMath::Max(MinSpawnRadius, MaxSpawnRadius));

		const FVector Candidate = Around + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);

		// Must land on actual floor. Without this, clankers spawn off the edge of the arena
		// and fall forever, quietly eating the live cap.
		const FVector TraceStart = Candidate + FVector(0.0f, 0.0f, GroundTraceDistance * 0.5f);
		const FVector TraceEnd = Candidate - FVector(0.0f, 0.0f, GroundTraceDistance);

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
		{
			OutLocation = Hit.ImpactPoint + FVector(0.0f, 0.0f, SpawnHeightOffset);
			return true;
		}
	}

	return false;
}

void AMsWaveSpawner::SpawnOne(TSubclassOf<AMsClankerBase> ClankerClass, const FVector& Around)
{
	UWorld* World = GetWorld();
	if (!World || !ClankerClass)
	{
		return;
	}

	FVector SpawnLocation;
	if (!FindSpawnLocation(Around, SpawnLocation))
	{
		return;
	}

	// Face the player on arrival so they do not spend their first second turning around.
	FVector ToPlayer = Around - SpawnLocation;
	ToPlayer.Z = 0.0f;
	const FRotator SpawnRotation = ToPlayer.IsNearlyZero() ? FRotator::ZeroRotator : ToPlayer.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = this;

	if (AMsClankerBase* Spawned = World->SpawnActor<AMsClankerBase>(ClankerClass, SpawnLocation, SpawnRotation, SpawnParams))
	{
		AliveClankers.Add(Spawned);
	}
}

void AMsWaveSpawner::SpawnWave()
{
	const APawn* Player = FindPlayer();
	if (!Player)
	{
		return;
	}

	++CurrentWave;

	const FVector Around = Player->GetActorLocation();

	int32 Count = FMath::RoundToInt(BaseWaveCount + WaveCountGrowth * (CurrentWave - 1));

	// Never overshoot the live cap - a wave that arrives on top of the previous one is how a
	// difficulty curve turns into a wall.
	const int32 Headroom = FMath::Max(0, MaxAliveClankers - AliveClankers.Num());
	Count = FMath::Min(Count, Headroom);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (const TSubclassOf<AMsClankerBase> ClankerClass = PickClankerClass())
		{
			SpawnOne(ClankerClass, Around);
		}
	}
}
