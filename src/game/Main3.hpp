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
//   weapons and enemies: Main3Weapons/Timed/Sites/Flyers.cpp (STRTEXP.ASM)     (Phase 5, done)
//   sound         ADL.ASM    voiceon, adlwrt ...                               (Phase 7)
//   front end     TOWER.EXE  briefings, scores (the end pictures are in ui/MissionEnd)  (Phase 8)
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
#include <vector>

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
    void silence() { voiceoff(0, 0x100); voiceoff(2, 0x10); }   // the engine and the blams fall silent (as in the pause)
    // Sideways walking on foot (the original has none), -1 = left, 1 = right, analog
    void setStrafe(float s) { m_strafe = s; }

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
    void setSound(Sound* s) {
        m_snd = s;
        m_eng.onFlip([this] { qstopblam(); });
    }

    // What 3.ASM does once the loop is over, before the result file is written: the planes lost lose the
    // kill-me-tough ones (they were suicide missions), and the status is patched for them
    void finishResults();
    // 3.ASM at the end of a combat mission: the objects go back into the tiles and the universe is written out,
    // with what was destroyed (the history of the theater)
    std::vector<uint8_t> saveUniverse();
    // The result block the original writes to 3.res (nresbytes = 140 bytes from missionstatus on), which
    // the tower reads to tally the mission
    std::vector<uint8_t> resultBytes() const;
    // The control tower and the boss key: what the F keys and the tower door ask for. The callback shows the screen
    // and, for Info while in the plane, may choose a mission: it fills start with x, y, z and the yaw and returns true.
    enum class TowerKind { Boss, Info, Score, Detail };
    void onTower(std::function<bool(TowerKind kind, bool onFoot, int32_t* start)> f) { m_tower = std::move(f); }
    // Told when the plane crashes: the screen goes red and shakes (about 1.8 seconds)
    void onCrashScreen(std::function<void()> f) { m_crashScreen = std::move(f); }

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
    // dwtadj without the 32-bit overflow of the original (a rate of 3000 in 16.16 times a frame of 400 ticks), rounded
    i32 dwtadjWide(i32 v) const;
    // a per-frame change written for the 20 fps frame, made for the frame that is really being run
    i32 frameScale(i32 v) const;
    // v kept in proportion keep (0..1) per 20 fps frame, for the frame being run: the exponential fall-offs of the original
    i32 decayStep(i32 v, double keep) const;
    u16 dispScale(u16 range) const;
    // the factor itself, as a 1.15 number (for the multiplies the original does)
    i16 keepPow15(i16 keep15) const;
    static constexpr u32 REF_TICKS = 58;   // timer ticks in the original's 20 fps frame
    static i16 uwtadjRef(i16 v);
    u32 m_refAcc = 0;
    mutable u32 m_dither = 0;
    bool m_fineAngles = false;   // short frames: the angles out of the matrix without the 8 unit grid of the atn2 table
    u16 m_refSteps = 1;
    u32 m_refTotal = 0;     // all of them since the start
    bool m_popupDone = false;   // 20 fps frames passed since the last frame
    // The whole part of a 16.16 amount with the fraction carried to the next frame: the original dropped it every frame, which
    // at a short frame was as large as the amount itself
    static i16 takeWhole(i32 value, u16& frac);
    // A 1.15 matrix times a velocity vector, rounded to nearest (the matvmul of the engine floors every product and
    // drops the low bit of every sum, a loss of several units a frame that does not shrink with the frame time)
    void velmul(u16 si, u16 bx, u16 di);

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
    void initperms();
    void interiorview();
    void fullsize();
    void restoreview();
    void recalcmats();
    void dampchute();
    void moveme();
    void moveobs();
    void step(i16 stepsz, i16 side = 0);
    void goodstep(i16 stepsz, i16 side = 0);
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

    // ---- map mode (MAP.ASM, Main3Map.cpp) ----
    void mapRequest();
    void mapStart();
    void mapFrame();
    void mapkey();
    void kinetic(u16 si);
    void pgupanddown(u16 si);
    void getjoy(i16& ax, i16& bx);
    void restoreox();
    void setyah();
    void drawyah();
    void changewallobjs();
    void chgwloop(u16& si, int cx, i16 dx);

    // ---- DEFOBJS.ASM / STRTEXP.ASM routines of objects ----
    bool evaldist(u16 si, u16& dist);
    void placeplane(u16 si);
    void obj5datim(u16 si, u16 di, bool carry);
    void fdest(u16 si, u16 di, bool carry);
    void fdestrdr(u16 si, u16 di, bool carry);
    void gmdest(u16 si, u16 di, bool carry);
    void odest(u16 si, u16 di, bool carry);

    // ---- STRTEXP.ASM: the timed objects and the explosions (Main3Timed.cpp) ----
    bool objmov(u16 si);                          // true = hit the ground
    void expmov();
    bool startPiece(u16 tmpl, u16 src, u16& neu);
    i16 spread(u16 range);
    bool strtflt(u16 src, u16 count);             // true = no room
    bool strtincend(u16 src, u16 count);
    bool strtshard(u16 src, u16 count);
    bool strtpuff(u16 src);
    bool strtboom(u16 src);
    void flttim(u16 si, bool carry);
    void incendtim(u16 si, bool carry);
    void shardtim(u16 si, bool carry);
    void boomtim(u16 si);
    u8 hitStrength(u16 di) const;
    bool fdestcheck(u16 si, u16 di, bool planeKills);
    void qincfdest(u16 di);
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
    void qstopblam();
    void bigboom();
    void soundToggle();

    // ---- hooks owned by later phases ----
    // ---- the enemies and the mission objects (Main3Sites.cpp, Main3Flyers.cpp) ----
    u16 getqtblptr(u32 table, u16 si, u8& quality);   // the entry of a quality table for the object at si
    bool enoughits(u16 si, u16 di);                   // true = destroyed
    void incW(u32 off) { SW(off, static_cast<i16>(W(off) + 1)); }
    bool radarprocess(u16 si);                        // true = too far
    void addaa(u16 si);
    void addball(u16 si);
    i16 getobsyaw();
    i16 getobspit();
    void adjdballv(u16 q);
    void aatim(u16 si, u16 di, bool carry);
    void sauceraa(u16 si);
    void aashelltim(u16 si, bool carry);
    void dballtim(u16 si, bool carry);
    bool strtdball(u16 src);
    bool strtvech(u16 src);
    void killvech(u16 si);
    void resetvech(u16 si);
    void followangles(u16 si);
    void vechtim(u16 si, u16 di, bool carry);
    void vesttim(u16 si, u16 di, bool carry);
    void dbsttim(u16 si, u16 di, bool carry);
    void resetvest(u16 si);
    void resetdbst(u16 si);
    void bbtim(u16 si, bool carry);
    void bbsmtim(u16 si, bool carry);
    void mortim(u16 si, u16 di, bool carry);
    void walltim(u16 si, bool carry);
    void lwalltim(u16 si, bool carry);
    void initnumbers();
    void bigdest(u16 si, u16 di, bool carry);
    void dhqtim(u16 si);
    void hqtim(u16 si);
    void hqtim1(u16 si);
    void attractplane(u16 si);
    void incondest(u16 si);
    void randspin(u16 si);
    void shelltim(u16 si, bool carry);
    void mandest(u16 si, u16 di, bool carry);
    void huttim(u16 si, bool carry);
    bool strtbmr(u16 src);
    void bmrtim(u16 si, u16 di, bool carry);
    void bmrstim(u16 si, u16 di, bool carry);
    void bmrmodes(u16 si, u16 q, int mode);
    void startvector(u16 si);
    void shootshells(u16 si);
    void stettim(u16 si, u16 di, bool carry);
    void tettim(u16 si, u16 di, bool carry);
    void altim(u16 si, u16 di, bool carry);
    void xptim(u16 si, u16 di, bool carry);
    void qxpdamage(i16 bx);
    i16 hoparound(i16 ax, u16 bx);
    void rescuestart();

    // ---- the weapons (Main3Weapons.cpp) ----
    bool getobsvec(u16 si, bool playerOnly = false);   // true = the observer is too far
    bool findbul(u16& si);
    void bulstrt();
    void bobjmov(u16 si);
    void bulmov();
    void bdest(u16 si, u16 di, bool carry);
    bool strtmis();                           // true = no room
    void mistim(u16 si, bool carry);
    bool strtbomb();
    bool strtbomb1(u16 src);
    void bombtim(u16 si, bool carry);
    void crtrtim(u16 si, bool carry);

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
    std::function<void()> m_crashScreen;
    std::function<bool(TowerKind, bool, int32_t*)> m_tower;
    void dotower();
    u16 m_holdColors = 0;
    bool m_padActive = false;
    float m_padRoll = 0, m_padPitch = 0, m_padRudder = 0;

    static constexpr int GNPTS = engine::F3dVec::GNPTS;
    bool m_done = false;
    bool m_paused = false;
    float m_strafe = 0;
    u16 m_velFrac[3] = {0, 0, 0};   // fractions of a velocity unit carried between frames (the plane frame)
    u16 m_angFrac[3] = {0, 0, 0};   // fractions of an angle unit: pitch, yaw, roll
    i32 m_posFrac[3] = {0, 0, 0};   // the position bits shifted out each frame by moveme
    u16 m_thrFrac = 0;              // the fraction of a throttle step (a held key moves it by a step per 20 fps frame)
    bool m_mapActive = false;    // maploop is running
    bool m_mapCockpit = false;   // it was started from the cockpit, which has to come back
};

} // namespace corncob::game
