// theaterstats - counts the objects of each template type in the tiles of a theater (dev tool)
#define SDL_MAIN_HANDLED
#include "DGroup.hpp"
#include "game/GameState.hpp"
#include "game/World.hpp"
#include "render/EgaScreen.hpp"
#include "engine/F3dVec.hpp"
#include <cstdio>
#include <map>
#include <string>

using namespace corncob;
namespace d = corncob::dgroup;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: theaterstats <file.TWR> <file.CCT>\n");
        return 1;
    }
    GameState gs;
    EgaScreen scr(1);
    engine::F3dVec eng(gs, scr);
    game::World world(gs, eng);
    world.cleanexp();
    if (!world.readunivfile(argv[1], argv[2])) {
        std::printf("can't read theater\n");
        return 1;
    }
    std::map<int, int> count;
    for (int ty = 0; ty < game::World::NTILESY; ty++)
        for (int tx = 0; tx < game::World::NTILESX; tx++) {
            gs.setW(d::f3dvec::curtilex, static_cast<int16_t>(tx));
            gs.setW(d::f3dvec::curtiley, static_cast<int16_t>(ty));
            const uint16_t seg = world.gettileadr();
            for (int k = 0; k < game::World::NOBJTILE; k++) {
                if (world.decodeobj(seg, static_cast<uint16_t>(k * game::World::NBYTEOBJ), static_cast<uint16_t>(d::f3dvec::memobjbuf))) break;
                count[gs.w(d::f3dvec::memobjbuf + 72)]++;
            }
        }
    for (auto& [idx, n] : count) std::printf("%d %d\n", idx, n);
    return 0;
}
