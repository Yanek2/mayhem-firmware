#pragma once

#include "ui.hpp"
#include "ui_widget.hpp"
#include "ui_receiver.hpp"
#include "ui_spectrum.hpp"
#include "receiver_model.hpp"
#include "baseband_api.hpp"
#include "radio_state.hpp"
#include "app_settings.hpp"
#include "string_format.hpp"
#include "audio.hpp"

namespace ui::external_app::tetra_detect {

#define TETRA_BW 750000

class TetraDetectView : public View {
   public:
    TetraDetectView(NavigationView& nav);
    ~TetraDetectView();
    void focus() override;
    std::string title() const override { return "TETRA Det"; };

   private:
    NavigationView& nav_;
    RxRadioState radio_state_{};

    // detection state
    int32_t  persist_count_{0};
    uint32_t gate_start_ms_{0};
    bool     gating_{false};
    bool     alert_active_{false};
    uint32_t last_beep_ms_{0};

    // settings (saved/restored)
    int32_t threshold_db_{-65};
    int32_t persist_target_{6};
    int32_t gate_sec_{2};

    app_settings::SettingsManager settings_{
        "rx_tetra_detect",
        app_settings::Mode::RX,
        {
            {"threshold"sv, &threshold_db_},
            {"persist"sv,   &persist_target_},
            {"gate"sv,      &gate_sec_},
        }};

    void on_statistics_update(const ChannelStatistics& statistics);
    void on_timer();
    void trigger_alert();
    void clear_alert();
    size_t init_radio();

    // ── Row 0: gains ────────────────────────────────
    Labels labels_gains{
        {{UI_POS_X(0),  UI_POS_Y(0)}, "LNA:   VGA:   AMP:", Theme::getInstance()->fg_light->foreground},
    };
    LNAGainField field_lna{{UI_POS_X(4),  UI_POS_Y(0)}};
    VGAGainField field_vga{{UI_POS_X(11), UI_POS_Y(0)}};
    RFAmpField   field_amp{{UI_POS_X(19), UI_POS_Y(0)}};

    // ── Row 1: frequency ────────────────────────────
    Labels label_freq{
        {{UI_POS_X(0), UI_POS_Y(1)}, "Freq:", Theme::getInstance()->fg_light->foreground},
    };
    FrequencyField field_freq{{UI_POS_X(5), UI_POS_Y(1)}};

    // ── Row 2: threshold / persist / gate ───────────
    Labels labels_params{
        {{UI_POS_X(0),  UI_POS_Y(2)}, "THR:",  Theme::getInstance()->fg_light->foreground},
        {{UI_POS_X(9),  UI_POS_Y(2)}, "PST:",  Theme::getInstance()->fg_light->foreground},
        {{UI_POS_X(18), UI_POS_Y(2)}, "GATE:", Theme::getInstance()->fg_light->foreground},
    };
    NumberField field_threshold{
        {UI_POS_X(4), UI_POS_Y(2)}, 4, {-120, 0}, 1, ' '};
    NumberField field_persist{
        {UI_POS_X(13), UI_POS_Y(2)}, 2, {1, 20}, 1, ' '};
    NumberField field_gate{
        {UI_POS_X(23), UI_POS_Y(2)}, 2, {1, 30}, 1, ' '};

    // ── Row 3: power + rssi readout ─────────────────
    Text text_power{
        {UI_POS_X(0), UI_POS_Y(3), UI_POS_WIDTH(15), UI_POS_DEFAULT_HEIGHT}, ""};
    Text text_rssi_vals{
        {UI_POS_X(15), UI_POS_Y(3), UI_POS_WIDTH(15), UI_POS_DEFAULT_HEIGHT}, ""};

    // ── Row 4: persist bar label ─────────────────────
    Text text_persist_label{
        {UI_POS_X(0), UI_POS_Y(4), UI_POS_WIDTH(8), UI_POS_DEFAULT_HEIGHT}, "Persist:"};
    Text text_persist_val{
        {UI_POS_X(8), UI_POS_Y(4), UI_POS_WIDTH(10), UI_POS_DEFAULT_HEIGHT}, "0/6"};

    // ── Row 5: status / alert ────────────────────────
    Text text_status{
        {UI_POS_X(0), UI_POS_Y(5), UI_POS_WIDTH(30), UI_POS_DEFAULT_HEIGHT}, "Scanning..."};

    // ── Rows 6+: RSSI graph ──────────────────────────
    RSSIGraph rssi_graph{
        {UI_POS_X(0), UI_POS_Y(6), UI_POS_WIDTH_REMAINING(5), UI_POS_HEIGHT_REMAINING(5)}};
    RSSI rssi{
        {UI_POS_X_RIGHT(5), UI_POS_Y(6), UI_POS_WIDTH(5), UI_POS_HEIGHT_REMAINING(5)}};

    MessageHandlerRegistration message_handler_stats{
        Message::ID::ChannelStatistics,
        [this](const Message* const p) {
            on_statistics_update(
                static_cast<const ChannelStatisticsMessage*>(p)->statistics);
        }};

    MessageHandlerRegistration message_handler_frame_sync{
        Message::ID::DisplayFrameSync,
        [this](const Message* const) { on_timer(); }};
};

}  // namespace ui::external_app::tetra_detect