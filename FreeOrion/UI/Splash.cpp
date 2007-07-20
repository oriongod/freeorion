#include "Splash.h"

#include "ClientUI.h"
#include "../util/MultiplayerCommon.h"

#include <GG/GUI.h>
#include <GG/StaticGraphic.h>


void LoadSplashGraphics(std::vector<std::vector<GG::StaticGraphic*> >& graphics)
{
    const int IMAGE_CELLS_X = 3;
    const int IMAGE_CELLS_Y = 2;
    int total_width = 0;
    int total_height = 0;
    std::vector<std::vector<boost::shared_ptr<GG::Texture> > > textures(IMAGE_CELLS_Y, std::vector<boost::shared_ptr<GG::Texture> >(IMAGE_CELLS_X));
    for (int y = 0; y < IMAGE_CELLS_Y; ++y) {
        for (int x = 0; x < IMAGE_CELLS_X; ++x) {
            textures[y][x] = ClientUI::GetTexture(ClientUI::ArtDir() / ("splash" + boost::lexical_cast<std::string>(y) + boost::lexical_cast<std::string>(x) + ".png"));
            if (!y)
                total_width += textures[0][x]->DefaultWidth();
        }
        total_height += textures[y][0]->DefaultHeight();
    }
    double x_scale_factor = GG::GUI::GetGUI()->AppWidth() / static_cast<double>(total_width);
    double y_scale_factor = GG::GUI::GetGUI()->AppHeight() / static_cast<double>(total_height);

    int graphic_position_x = 0;
    int graphic_position_y = 0;
    graphics.resize(IMAGE_CELLS_Y, std::vector<GG::StaticGraphic*>(IMAGE_CELLS_X));
    for (int y = 0; y < IMAGE_CELLS_Y; ++y) {
        int height = static_cast<int>(textures[y][0]->DefaultHeight() * y_scale_factor);
        graphic_position_x = 0;
        for (int x = 0; x < IMAGE_CELLS_X; ++x) {
            int width = static_cast<int>(textures[0][x]->DefaultWidth() * x_scale_factor);
            graphics[y][x] = new GG::StaticGraphic(graphic_position_x, graphic_position_y, width, height, textures[y][x], GG::GR_FITGRAPHIC);
            GG::GUI::GetGUI()->Register(graphics[y][x]);
            graphic_position_x += width;
        }
        graphic_position_y += height;
    }
}
