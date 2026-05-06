#include "ui.hpp"
#include "ui_tetra_detect.hpp"
#include "ui_navigation.hpp"
#include "external_app.hpp"

namespace ui::external_app::tetra_detect {
void initialize_app(ui::NavigationView& nav) {
    nav.push<TetraDetectView>();
}
}  // namespace ui::external_app::tetra_detect

extern "C" {
__attribute__((section(".external_app.app_tetra_detect.application_information"), used))
application_information_t _application_information_tetra_detect = {
    /*.memory_location = */ (uint8_t*)0x00000000,
    /*.externalAppEntry = */ ui::external_app::tetra_detect::initialize_app,
    /*.header_version = */ CURRENT_HEADER_VERSION,
    /*.app_version = */ VERSION_MD5,
    /*.app_name = */ "TETRA Det",
    /*.bitmap_data = */ {
        0x00, 0x00, 0xFE, 0x7F, 0x02, 0x40, 0x02, 0x40,
        0xE2, 0x47, 0x22, 0x44, 0x22, 0x44, 0xE2, 0x47,
        0x22, 0x44, 0x22, 0x44, 0xE2, 0x47, 0x02, 0x40,
        0x02, 0x40, 0xFE, 0x7F, 0x00, 0x00, 0x00, 0x00
    },
    /*.icon_color = */ ui::Color::green().v,
    /*.menu_location = */ app_location_t::RX,
    /*.desired_menu_position = */ -1,
    /*.m4_app_tag = */ {'S', 'P', 'E', 'C'},
    /*.m4_app_offset = */ 0x00000000,
};
}