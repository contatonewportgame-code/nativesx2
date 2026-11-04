#include <cstdlib>
#include "runtime/ps2_pad.h"
#include "ps2_host_backend.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#ifdef _WIN32
bool ps2PadWindowsRumble(uint8_t large, bool small); // ps2_pad_rumble_win.cpp
#else
extern "C" {
    const char* glfwGetJoystickGUID(int jid);
    const char* glfwGetJoystickName(int jid);
    int glfwJoystickPresent(int jid);
    int glfwJoystickIsGamepad(int jid);
    const float* glfwGetJoystickAxes(int jid, int* count);
    const unsigned char* glfwGetJoystickButtons(int jid, int* count);
    const unsigned char* glfwGetJoystickHats(int jid, int* count);
}

static void autoConfigureGamepadsLinux()
{
    static int s_checkCounter = 0;
    if (++s_checkCounter > 1 && (s_checkCounter % 120 != 0))
        return;

    const char* envMapping = std::getenv("NATIVESX2_CONTROLLER_MAPPING");
    if (envMapping && *envMapping)
        SetGamepadMappings(envMapping);
    const char* envSdl = std::getenv("SDL_GAMECONTROLLERCONFIG");
    if (envSdl && *envSdl)
        SetGamepadMappings(envSdl);

    static bool s_dbLoaded = false;
    if (!s_dbLoaded)
    {
        const char *dbCandidates[] = {
            "gamecontrollerdb.txt",
            "/opt/nativesx2/gamecontrollerdb.txt",
            "/home/cristiano/.local/share/NativeSX2/gamecontrollerdb.txt",
            "/home/cristiano/newportgame4/gamecontrollerdb.txt",
            nullptr
        };
        for (int i = 0; dbCandidates[i]; ++i)
        {
            std::FILE *f = std::fopen(dbCandidates[i], "rb");
            if (f)
            {
                std::fseek(f, 0, SEEK_END);
                long sz = std::ftell(f);
                std::fseek(f, 0, SEEK_SET);
                if (sz > 0 && sz < 5 * 1024 * 1024)
                {
                    char *buf = new char[sz + 1];
                    std::size_t readBytes = std::fread(buf, 1, sz, f);
                    buf[readBytes] = '\0';
                    SetGamepadMappings(buf);
                    delete[] buf;
                    s_dbLoaded = true;
                }
                std::fclose(f);
                if (s_dbLoaded) break;
            }
        }
    }

    std::string newMappings;
    for (int jid = 0; jid <= 15; ++jid)
    {
        if (!glfwJoystickPresent(jid)) continue;
        if (glfwJoystickIsGamepad(jid)) continue;

        const char *cguid = glfwGetJoystickGUID(jid);
        const char *cname = glfwGetJoystickName(jid);
        if (!cguid || !cname) continue;
        std::string guid(cguid);
        std::string name(cname);

        int axisCount = 0;
        glfwGetJoystickAxes(jid, &axisCount);
        int btnCount = 0;
        glfwGetJoystickButtons(jid, &btnCount);
        int hatCount = 0;
        glfwGetJoystickHats(jid, &hatCount);

        std::fprintf(stderr, "[ps2_pad] Auto-configurando joystick %d: '%s' (GUID: %s, Eixos: %d, Botões: %d, Hats: %d)\n",
                     jid, name.c_str(), guid.c_str(), axisCount, btnCount, hatCount);

        bool matchedVendorProduct = false;
        if (guid.length() >= 24)
        {
            std::string venProd = guid.substr(8, 16);
            const char *dbCandidates[] = {
                "gamecontrollerdb.txt",
                "/opt/nativesx2/gamecontrollerdb.txt",
                "/home/cristiano/.local/share/NativeSX2/gamecontrollerdb.txt",
                "/home/cristiano/newportgame4/gamecontrollerdb.txt",
                nullptr
            };
            for (int d = 0; dbCandidates[d] && !matchedVendorProduct; ++d)
            {
                std::ifstream f(dbCandidates[d]);
                if (!f) continue;
                std::string line;
                while (std::getline(f, line))
                {
                    if (line.empty() || line[0] == '#') continue;
                    if (line.find("platform:Linux") == std::string::npos) continue;
                    if (line.length() >= 24 && line.substr(8, 16) == venProd)
                    {
                        size_t firstComma = line.find(',');
                        if (firstComma != std::string::npos)
                        {
                            std::string clone = guid + "," + name + line.substr(line.find(',', firstComma + 1));
                            newMappings += clone + "\n";
                            matchedVendorProduct = true;
                            break;
                        }
                    }
                }
            }
        }

        if (!matchedVendorProduct)
        {
            std::string m = guid + "," + name + ",a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,guide:b8,leftstick:b9,rightstick:b10,";
            if (hatCount > 0)
                m += "dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,";
            else if (btnCount >= 15)
                m += "dpup:b11,dpright:b12,dpdown:b13,dpleft:b14,";
            if (axisCount >= 6)
                m += "leftx:a0,lefty:a1,lefttrigger:a2,rightx:a3,righty:a4,righttrigger:a5,";
            else if (axisCount >= 4)
                m += "leftx:a0,lefty:a1,rightx:a2,righty:a3,";
            m += "platform:Linux,\n";
            newMappings += m;
        }
    }

    if (!newMappings.empty())
    {
        SetGamepadMappings(newMappings.c_str());
    }
    PollInputEvents();
}
#endif

namespace
{
    constexpr uint8_t kPadAnalogMarker = 0x73;
    constexpr uint8_t kPadStickCenter = 0x80;

    constexpr uint16_t PAD_LEFT = 0x0080u;
    constexpr uint16_t PAD_DOWN = 0x0040u;
    constexpr uint16_t PAD_RIGHT = 0x0020u;
    constexpr uint16_t PAD_UP = 0x0010u;
    constexpr uint16_t PAD_START = 0x0008u;
    constexpr uint16_t PAD_R3 = 0x0004u;
    constexpr uint16_t PAD_L3 = 0x0002u;
    constexpr uint16_t PAD_SELECT = 0x0001u;
    constexpr uint16_t PAD_SQUARE = 0x8000u;
    constexpr uint16_t PAD_CROSS = 0x4000u;
    constexpr uint16_t PAD_CIRCLE = 0x2000u;
    constexpr uint16_t PAD_TRIANGLE = 0x1000u;
    constexpr uint16_t PAD_R1 = 0x0800u;
    constexpr uint16_t PAD_L1 = 0x0400u;
    constexpr uint16_t PAD_R2 = 0x0200u;
    constexpr uint16_t PAD_L2 = 0x0100u;
}

bool PSPadBackend::readState(int /*port*/, int /*slot*/, uint8_t *data, size_t size)
{
    if (!data || size < 32)
        return false;

#ifndef _WIN32
    autoConfigureGamepadsLinux();
#endif

    std::memset(data, 0, 32);
    data[0] = 0x01;
    data[1] = kPadAnalogMarker;
    data[2] = 0xFF;
    data[3] = 0xFF;
    data[4] = data[5] = data[6] = data[7] = kPadStickCenter;

    uint16_t btns = 0xFFFFu;
    // GOW-Port: GLFW can list racing gear (handbrake, pedals) as gamepads before the real pad. The pad whose
    // button was pressed last becomes the active one (press edges, so idle devices never take over);
    // until then, the first available slot.
    static int s_activeGamepad = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (i == s_activeGamepad || !IsGamepadAvailable(i))
            continue;
        for (int button = GAMEPAD_BUTTON_LEFT_FACE_UP; button <= GAMEPAD_BUTTON_RIGHT_THUMB; ++button)
            if (IsGamepadButtonPressed(i, button))
            {
                s_activeGamepad = i;
                break;
            }
    }
    if (s_activeGamepad >= 0 && !IsGamepadAvailable(s_activeGamepad))
        s_activeGamepad = -1;
    int kGamepad = s_activeGamepad;
    for (int i = 0; i < 4 && kGamepad < 0; ++i)
        if (IsGamepadAvailable(i))
            kGamepad = i;
    // GOW-Port: with GOW_PAD_TEST=1 the test script alone drives the game; a gamepad switched on or touched while the
    // automated tests run (the player using the PC meanwhile) changed the title menu's state and broke them.
    static const bool scripted = [] { const char *v = std::getenv("GOW_PAD_TEST"); return v && v[0] == '1' && v[1] == 0; }();
    if (scripted)
        kGamepad = -1;
    const bool useGamepad = kGamepad >= 0;
    static int s_loggedGamepad = -2;
    if (kGamepad != s_loggedGamepad)
    {
        s_loggedGamepad = kGamepad;
        std::fprintf(stderr, "[pad] gamepad slot %d: %s\n", kGamepad, useGamepad ? GetGamepadName(kGamepad) : "none (keyboard)");
    }
    auto clearBit = [&btns](uint16_t mask)
    { btns &= ~mask; };

    auto envInt = [](const char *name, int defVal) -> int {
        const char *v = std::getenv(name);
        return (v && *v) ? std::atoi(v) : defVal;
    };

    if (useGamepad)
    {
        static const int bUp       = envInt("NATIVESX2_BTN_UP",       GAMEPAD_BUTTON_LEFT_FACE_UP);
        static const int bDown     = envInt("NATIVESX2_BTN_DOWN",     GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        static const int bLeft     = envInt("NATIVESX2_BTN_LEFT",     GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        static const int bRight    = envInt("NATIVESX2_BTN_RIGHT",    GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        static const int bCross    = envInt("NATIVESX2_BTN_CROSS",    GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        static const int bCircle   = envInt("NATIVESX2_BTN_CIRCLE",   GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        static const int bSquare   = envInt("NATIVESX2_BTN_SQUARE",   GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        static const int bTriangle = envInt("NATIVESX2_BTN_TRIANGLE", GAMEPAD_BUTTON_RIGHT_FACE_UP);
        static const int bL1       = envInt("NATIVESX2_BTN_L1",       GAMEPAD_BUTTON_LEFT_TRIGGER_1);
        static const int bR1       = envInt("NATIVESX2_BTN_R1",       GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
        static const int bL2       = envInt("NATIVESX2_BTN_L2",       GAMEPAD_BUTTON_LEFT_TRIGGER_2);
        static const int bR2       = envInt("NATIVESX2_BTN_R2",       GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
        static const int bStart    = envInt("NATIVESX2_BTN_START",    GAMEPAD_BUTTON_MIDDLE_RIGHT);
        static const int bSelect   = envInt("NATIVESX2_BTN_SELECT",   GAMEPAD_BUTTON_MIDDLE_LEFT);
        static const int bL3       = envInt("NATIVESX2_BTN_L3",       GAMEPAD_BUTTON_LEFT_THUMB);
        static const int bR3       = envInt("NATIVESX2_BTN_R3",       GAMEPAD_BUTTON_RIGHT_THUMB);

        if (IsGamepadButtonDown(kGamepad, bUp))       clearBit(PAD_UP);
        if (IsGamepadButtonDown(kGamepad, bDown))     clearBit(PAD_DOWN);
        if (IsGamepadButtonDown(kGamepad, bLeft))     clearBit(PAD_LEFT);
        if (IsGamepadButtonDown(kGamepad, bRight))    clearBit(PAD_RIGHT);
        if (IsGamepadButtonDown(kGamepad, bCross))    clearBit(PAD_CROSS);
        if (IsGamepadButtonDown(kGamepad, bCircle))   clearBit(PAD_CIRCLE);
        if (IsGamepadButtonDown(kGamepad, bSquare))   clearBit(PAD_SQUARE);
        if (IsGamepadButtonDown(kGamepad, bTriangle)) clearBit(PAD_TRIANGLE);
        if (IsGamepadButtonDown(kGamepad, bL1))       clearBit(PAD_L1);
        if (IsGamepadButtonDown(kGamepad, bR1))       clearBit(PAD_R1);
        if (IsGamepadButtonDown(kGamepad, bL2))       clearBit(PAD_L2);
        if (IsGamepadButtonDown(kGamepad, bR2))       clearBit(PAD_R2);
        if (IsGamepadButtonDown(kGamepad, bStart))    clearBit(PAD_START);
        if (IsGamepadButtonDown(kGamepad, bSelect))   clearBit(PAD_SELECT);
        if (IsGamepadButtonDown(kGamepad, bL3))       clearBit(PAD_L3);
        if (IsGamepadButtonDown(kGamepad, bR3))       clearBit(PAD_R3);

        float lx = GetGamepadAxisMovement(kGamepad, GAMEPAD_AXIS_LEFT_X);
        float ly = GetGamepadAxisMovement(kGamepad, GAMEPAD_AXIS_LEFT_Y);
        float rx = GetGamepadAxisMovement(kGamepad, GAMEPAD_AXIS_RIGHT_X);
        float ry = GetGamepadAxisMovement(kGamepad, GAMEPAD_AXIS_RIGHT_Y);
        data[6] = static_cast<uint8_t>(128 + lx * 127);
        data[7] = static_cast<uint8_t>(128 + ly * 127);
        data[4] = static_cast<uint8_t>(128 + rx * 127);
        data[5] = static_cast<uint8_t>(128 + ry * 127);
    }

    // Leitura simultânea do teclado (sempre disponível para ambos funcionarem perfeitamente juntos)
    static const int kUp       = envInt("NATIVESX2_KEY_UP",       KEY_UP);
    static const int kDown     = envInt("NATIVESX2_KEY_DOWN",     KEY_DOWN);
    static const int kLeft     = envInt("NATIVESX2_KEY_LEFT",     KEY_LEFT);
    static const int kRight    = envInt("NATIVESX2_KEY_RIGHT",    KEY_RIGHT);
    static const int kCross    = envInt("NATIVESX2_KEY_CROSS",    KEY_X);
    static const int kCircle   = envInt("NATIVESX2_KEY_CIRCLE",   KEY_C);
    static const int kSquare   = envInt("NATIVESX2_KEY_SQUARE",   KEY_Z);
    static const int kTriangle = envInt("NATIVESX2_KEY_TRIANGLE", KEY_V);
    static const int kL1       = envInt("NATIVESX2_KEY_L1",       KEY_Q);
    static const int kR1       = envInt("NATIVESX2_KEY_R1",       KEY_E);
    static const int kL2       = envInt("NATIVESX2_KEY_L2",       KEY_LEFT_SHIFT);
    static const int kR2       = envInt("NATIVESX2_KEY_R2",       KEY_RIGHT_SHIFT);
    static const int kStart    = envInt("NATIVESX2_KEY_START",    KEY_ENTER);
    static const int kSelect   = envInt("NATIVESX2_KEY_SELECT",   KEY_TAB);
    static const int kLxNeg    = envInt("NATIVESX2_KEY_LX_NEG",   KEY_A);
    static const int kLxPos    = envInt("NATIVESX2_KEY_LX_POS",   KEY_D);
    static const int kLyNeg    = envInt("NATIVESX2_KEY_LY_NEG",   KEY_W);
    static const int kLyPos    = envInt("NATIVESX2_KEY_LY_POS",   KEY_S);
    static const int kRxNeg    = envInt("NATIVESX2_KEY_RX_NEG",   KEY_J);
    static const int kRxPos    = envInt("NATIVESX2_KEY_RX_POS",   KEY_L);
    static const int kRyNeg    = envInt("NATIVESX2_KEY_RY_NEG",   KEY_I);
    static const int kRyPos    = envInt("NATIVESX2_KEY_RY_POS",   KEY_K);

    if (IsKeyDown(kUp))       clearBit(PAD_UP);
    if (IsKeyDown(kDown))     clearBit(PAD_DOWN);
    if (IsKeyDown(kLeft))     clearBit(PAD_LEFT);
    if (IsKeyDown(kRight))    clearBit(PAD_RIGHT);
    if (IsKeyDown(kCross)    || (kCross == KEY_X && IsKeyDown(KEY_SPACE)))     clearBit(PAD_CROSS);
    if (IsKeyDown(kCircle)   || (kCircle == KEY_C && IsKeyDown(KEY_ESCAPE)))   clearBit(PAD_CIRCLE);
    if (IsKeyDown(kSquare)   || (kSquare == KEY_Z && IsKeyDown(KEY_KP_0)))     clearBit(PAD_SQUARE);
    if (IsKeyDown(kTriangle) || (kTriangle == KEY_V && IsKeyDown(KEY_KP_1)))   clearBit(PAD_TRIANGLE);
    if (IsKeyDown(kL1))       clearBit(PAD_L1);
    if (IsKeyDown(kR1))       clearBit(PAD_R1);
    if (IsKeyDown(kL2))       clearBit(PAD_L2);
    if (IsKeyDown(kR2))       clearBit(PAD_R2);
    if (IsKeyDown(kStart))    clearBit(PAD_START);
    if (IsKeyDown(kSelect))   clearBit(PAD_SELECT);

    if (IsKeyDown(kLxNeg)) data[6] = 0;
    else if (IsKeyDown(kLxPos)) data[6] = 255;
    if (IsKeyDown(kLyNeg)) data[7] = 0;
    else if (IsKeyDown(kLyPos)) data[7] = 255;
    if (IsKeyDown(kRxNeg)) data[4] = 0;
    else if (IsKeyDown(kRxPos)) data[4] = 255;
    if (IsKeyDown(kRyNeg)) data[5] = 0;
    else if (IsKeyDown(kRyPos)) data[5] = 255;

    data[2] = static_cast<uint8_t>(btns & 0xFF);
    data[3] = static_cast<uint8_t>(btns >> 8);

    if (kGamepad != m_activeGamepad)
    {
        m_activeGamepad = kGamepad;
        applyRumble(false); // the new gamepad takes over the current actuator state
    }
    else
        applyRumble(true);
    return true;
}

void PSPadBackend::setActuators(int port, bool small, uint8_t large)
{
    if (port != 0 || (small == m_small && large == m_large))
        return;
    m_small = small;
    m_large = large;
    applyRumble(false);
}

// refresh = periodic call from readState: only backends whose rumble times out need it resent.
void PSPadBackend::applyRumble(bool refresh)
{
#ifdef _WIN32
    // raylib's GLFW backend has no vibration: XInput and Sony HID pads directly. Both keep the motors running until changed.
    if (refresh)
        return;
    ps2PadWindowsRumble(m_large, m_small);
#else
    // Other platforms (SDL, Switch): raylib's rumble, which stops after at most 2 s, so it is resent every second.
    if (m_activeGamepad < 0 || !IsWindowReady())
        return;
    const bool on = m_small || m_large;
    const double now = GetTime();
    if (refresh && (!on || now - m_rumbleSentAt < 1.0))
        return;
    m_rumbleSentAt = now;
    SetGamepadVibration(m_activeGamepad, m_large / 255.0f, m_small ? 1.0f : 0.0f, on ? 2.0f : 0.1f); // zero motors with any duration stop it
#endif
}
