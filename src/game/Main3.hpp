// Main3.hpp - 3.ASM: the main loop, the flight model, the player on foot, crashes, ejecting
//
// frame() is one pass of the original's `anml1` loop (3.ASM, from "inc nframes" to the jump back),
// split into the same stages in the same order. Procs of 3.ASM are methods of the same name.
// All state lives in the DGROUP image (GameState), so the tick-timed math sees exactly the data
// the ASM saw; register temporaries became locals.
//
// What the loop calls but other phases own goes through small hooks that do nothing yet, each
// marked with the phase that fills it in:
//   instruments   INST.ASM   drawbutton, drawndles, flashdamage ...           (Phase 4)
//   weapons       STRTEXP    bulstrt, strtmis, strtbomb, bulmov, expmov        (Phase 5)
//   sound         ADL.ASM    voiceon, adlwrt ...                               (Phase 7)
//   front end     TOWER.EXE  briefings, the end-of-mission pictures            (Phase 8)
//
// Original quirks kept on purpose: commands fire on key release (scncode with bit 7), controls are
// integrated over the time since the last read (scalekey), JS/JNS after CMP/SUB test the sign of
// the 16-bit difference, and dword pairs (pitdotl/pitdoth ...) are 32-bit values split in two words.

#pragma once
#include "audio/Sound.hpp"
#include "core/SasLib.hpp"
#include "engine/F3dVec.hpp"
#include "game/GameState.hpp"
#include "game/Inst.hpp"
#include "game/Keyboard.hpp"
#include "game/TickClock.hpp"
#include "game/World.hpp"
#include <cstdint>
#include <functional>

namespace corncob::game {

using engine::i16;
using engine::i32;
using engine::u16;
using engine::u32;
using engine::u8;

class Main3 : public engine::GameHooks {
public:
    Main3(GameState& gs, engine::F3dVec& eng, World& world, DosKeyboard& kbd, TickClock& clock, Instruments& inst);

    // The set-up 3.ASM does before the loop starts, with the theater already loaded
    void startUp(bool readFile);

    // One pass of the main loop. Returns false once the mission is over.
    bool frame();

    bool done() const { return m_done; }

    // An analog controller, each in [-1, 1]: roll and pitch (down = nose up) take the place of
    // the joystick the original read, rudder is added to the rudder keys. Used only while no
    // arrow key is down, as in the original's joytd.
    void setPad(bool active, float roll, float pitch, float rudder) {
        m_padActive = active;
        m_padRoll = roll;
        m_padPitch = pitch;
        m_padRudder = rudder;
    }
    bool paused() const { return m_paused; }

    // Told when the window layout changes: true = the cockpit with the instrument panel,
    // false = the full screen view (so the panel can be drawn or cleared)
    void onViewChange(std::function<void(bool)> f) { m_viewChanged = std::move(f); }
    // Told to show a full-screen picture (help screen n, 3Dn.IMG)
    void onShowImage(std::function<void(int)> f) { m_showImage = std::move(f); }

    // GameHooks (called by the engine)
    void objroutine(u16 type, u16 si, u16 di, bool carry) override;
    void damageplane(u16 count) override;
    void crashSound(bool boom) override;

    // The AdLib sound; without it the game is silent
    void setSound(Sound* s) { m_snd = s; }

private:
    // ---- DGROUP access ----
    i16 W(u32 o) const { return m_g.w(o); }
    u16 UW(u32 o) const { return m_g.uw(o); }
    void SW(u32 o, i16 v) { m_g.setW(o, v); }
    u8 B(u32 o) const { return m_g.b(o); }
    void SB(u32 o, u8 v) { m_g.setB(o, v); }
    i32 D(u32 o) const { return m_g.dw(o); }
    void SD(u32 o, i32 v) { m_g.setDw(o, v); }
    // a dword kept as two separate words
    i32 dwGet(u32 lo, u32 hi) const;
    void dwSet(u32 lo, u32 hi, i32 v);
    void dwAdd(u32 lo, u32 hi, i32 v);
    void addToDword(u32 off, i32 v) { SD(off, static_cast<i32>(static_cast<u32>(D(off)) + static_cast<u32>(v))); }

    // wtadj / uwtadj / dwtadj (3DMAC.INC): scale by the frame time
    i16 wtadj(i16 v) const;
    i16 uwtadj(i16 v) const;
    i32 dwtadj(i32 v) const;

    bool key(u8 scan) const { return m_kbd.getkbit(scan) != 0; }
    u8 scncode() const { return m_kbd.scncode(); }
    void clearScn() { m_kbd.setScncode(0); }
    bool scnIs(u8 makeCode) const { return m_kbd.scncode() == static_cast<u8>(makeCode + 0x80); }   // key released

    static const u32 SCNCODE;

    // ---- the stages of the loop ----
    void timing();
    bool pauseAndHelp();
    void homeArrow();
    void viewMatrices();
    void drawWorld();
    void drawGroundDots();
    void engineThermo();
    void freezeAndAngles();
    void sticks(i16& roll, i16& pitch);
    void flightDynamics(i16 stickRoll, i16 stickPitch);
    void forcesAndVelocity();
    void groundAndCrash();
    void movement();
    void ejectedMovement();
    void everyFrame();
    void matricesAfterMove();
    void altitudeLights();
    bool highAltitude();
    void getBackIntoPlane();
    void viewKeys();
    void propAndWeapons();
    void throttleKeys();
    void miscKeys();
    void eject();
    bool exitCheck();
    bool endMission();
    bool finishMission();

    // ---- procs of 3.ASM ----
    void joytd(i16& ax, i16& bx);
    void bumpangles(i16 bx, i16 cx);
    void swing();
    void cleardamage();
    void wingdam();
    void yawdamage();
    void stabdamage();
    void engdamage();
    void getinplane(u16 si);
    void interiorview();
    void fullsize();
    void restoreview();
    void recalcmats();
    void dampchute();
    void moveme();
    void moveobs();
    void step(i16 stepsz);
    void goodstep(i16 stepsz);
    void bouncevert();
    void killss();
    void set5dbits();
    void qcrashland();
    void qhomebase();
    void brake(i16 force, u16& mag, i16& axOut);
    bool findheight();
    bool pitchlock();
    void dohlock();
    bool dolandinggear();
    void dodvelavg();
    void jumpadd();
    void crash();
    void resetplane();
    void donewmiss();
    void doxp();
    void drawtodxvec(u16 si, bool& tooFar);
    void takedrugs();
    void getang(u16 bx, u16 si);
    u16 vmag(u16 si) { return m_eng.vmag(si); }
    i32 grand(i16& hi);
    i32 drandr(i32 range);
    void grndpt(u32 point);
    void fgrndpt(u32 point);
    void sgrndpt(u32 point);

    // ---- DEFOBJS.ASM / STRTEXP.ASM routines of objects ----
    bool evaldist(u16 si, u16& dist);
    void placeplane(u16 si);
    void obj5datim(u16 si, u16 di, bool carry);
    void fdest(u16 si, u16 di, bool carry);
    void odest(u16 si, u16 di, bool carry);
    void funeral5d(bool wrecked);
    void towertim(u16 si, bool carry);
    void homedetect(u16 si);

    // ---- the instrument procs of 3.ASM (INST.ASM does the drawing) ----
    void drawbutton(u16 colors, int button) { m_inst.drawbutton(colors, button); }
    void drawndles() { m_inst.drawndles(); }
    void erasndles() { m_inst.erasndles(); }
    void flashdamage(int index) { m_inst.flashdamage(index); }
    void drawdamage(int index) { m_inst.drawdamage(index); }
    void drawgs() { m_inst.drawgs(); }
    void damageinit();
    void redrawbuts();
    void drawflapbut();
    void drawautobut();
    void drawcmpltbut();
    void drawabomb(int number);
    void flashalt();
    void flasheject();
    void flashstall();

    // ---- sound (ADL.ASM does the chip work) ----
    void voiceon(u8 voice, u16 ax) { if (m_snd) m_snd->voiceon(voice, ax); }
    void voiceoff(u8 voice, u16 ax) { if (m_snd) m_snd->voiceoff(voice, ax); }
    void adlwrt(u8 reg, u8 value) { if (m_snd) m_snd->adlwrt(reg, value); }
    void orbAndSaucerSound();
    void klaSound();
    void engineSound();
    void makeboom();
    void bigboom();
    void soundToggle();

    // ---- hooks owned by later phases ----
    void bulstrt() {}                         // Phase 5
    bool strtmis() { return true; }           // Phase 5 (true = failed)
    void strtbomb() {}                        // Phase 5
    void expmovFull() {}                      // Phase 5: movement of the timed objects
    void bulmov() {}                          // Phase 5
    void initnumbers() {}                     // Phase 5

    GameState& m_g;
    engine::F3dVec& m_eng;
    World& m_world;
    DosKeyboard& m_kbd;
    TickClock& m_clock;
    Instruments& m_inst;
    Sound* m_snd = nullptr;
    sas::Random m_rand;
    std::function<void(bool)> m_viewChanged;
    std::function<void(int)> m_showImage;
    u16 m_holdColors = 0;
    bool m_padActive = false;
    float m_padRoll = 0, m_padPitch = 0, m_padRudder = 0;

    static constexpr int GNPTS = engine::F3dVec::GNPTS;
    bool m_done = false;
    bool m_paused = false;
};

} // namespace corncob::game
