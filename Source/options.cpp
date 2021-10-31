/**
 * @file options.cpp
 *
 * Load and save options from the diablo.ini file.
 */

#include <cstdint>
#include <fstream>
#include <locale>

#ifdef __ANDROID__
#include "SDL.h"
#include <jni.h>
#endif

#ifdef __vita__
#include <psp2/apputil.h>
#include <psp2/system_param.h>
#endif

#ifdef __3DS__
#include "platform/ctr/locale.hpp"
#endif

#define SI_SUPPORT_IOSTREAMS
#include <SimpleIni.h>

#include "diablo.h"
#include "engine/demomode.h"
#include "options.h"
#include "utils/file_util.h"
#include "utils/language.h"
#include "utils/paths.h"

namespace devilution {

#ifndef DEFAULT_WIDTH
#define DEFAULT_WIDTH 800
#endif
#ifndef DEFAULT_HEIGHT
#define DEFAULT_HEIGHT 600
#endif
#ifndef DEFAULT_AUDIO_SAMPLE_RATE
#define DEFAULT_AUDIO_SAMPLE_RATE 22050
#endif
#ifndef DEFAULT_AUDIO_CHANNELS
#define DEFAULT_AUDIO_CHANNELS 2
#endif
#ifndef DEFAULT_AUDIO_BUFFER_SIZE
#define DEFAULT_AUDIO_BUFFER_SIZE 2048
#endif
#ifndef DEFAULT_AUDIO_RESAMPLING_QUALITY
#define DEFAULT_AUDIO_RESAMPLING_QUALITY 5
#endif

#if defined(VIRTUAL_GAMEPAD) && !defined(USE_SDL1)
#define AUTO_PICKUP_DEFAULT(bValue) true
#else
#define AUTO_PICKUP_DEFAULT(bValue) bValue
#endif

namespace {

std::string GetIniPath()
{
	auto path = paths::ConfigPath() + std::string("diablo.ini");
	return path;
}

CSimpleIni &GetIni()
{
	static CSimpleIni ini;
	static bool isIniLoaded = false;
	if (!isIniLoaded) {
		auto path = GetIniPath();
		auto stream = CreateFileStream(path.c_str(), std::fstream::in | std::fstream::binary);
		ini.SetSpaces(false);
		if (stream)
			ini.LoadData(*stream);
		isIniLoaded = true;
	}
	return ini;
}

bool IniChanged = false;

/**
 * @brief Checks if a ini entry is changed by comparing value before and after
 */
class IniChangedChecker {
public:
	IniChangedChecker(const char *sectionName, const char *keyName)
	{
		this->sectionName_ = sectionName;
		this->keyName_ = keyName;
		std::list<CSimpleIni::Entry> values;
		if (!GetIni().GetAllValues(sectionName, keyName, values)) {
			// No entry found in original ini => new entry => changed
			IniChanged = true;
		}
		const auto *value = GetIni().GetValue(sectionName, keyName);
		if (value != nullptr)
			oldValue_ = value;
	}
	~IniChangedChecker()
	{
		const auto *value = GetIni().GetValue(sectionName_, keyName_);
		std::string newValue;
		if (value != nullptr)
			newValue = value;
		if (oldValue_ != newValue)
			IniChanged = true;
	}

private:
	std::string oldValue_;
	const char *sectionName_;
	const char *keyName_;
};

int GetIniInt(const char *keyname, const char *valuename, int defaultValue)
{
	return GetIni().GetLongValue(keyname, valuename, defaultValue);
}

bool GetIniBool(const char *sectionName, const char *keyName, bool defaultValue)
{
	return GetIni().GetBoolValue(sectionName, keyName, defaultValue);
}

float GetIniFloat(const char *sectionName, const char *keyName, float defaultValue)
{
	return (float)GetIni().GetDoubleValue(sectionName, keyName, defaultValue);
}

void SetIniValue(const char *keyname, const char *valuename, int value)
{
	IniChangedChecker changedChecker(keyname, valuename);
	GetIni().SetLongValue(keyname, valuename, value);
}

void SetIniValue(const char *keyname, const char *valuename, std::uint8_t value)
{
	IniChangedChecker changedChecker(keyname, valuename);
	GetIni().SetLongValue(keyname, valuename, value);
}

void SetIniValue(const char *keyname, const char *valuename, std::uint32_t value)
{
	IniChangedChecker changedChecker(keyname, valuename);
	GetIni().SetLongValue(keyname, valuename, value);
}

void SetIniValue(const char *keyname, const char *valuename, bool value)
{
	IniChangedChecker changedChecker(keyname, valuename);
	GetIni().SetLongValue(keyname, valuename, value ? 1 : 0);
}

void SetIniValue(const char *keyname, const char *valuename, float value)
{
	IniChangedChecker changedChecker(keyname, valuename);
	GetIni().SetDoubleValue(keyname, valuename, value);
}

void SaveIni()
{
	if (!IniChanged)
		return;
	auto iniPath = GetIniPath();
	auto stream = CreateFileStream(iniPath.c_str(), std::fstream::out | std::fstream::trunc | std::fstream::binary);
	GetIni().Save(*stream, true);
	IniChanged = false;
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
bool HardwareCursorDefault()
{
#ifdef __ANDROID__
	// See https://github.com/diasurgical/devilutionX/issues/2502
	return false;
#else
	SDL_version v;
	SDL_GetVersion(&v);
	return SDL_VERSIONNUM(v.major, v.minor, v.patch) >= SDL_VERSIONNUM(2, 0, 12);
#endif
}
#endif

} // namespace

void SetIniValue(const char *sectionName, const char *keyName, const char *value, int len)
{
	IniChangedChecker changedChecker(sectionName, keyName);
	auto &ini = GetIni();
	std::string stringValue(value, len != 0 ? len : strlen(value));
	ini.SetValue(sectionName, keyName, stringValue.c_str());
}

bool GetIniValue(const char *sectionName, const char *keyName, char *string, int stringSize, const char *defaultString)
{
	const char *value = GetIni().GetValue(sectionName, keyName);
	if (value == nullptr) {
		strncpy(string, defaultString, stringSize);
		return false;
	}
	strncpy(string, value, stringSize - 1);
	string[stringSize - 1] = '\0';
	return true;
}

/** Game options */
Options sgOptions;
bool sbWasOptionsLoaded = false;

void LoadOptions()
{
	sgOptions.Diablo.bIntro = GetIniBool("Diablo", "Intro", true);
	sgOptions.Diablo.lastSinglePlayerHero = GetIniInt("Diablo", "LastSinglePlayerHero", 0);
	sgOptions.Diablo.lastMultiplayerHero = GetIniInt("Diablo", "LastMultiplayerHero", 0);
	sgOptions.Hellfire.bIntro = GetIniBool("Hellfire", "Intro", true);
	sgOptions.Hellfire.lastSinglePlayerHero = GetIniInt("Hellfire", "LastSinglePlayerHero", 0);
	sgOptions.Hellfire.lastMultiplayerHero = GetIniInt("Hellfire", "LastMultiplayerHero", 0);
	sgOptions.Hellfire.startUpGameOption = static_cast<StartUpGameOption>(GetIniInt("Hellfire", "StartUpGameOption", static_cast<int>(StartUpGameOption::None)));
	GetIniValue("Hellfire", "SItem", sgOptions.Hellfire.szItem, sizeof(sgOptions.Hellfire.szItem), "");

	sgOptions.Audio.nSoundVolume = GetIniInt("Audio", "Sound Volume", VOLUME_MAX);
	sgOptions.Audio.nMusicVolume = GetIniInt("Audio", "Music Volume", VOLUME_MAX);
	sgOptions.Audio.bWalkingSound = GetIniBool("Audio", "Walking Sound", true);
	sgOptions.Audio.bAutoEquipSound = GetIniBool("Audio", "Auto Equip Sound", AUTO_PICKUP_DEFAULT(true));
	sgOptions.Audio.bItemPickupSound = GetIniBool("Audio", "Item Pickup Sound", AUTO_PICKUP_DEFAULT(true));

	sgOptions.Audio.nSampleRate = GetIniInt("Audio", "Sample Rate", DEFAULT_AUDIO_SAMPLE_RATE);
	sgOptions.Audio.nChannels = GetIniInt("Audio", "Channels", DEFAULT_AUDIO_CHANNELS);
	sgOptions.Audio.nBufferSize = GetIniInt("Audio", "Buffer Size", DEFAULT_AUDIO_BUFFER_SIZE);
	sgOptions.Audio.nResamplingQuality = GetIniInt("Audio", "Resampling Quality", DEFAULT_AUDIO_RESAMPLING_QUALITY);

	sgOptions.Graphics.nWidth = GetIniInt("Graphics", "Width", DEFAULT_WIDTH);
	sgOptions.Graphics.nHeight = GetIniInt("Graphics", "Height", DEFAULT_HEIGHT);
#ifndef __vita__
	sgOptions.Graphics.bFullscreen = GetIniBool("Graphics", "Fullscreen", true);
#else
	sgOptions.Graphics.bFullscreen = true;
#endif
#if !defined(USE_SDL1)
	sgOptions.Graphics.bUpscale = GetIniBool("Graphics", "Upscale", true);
#else
	sgOptions.Graphics.bUpscale = false;
#endif
	sgOptions.Graphics.bFitToScreen = GetIniBool("Graphics", "Fit to Screen", true);
	GetIniValue("Graphics", "Scaling Quality", sgOptions.Graphics.szScaleQuality, sizeof(sgOptions.Graphics.szScaleQuality), "2");
	sgOptions.Graphics.bIntegerScaling = GetIniBool("Graphics", "Integer Scaling", false);
	sgOptions.Graphics.bVSync = GetIniBool("Graphics", "Vertical Sync", true);
	sgOptions.Graphics.bBlendedTransparancy = GetIniBool("Graphics", "Blended Transparency", true);
	sgOptions.Graphics.nGammaCorrection = GetIniInt("Graphics", "Gamma Correction", 100);
	sgOptions.Graphics.bColorCycling = GetIniBool("Graphics", "Color Cycling", true);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	sgOptions.Graphics.bHardwareCursor = GetIniBool("Graphics", "Hardware Cursor", HardwareCursorDefault());
	sgOptions.Graphics.bHardwareCursorForItems = GetIniBool("Graphics", "Hardware Cursor For Items", false);
	sgOptions.Graphics.nHardwareCursorMaxSize = GetIniInt("Graphics", "Hardware Cursor Maximum Size", 128);
#endif
	sgOptions.Graphics.bFPSLimit = GetIniBool("Graphics", "FPS Limiter", true);
	sgOptions.Graphics.bShowFPS = (GetIniInt("Graphics", "Show FPS", 0) != 0);

	sgOptions.Gameplay.nTickRate = GetIniInt("Game", "Speed", 20);
	sgOptions.Gameplay.bRunInTown = GetIniBool("Game", "Run in Town", AUTO_PICKUP_DEFAULT(false));
	sgOptions.Gameplay.bGrabInput = GetIniBool("Game", "Grab Input", false);
	sgOptions.Gameplay.bTheoQuest = GetIniBool("Game", "Theo Quest", false);
	sgOptions.Gameplay.bCowQuest = GetIniBool("Game", "Cow Quest", false);
	sgOptions.Gameplay.bFriendlyFire = GetIniBool("Game", "Friendly Fire", false);
	sgOptions.Gameplay.bTestBard = GetIniBool("Game", "Test Bard", false);
	sgOptions.Gameplay.bTestBarbarian = GetIniBool("Game", "Test Barbarian", false);
	sgOptions.Gameplay.bExperienceBar = GetIniBool("Game", "Experience Bar", true);
	sgOptions.Gameplay.bEnemyHealthBar = GetIniBool("Game", "Enemy Health Bar", true);
	sgOptions.Gameplay.bAutoGoldPickup = GetIniBool("Game", "Auto Gold Pickup", AUTO_PICKUP_DEFAULT(true));
	sgOptions.Gameplay.bAdriaRefillsMana = GetIniBool("Game", "Adria Refills Mana", true);
	sgOptions.Gameplay.bAutoEquipWeapons = GetIniBool("Game", "Auto Equip Weapons", true);
	sgOptions.Gameplay.bAutoEquipArmor = GetIniBool("Game", "Auto Equip Armor", AUTO_PICKUP_DEFAULT(true));
	sgOptions.Gameplay.bAutoEquipHelms = GetIniBool("Game", "Auto Equip Helms", AUTO_PICKUP_DEFAULT(true));
	sgOptions.Gameplay.bAutoEquipShields = GetIniBool("Game", "Auto Equip Shields", AUTO_PICKUP_DEFAULT(true));
	sgOptions.Gameplay.bAutoEquipJewelry = GetIniBool("Game", "Auto Equip Jewelry", AUTO_PICKUP_DEFAULT(true));
	sgOptions.Gameplay.bRandomizeQuests = GetIniBool("Game", "Randomize Quests", true);
	sgOptions.Gameplay.bShowMonsterType = GetIniBool("Game", "Show Monster Type", true);
	sgOptions.Gameplay.bDisableCripplingShrines = GetIniBool("Game", "Disable Crippling Shrines", false);
	sgOptions.Gameplay.bAutoRefillBelt = GetIniBool("Game", "Auto Refill Belt", AUTO_PICKUP_DEFAULT(true));

	GetIniValue("Network", "Bind Address", sgOptions.Network.szBindAddress, sizeof(sgOptions.Network.szBindAddress), "0.0.0.0");
	sgOptions.Network.nPort = GetIniInt("Network", "Port", 6112);
	GetIniValue("Network", "Previous Host", sgOptions.Network.szPreviousHost, sizeof(sgOptions.Network.szPreviousHost), "");

	for (size_t i = 0; i < QUICK_MESSAGE_OPTIONS; i++)
		GetIniValue("NetMsg", QuickMessages[i].key, sgOptions.Chat.szHotKeyMsgs[i], MAX_SEND_STR_LEN, "");

	GetIniValue("Controller", "Mapping", sgOptions.Controller.szMapping, sizeof(sgOptions.Controller.szMapping), "");
	sgOptions.Controller.bSwapShoulderButtonMode = GetIniBool("Controller", "Swap Shoulder Button Mode", false);
	sgOptions.Controller.bDpadHotkeys = GetIniBool("Controller", "Dpad Hotkeys", false);
	sgOptions.Controller.fDeadzone = GetIniFloat("Controller", "deadzone", 0.15F);
#ifdef __vita__
	sgOptions.Controller.bRearTouch = GetIniBool("Controller", "Enable Rear Touchpad", true);
#endif

#ifdef __ANDROID__
	JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
	jobject activity = (jobject)SDL_AndroidGetActivity();
	jclass clazz(env->GetObjectClass(activity));
	jmethodID method_id = env->GetMethodID(clazz, "getLocale", "()Ljava/lang/String;");
	jstring jLocale = (jstring)env->CallObjectMethod(activity, method_id);
	const char *cLocale = env->GetStringUTFChars(jLocale, nullptr);
	std::string locale = cLocale;
	env->ReleaseStringUTFChars(jLocale, cLocale);
	env->DeleteLocalRef(jLocale);
	env->DeleteLocalRef(activity);
	env->DeleteLocalRef(clazz);
#elif defined(__vita__)
	int32_t language = SCE_SYSTEM_PARAM_LANG_ENGLISH_US; // default to english
	const char *vita_locales[] = {
		"ja_JP",
		"en_US",
		"fr_FR",
		"es_ES",
		"de_DE",
		"it_IT",
		"nl_NL",
		"pt_PT",
		"ru_RU",
		"ko_KR",
		"zh_TW",
		"zh_CN",
		"fi_FI",
		"sv_SE",
		"da_DK",
		"no_NO",
		"pl_PL",
		"pt_BR",
		"en_GB",
		"tr_TR",
	};
	SceAppUtilInitParam initParam;
	SceAppUtilBootParam bootParam;
	memset(&initParam, 0, sizeof(SceAppUtilInitParam));
	memset(&bootParam, 0, sizeof(SceAppUtilBootParam));
	sceAppUtilInit(&initParam, &bootParam);
	sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &language);
	if (language < 0 || language > SCE_SYSTEM_PARAM_LANG_TURKISH)
		language = SCE_SYSTEM_PARAM_LANG_ENGLISH_US; // default to english
	std::string locale = std::string(vita_locales[language]);
	sceAppUtilShutdown();
#elif defined(__3DS__)
	std::string locale = n3ds::GetLocale();
#else
	std::string locale = std::locale("").name().substr(0, 5);
#endif

	locale = locale.substr(0, 5);
	LogVerbose("Prefered locale: {}", locale);
	if (!HasTranslation(locale)) {
		locale = locale.substr(0, 2);
		if (!HasTranslation(locale)) {
			locale = "pt_BR";
		}
	}
	LogVerbose("Best match locale: {}", locale);

	GetIniValue("Language", "Code", sgOptions.Language.szCode, sizeof(sgOptions.Language.szCode), locale.c_str());

	keymapper.Load();

	if (demo::IsRunning())
		demo::OverrideOptions();

	sbWasOptionsLoaded = true;
}

void SaveOptions()
{
	SetIniValue("Diablo", "Intro", sgOptions.Diablo.bIntro);
	SetIniValue("Diablo", "LastSinglePlayerHero", sgOptions.Diablo.lastSinglePlayerHero);
	SetIniValue("Diablo", "LastMultiplayerHero", sgOptions.Diablo.lastMultiplayerHero);
	SetIniValue("Hellfire", "Intro", sgOptions.Hellfire.bIntro);
	SetIniValue("Hellfire", "SItem", sgOptions.Hellfire.szItem);
	SetIniValue("Hellfire", "LastSinglePlayerHero", sgOptions.Hellfire.lastSinglePlayerHero);
	SetIniValue("Hellfire", "LastMultiplayerHero", sgOptions.Hellfire.lastMultiplayerHero);
	SetIniValue("Hellfire", "StartUpGameOption", static_cast<int>(sgOptions.Hellfire.startUpGameOption));

	SetIniValue("Audio", "Sound Volume", sgOptions.Audio.nSoundVolume);
	SetIniValue("Audio", "Music Volume", sgOptions.Audio.nMusicVolume);
	SetIniValue("Audio", "Walking Sound", sgOptions.Audio.bWalkingSound);
	SetIniValue("Audio", "Auto Equip Sound", sgOptions.Audio.bAutoEquipSound);
	SetIniValue("Audio", "Item Pickup Sound", sgOptions.Audio.bItemPickupSound);

	SetIniValue("Audio", "Sample Rate", sgOptions.Audio.nSampleRate);
	SetIniValue("Audio", "Channels", sgOptions.Audio.nChannels);
	SetIniValue("Audio", "Buffer Size", sgOptions.Audio.nBufferSize);
	SetIniValue("Audio", "Resampling Quality", sgOptions.Audio.nResamplingQuality);
	SetIniValue("Graphics", "Width", sgOptions.Graphics.nWidth);
	SetIniValue("Graphics", "Height", sgOptions.Graphics.nHeight);
#ifndef __vita__
	SetIniValue("Graphics", "Fullscreen", sgOptions.Graphics.bFullscreen);
#endif
#if !defined(USE_SDL1)
	SetIniValue("Graphics", "Upscale", sgOptions.Graphics.bUpscale);
#endif
	SetIniValue("Graphics", "Fit to Screen", sgOptions.Graphics.bFitToScreen);
	SetIniValue("Graphics", "Scaling Quality", sgOptions.Graphics.szScaleQuality);
	SetIniValue("Graphics", "Integer Scaling", sgOptions.Graphics.bIntegerScaling);
	SetIniValue("Graphics", "Vertical Sync", sgOptions.Graphics.bVSync);
	SetIniValue("Graphics", "Blended Transparency", sgOptions.Graphics.bBlendedTransparancy);
	SetIniValue("Graphics", "Gamma Correction", sgOptions.Graphics.nGammaCorrection);
	SetIniValue("Graphics", "Color Cycling", sgOptions.Graphics.bColorCycling);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SetIniValue("Graphics", "Hardware Cursor", sgOptions.Graphics.bHardwareCursor);
	SetIniValue("Graphics", "Hardware Cursor For Items", sgOptions.Graphics.bHardwareCursorForItems);
	SetIniValue("Graphics", "Hardware Cursor Maximum Size", sgOptions.Graphics.nHardwareCursorMaxSize);
#endif
	SetIniValue("Graphics", "FPS Limiter", sgOptions.Graphics.bFPSLimit);
	SetIniValue("Graphics", "Show FPS", sgOptions.Graphics.bShowFPS);

	SetIniValue("Game", "Speed", sgOptions.Gameplay.nTickRate);
	SetIniValue("Game", "Run in Town", sgOptions.Gameplay.bRunInTown);
	SetIniValue("Game", "Grab Input", sgOptions.Gameplay.bGrabInput);
	SetIniValue("Game", "Theo Quest", sgOptions.Gameplay.bTheoQuest);
	SetIniValue("Game", "Cow Quest", sgOptions.Gameplay.bCowQuest);
	SetIniValue("Game", "Friendly Fire", sgOptions.Gameplay.bFriendlyFire);
	SetIniValue("Game", "Test Bard", sgOptions.Gameplay.bTestBard);
	SetIniValue("Game", "Test Barbarian", sgOptions.Gameplay.bTestBarbarian);
	SetIniValue("Game", "Experience Bar", sgOptions.Gameplay.bExperienceBar);
	SetIniValue("Game", "Enemy Health Bar", sgOptions.Gameplay.bEnemyHealthBar);
	SetIniValue("Game", "Auto Gold Pickup", sgOptions.Gameplay.bAutoGoldPickup);
	SetIniValue("Game", "Adria Refills Mana", sgOptions.Gameplay.bAdriaRefillsMana);
	SetIniValue("Game", "Auto Equip Weapons", sgOptions.Gameplay.bAutoEquipWeapons);
	SetIniValue("Game", "Auto Equip Armor", sgOptions.Gameplay.bAutoEquipArmor);
	SetIniValue("Game", "Auto Equip Helms", sgOptions.Gameplay.bAutoEquipHelms);
	SetIniValue("Game", "Auto Equip Shields", sgOptions.Gameplay.bAutoEquipShields);
	SetIniValue("Game", "Auto Equip Jewelry", sgOptions.Gameplay.bAutoEquipJewelry);
	SetIniValue("Game", "Randomize Quests", sgOptions.Gameplay.bRandomizeQuests);
	SetIniValue("Game", "Show Monster Type", sgOptions.Gameplay.bShowMonsterType);
	SetIniValue("Game", "Disable Crippling Shrines", sgOptions.Gameplay.bDisableCripplingShrines);
	SetIniValue("Game", "Auto Refill Belt", sgOptions.Gameplay.bAutoRefillBelt);

	SetIniValue("Network", "Bind Address", sgOptions.Network.szBindAddress);
	SetIniValue("Network", "Port", sgOptions.Network.nPort);
	SetIniValue("Network", "Previous Host", sgOptions.Network.szPreviousHost);

	for (size_t i = 0; i < QUICK_MESSAGE_OPTIONS; i++)
		SetIniValue("NetMsg", QuickMessages[i].key, sgOptions.Chat.szHotKeyMsgs[i]);

	SetIniValue("Controller", "Mapping", sgOptions.Controller.szMapping);
	SetIniValue("Controller", "Swap Shoulder Button Mode", sgOptions.Controller.bSwapShoulderButtonMode);
	SetIniValue("Controller", "Dpad Hotkeys", sgOptions.Controller.bDpadHotkeys);
	SetIniValue("Controller", "deadzone", sgOptions.Controller.fDeadzone);
#ifdef __vita__
	SetIniValue("Controller", "Enable Rear Touchpad", sgOptions.Controller.bRearTouch);
#endif

	SetIniValue("Language", "Code", sgOptions.Language.szCode);

	keymapper.Save();

	SaveIni();
}

} // namespace devilution
