#include "stdafx.h"
#include "furniture_usage.h"
#include "furniture.h"
#include "creature_factory.h"
#include "furniture_type.h"
#include "tribe.h"
#include "furniture_factory.h"
#include "creature.h"
#include "player_message.h"
#include "creature_group.h"
#include "game.h"
#include "event_listener.h"
#include "item.h"
#include "effect.h"
#include "lasting_effect.h"
#include "creature_name.h"
#include "level.h"
#include "sound.h"
#include "body.h"
#include "creature_attributes.h"
#include "gender.h"
#include "collective.h"
#include "territory.h"
#include "game_event.h"
#include "effect.h"
#include "name_generator.h"
#include "content_factory.h"
#include "item_list.h"
#include "effect_type.h"
#include "item_types.h"

struct ChestInfo {
  FurnitureType openedType;
  struct CreatureInfo {
    optional<CreatureGroup> creature;
    int creatureChance;
    int numCreatures;
    string msgCreature;
  };
  optional<CreatureInfo> creatureInfo;
  struct ItemInfo {
    ItemListId items;
    string msgItem;
  };
  optional<ItemInfo> itemInfo;
};

static void useChest(Position pos, WConstFurniture furniture, Creature* c, const ChestInfo& chestInfo) {
  c->secondPerson("You open the " + furniture->getName());
  c->thirdPerson(c->getName().the() + " opens the " + furniture->getName());
  pos.removeFurniture(furniture, pos.getGame()->getContentFactory()->furniture.getFurniture(
      chestInfo.openedType, furniture->getTribe()));
  if (auto creatureInfo = chestInfo.creatureInfo)
    if (creatureInfo->creatureChance > 0 && Random.roll(creatureInfo->creatureChance)) {
      int numSpawned = 0;
      for (int i : Range(creatureInfo->numCreatures))
        if (pos.getLevel()->landCreature({pos}, CreatureGroup(*creatureInfo->creature).random(
            &pos.getGame()->getContentFactory()->getCreatures())))
          ++numSpawned;
      if (numSpawned > 0)
        c->message(creatureInfo->msgCreature);
      return;
    }
  if (auto itemInfo = chestInfo.itemInfo) {
    c->message(itemInfo->msgItem);
    auto itemList = pos.getGame()->getContentFactory()->itemFactory.get(itemInfo->items);
    vector<PItem> items = itemList.random(pos.getGame()->getContentFactory());
    c->getGame()->addEvent(EventInfo::ItemsAppeared{pos, getWeakPointers(items)});
    pos.dropItems(std::move(items));
  }
}

static void usePortal(Position pos, Creature* c) {
  c->you(MsgType::ENTER_PORTAL, "");
  if (auto otherPos = pos.getOtherPortal())
    for (auto f : otherPos->getFurniture())
      if (f->hasUsageType(BuiltinUsageId::PORTAL)) {
        if (pos.canMoveCreature(*otherPos)) {
          pos.moveCreature(*otherPos, true);
          return;
        }
        for (Position v : otherPos->neighbors8(Random))
          if (pos.canMoveCreature(v)) {
            pos.moveCreature(v, true);
            return;
          }
      }
  c->privateMessage("The portal is inactive. Create another one to open a connection.");
}

static void sitOnThrone(Position pos, WConstFurniture furniture, Creature* c) {
  c->thirdPerson(c->getName().the() + " sits on the " + furniture->getName());
  c->secondPerson("You sit on the " + furniture->getName());
  if (furniture->getTribe() == c->getTribeId())
    c->privateMessage("Frankly, it's not as exciting as it sounds");
  else {
    auto collective = [&]() -> WCollective {
      for (auto col : c->getGame()->getCollectives())
        if (col->getTerritory().contains(pos))
          return col;
      return nullptr;
    }();
    if (!collective)
      return;
    bool wasTeleported = false;
    auto tryTeleporting = [&] (Creature* enemy) {
      if (enemy->getPosition().dist8(pos).value_or(4) > 3 || !c->canSee(enemy))
        if (auto landing = pos.getLevel()->getClosestLanding({pos}, enemy)) {
          enemy->getPosition().moveCreature(*landing, true);
          wasTeleported = true;
          enemy->removeEffect(LastingEffect::SLEEP);
        }
    };
    for (auto enemy : collective->getCreatures(MinionTrait::FIGHTER))
      tryTeleporting(enemy);
    if (auto l = collective->getLeader())
      tryTeleporting(l);
    if (wasTeleported)
      c->privateMessage(PlayerMessage("Thy audience hath been summoned"_s +
          get(c->getAttributes().getGender(), ", Sire", ", Dame", ""), MessagePriority::HIGH));
    else
      c->privateMessage("Nothing happens");
  }
}

void FurnitureUsage::handle(FurnitureUsageType type, Position pos, WConstFurniture furniture, Creature* c) {
  CHECK(c != nullptr);
  type.visit([&] (BuiltinUsageId id) {
    switch (id) {
      case BuiltinUsageId::CHEST:
        useChest(pos, furniture, c,
            ChestInfo {
                FurnitureType("OPENED_CHEST"),
                ChestInfo::CreatureInfo {
                    CreatureGroup::singleType(TribeId::getPest(), CreatureId("RAT")),
                    10,
                    Random.get(3, 6),
                    "It's full of rats!",
                },
                ChestInfo::ItemInfo {
                    ItemListId("chest"),
                    "There is an item inside"
                }
            });
        break;
      case BuiltinUsageId::COFFIN:
        useChest(pos, furniture, c,
            ChestInfo {
                FurnitureType("OPENED_COFFIN"),
                none,
                ChestInfo::ItemInfo {
                    ItemListId("chest"),
                    "There is a rotting corpse inside. You find an item."
                }
            });
        break;
      case BuiltinUsageId::VAMPIRE_COFFIN:
        useChest(pos, furniture, c,
            ChestInfo {
                FurnitureType("OPENED_COFFIN"),
                ChestInfo::CreatureInfo {
                    CreatureGroup::singleType(TribeId::getMonster(), CreatureId("VAMPIRE_LORD")), 1, 1,
                    "There is a rotting corpse inside. The corpse is alive!"
                },
                none
            });
        break;
      case BuiltinUsageId::KEEPER_BOARD:
        c->getGame()->handleMessageBoard(pos, c);
        break;
      case BuiltinUsageId::STAIRS:
        c->getLevel()->changeLevel(*pos.getLandingLink(), c);
        break;
      case BuiltinUsageId::TIE_UP:
        c->addEffect(LastingEffect::TIED_UP, 100_visible);
        break;
      case BuiltinUsageId::TRAIN:
        c->addSound(SoundId::MISSED_ATTACK);
        break;
      case BuiltinUsageId::PORTAL:
        usePortal(pos, c);
        break;
      case BuiltinUsageId::SIT_ON_THRONE:
        sitOnThrone(pos, furniture, c);
        break;
      case BuiltinUsageId::DEMON_RITUAL:
      case BuiltinUsageId::STUDY:
      case BuiltinUsageId::ARCHERY_RANGE:
        break;
    }
  },
  [&](const UsageEffect& effect) {
    effect.effect.apply(c->getPosition());
  });
}

bool FurnitureUsage::canHandle(FurnitureUsageType type, const Creature* c) {
  if (auto id = type.getReferenceMaybe<BuiltinUsageId>())
    switch (*id) {
      case BuiltinUsageId::KEEPER_BOARD:
      case BuiltinUsageId::VAMPIRE_COFFIN:
      case BuiltinUsageId::COFFIN:
      case BuiltinUsageId::CHEST:
        return c->getBody().isHumanoid();
      default:
        return true;
    }
  return true;
}

string FurnitureUsage::getUsageQuestion(FurnitureUsageType type, string furnitureName) {
  return type.visit(
      [&] (BuiltinUsageId id) {
        switch (id) {
          case BuiltinUsageId::STAIRS: return "use " + furnitureName;
          case BuiltinUsageId::COFFIN:
          case BuiltinUsageId::VAMPIRE_COFFIN:
          case BuiltinUsageId::CHEST: return "open " + furnitureName;
          case BuiltinUsageId::KEEPER_BOARD: return "view " + furnitureName;
          case BuiltinUsageId::PORTAL: return "enter " + furnitureName;
          case BuiltinUsageId::SIT_ON_THRONE: return "sit on " + furnitureName;
          default:
            return "use " + furnitureName;
        }
      },
      [&](const UsageEffect& e) {
        return e.usageVerb + " " + furnitureName;
      }
  );
}

void FurnitureUsage::beforeRemoved(FurnitureUsageType type, Position pos) {
  if (auto id = type.getReferenceMaybe<BuiltinUsageId>())
    switch (*id) {
      case BuiltinUsageId::PORTAL:
        pos.removePortal();
        break;
      default:
        break;
    }
}
