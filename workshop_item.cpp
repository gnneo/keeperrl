#include "stdafx.h"
#include "workshop_item.h"
#include "item_factory.h"
#include "item.h"
#include "view_object.h"
#include "content_factory.h"

static string getItemName(const ContentFactory* factory, Item* item, bool plural) {
  auto name = item->getName(plural);
  if (name.size() > 30)
    return item->getShortName(factory, nullptr, plural);
  return name;
}

WorkshopItem WorkshopItemCfg::get(WorkshopType type, const ContentFactory* factory) const {
  PROFILE;
  // for some reason removing this line causes a linker error, probably a compiler bug
  auto t = tech;
  PItem elem = item.get(factory);
  auto itemScaling = factory->workshopInfo.at(type).itemScaling;
  if (itemScaling > 1)
    for (auto& attr : factory->attrOrder)
      if (elem->getModifierValues().count(attr))
        elem->addModifier(attr, elem->getModifier(attr) * (itemScaling - 1));
  vector<string> description;
  if (elem->getNameAndModifiers(factory) != elem->getName())
    description.push_back(elem->getNameAndModifiers(factory));
  description.append(elem->getDescription(factory));
  return WorkshopItem {
    item,
    getItemName(factory, elem.get(), false),
    getItemName(factory, elem.get(), true),
    elem->getViewObject().getViewIdList(),
    cost.value_or(elem->getCraftingCost()),
    std::move(description),
    work,
    tech,
    tutorialHighlight,
    elem->getAppliedUpgradeType(),
    elem->getMaxUpgrades(),
    requireIngredient,
    applyImmediately,
    materialTab,
    requiresUpgrades
  };
}
