#include "RE/B/BGSActorCellEvent.h"
#include "RE/B/BGSFootstep.h"
#include "RE/B/BSTEvent.h"
#include "REX/TSingleton.h"
using namespace StyyxUtil;

// Constants:
constexpr auto PLUGIN_NAME = "dropchances.esl";
constexpr RE::FormID RAGS_CHEST_ID = 0x1;
constexpr RE::FormID RAGS_LEGS_ID = 0x2;
constexpr RE::FormID EXCEPTION_LIST_ID = 0x3;
constexpr RE::FormID EXCEPTION_KYWD_ID = 0x4;

constexpr auto TOML_P_D = "Data/SKSE/Plugins/drop-chances.toml";
constexpr auto TOML_P_C = "Data/SKSE/Plugins/drop-chances_custom.toml";
constexpr auto TOML_SEC_SET = "DropSettings";

// glob var
bool IsRelevantMenuOpen = false;
bool openingActorInventory = false;

// store processed actor + hidden items.
// should maybe clear map on location change or something
std::unordered_map<RE::TESObjectREFR *, std::unordered_set<RE::FormID>>
    rejectedItems;
RE::TESObjectREFR *openingRef{};

namespace POOP {

namespace FORMS {
inline RE::TESObjectARMO *replacer_rags_chest{nullptr};
inline RE::TESObjectARMO *replacer_rags_legs{nullptr};
inline RE::BGSListForm *exception_formlist{nullptr};
inline RE::BGSListForm *exception_keyword_formlist{nullptr};

bool AllFormsValid() {
  return replacer_rags_chest && replacer_rags_legs && exception_formlist &&
         exception_keyword_formlist;
}

void LoadForms() {
  if (!MiscUtil::IsModLoaded(PLUGIN_NAME)) {
    REX::FAIL(
        "{} is not loaded, please make sure it is enabled and/or deployed",
        PLUGIN_NAME);
  }
  auto dh = RE::TESDataHandler::GetSingleton();

  replacer_rags_chest =
      dh->LookupForm<RE::TESObjectARMO>(RAGS_CHEST_ID, PLUGIN_NAME);
  replacer_rags_legs =
      dh->LookupForm<RE::TESObjectARMO>(RAGS_LEGS_ID, PLUGIN_NAME);
  exception_formlist =
      dh->LookupForm<RE::BGSListForm>(EXCEPTION_LIST_ID, PLUGIN_NAME);
  exception_keyword_formlist =
      dh->LookupForm<RE::BGSListForm>(EXCEPTION_KYWD_ID, PLUGIN_NAME);

  if (!AllFormsValid()) {
    REX::FAIL("Could not load all needed forms, make sure the mod is fully "
              "updated and you have no old version of "
              "{} left",
              PLUGIN_NAME);
  }
}

} // namespace FORMS

namespace CONF {
inline REX::TOML::F32 drop_removal_chance_weapons{
    TOML_SEC_SET, "fDropRemoveChanceWeapons", 50.0f};
inline REX::TOML::F32 drop_removal_chance_armor{
    TOML_SEC_SET, "fDropRemoveChanceArmor", 50.0f};
inline REX::TOML::F32 drop_removal_chance_jewelry{
    TOML_SEC_SET, "fDropRemoveChanceJewelry", 50.0f};

inline REX::TOML::Bool never_replace_enchanted{TOML_SEC_SET,
                                               "bNeverRemoveEnchanted", true};

void UpdateSettings(const bool a_save = false) {
  auto t = REX::TSingleton<REX::FTomlSettingStore>::GetSingleton();
  t->Init(TOML_P_D, TOML_P_C);
  a_save ? t->Save() : t->Load();
}

} // namespace CONF

struct LocChangeEv : REX::TSingleton<LocChangeEv>,
                     RE::BSTEventSink<RE::BGSActorCellEvent> {

  static void RegisterCellEvent() {

    if (auto player = RE::PlayerCharacter::GetSingleton()) {
      player->AddEventSink<RE::BGSActorCellEvent>(GetSingleton());
    }
  }
  RE::BSEventNotifyControl
  ProcessEvent(const RE::BGSActorCellEvent *a_event,
               RE::BSTEventSource<RE::BGSActorCellEvent> *) override {

    if (!a_event) {
      return RE::BSEventNotifyControl::kContinue;
    }
    RE::TESObjectCELL *cell =
        RE::TESForm::LookupByID<RE::TESObjectCELL>(a_event->cellID);

    if (cell && cell->IsExteriorCell()) {
      return RE::BSEventNotifyControl::kContinue;
    }
    if (a_event->flags == RE::BGSActorCellEvent::CellFlag::kEnter) {
      std::erase_if(rejectedItems, [cell](const auto &entry) {
        auto *reference = entry.first;
        return !reference || reference->GetParentCell() != cell;
      });
    }
    return RE::BSEventNotifyControl::kContinue;
  }
};

// heavily inspired and mostly copied from:
// https://github.com/Horf/HiddenLoot/blob/97b085ead7b7fd1fd3c8810cf617d2b515ecaf0a/src/LootHook.h#L95
struct MenuEventListener : REX::TSingleton<MenuEventListener>,
                           RE::BSTEventSink<RE::MenuOpenCloseEvent> {
  std::atomic<bool> bLootMenuOpen{false};
  std::atomic<long long> lastLootMenuCloseTime{0};
  std::atomic<bool> bContainerMenuOpen{false};
  std::atomic<bool> bOtherMenuOpen{false};

  // in class Register function is my addition.
  static void RegisterMenu() {
    auto ui = RE::UI::GetSingleton();
    if (ui) {
      ui->AddEventSink<RE::MenuOpenCloseEvent>(GetSingleton());
      REX::INFO("registered for inventory");
    }
  }
  RE::BSEventNotifyControl
  ProcessEvent(const RE::MenuOpenCloseEvent *a_event,
               RE::BSTEventSource<RE::MenuOpenCloseEvent> *) override {
    if (!a_event) {
      return RE::BSEventNotifyControl::kContinue;
    }

    static const RE::BSFixedString lootMenuName("LootMenu");

    if (a_event->menuName == lootMenuName) {
      bLootMenuOpen = a_event->opening;
      if (!a_event->opening) {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        lastLootMenuCloseTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
      }
    } else if (a_event->menuName == RE::ContainerMenu::MENU_NAME) {
      // bool and ref reset is my addition
      bContainerMenuOpen.store(a_event->opening);
      if (!a_event->opening) {
        openingActorInventory = false;
        openingRef = nullptr;
      }
    } else if (a_event->menuName == RE::InventoryMenu::MENU_NAME ||
               a_event->menuName == RE::MagicMenu::MENU_NAME ||
               a_event->menuName == RE::FavoritesMenu::MENU_NAME ||
               a_event->menuName == RE::BarterMenu::MENU_NAME ||
               a_event->menuName == RE::CraftingMenu::MENU_NAME ||
               a_event->menuName == RE::GiftMenu::MENU_NAME) {
      bOtherMenuOpen = a_event->opening;
    }
    return RE::BSEventNotifyControl::kContinue;
  }
  bool IsLootMenuEffectivelyOpen() const {
    if (bLootMenuOpen)
      return true;
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto nowMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    // 250ms grace period bridges the gap during UI fade-out animations or
    // rapid crosshair jitter to prevent items from "blinking" into view
    if (nowMs - lastLootMenuCloseTime < 250)
      return true;
    return false;
  }
};

struct OpenInvActorHook {
  static void Call(RE::TESObjectREFR *a_ref,
                   RE::ContainerMenu::ContainerMode a_mode) {
    auto it = rejectedItems.find(a_ref);
    openingRef = a_ref;
    if (it == rejectedItems.end()) {
      openingActorInventory = true;
    }

    func(a_ref, a_mode);

    auto menu = MenuEventListener::GetSingleton();
    if (menu->bContainerMenuOpen) {
      openingActorInventory = false;
      openingRef = nullptr;
    }
  }
  static inline REL::THook func{REL::ID(24715), 0x3E7, Call};
};

bool IsItemArtifact(RE::TESBoundObject *a_item) {
  if (a_item->HasKeywordByEditorID("DaedricArtifact")) {
    return true;
  }
  return false;
}

bool IsMaybeUniqueItem(RE::TESBoundObject *a_item) {
  if (a_item->HasKeywordByEditorID("MagicDisallowEnchanting")) {
    return true;
  }
  return false;
}

bool IsItemEnchanted(RE::TESBoundObject *a_item, RE::TESObjectREFR *a_ref) {
  auto inventory = a_ref->GetInventory(
      [&](RE::TESBoundObject &a_object) { return &a_object == a_item; }, false);

  if (auto it = inventory.find(a_item); it != inventory.end()) {
    auto entry = it->second.second.get();

    if (entry && entry->GetEnchantment() != nullptr) {
      return true;
    }
  }

  return false;
}

bool IsQuestItem(RE::TESBoundObject *a_item, RE::TESObjectREFR *a_ref) {

  auto inventory = a_ref->GetInventory(
      [&](RE::TESBoundObject &a_object) { return &a_object == a_item; }, false);

  if (auto it = inventory.find(a_item); it != inventory.end()) {

    auto entry = it->second.second.get();

    if (entry && entry->IsQuestObject()) {
      return true;
    }
  }

  return false;
}

bool IsItemExcluded(RE::TESBoundObject *a_item) {

  if (FORMS::exception_formlist->HasForm(a_item)) {
    return true;
  }
  if (a_item->HasKeywordInList(FORMS::exception_keyword_formlist, false)) {
    return true;
  }
  return false;
}

bool CanHideItem(RE::TESBoundObject *a_item, RE::TESObjectREFR *a_ref) {
  if (IsItemExcluded(a_item)) {
    return false;
  }

  if (IsQuestItem(a_item, a_ref)) {
    return false;
  }

  if (!CONF::never_replace_enchanted.GetValue()) {
    if (IsItemEnchanted(a_item, a_ref)) {
      return false;
    }
  }

  if (IsItemArtifact(a_item) || IsMaybeUniqueItem(a_item)) {
    return false;
  }

  return true;
}

float GetActualChance(RE::TESBoundObject *a_item) {

  if (a_item->Is(RE::FormType::Weapon)) {
    return CONF::drop_removal_chance_weapons.GetValue();
  }
  if (a_item->Is(RE::FormType::Armor)) {
    RE::TESObjectARMO *armo = a_item->As<RE::TESObjectARMO>();
    if (armo) {

      using slot = RE::BGSBipedObjectForm::BipedObjectSlot;
      switch (armo->GetSlotMask()) {
      case slot::kAmulet:
      case slot::kCirclet:
      case slot::kRing:
        return CONF::drop_removal_chance_jewelry.GetValue();
      default:
        return CONF::drop_removal_chance_armor.GetValue();
      }
    }
    return CONF::drop_removal_chance_armor.GetValue();
  }
  return 0.0f;
}

bool ProcessItem(RE::TESBoundObject *a_item, bool a_original) {
  if (!a_original) {
    return a_original;
  }

  // check if actor had items processed before, return false for those items
  // that shouldn't be shown.
  if (openingRef) {
    if (auto it = rejectedItems.find(openingRef);
        it != rejectedItems.end() && it->second.contains(a_item->GetFormID())) {
      return false;
    }
  }

  // only runs once per actor ensured by OpenInvActorHook
  if (openingActorInventory && openingRef && CanHideItem(a_item, openingRef)) {
    auto cmp = GetActualChance(a_item);
    auto remove = RandomiserUtil::IsPercentageChanceFloat(cmp);
    // if randomiser returns true, skip the map cause i only need hidden items
    // true means item should be shown!
    if (remove) {
      REX::INFO("chance for {} is: {}", a_item->GetName(), cmp);
      rejectedItems[openingRef].insert(a_item->GetFormID());
      return false;
    }
  }
  return true;
}

struct PlayableHooks {
  static bool CallArmor(RE::TESBoundObject *a_armo) {
    return ProcessItem(a_armo, funcArmor(a_armo));
  }
  static bool CallWeapon(RE::TESBoundObject *a_weapon) {
    return ProcessItem(a_weapon, funcWeapon(a_weapon));
  }
  static inline REL::THookVFT funcArmor{RE::TESObjectARMO::VTABLE[0], 0x19,
                                        CallArmor};
  static inline REL::THookVFT funcWeapon{RE::TESObjectWEAP::VTABLE[0], 0x19,
                                         CallWeapon};
};

} // namespace POOP

void List(SKSE::MessagingInterface::Message *a_msg) {
  switch (a_msg->type) {
  case SKSE::MessagingInterface::kDataLoaded:
    POOP::MenuEventListener::RegisterMenu();
    POOP::LocChangeEv::RegisterCellEvent();
    POOP::FORMS::LoadForms();
  }
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface *a_skse) {
  SKSE::Init(a_skse, {.trampoline = true});
  POOP::CONF::UpdateSettings();
  if (!SKSE::GetMessagingInterface()->RegisterListener(List)) {
    return false;
  }
  return true;
}
