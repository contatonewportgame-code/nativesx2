#include "raylib.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define setenv(k, v, o) _putenv_s(k, v)
#endif

namespace fs = std::filesystem;

static Font g_font = {0};
static Font g_fontRegular = {0};
static bool g_hasCustomFont = false;
static bool g_hasRegularFont = false;

static void drawSmoothText(const char *text, float x, float y, float fontSize, Color col, bool bold = true)
{
    if (!bold && g_hasRegularFont)
        DrawTextEx(g_fontRegular, text, Vector2{x, y}, fontSize, 0.6f, col);
    else if (g_hasCustomFont)
        DrawTextEx(g_font, text, Vector2{x, y}, fontSize, 0.6f, col);
    else
        DrawText(text, static_cast<int>(x), static_cast<int>(y), static_cast<int>(fontSize), col);
}

static float measureSmoothText(const char *text, float fontSize, bool bold = true)
{
    if (!bold && g_hasRegularFont)
        return MeasureTextEx(g_fontRegular, text, fontSize, 0.6f).x;
    if (g_hasCustomFont)
        return MeasureTextEx(g_font, text, fontSize, 0.6f).x;
    return static_cast<float>(MeasureText(text, static_cast<int>(fontSize)));
}

static float drawWrappedText(const std::string &text, float x, float y, float maxWidth, float fontSize, float lineHeight, Color col, bool bold = false)
{
    std::istringstream iss(text);
    std::string word;
    std::string currentLine;
    float curY = y;

    while (iss >> word)
    {
        std::string testLine = currentLine.empty() ? word : (currentLine + " " + word);
        if (measureSmoothText(testLine.c_str(), fontSize, bold) > maxWidth && !currentLine.empty())
        {
            drawSmoothText(currentLine.c_str(), x, curY, fontSize, col, bold);
            curY += lineHeight;
            currentLine = word;
        }
        else
        {
            currentLine = testLine;
        }
    }
    if (!currentLine.empty())
    {
        drawSmoothText(currentLine.c_str(), x, curY, fontSize, col, bold);
        curY += lineHeight;
    }
    return curY;
}

static float measureWrappedTextHeight(const std::string &text, float maxWidth, float fontSize, float lineHeight, bool bold = false)
{
    std::istringstream iss(text);
    std::string word;
    std::string currentLine;
    float totalH = 0.0f;

    while (iss >> word)
    {
        std::string testLine = currentLine.empty() ? word : (currentLine + " " + word);
        if (measureSmoothText(testLine.c_str(), fontSize, bold) > maxWidth && !currentLine.empty())
        {
            totalH += lineHeight;
            currentLine = word;
        }
        else
        {
            currentLine = testLine;
        }
    }
    if (!currentLine.empty())
    {
        totalH += lineHeight;
    }
    return totalH;
}

static void loadSmoothFont(const fs::path &appDir)
{
    std::vector<fs::path> boldCandidates = {
        appDir / "nativesx2_font.ttf",
        appDir.parent_path() / "nativesx2_font.ttf",
#ifdef _WIN32
        "C:\\Windows\\Fonts\\segoeuib.ttf",
        "C:\\Windows\\Fonts\\arialbd.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
#endif
    };

    std::vector<fs::path> regularCandidates = {
        appDir / "nativesx2_font_regular.ttf",
        appDir.parent_path() / "nativesx2_font_regular.ttf",
#ifdef _WIN32
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
    };

    // Carrega ASCII + acentos em Portugues (Latin-1) + pontuacao Unicode (bullet, travessao, seta)
    std::vector<int> codepoints;
    for (int c = 32; c <= 255; ++c)
        codepoints.push_back(c);
    codepoints.push_back(0x2022); // •
    codepoints.push_back(0x2013); // –
    codepoints.push_back(0x2014); // —
    codepoints.push_back(0x2192); // →

    for (const auto &fc : boldCandidates)
    {
        if (fs::exists(fc))
        {
            g_font = LoadFontEx(fc.string().c_str(), 44, codepoints.data(), static_cast<int>(codepoints.size()));
            if (g_font.texture.id != 0)
            {
                GenTextureMipmaps(&g_font.texture);
                SetTextureFilter(g_font.texture, TEXTURE_FILTER_TRILINEAR);
                g_hasCustomFont = true;
                break;
            }
        }
    }

    for (const auto &fc : regularCandidates)
    {
        if (fs::exists(fc))
        {
            g_fontRegular = LoadFontEx(fc.string().c_str(), 44, codepoints.data(), static_cast<int>(codepoints.size()));
            if (g_fontRegular.texture.id != 0)
            {
                GenTextureMipmaps(&g_fontRegular.texture);
                SetTextureFilter(g_fontRegular.texture, TEXTURE_FILTER_TRILINEAR);
                g_hasRegularFont = true;
                break;
            }
        }
    }
}

struct PadBinding
{
    const char *label;
    const char *envKey;
    const char *envBtn;
    int defaultKey;
    int defaultBtn;
    int key;
    int btn;
    bool hasGamepadBtn;
};

struct LauncherConfig
{
    std::string isoPath;
    int scale = 1;            // 1, 2, 3
    int displayMode = 0;      // 0 = 1280x720, 1 = 1920x1080, 2 = Fullscreen
    bool fastShadows = true;  // GOW_GS_FAST_SHADOWS
    bool wideExpanded = false;// GOW_PANORAMICO=ampliado
    bool showFps = true;      // GOW_FPS
    bool audioEnabled = true; // GOW_AUDIO
    int lang = 0;             // 0 = PT-BR, 1 = English
};

static std::vector<PadBinding> g_bindings = {
    {"X (Pulo / Confirma)",       "NATIVESX2_KEY_CROSS",    "NATIVESX2_BTN_CROSS",    KEY_X,           GAMEPAD_BUTTON_RIGHT_FACE_DOWN,  KEY_X,           GAMEPAD_BUTTON_RIGHT_FACE_DOWN,  true},
    {"Círculo (Ação / Baú)",      "NATIVESX2_KEY_CIRCLE",   "NATIVESX2_BTN_CIRCLE",   KEY_C,           GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, KEY_C,           GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, true},
    {"Quadrado (Atq. Rápido)",    "NATIVESX2_KEY_SQUARE",   "NATIVESX2_BTN_SQUARE",   KEY_Z,           GAMEPAD_BUTTON_RIGHT_FACE_LEFT,  KEY_Z,           GAMEPAD_BUTTON_RIGHT_FACE_LEFT,  true},
    {"Triângulo (Atq. Forte)",    "NATIVESX2_KEY_TRIANGLE", "NATIVESX2_BTN_TRIANGLE", KEY_V,           GAMEPAD_BUTTON_RIGHT_FACE_UP,    KEY_V,           GAMEPAD_BUTTON_RIGHT_FACE_UP,    true},
    {"L1 (Defesa / Bloqueio)",    "NATIVESX2_KEY_L1",       "NATIVESX2_BTN_L1",       KEY_Q,           GAMEPAD_BUTTON_LEFT_TRIGGER_1,   KEY_Q,           GAMEPAD_BUTTON_LEFT_TRIGGER_1,   true},
    {"R1 (Magia / Interagir)",    "NATIVESX2_KEY_R1",       "NATIVESX2_BTN_R1",       KEY_E,           GAMEPAD_BUTTON_RIGHT_TRIGGER_1,  KEY_E,           GAMEPAD_BUTTON_RIGHT_TRIGGER_1,  true},
    {"L2 (Usar Magia)",           "NATIVESX2_KEY_L2",       "NATIVESX2_BTN_L2",       KEY_LEFT_SHIFT,  GAMEPAD_BUTTON_LEFT_TRIGGER_2,   KEY_LEFT_SHIFT,  GAMEPAD_BUTTON_LEFT_TRIGGER_2,   true},
    {"R2 (Trocar Arma)",          "NATIVESX2_KEY_R2",       "NATIVESX2_BTN_R2",       KEY_RIGHT_SHIFT, GAMEPAD_BUTTON_RIGHT_TRIGGER_2,  KEY_RIGHT_SHIFT, GAMEPAD_BUTTON_RIGHT_TRIGGER_2,  true},
    {"START (Pausa / Menu)",      "NATIVESX2_KEY_START",    "NATIVESX2_BTN_START",    KEY_ENTER,       GAMEPAD_BUTTON_MIDDLE_RIGHT,     KEY_ENTER,       GAMEPAD_BUTTON_MIDDLE_RIGHT,     true},
    {"SELECT (Opções / Info)",    "NATIVESX2_KEY_SELECT",   "NATIVESX2_BTN_SELECT",   KEY_TAB,         GAMEPAD_BUTTON_MIDDLE_LEFT,      KEY_TAB,         GAMEPAD_BUTTON_MIDDLE_LEFT,      true},
    {"Analóg. Esq: Cima",         "NATIVESX2_KEY_LY_NEG",   "NATIVESX2_BTN_UP",       KEY_W,           GAMEPAD_BUTTON_LEFT_FACE_UP,     KEY_W,           GAMEPAD_BUTTON_LEFT_FACE_UP,     true},
    {"Analóg. Esq: Baixo",        "NATIVESX2_KEY_LY_POS",   "NATIVESX2_BTN_DOWN",     KEY_S,           GAMEPAD_BUTTON_LEFT_FACE_DOWN,   KEY_S,           GAMEPAD_BUTTON_LEFT_FACE_DOWN,   true},
    {"Analóg. Esq: Esq",          "NATIVESX2_KEY_LX_NEG",   "NATIVESX2_BTN_LEFT",     KEY_A,           GAMEPAD_BUTTON_LEFT_FACE_LEFT,   KEY_A,           GAMEPAD_BUTTON_LEFT_FACE_LEFT,   true},
    {"Analóg. Esq: Dir",          "NATIVESX2_KEY_LX_POS",   "NATIVESX2_BTN_RIGHT",    KEY_D,           GAMEPAD_BUTTON_LEFT_FACE_RIGHT,  KEY_D,           GAMEPAD_BUTTON_LEFT_FACE_RIGHT,  true},
    {"Analóg. Dir: Cima",         "NATIVESX2_KEY_RY_NEG",   "",                       KEY_I,           0,                               KEY_I,           0,                               false},
    {"Analóg. Dir: Baixo",        "NATIVESX2_KEY_RY_POS",   "",                       KEY_K,           0,                               KEY_K,           0,                               false},
    {"Analóg. Dir: Esq",          "NATIVESX2_KEY_RX_NEG",   "",                       KEY_J,           0,                               KEY_J,           0,                               false},
    {"Analóg. Dir: Dir",          "NATIVESX2_KEY_RX_POS",   "",                       KEY_L,           0,                               KEY_L,           0,                               false},
};

static std::string keyName(int k)
{
    if (k >= KEY_A && k <= KEY_Z)
    {
        char b[2] = {static_cast<char>('A' + (k - KEY_A)), 0};
        return b;
    }
    if (k >= KEY_ZERO && k <= KEY_NINE)
    {
        char b[2] = {static_cast<char>('0' + (k - KEY_ZERO)), 0};
        return b;
    }
    switch (k)
    {
    case KEY_SPACE: return "Espaço";
    case KEY_ENTER: return "Enter";
    case KEY_TAB: return "Tab";
    case KEY_BACKSPACE: return "Backspace";
    case KEY_ESCAPE: return "Esc";
    case KEY_UP: return "Seta Cima";
    case KEY_DOWN: return "Seta Baixo";
    case KEY_LEFT: return "Seta Esq";
    case KEY_RIGHT: return "Seta Dir";
    case KEY_LEFT_SHIFT: return "Shift Esq";
    case KEY_RIGHT_SHIFT: return "Shift Dir";
    case KEY_LEFT_CONTROL: return "Ctrl Esq";
    case KEY_RIGHT_CONTROL: return "Ctrl Dir";
    case KEY_LEFT_ALT: return "Alt Esq";
    case KEY_RIGHT_ALT: return "Alt Dir";
    case KEY_KP_0: return "Num 0";
    case KEY_KP_1: return "Num 1";
    case KEY_KP_2: return "Num 2";
    case KEY_KP_3: return "Num 3";
    case KEY_KP_4: return "Num 4";
    case KEY_KP_5: return "Num 5";
    case KEY_KP_6: return "Num 6";
    case KEY_KP_7: return "Num 7";
    case KEY_KP_8: return "Num 8";
    case KEY_KP_9: return "Num 9";
    default: return "Tecla " + std::to_string(k);
    }
}

static std::string padBtnName(int b)
{
    switch (b)
    {
    case GAMEPAD_BUTTON_RIGHT_FACE_DOWN: return "X / Cruz";
    case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT: return "Bolinha";
    case GAMEPAD_BUTTON_RIGHT_FACE_LEFT: return "Quadrado";
    case GAMEPAD_BUTTON_RIGHT_FACE_UP: return "Triângulo";
    case GAMEPAD_BUTTON_LEFT_TRIGGER_1: return "L1";
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_1: return "R1";
    case GAMEPAD_BUTTON_LEFT_TRIGGER_2: return "L2";
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_2: return "R2";
    case GAMEPAD_BUTTON_MIDDLE_RIGHT: return "START";
    case GAMEPAD_BUTTON_MIDDLE_LEFT: return "SELECT";
    case GAMEPAD_BUTTON_LEFT_THUMB: return "L3";
    case GAMEPAD_BUTTON_RIGHT_THUMB: return "R3";
    case GAMEPAD_BUTTON_LEFT_FACE_UP: return "Seta Cima";
    case GAMEPAD_BUTTON_LEFT_FACE_DOWN: return "Seta Baixo";
    case GAMEPAD_BUTTON_LEFT_FACE_LEFT: return "Seta Esq";
    case GAMEPAD_BUTTON_LEFT_FACE_RIGHT: return "Seta Dir";
    default: return "Botão " + std::to_string(b);
    }
}

// Extrator ISO9660 nativo em C++ (extrai SCUS_973.99, SYSTEM.CNF, GODOFWAR.TOC e *.IRX em < 0.2s)
static bool extractBootFilesFromIso(const fs::path &isoPath, const fs::path &gameDir)
{
    std::ifstream f(isoPath, std::ios::binary);
    if (!f)
        return false;
    constexpr uint64_t kSector = 2048;
    f.seekg(16 * kSector);
    uint8_t pvd[kSector];
    if (!f.read(reinterpret_cast<char *>(pvd), kSector))
        return false;
    if (std::memcmp(pvd + 1, "CD001", 5) != 0)
        return false;

    uint32_t rootLba = 0, rootLen = 0;
    std::memcpy(&rootLba, pvd + 156 + 2, 4);
    std::memcpy(&rootLen, pvd + 156 + 10, 4);
    if (rootLba == 0 || rootLen == 0 || rootLen > 1024 * 1024)
        return false;

    std::vector<uint8_t> dirData(rootLen);
    f.seekg(static_cast<uint64_t>(rootLba) * kSector);
    if (!f.read(reinterpret_cast<char *>(dirData.data()), rootLen))
        return false;

    fs::create_directories(gameDir);
    fs::path iopModDir = gameDir / "IOP_MOD";
    fs::create_directories(iopModDir);

    size_t off = 0;
    while (off < dirData.size())
    {
        uint8_t rlen = dirData[off];
        if (rlen == 0)
        {
            off = ((off / kSector) + 1) * kSector;
            continue;
        }
        if (off + rlen > dirData.size())
            break;
        const uint8_t *rec = dirData.data() + off;
        uint32_t extLba = 0, extLen = 0;
        std::memcpy(&extLba, rec + 2, 4);
        std::memcpy(&extLen, rec + 10, 4);
        uint8_t flags = rec[25];
        uint8_t nameLen = rec[32];
        std::string name(reinterpret_cast<const char *>(rec + 33), nameLen);
        off += rlen;
        if ((flags & 2) || nameLen <= 1)
            continue;
        size_t semi = name.find(';');
        if (semi != std::string::npos)
            name = name.substr(0, semi);

        std::string upper = name;
        for (char &c : upper)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        bool want = (upper == "SCUS_973.99" || upper == "SYSTEM.CNF" || upper == "GODOFWAR.TOC" ||
                     upper == "IOPRP300.IMG" || (upper.size() > 4 && upper.substr(upper.size() - 4) == ".IRX"));
        if (!want || extLen > 32 * 1024 * 1024)
            continue;

        std::vector<char> buf(extLen);
        f.seekg(static_cast<uint64_t>(extLba) * kSector);
        if (!f.read(buf.data(), extLen))
            continue;

        auto writeOut = [&](const fs::path &p) {
            std::ofstream out(p, std::ios::binary);
            if (out)
                out.write(buf.data(), extLen);
        };
        writeOut(gameDir / upper);
        std::string lower = upper;
        for (char &c : lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower != upper && lower.size() > 4 && lower.substr(lower.size() - 4) == ".irx")
            writeOut(gameDir / lower);
        if (upper.size() > 4 && upper.substr(upper.size() - 4) == ".IRX")
        {
            writeOut(iopModDir / upper);
            writeOut(iopModDir / lower);
        }
    }
    return fs::exists(gameDir / "SCUS_973.99");
}

static void loadConfig(const fs::path &cfgFile, LauncherConfig &cfg, const fs::path &appDir)
{
    std::vector<fs::path> candidates = {
        appDir / "God of War (USA).iso",
        appDir.parent_path() / "God of War (USA).iso",
        appDir / "artifacts" / "iso" / "God of War (USA).iso",
        appDir.parent_path() / "artifacts" / "iso" / "God of War (USA).iso",
    };
    for (const auto &c : candidates)
    {
        if (fs::exists(c))
        {
            cfg.isoPath = fs::absolute(c).string();
            break;
        }
    }

    std::ifstream in(cfgFile);
    if (!in)
        return;
    std::string line;
    while (std::getline(in, line))
    {
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        if (k == "isoPath" && !v.empty() && fs::exists(v))
            cfg.isoPath = v;
        else if (k == "scale")
            cfg.scale = std::clamp(std::atoi(v.c_str()), 1, 3);
        else if (k == "displayMode")
            cfg.displayMode = std::clamp(std::atoi(v.c_str()), 0, 2);
        else if (k == "fastShadows")
            cfg.fastShadows = (v == "1");
        else if (k == "wideExpanded")
            cfg.wideExpanded = (v == "1");
        else if (k == "showFps")
            cfg.showFps = (v == "1");
        else if (k == "audioEnabled")
            cfg.audioEnabled = (v == "1");
        else if (k == "lang")
            cfg.lang = std::clamp(std::atoi(v.c_str()), 0, 1);
        else
        {
            for (auto &b : g_bindings)
            {
                if (k == b.envKey)
                    b.key = std::atoi(v.c_str());
                else if (b.hasGamepadBtn && k == b.envBtn)
                    b.btn = std::atoi(v.c_str());
            }
        }
    }
}

static void saveConfig(const fs::path &cfgFile, const LauncherConfig &cfg)
{
    std::ofstream out(cfgFile);
    if (!out)
        return;
    out << "isoPath=" << cfg.isoPath << "\n";
    out << "scale=" << cfg.scale << "\n";
    out << "displayMode=" << cfg.displayMode << "\n";
    out << "fastShadows=" << (cfg.fastShadows ? 1 : 0) << "\n";
    out << "wideExpanded=" << (cfg.wideExpanded ? 1 : 0) << "\n";
    out << "showFps=" << (cfg.showFps ? 1 : 0) << "\n";
    out << "audioEnabled=" << (cfg.audioEnabled ? 1 : 0) << "\n";
    out << "lang=" << cfg.lang << "\n";
    for (const auto &b : g_bindings)
    {
        out << b.envKey << "=" << b.key << "\n";
        if (b.hasGamepadBtn)
            out << b.envBtn << "=" << b.btn << "\n";
    }
}

static fs::path findRunnerBinary(const fs::path &appDir)
{
#ifdef _WIN32
    std::vector<fs::path> candidates = {
        appDir / "ps2EntryRunner.exe",
        appDir / "build_win" / "PS2Recomp" / "out" / "build" / "ps2xRuntime" / "ps2EntryRunner.exe",
        appDir.parent_path() / "build_win" / "PS2Recomp" / "out" / "build" / "ps2xRuntime" / "ps2EntryRunner.exe",
    };
#else
    std::vector<fs::path> candidates = {
        appDir / "ps2EntryRunner",
        appDir / "recomp" / "build" / "ps2xRuntime" / "ps2EntryRunner",
        appDir.parent_path() / "recomp" / "build" / "ps2xRuntime" / "ps2EntryRunner",
    };
#endif
    for (const auto &c : candidates)
        if (fs::exists(c))
            return fs::absolute(c);
    return candidates[0];
}

static fs::path findGameDir(const fs::path &appDir)
{
    if (fs::exists(appDir / "game" / "SCUS_973.99"))
        return fs::absolute(appDir / "game");
    if (fs::exists(appDir.parent_path() / "game" / "SCUS_973.99"))
        return fs::absolute(appDir.parent_path() / "game");
    return fs::absolute(appDir / "game");
}

static bool launchGame(const LauncherConfig &cfg, const fs::path &appDir, std::string &errMsg)
{
    if (cfg.isoPath.empty() || !fs::exists(cfg.isoPath))
    {
        errMsg = "Arquivo .ISO não encontrado! Selecione a ISO de God of War.";
        return false;
    }
    fs::path gameDir = findGameDir(appDir);
    if (!fs::exists(gameDir / "SCUS_973.99"))
    {
        if (!extractBootFilesFromIso(cfg.isoPath, gameDir))
        {
            errMsg = "Não foi possível extrair SCUS_973.99 da ISO selecionada.";
            return false;
        }
    }
    fs::path runner = findRunnerBinary(appDir);
    if (!fs::exists(runner))
    {
        errMsg = "Motor ps2EntryRunner não encontrado em: " + runner.string();
        return false;
    }

    setenv("GOW_ISO", cfg.isoPath.c_str(), 1);
    setenv("PS2X_GS_WORKER", "1", 1);
    setenv("GOW_GS_FINISH_ASINCRONO", "1", 1);
    setenv("GOW_AUDIO_DEVICE", "default", 1);
    setenv("MESA_GL_VERSION_OVERRIDE", "4.5", 1);
    setenv("MESA_GLSL_VERSION_OVERRIDE", "450", 1);
    setenv("PS2X_GS_SCALE", "1", 1);
    setenv("PS2X_GS_HW_SCALE", std::to_string(cfg.scale).c_str(), 1);
    setenv("GOW_SCALER", "bilinear", 1);

    if (cfg.fastShadows)
    {
        setenv("GOW_GS_FAST_SHADOWS", "1", 1);
        setenv("GOW_VU1_FAST", "1", 1);
        setenv("GOW_DISPATCH_FULL", "0", 1);
    }
    else
    {
        setenv("GOW_GS_FAST_SHADOWS", "", 1);
        setenv("GOW_VU1_FAST", "", 1);
        setenv("GOW_DISPATCH_FULL", "1", 1);
    }

    if (cfg.wideExpanded)
        setenv("GOW_PANORAMICO", "ampliado", 1);
    else
        setenv("GOW_PANORAMICO", "", 1);

    if (cfg.displayMode == 2)
    {
        setenv("GOW_PANTALLA_COMPLETA", "1", 1);
    }
    else
    {
        setenv("GOW_PANTALLA_COMPLETA", "0", 1);
        setenv("GOW_WINDOW", cfg.displayMode == 1 ? "1920x1080" : "1280x720", 1);
    }

    setenv("GOW_FPS", cfg.showFps ? "1" : "0", 1);
    setenv("GOW_AUDIO", cfg.audioEnabled ? "1" : "0", 1);

    for (const auto &b : g_bindings)
    {
        setenv(b.envKey, std::to_string(b.key).c_str(), 1);
        if (b.hasGamepadBtn)
            setenv(b.envBtn, std::to_string(b.btn).c_str(), 1);
    }

    fs::path elfPath = gameDir / "SCUS_973.99";
    fs::current_path(runner.parent_path());

#ifdef _WIN32
    std::string cmd = "\"\"" + runner.string() + "\" \"" + elfPath.string() + "\"\"";
#else
    std::string cmd = "\"" + runner.string() + "\" \"" + elfPath.string() + "\"";
#endif
    std::exit(std::system(cmd.c_str()));
    return true;
}

static std::string browseForIso(const fs::path &appDir)
{
#ifdef _WIN32
    std::string cmd = "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; "
                      "$f = New-Object System.Windows.Forms.OpenFileDialog; "
                      "$f.Filter = 'Imagens ISO de PS2 (*.iso)|*.iso|Todos os arquivos (*.*)|*.*'; "
                      "if ($f.ShowDialog() -eq 'OK') { Write-Output $f.FileName }\"";
    FILE *p = _popen(cmd.c_str(), "r");
#else
    std::string cmd = "zenity --file-selection --title=\"Selecione a ISO do Jogo (NativeSX2)\" --file-filter=\"*.iso *.ISO\" 2>/dev/null";
    FILE *p = popen(cmd.c_str(), "r");
#endif
    if (!p)
        return "";
    char buf[1024];
    std::string res;
    while (std::fgets(buf, sizeof(buf), p))
        res += buf;
#ifdef _WIN32
    _pclose(p);
#else
    pclose(p);
#endif
    while (!res.empty() && (res.back() == '\n' || res.back() == '\r'))
        res.pop_back();
    return res;
}

static bool registerIsoAssociation(const fs::path &exePath, std::string &msg)
{
#ifdef _WIN32
    std::string exe = exePath.string();
    std::string c1 = "reg add \"HKCU\\Software\\Classes\\.iso\" /ve /d \"NativeSX2.iso\" /f >nul 2>&1";
    std::string c2 = "reg add \"HKCU\\Software\\Classes\\.iso\\OpenWithProgids\" /v \"NativeSX2.iso\" /t REG_NONE /f >nul 2>&1";
    std::string c3 = "reg add \"HKCU\\Software\\Classes\\NativeSX2.iso\" /ve /d \"Jogo de PS2 - NativeSX2 (by NewPortGame)\" /f >nul 2>&1";
    std::string c4 = "reg add \"HKCU\\Software\\Classes\\NativeSX2.iso\\shell\\open\\command\" /ve /d \"\\\"" + exe + "\\\" \\\"%1\\\"\" /f >nul 2>&1";
    std::string c_icon = "reg add \"HKCU\\Software\\Classes\\NativeSX2.iso\\DefaultIcon\" /ve /d \"\\\"" + exe + "\\\",0\" /f >nul 2>&1";
    std::string c5 = "reg add \"HKCU\\Software\\Classes\\Applications\\NativeSX2.exe\" /v \"FriendlyAppName\" /d \"NativeSX2 (by NewPortGame)\" /f >nul 2>&1";
    std::string c6 = "reg add \"HKCU\\Software\\Classes\\Applications\\NativeSX2.exe\\shell\\open\\command\" /ve /d \"\\\"" + exe + "\\\" \\\"%1\\\"\" /f >nul 2>&1";
    bool ok = (std::system(c1.c_str()) == 0 && std::system(c2.c_str()) == 0 &&
               std::system(c3.c_str()) == 0 && std::system(c4.c_str()) == 0 &&
               std::system(c_icon.c_str()) == 0 &&
               std::system(c5.c_str()) == 0 && std::system(c6.c_str()) == 0);
    if (ok)
    {
        msg = "Arquivos .ISO vinculados ao NativeSX2 no Windows! De 2 cliques na .ISO para jogar!";
        return true;
    }
    msg = "Falha ao registrar .ISO no Windows.";
    return false;
#else
    const char *home = std::getenv("HOME");
    if (!home)
    {
        msg = "Variável HOME não encontrada.";
        return false;
    }
    fs::path appsDir = fs::path(home) / ".local" / "share" / "applications";
    fs::create_directories(appsDir);
    fs::path desktopFile = appsDir / "nativesx2.desktop";
    std::ofstream out(desktopFile);
    if (!out)
    {
        msg = "Erro ao criar nativesx2.desktop.";
        return false;
    }
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=NativeSX2 (by NewPortGame)\n"
        << "Comment=Motor Nativo de Jogos PS2 para PC\n"
        << "Exec=\"" << exePath.string() << "\" %f\n"
        << "Terminal=false\n"
        << "Categories=Game;\n"
        << "MimeType=application/x-cd-image;application/x-iso9660-image;\n";
    out.close();
    std::system("xdg-mime default nativesx2.desktop application/x-cd-image 2>/dev/null");
    std::system("xdg-mime default nativesx2.desktop application/x-iso9660-image 2>/dev/null");
    msg = "Atalho e associação .ISO criados no Linux com sucesso!";
    return true;
#endif
}

static bool drawButton(Rectangle r, const char *text, Color baseCol, Color hoverCol, Color textCol = WHITE, float fontSize = 18.0f, float roundness = 0.22f)
{
    Vector2 m = GetMousePosition();
    bool hot = CheckCollisionPointRec(m, r);
    DrawRectangleRounded(r, roundness, 8, hot ? hoverCol : baseCol);
    DrawRectangleRoundedLinesEx(r, roundness, 8, 1.8f, hot ? Color{130, 210, 255, 255} : Color{65, 105, 160, 220});

    // Ajusta automaticamente o tamanho da fonte para manter margem simétrica dentro do botão
    float fitFontSize = fontSize;
    float maxTextWidth = r.width - 28.0f;
    float tw = measureSmoothText(text, fitFontSize, true);
    while (tw > maxTextWidth && fitFontSize > 11.0f)
    {
        fitFontSize -= 0.5f;
        tw = measureSmoothText(text, fitFontSize, true);
    }

    drawSmoothText(text, r.x + (r.width - tw) * 0.5f, r.y + (r.height - fitFontSize) * 0.5f, fitFontSize, textCol, true);
    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

int main(int argc, char **argv)
{
    fs::path exePath = fs::absolute(argv[0]);
    fs::path appDir = exePath.parent_path();
    fs::path cfgFile = appDir / "nativesx2_config.cfg";

    LauncherConfig cfg;
    loadConfig(cfgFile, cfg, appDir);

    // MODO DIRETO: 2 cliques no arquivo .ISO abre o jogo imediatamente sem janela intermediária!
    if (argc >= 2 && argv[1][0] != '-')
    {
        fs::path argPath = fs::absolute(argv[1]);
        if (fs::exists(argPath))
        {
            cfg.isoPath = argPath.string();
            saveConfig(cfgFile, cfg);
            std::string errMsg;
            if (!launchGame(cfg, appDir, errMsg))
            {
                std::fprintf(stderr, "[NativeSX2] %s\n", errMsg.c_str());
                return 1;
            }
            return 0;
        }
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1020, 680, "NativeSX2 (by NewPortGame) - Central de Jogos Nativos PS2");
    SetTargetFPS(60);
    SetExitKey(0);
    loadSmoothFont(appDir);

    // Registra automaticamente a associacao de .ISO ao abrir o NativeSX2 pela primeira vez
    {
        std::string autoMsg;
        registerIsoAssociation(exePath, autoMsg);
    }

    int tab = 0; // 0 = Jogar & Gráficos, 1 = Mapear Controle/Teclado, 2 = Memory Card & Sistema, 3 = Créditos
    int waitingKeyIdx = -1;
    int waitingBtnIdx = -1;
    float creditsScrollY = 0.0f;
    bool draggingScrollbar = false;
    std::string statusMsg = "Pronto! Arraste uma .ISO para a janela ou clique em JOGAR AGORA.";
    Color statusCol = {140, 225, 255, 255};

    while (!WindowShouldClose())
    {
        if (IsFileDropped())
        {
            FilePathList dropped = LoadDroppedFiles();
            if (dropped.count > 0)
            {
                cfg.isoPath = dropped.paths[0];
                saveConfig(cfgFile, cfg);
                statusMsg = "ISO carregada: " + fs::path(cfg.isoPath).filename().string();
                statusCol = {100, 255, 160, 255};
            }
            UnloadDroppedFiles(dropped);
        }

        if (waitingKeyIdx >= 0)
        {
            int k = GetKeyPressed();
            if (k == KEY_ESCAPE)
            {
                waitingKeyIdx = -1;
                statusMsg = "Mapeamento cancelado.";
            }
            else if (k > 0)
            {
                g_bindings[waitingKeyIdx].key = k;
                saveConfig(cfgFile, cfg);
                statusMsg = std::string("Tecla mapeada para ") + g_bindings[waitingKeyIdx].label + ": " + keyName(k);
                statusCol = {100, 255, 160, 255};
                waitingKeyIdx = -1;
            }
        }
        else if (waitingBtnIdx >= 0)
        {
            if (IsKeyPressed(KEY_ESCAPE))
            {
                waitingBtnIdx = -1;
                statusMsg = "Mapeamento de controle cancelado.";
            }
            else
            {
                for (int pad = 0; pad < 4; ++pad)
                {
                    if (!IsGamepadAvailable(pad))
                        continue;
                    for (int b = 1; b <= GAMEPAD_BUTTON_RIGHT_THUMB; ++b)
                    {
                        if (IsGamepadButtonPressed(pad, b))
                        {
                            g_bindings[waitingBtnIdx].btn = b;
                            saveConfig(cfgFile, cfg);
                            statusMsg = std::string("Botão do controle mapeado para ") + g_bindings[waitingBtnIdx].label + ": " + padBtnName(b);
                            statusCol = {100, 255, 160, 255};
                            waitingBtnIdx = -1;
                            break;
                        }
                    }
                }
            }
        }

        BeginDrawing();
        ClearBackground(Color{14, 20, 36, 255});

        // Topo estilo PS2
        DrawRectangleGradientV(0, 0, 1020, 86, Color{20, 40, 84, 255}, Color{14, 22, 44, 255});
        DrawLine(0, 86, 1020, 86, Color{60, 135, 235, 255});
        drawSmoothText("NativeSX2", 28, 12, 38, Color{95, 195, 255, 255});
        drawSmoothText(cfg.lang == 0 ? "by NewPortGame   |   Plataforma de Jogos PS2 Nativos para PC"
                                     : "by NewPortGame   |   Native PS2 Game Platform for PC",
                       30, 54, 19, Color{195, 215, 245, 255});

        // Botões de Idioma (PT / EN) circulares no canto superior direito
        // Botões redondos para alternar idioma (PT / EN)
        Rectangle btnPt = {884, 22, 44, 38};
        Rectangle btnEn = {938, 22, 44, 38};
        if (drawButton(btnPt, "PT", cfg.lang == 0 ? Color{35, 140, 85, 255} : Color{25, 42, 70, 255}, Color{45, 170, 105, 255}, WHITE, 16, 0.5f))
        {
            cfg.lang = 0;
            saveConfig(cfgFile, cfg);
        }
        if (drawButton(btnEn, "EN", cfg.lang == 1 ? Color{35, 140, 85, 255} : Color{25, 42, 70, 255}, Color{45, 170, 105, 255}, WHITE, 16, 0.5f))
        {
            cfg.lang = 1;
            saveConfig(cfgFile, cfg);
        }

        // Abas (4 abas alinhadas em 964px: 232px cada + 12px de espaçamento)
        const char *tabNames[4][2] = {
            {"1. JOGAR E GRÁFICOS", "1. PLAY & GRAPHICS"},
            {"2. CONTROLE E TECLADO", "2. CONTROLLER & KEYS"},
            {"3. MEMORY CARD E .ISO", "3. MEMORY CARD & .ISO"},
            {"4. CRÉDITOS E LICENÇA", "4. CREDITS & LICENSE"}
        };
        if (drawButton(Rectangle{28, 98, 232, 42}, tabNames[0][cfg.lang],
                       tab == 0 ? Color{35, 100, 195, 255} : Color{22, 36, 64, 255}, Color{50, 125, 225, 255}, WHITE, 18))
        {
            tab = 0; waitingKeyIdx = waitingBtnIdx = -1;
        }
        if (drawButton(Rectangle{272, 98, 232, 42}, tabNames[1][cfg.lang],
                       tab == 1 ? Color{35, 100, 195, 255} : Color{22, 36, 64, 255}, Color{50, 125, 225, 255}, WHITE, 18))
        {
            tab = 1; waitingKeyIdx = waitingBtnIdx = -1;
        }
        if (drawButton(Rectangle{516, 98, 232, 42}, tabNames[2][cfg.lang],
                       tab == 2 ? Color{35, 100, 195, 255} : Color{22, 36, 64, 255}, Color{50, 125, 225, 255}, WHITE, 18))
        {
            tab = 2; waitingKeyIdx = waitingBtnIdx = -1;
        }
        if (drawButton(Rectangle{760, 98, 232, 42}, tabNames[3][cfg.lang],
                       tab == 3 ? Color{35, 100, 195, 255} : Color{22, 36, 64, 255}, Color{50, 125, 225, 255}, WHITE, 18))
        {
            tab = 3; waitingKeyIdx = waitingBtnIdx = -1;
        }

        if (tab == 0)
        {
            // Caixa ISO
            DrawRectangleRounded(Rectangle{28, 152, 964, 88}, 0.12f, 8, Color{20, 31, 56, 255});
            drawSmoothText(cfg.lang == 0 ? "IMAGEM DO DISCO (.ISO):" : "DISC IMAGE (.ISO):", 46, 162, 19, Color{115, 200, 255, 255});
            std::string isoDefault = (cfg.lang == 0) ? "Nenhuma ISO selecionada (Clique em Procurar .ISO ou arraste o arquivo aqui)"
                                                     : "No ISO selected (Click Browse .ISO or drag and drop file here)";
            std::string isoDisplay = cfg.isoPath.empty() ? isoDefault : cfg.isoPath;
            if (isoDisplay.size() > 74)
                isoDisplay = "..." + isoDisplay.substr(isoDisplay.size() - 71);
            drawSmoothText(isoDisplay.c_str(), 46, 194, 19, cfg.isoPath.empty() ? Color{255, 135, 135, 255} : WHITE);

            if (drawButton(Rectangle{792, 168, 182, 52}, cfg.lang == 0 ? "Procurar .ISO" : "Browse .ISO",
                           Color{35, 90, 170, 255}, Color{55, 120, 215, 255}, WHITE, 19))
            {
                std::string picked = browseForIso(appDir);
                if (!picked.empty())
                {
                    cfg.isoPath = picked;
                    saveConfig(cfgFile, cfg);
                    statusMsg = (cfg.lang == 0 ? "ISO selecionada: " : "ISO selected: ") + fs::path(picked).filename().string();
                    statusCol = {100, 255, 160, 255};
                }
            }

            // Perfis Rápidos
            DrawRectangleRounded(Rectangle{28, 252, 964, 92}, 0.12f, 8, Color{20, 31, 56, 255});
            drawSmoothText(cfg.lang == 0 ? "PERFIS RÁPIDOS DE DESEMPENHO (1 CLIQUE):" : "QUICK HARDWARE PRESETS (1 CLICK):", 46, 262, 19, Color{115, 200, 255, 255});
            if (drawButton(Rectangle{46, 290, 445, 42},
                           cfg.lang == 0 ? "Modo PC Fraco / Notebook (1x + Sombras Leves)" : "Low Spec PC / Laptop Mode (1x + Fast Shadows)",
                           (cfg.scale == 1 && cfg.fastShadows) ? Color{30, 135, 92, 255} : Color{30, 50, 84, 255},
                           Color{45, 165, 115, 255}, WHITE, 18))
            {
                cfg.scale = 1;
                cfg.fastShadows = true;
                cfg.displayMode = 0;
                saveConfig(cfgFile, cfg);
                statusMsg = (cfg.lang == 0) ? "Perfil PC Fraco ativado (Máximo de FPS para Intel HD / Notebooks)!"
                                            : "Low Spec profile enabled (Maximum FPS for Intel HD / Laptops)!";
                statusCol = {100, 255, 160, 255};
            }
            if (drawButton(Rectangle{515, 290, 458, 42},
                           cfg.lang == 0 ? "Modo PC Forte / Gamer (2x HD + Tela Cheia)" : "High Spec PC / Gaming Mode (2x HD + Fullscreen)",
                           (cfg.scale >= 2 && !cfg.fastShadows) ? Color{30, 135, 92, 255} : Color{30, 50, 84, 255},
                           Color{45, 165, 115, 255}, WHITE, 18))
            {
                cfg.scale = 2;
                cfg.fastShadows = false;
                cfg.displayMode = 2;
                saveConfig(cfgFile, cfg);
                statusMsg = (cfg.lang == 0) ? "Perfil PC Forte ativado (Resolução 2x HD + Sombras Originais + Tela Cheia)!"
                                            : "High Spec profile enabled (2x HD Resolution + Fullscreen)!";
                statusCol = {100, 255, 160, 255};
            }

            // Ajustes Individuais
            DrawRectangleRounded(Rectangle{28, 356, 964, 230}, 0.08f, 8, Color{20, 31, 56, 255});
            drawSmoothText(cfg.lang == 0 ? "AJUSTES GRÁFICOS E DE ÁUDIO:" : "GRAPHICS & AUDIO SETTINGS:", 46, 368, 19, Color{115, 200, 255, 255});

            const char *scaleLabels[2][3] = {
                {"Resolução: 1x Nativa (Mais FPS)", "Resolução: 2x Alta Definição", "Resolução: 3x Ultra HD"},
                {"Resolution: 1x Native (Higher FPS)", "Resolution: 2x High Definition", "Resolution: 3x Ultra HD"}
            };
            if (drawButton(Rectangle{46, 400, 445, 46}, scaleLabels[cfg.lang][cfg.scale - 1], Color{30, 56, 98, 255}, Color{48, 84, 146, 255}, WHITE, 19))
            {
                cfg.scale = (cfg.scale % 3) + 1;
                saveConfig(cfgFile, cfg);
            }

            const char *dispLabels[2][3] = {
                {"Tela: Janela 1280x720", "Tela: Janela 1920x1080", "Tela: Tela Cheia Sem Bordas"},
                {"Display: Window 1280x720", "Display: Window 1920x1080", "Display: Borderless Fullscreen"}
            };
            if (drawButton(Rectangle{515, 400, 458, 46}, dispLabels[cfg.lang][cfg.displayMode], Color{30, 56, 98, 255}, Color{48, 84, 146, 255}, WHITE, 19))
            {
                cfg.displayMode = (cfg.displayMode + 1) % 3;
                saveConfig(cfgFile, cfg);
            }

            const char *shadowText = (cfg.lang == 0)
                ? (cfg.fastShadows ? "Sombras: Rápidas (Mais FPS)" : "Sombras: Originais do PS2")
                : (cfg.fastShadows ? "Shadows: Fast (Higher FPS)" : "Shadows: Original PS2");
            if (drawButton(Rectangle{46, 460, 445, 46}, shadowText, Color{30, 56, 98, 255}, Color{48, 84, 146, 255}, WHITE, 19))
            {
                cfg.fastShadows = !cfg.fastShadows;
                saveConfig(cfgFile, cfg);
            }

            const char *wideText = (cfg.lang == 0)
                ? (cfg.wideExpanded ? "Câmera: Widescreen 16:9 Ampliado" : "Câmera: Widescreen 16:9 Padrão")
                : (cfg.wideExpanded ? "Camera: Expanded 16:9 Wide" : "Camera: Standard 16:9 Wide");
            if (drawButton(Rectangle{515, 460, 458, 46}, wideText, Color{30, 56, 98, 255}, Color{48, 84, 146, 255}, WHITE, 19))
            {
                cfg.wideExpanded = !cfg.wideExpanded;
                saveConfig(cfgFile, cfg);
            }

            const char *fpsText = (cfg.lang == 0)
                ? (cfg.showFps ? "Medidor de FPS (F2): Visível" : "Medidor de FPS (F2): Oculto")
                : (cfg.showFps ? "FPS Overlay (F2): Visible" : "FPS Overlay (F2): Hidden");
            if (drawButton(Rectangle{46, 520, 445, 46}, fpsText, Color{30, 56, 98, 255}, Color{48, 84, 146, 255}, WHITE, 19))
            {
                cfg.showFps = !cfg.showFps;
                saveConfig(cfgFile, cfg);
            }

            const char *audioText = (cfg.lang == 0)
                ? (cfg.audioEnabled ? "Áudio (SPU2 / 989snd): Ativado" : "Áudio (SPU2 / 989snd): Mudo")
                : (cfg.audioEnabled ? "Audio (SPU2 / 989snd): Enabled" : "Audio (SPU2 / 989snd): Muted");
            if (drawButton(Rectangle{515, 520, 458, 46}, audioText, Color{30, 56, 98, 255}, Color{48, 84, 146, 255}, WHITE, 19))
            {
                cfg.audioEnabled = !cfg.audioEnabled;
                saveConfig(cfgFile, cfg);
            }
        }
        else if (tab == 1)
        {
            // Aba de Mapeamento de Controle e Teclado
            DrawRectangleRounded(Rectangle{28, 148, 964, 440}, 0.06f, 8, Color{20, 31, 56, 255});

            std::string padStatus = (cfg.lang == 0)
                ? "Controle USB / Bluetooth: Nenhum detectado (Conecte o controle para mapear)"
                : "USB / Bluetooth Gamepad: None detected (Connect controller to map)";
            Color padCol = {235, 190, 105, 255};
            for (int i = 0; i < 4; ++i)
            {
                if (IsGamepadAvailable(i))
                {
                    padStatus = (cfg.lang == 0 ? "Controle Ativo: " : "Active Gamepad: ") + std::string(GetGamepadName(i));
                    padCol = {100, 255, 160, 255};
                    break;
                }
            }
            drawSmoothText(padStatus.c_str(), 44, 158, 18, padCol);

            if (drawButton(Rectangle{760, 154, 214, 32}, cfg.lang == 0 ? "Restaurar Padrão" : "Reset Defaults",
                           Color{115, 48, 58, 255}, Color{155, 65, 80, 255}, WHITE, 17))
            {
                for (auto &b : g_bindings)
                {
                    b.key = b.defaultKey;
                    b.btn = b.defaultBtn;
                }
                saveConfig(cfgFile, cfg);
                statusMsg = (cfg.lang == 0) ? "Mapeamento de Teclado e Controle restaurado para o padrão original!"
                                            : "Keyboard & Controller mapping reset to default!";
                statusCol = {100, 255, 160, 255};
            }

            for (size_t i = 0; i < g_bindings.size(); ++i)
            {
                int col = (i >= 9) ? 1 : 0;
                int row = static_cast<int>(i % 9);
                float x0 = col == 0 ? 42.0f : 522.0f;
                float y0 = 196.0f + row * 42.0f;

                drawSmoothText(g_bindings[i].label, x0, y0 + 8.0f, 16.5f, WHITE, true);

                std::string kText = (waitingKeyIdx == static_cast<int>(i)) ? "Aperte tecla..." : ("Tecl: " + keyName(g_bindings[i].key));
                if (drawButton(Rectangle{x0 + 212, y0, 122, 35}, kText.c_str(),
                               waitingKeyIdx == static_cast<int>(i) ? Color{195, 125, 20, 255} : Color{32, 58, 100, 255},
                               Color{52, 90, 150, 255}, WHITE, 16))
                {
                    waitingKeyIdx = static_cast<int>(i);
                    waitingBtnIdx = -1;
                    statusMsg = std::string("Pressione no TECLADO a tecla para: ") + g_bindings[i].label + " (Esc cancela)";
                    statusCol = {255, 225, 100, 255};
                }

                if (g_bindings[i].hasGamepadBtn)
                {
                    std::string bText = (waitingBtnIdx == static_cast<int>(i)) ? "Aperte botão..." : padBtnName(g_bindings[i].btn);
                    if (drawButton(Rectangle{x0 + 340, y0, 118, 35}, bText.c_str(),
                                   waitingBtnIdx == static_cast<int>(i) ? Color{195, 125, 20, 255} : Color{36, 74, 112, 255},
                                   Color{56, 110, 162, 255}, WHITE, 16))
                    {
                        waitingBtnIdx = static_cast<int>(i);
                        waitingKeyIdx = -1;
                        statusMsg = std::string("Pressione no CONTROLE o botão para: ") + g_bindings[i].label + " (Esc cancela)";
                        statusCol = {255, 225, 100, 255};
                    }
                }
            }
        }
        else if (tab == 2)
        {
            // Aba Memory Card e Associação .ISO
            DrawRectangleRounded(Rectangle{28, 152, 964, 200}, 0.08f, 8, Color{20, 31, 56, 255});
            drawSmoothText(cfg.lang == 0 ? "MEMORY CARD VIRTUAL DO PS2 (SLOT 1 - 8 MB):" : "VIRTUAL PS2 MEMORY CARD (SLOT 1 - 8 MB):", 46, 166, 19, Color{115, 200, 255, 255});

            fs::path gameDir = findGameDir(appDir);
            fs::path mcdPath = gameDir / "Mcd001.ps2";
            fs::path backupPath = appDir / "backups_saves" / "Mcd001_backup.ps2";

            std::string mcdStatus;
            if (cfg.lang == 0)
                mcdStatus = fs::exists(mcdPath) ? "Status: Mcd001.ps2 ativo (8 MB - Pronto para salvar na pasta game/)"
                                                : "Status: Será criado automaticamente ao salvar (em game/Mcd001.ps2)";
            else
                mcdStatus = fs::exists(mcdPath) ? "Status: Mcd001.ps2 active (8 MB - Ready to save in game/ folder)"
                                                : "Status: Will be created automatically on save (in game/Mcd001.ps2)";

            drawSmoothText(mcdStatus.c_str(), 46, 200, 18, fs::exists(mcdPath) ? Color{125, 240, 185, 255} : WHITE);
            drawSmoothText(cfg.lang == 0 ? "Dentro do jogo, aperte R1 (no controle) ou E (no teclado) nos feixes de luz para salvar!"
                                         : "In-game, press R1 (controller) or E (keyboard) at glowing save pillars!",
                           46, 230, 18, Color{160, 230, 185, 255});

            if (drawButton(Rectangle{46, 272, 290, 54}, cfg.lang == 0 ? "Fazer Backup do Save" : "Backup Save",
                           Color{30, 98, 155, 255}, Color{45, 130, 200, 255}, WHITE, 19))
            {
                if (fs::exists(mcdPath))
                {
                    fs::create_directories(backupPath.parent_path());
                    fs::copy_file(mcdPath, backupPath, fs::copy_options::overwrite_existing);
                    statusMsg = (cfg.lang == 0) ? "Backup do Memory Card salvo em: backups_saves/Mcd001_backup.ps2!"
                                                : "Memory Card backup saved to: backups_saves/Mcd001_backup.ps2!";
                    statusCol = {100, 255, 160, 255};
                }
                else
                {
                    statusMsg = (cfg.lang == 0) ? "Ainda não há Memory Card criado. Inicie o jogo uma vez primeiro!"
                                                : "No Memory Card file yet. Start the game once first!";
                    statusCol = {255, 185, 100, 255};
                }
            }

            if (drawButton(Rectangle{356, 272, 300, 54}, cfg.lang == 0 ? "Restaurar Backup do Save" : "Restore Backup Save",
                           Color{35, 78, 125, 255}, Color{55, 110, 170, 255}, WHITE, 19))
            {
                if (fs::exists(backupPath))
                {
                    fs::create_directories(mcdPath.parent_path());
                    fs::copy_file(backupPath, mcdPath, fs::copy_options::overwrite_existing);
                    statusMsg = (cfg.lang == 0) ? "Backup do Memory Card restaurado com sucesso!"
                                                : "Memory Card backup restored successfully!";
                    statusCol = {100, 255, 160, 255};
                }
                else
                {
                    statusMsg = (cfg.lang == 0) ? "Nenhum backup encontrado em backups_saves/Mcd001_backup.ps2."
                                                : "No backup found in backups_saves/Mcd001_backup.ps2.";
                    statusCol = {255, 185, 100, 255};
                }
            }

            if (drawButton(Rectangle{676, 272, 296, 54}, cfg.lang == 0 ? "Abrir Pasta de Saves" : "Open Saves Folder",
                           Color{35, 78, 125, 255}, Color{55, 110, 170, 255}, WHITE, 19))
            {
                fs::create_directories(gameDir);
#ifdef _WIN32
                std::string cmd = "explorer \"" + gameDir.string() + "\"";
#else
                std::string cmd = "xdg-open \"" + gameDir.string() + "\" >/dev/null 2>&1 &";
#endif
                std::system(cmd.c_str());
                statusMsg = (cfg.lang == 0) ? "Pasta de saves aberta!" : "Saves folder opened!";
            }

            // Associação de .ISO (2 cliques)
            DrawRectangleRounded(Rectangle{28, 368, 964, 214}, 0.08f, 8, Color{20, 31, 56, 255});
            drawSmoothText(cfg.lang == 0 ? "ABRIR ARQUIVOS .ISO COM 2 CLIQUES (MODO DIRETO):" : "DIRECT 2-CLICK .ISO LAUNCH MODE:", 46, 384, 19, Color{115, 200, 255, 255});
            drawSmoothText(cfg.lang == 0 ? "Ao associar os arquivos .ISO ao NativeSX2, basta dar 2 cliques em cima do arquivo .ISO"
                                         : "Once associated with NativeSX2, double-clicking any .ISO file in your folders",
                           46, 418, 18, WHITE);
            drawSmoothText(cfg.lang == 0 ? "para o jogo abrir imediatamente usando as configurações salvas aqui nesta Central!"
                                         : "immediately launches the game natively using the settings configured in this Hub!",
                           46, 446, 18, WHITE);

            if (drawButton(Rectangle{46, 490, 540, 60}, cfg.lang == 0 ? "Associar Arquivos .ISO ao NativeSX2 Agora"
                                                                      : "Associate .ISO Files with NativeSX2 Now",
                           Color{35, 115, 180, 255}, Color{50, 150, 230, 255}, WHITE, 20))
            {
                std::string msg;
                bool ok = registerIsoAssociation(exePath, msg);
                statusMsg = msg;
                statusCol = ok ? Color{100, 255, 160, 255} : Color{255, 130, 130, 255};
            }
        }
        else if (tab == 3)
        {
            // Aba 4: Créditos, Engenharia NewPortGame, Colaboradores e Licença (com Layout Dinâmico e Rolagem)
            Rectangle scrollBox = {28, 148, 964, 368};
            DrawRectangleRounded(scrollBox, 0.05f, 8, Color{20, 31, 56, 255});
            DrawRectangleRoundedLinesEx(scrollBox, 0.05f, 8, 1.5f, Color{55, 95, 155, 200});

            const float viewX = 44.0f;
            const float viewY = 156.0f;
            const float viewW = 914.0f;
            const float viewH = 352.0f;
            const float textMaxW = 882.0f;

            // Conteúdo textual multilíngue da NewPortGame
            std::string npgText1 = (cfg.lang == 0)
                ? "Pesquisa de Boot EE/IOP e Cadeia Causal: Mapeamento estático e dinâmico da inicialização de executáveis ELF, "
                  "sincronização entre threads e semáforos (WaitSema / SignalSema), protocolo de comunicação SIF RPC (SifBindRpc, "
                  "SifCallRpc e flag CMDINIT no registrador SMFLAG), scanner de syscalls MIPS R5900 e auditoria de módulos IOPRP/IRX."
                : "EE/IOP Boot & Causal Chain Research: Static and dynamic mapping of ELF boot sequence, thread synchronization "
                  "and semaphores (WaitSema / SignalSema), SIF RPC communication protocol (SifBindRpc, SifCallRpc and CMDINIT flag in "
                  "SMFLAG register), MIPS R5900 syscall scanner and IOPRP/IRX modules audit.";

            std::string npgText2 = (cfg.lang == 0)
                ? "Motor e Plataforma NativeSX2: Unificação multiplataforma (Windows e Linux), criação do PresentCache e do modo "
                  "FastShadows no backend GS OpenGL (eliminando gargalos de glTextureBarrier e glReadPixels), correção de fluidez em "
                  "tempo real do interpretador VU1, extrator nativo ISO9660 em C++, abertura direta de .ISO com 2 cliques, mapeamento "
                  "dinâmico de Controle/Teclado e gerenciador de Memory Card Virtual."
                : "NativeSX2 Engine & Platform: Cross-platform architecture (Windows and Linux), PresentCache and FastShadows in "
                  "GS OpenGL backend (removing glTextureBarrier and glReadPixels bottlenecks), real-time VU1 interpreter performance fix, "
                  "pure C++ ISO9660 native extractor, direct 2-click .ISO launch, dynamic Gamepad/Keyboard remapping and Virtual Memory Card manager.";

            float npgH1 = measureWrappedTextHeight(npgText1, textMaxW, 15.0f, 19.5f, false);
            float npgH2 = measureWrappedTextHeight(npgText2, textMaxW, 15.0f, 19.5f, false);
            float card1H = 12.0f + 24.0f + 22.0f + npgH1 + 10.0f + npgH2 + 14.0f + 26.0f + 16.0f;

            struct CreditEntry
            {
                const char *authorBold;
                const char *descRegularPT;
                const char *descRegularEN;
                const char *url;
            };
            const CreditEntry entries[] = {
                {
                    "• ran-j e colaboradores — Projeto PS2Recomp",
                    "Desenvolvimento do recompilador estático MIPS R5900 para C++ e arquitetura base do runtime de execução (Licença GPL-3.0).",
                    "Static MIPS R5900 to C++ recompiler and foundational runtime execution architecture (GPL-3.0 License).",
                    "https://github.com/ran-j/PS2Recomp"
                },
                {
                    "• Waffle565 e colaboradores — God-Of-War-Native-Port & GoW Recomp",
                    "Desenvolvimento do runtime nativo para God of War (sintetizador gráfico GS, VU1, IOP, áudio 989snd, DMA e leitor de disco), overrides e ferramentas.",
                    "Native runtime development (GS graphics, VU1, IOP, 989snd audio, DMA and disc reader), overrides and pipeline tools.",
                    "https://github.com/Waffle565/God-Of-War-Native-Port"
                },
                {
                    "• Vnnc1 e colaboradores — gow-native-pc & Stack de Otimizações",
                    "Pesquisa de calibração para 60 FPS, benchmarks em hardware heterogêneo, patch stack (VU1 fast mode, áudio exponencial SPU2, pacotes de vídeo) e perfis.",
                    "60 FPS calibration research, heterogeneous hardware benchmarks, patch stack (VU1 fast mode, SPU2 exponential audio, video packets) and profiling.",
                    "https://github.com/Vnnc1/gow-native-pc"
                },
                {
                    "• Equipe e Autores do PCSX2 (pcsx2.net)",
                    "Referência do sintetizador gráfico GS original, interpretador do Emotion Engine (ee_ref) e operações das unidades vetoriais VU (vu_pcsx2) sob licença GPL-3.0+.",
                    "Reference implementation of original GS, Emotion Engine interpreter (ee_ref) and VU vector unit operations (vu_pcsx2, GPL-3.0+).",
                    "https://github.com/PCSX2/pcsx2"
                },
                {
                    "• Taylor N. Albarnaz (LightVelox) — Fork do Runtime de Shadow of the Colossus",
                    "Backend gráfico GS em OpenGL, fila de comandos e correções de precisão do processador EE (FPU, desvios de 64 bits, instruções LQ/SQ, VU0 macro e cadeias DMA).",
                    "OpenGL GS graphics backend, command queue, and EE processor precision fixes (FPU, 64-bit branches, LQ/SQ, VU0 macro and DMA chains).",
                    "https://github.com/LightVelox/PS2Recomp"
                },
                {
                    "• thiagordsfx — Projeto Urban Reign Recomp",
                    "Referência compartilhada de integração do sintetizador gráfico GS e convenção de sincronismo vertical de campo (VSync).",
                    "Shared reference for GS graphics integration and vertical blank field synchronization (VSync).",
                    "https://github.com/thiagordsfx/urban-reign-recomp"
                },
                {
                    "• nemesis2000, CRASHARKI, Ramon Santamaria (Raylib), Omar Cornut (ImGui) e libmpeg2",
                    "Valores do patch de câmera Widescreen 16:9 ampliado e bibliotecas de janela, controles, interface diagnóstica e decodificação nativa de vídeo MPEG-2.",
                    "Widescreen 16:9 camera patch values, and native window, controls, diagnostic UI and MPEG-2 video decoding libraries.",
                    "https://www.raylib.com"
                }
            };

            // Texto do Aviso Legal Genérico
            std::string legalText = (cfg.lang == 0)
                ? "Este software é uma plataforma e motor nativo independente sob a licença GNU GPL-3.0. Nenhum arquivo de mídia protegido, "
                  "BIOS proprietária ou imagem de disco (.ISO) de qualquer jogo é distribuído com este programa — o usuário deve fornecer sua "
                  "própria cópia (.ISO) legítima de jogos originais. Todas as marcas registradas, títulos de jogos e direitos autorais associados "
                  "pertencem aos seus respectivos proprietários. Este projeto independente não possui afiliação, patrocínio ou endosso da Sony "
                  "Interactive Entertainment ou de qualquer desenvolvedora/publicadora."
                : "This software is an independent native engine and launcher distributed under the GNU GPL-3.0 license. No copyrighted game "
                  "media, proprietary BIOS, or disc image (.ISO) is distributed with this application — users must supply their own legitimate "
                  "original disc copy (.ISO). All trademarks, console names, and game titles belong to their respective copyright holders. This "
                  "independent project is not affiliated with, sponsored, or endorsed by Sony Interactive Entertainment or any game publisher.";

            float legalH = measureWrappedTextHeight(legalText, textMaxW, 14.5f, 18.5f, false);
            float legalCardH = 10.0f + 22.0f + legalH + 14.0f;

            // Calcula a altura total dinâmica para o scrollbar perfeito
            float totalEntriesH = 0.0f;
            std::vector<float> entryHeights;
            for (const auto &e : entries)
            {
                const char *desc = (cfg.lang == 0) ? e.descRegularPT : e.descRegularEN;
                float dH = measureWrappedTextHeight(desc, textMaxW, 14.5f, 18.5f, false);
                float eh = 8.0f + 20.0f + dH + 6.0f + 18.0f + 8.0f;
                entryHeights.push_back(eh);
                totalEntriesH += eh + 10.0f;
            }

            float totalContentH = card1H + 24.0f + 28.0f + totalEntriesH + 16.0f + legalCardH + 24.0f;
            float maxScroll = std::max(0.0f, totalContentH - viewH);

            // Controle da rolagem (Roda do mouse, setas ou arrastando a barra lateral)
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f)
                creditsScrollY -= wheel * 42.0f;
            if (IsKeyDown(KEY_DOWN))
                creditsScrollY += 6.0f;
            if (IsKeyDown(KEY_UP))
                creditsScrollY -= 6.0f;

            Rectangle trackRec = {972.0f, viewY, 10.0f, viewH};
            float thumbH = std::max(40.0f, (viewH / totalContentH) * viewH);
            float thumbY = viewY + (maxScroll > 0.0f ? (creditsScrollY / maxScroll) * (viewH - thumbH) : 0.0f);
            Rectangle thumbRec = {972.0f, thumbY, 10.0f, thumbH};

            Vector2 mousePos = GetMousePosition();
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, Rectangle{966.0f, viewY, 22.0f, viewH}))
                draggingScrollbar = true;
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
                draggingScrollbar = false;
            if (draggingScrollbar && maxScroll > 0.0f)
            {
                float ratio = (mousePos.y - viewY - thumbH * 0.5f) / (viewH - thumbH);
                ratio = std::clamp(ratio, 0.0f, 1.0f);
                creditsScrollY = ratio * maxScroll;
            }
            creditsScrollY = std::clamp(creditsScrollY, 0.0f, maxScroll);

            BeginScissorMode(static_cast<int>(viewX - 6), static_cast<int>(viewY), static_cast<int>(viewW + 12), static_cast<int>(viewH));

            float cy = viewY + 6.0f - creditsScrollY;

            // Bloco 1: Engenharia e Pesquisa NewPortGame (NativeSX2) com link clicável oficial
            Rectangle card1Rec = {viewX, cy, viewW, card1H};
            bool card1Hover = CheckCollisionPointRec(mousePos, card1Rec);
            DrawRectangleRounded(card1Rec, 0.06f, 8, card1Hover ? Color{26, 52, 88, 255} : Color{22, 46, 78, 255});
            DrawRectangleRoundedLinesEx(card1Rec, 0.06f, 8, card1Hover ? 1.8f : 1.4f, card1Hover ? Color{115, 235, 180, 255} : Color{75, 165, 225, 220});
            cy += 12.0f;
            drawSmoothText(cfg.lang == 0 ? "★ ENGENHARIA E PESQUISA NEWPORTGAME (NativeSX2)" : "★ NEWPORTGAME RESEARCH & ENGINEERING (NativeSX2)",
                           viewX + 14.0f, cy, 18.0f, Color{125, 240, 185, 255}, true);
            cy += 24.0f;

            drawSmoothText(cfg.lang == 0 ? "• Cristiano Mariano (NewPortGame) — Fundador, Pesquisa e Desenvolvimento Principal"
                                         : "• Cristiano Mariano (NewPortGame) — Founder, Lead Research & Development",
                           viewX + 14.0f, cy, 16.5f, WHITE, true);
            cy += 22.0f;

            cy = drawWrappedText(npgText1, viewX + 14.0f, cy, textMaxW, 15.0f, 19.5f, Color{215, 228, 245, 255}, false);
            cy += 10.0f;

            cy = drawWrappedText(npgText2, viewX + 14.0f, cy, textMaxW, 15.0f, 19.5f, Color{215, 228, 245, 255}, false);
            cy += 14.0f;

            const char *npgUrl = "https://github.com/contatonewportgame-code/nativesx2";
            std::string npgLinkLabel = (cfg.lang == 0)
                ? "Repositório Oficial & Pesquisas: https://github.com/contatonewportgame-code/nativesx2 (Clique no card para abrir)"
                : "Official Repository & Research: https://github.com/contatonewportgame-code/nativesx2 (Click card to open)";
            drawSmoothText(npgLinkLabel.c_str(), viewX + 14.0f, cy, 14.0f, card1Hover ? Color{130, 255, 195, 255} : Color{105, 215, 165, 255}, true);

            if (card1Hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                OpenURL(npgUrl);
                statusMsg = (cfg.lang == 0 ? "Abrindo repositório oficial NewPortGame no navegador..." : "Opening official NewPortGame repository in browser...");
                statusCol = {100, 255, 160, 255};
            }
            cy = card1Rec.y + card1H + 20.0f;

            // Título da seção de Colaboradores da Comunidade
            drawSmoothText(cfg.lang == 0 ? "COLABORADORES E PROJETOS DA COMUNIDADE OPEN-SOURCE"
                                         : "OPEN-SOURCE COMMUNITY CONTRIBUTORS & PROJECTS",
                           viewX + 4.0f, cy, 18.0f, Color{115, 200, 255, 255}, true);
            cy += 26.0f;

            for (size_t i = 0; i < std::size(entries); ++i)
            {
                float eh = entryHeights[i];
                float cardStart = cy;
                Rectangle cardRec = {viewX, cardStart, viewW, eh};
                bool cardHover = CheckCollisionPointRec(mousePos, cardRec);
                DrawRectangleRounded(cardRec, 0.08f, 6, cardHover ? Color{24, 40, 72, 255} : Color{16, 26, 48, 255});
                if (cardHover)
                    DrawRectangleRoundedLinesEx(cardRec, 0.08f, 6, 1.2f, Color{85, 175, 255, 220});

                cy += 8.0f;
                drawSmoothText(entries[i].authorBold, viewX + 14.0f, cy, 16.0f, WHITE, true);
                cy += 20.0f;
                const char *desc = (cfg.lang == 0) ? entries[i].descRegularPT : entries[i].descRegularEN;
                cy = drawWrappedText(desc, viewX + 14.0f, cy, textMaxW, 14.5f, 18.5f, Color{205, 220, 240, 255}, false);
                cy += 4.0f;

                std::string linkLabel = std::string("Link: ") + entries[i].url + (cfg.lang == 0 ? " (Clique no card para abrir)" : " (Click card to open)");
                drawSmoothText(linkLabel.c_str(), viewX + 14.0f, cy, 13.5f, cardHover ? Color{135, 235, 255, 255} : Color{100, 180, 240, 255}, false);

                if (cardHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    OpenURL(entries[i].url);
                    statusMsg = (cfg.lang == 0 ? "Abrindo navegador: " : "Opening browser: ") + std::string(entries[i].url);
                    statusCol = {100, 255, 160, 255};
                }

                cy = cardStart + eh + 10.0f;
            }
            cy += 6.0f;

            // Bloco Final: Aviso Legal Genérico
            DrawRectangleRounded(Rectangle{viewX, cy, viewW, legalCardH}, 0.08f, 8, Color{15, 24, 44, 255});
            DrawRectangleRoundedLinesEx(Rectangle{viewX, cy, viewW, legalCardH}, 0.08f, 8, 1.2f, Color{140, 120, 65, 180});
            cy += 10.0f;
            drawSmoothText(cfg.lang == 0 ? "AVISO LEGAL E DIREITOS AUTORAIS (LICENÇA GNU GPL-3.0)"
                                         : "LEGAL DISCLAIMER & COPYRIGHT NOTICE (GNU GPL-3.0 LICENSE)",
                           viewX + 14.0f, cy, 16.0f, Color{255, 215, 115, 255}, true);
            cy += 22.0f;
            drawWrappedText(legalText, viewX + 14.0f, cy, textMaxW, 14.5f, 18.5f, Color{210, 220, 235, 255}, false);

            EndScissorMode();

            // Desenha a barra de rolagem lateral (Scrollbar)
            DrawRectangleRounded(trackRec, 0.5f, 6, Color{12, 18, 34, 255});
            thumbRec.y = viewY + (maxScroll > 0.0f ? (creditsScrollY / maxScroll) * (viewH - thumbH) : 0.0f);
            bool thumbHot = CheckCollisionPointRec(mousePos, thumbRec) || draggingScrollbar;
            DrawRectangleRounded(thumbRec, 0.5f, 6, thumbHot ? Color{110, 195, 255, 255} : Color{65, 130, 205, 255});

            if (drawButton(Rectangle{28, 526, 468, 54},
                           cfg.lang == 0 ? "Abrir LEIAME_E_CREDITOS.txt" : "Open README_AND_CREDITS.txt",
                           Color{35, 95, 165, 255}, Color{50, 130, 215, 255}, WHITE, 18))
            {
                fs::path readmeFile = appDir / "LEIAME_E_CREDITOS.txt";
                if (!fs::exists(readmeFile))
                    readmeFile = appDir / "LEIAME_WINDOWS_E_LINUX.txt";
#ifdef _WIN32
                std::string cmd = "notepad \"" + readmeFile.string() + "\"";
#else
                std::string cmd = "xdg-open \"" + readmeFile.string() + "\" >/dev/null 2>&1 &";
#endif
                std::system(cmd.c_str());
                statusMsg = (cfg.lang == 0) ? "Arquivo LEIAME_E_CREDITOS.txt aberto!" : "README_AND_CREDITS.txt opened!";
            }

            if (drawButton(Rectangle{516, 526, 476, 54},
                           cfg.lang == 0 ? "Abrir Licença Completa (GPL-3.0)" : "Open Full License (GPL-3.0)",
                           Color{35, 78, 135, 255}, Color{55, 110, 180, 255}, WHITE, 18))
            {
                fs::path licFile = appDir / "gow_port" / "LICENSE";
                if (!fs::exists(licFile))
                    licFile = appDir / "LICENSE";
#ifdef _WIN32
                std::string cmd = "notepad \"" + licFile.string() + "\"";
#else
                std::string cmd = "xdg-open \"" + licFile.string() + "\" >/dev/null 2>&1 &";
#endif
                std::system(cmd.c_str());
                statusMsg = (cfg.lang == 0) ? "Licença GPL-3.0 aberta!" : "GPL-3.0 License opened!";
            }
        }

        // Barra Inferior Fixa + Botão JOGAR AGORA
        DrawRectangle(0, 600, 1020, 80, Color{10, 15, 28, 255});
        DrawLine(0, 600, 1020, 600, Color{50, 105, 180, 255});
        drawSmoothText(statusMsg.c_str(), 28, 628, 18, statusCol);

        if (drawButton(Rectangle{740, 610, 252, 58}, cfg.lang == 0 ? "JOGAR AGORA" : "PLAY NOW",
                       Color{25, 160, 88, 255}, Color{40, 200, 112, 255}, WHITE, 24))
        {
            saveConfig(cfgFile, cfg);
            std::string errMsg;
            if (g_hasCustomFont)
                UnloadFont(g_font);
            CloseWindow();
            if (!launchGame(cfg, appDir, errMsg))
            {
                InitWindow(1020, 680, "NativeSX2 (by NewPortGame) - Central de Jogos Nativos PS2");
                SetTargetFPS(60);
                loadSmoothFont(appDir);
                statusMsg = errMsg;
                statusCol = {255, 120, 120, 255};
            }
        }

        EndDrawing();
    }

    saveConfig(cfgFile, cfg);
    if (g_hasCustomFont)
        UnloadFont(g_font);
    CloseWindow();
    return 0;
}
