#pragma once

#include "util.h"
#include "effect.h"

class Position;
class Furniture;
class Creature;

RICH_ENUM(
  BuiltinUsageId,
  CHEST,
  COFFIN,
  VAMPIRE_COFFIN,
  KEEPER_BOARD,
  STAIRS,
  TIE_UP,
  TRAIN,
  ARCHERY_RANGE,
  STUDY,
  PORTAL,
  SIT_ON_THRONE,
  DEMON_RITUAL
);

struct UsageEffect {
  string SERIAL(usageVerb);
  Effect SERIAL(effect);
  SERIALIZE_ALL(usageVerb, effect)
};

MAKE_VARIANT2(FurnitureUsageType, BuiltinUsageId, UsageEffect);

class FurnitureUsage {
  public:
  static void handle(FurnitureUsageType, Position, WConstFurniture, Creature*);
  static bool canHandle(FurnitureUsageType, const Creature*);
  static string getUsageQuestion(FurnitureUsageType, string furnitureName);
  static void beforeRemoved(FurnitureUsageType, Position);
};
