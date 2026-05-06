#pragma once

#include "ui.hpp"
#include "ui_widget.hpp"
#include "ui_navigation.hpp"
#include "baseband_api.hpp"
#include "receiver_model.hpp"
#include "spectrum_painter.hpp"
#include "portapack.hpp"
#include "message.hpp"
#include <cstdint>
#include <array>

namespace ui {

// ── tunable constants ─────────────────────────────────
static constexpr uint32_t TETRA_DEFAULT_FREQ_HZ  = 390'000'000;
static constexpr uint32_t TETRA_SAMPLE_RATE       = 4'000'000;
static constexpr int      TETRA_FFT_SIZE          = 256;
static constexpr float    TETRA_THRESHOLD_DB      = -65.0f;
static constexpr int      TETRA_MIN_BINS          = 3;
static constexpr int      TETRA_PERSIST_FRAMES    = 6;
static constexpr float    TETRA_TIME_GATE_SEC     = 2.0f;
static constexpr float    TETRA_BEEP_COOLDOWN_SEC = 5.0f;

class TetraDetectorView : public View {
public:
    TetraDetectorView(NavigationView& nav);
    ~TetraDetectorView();

    void focus() override;
    std::string title() const override { return "TETRA Detect"; }

    // called by the message dispatcher with fresh spectrum data
    void on_spectrum(const SpectrumStatistics& stats);

private:
    NavigationView& nav_;

    // ── detection state ──────────────────────────────
    int     persist_count_  {0};
    float   gate_elapsed_   {0.0f};
    bool    alert_active_   {false};
    float   beep_cooldown_  {0.0f};
    uint32_t last_tick_ms_  {0};

    // rolling FFT magnitude buffer (dB, shifted so centre = index 128)
    std::array<int8_t, TETRA_FFT_SIZE> spectrum_buf_ {};

    // ── UI widgets ───────────────────────────────────

    // title bar
    Labels lbl_title_ {
        { {0, 0}, "TETRA DETECTOR", Color::green() }
    };

    // frequency display
    Labels lbl_freq_caption_ {
        { {0, 14}, "CF:", Color::light_grey() }
    };
    FrequencyField field_freq_ {
        {24, 14}
    };

    // spectrum bar graph (16 bars across the screen)
    static constexpr int  BAR_COUNT  = 16;
    static constexpr int  BAR_W      = 15;
    static constexpr int  BAR_H_MAX  = 40;
    static constexpr int  BAR_Y_BASE = 70;

    // threshold / persist / gate labels
    Labels lbl_thresh_ {
        { {0, 76}, "THR:", Color::light_grey() }
    };
    NumberField field_thresh_ {
        {28, 76}, 4, {-120, 0}, 1, ' '
    };

    Labels lbl_persist_ {
        { {80, 76}, "PST:", Color::light_grey() }
    };
    NumberField field_persist_ {
        {108, 76}, 2, {1, 20}, 1, ' '
    };

    Labels lbl_gate_ {
        { {140, 76}, "GATE:", Color::light_grey() }
    };
    NumberField field_gate_ {
        {172, 76}, 2, {1, 30}, 1, ' '
    };

    // status line
    Text txt_status_ {
        {0, 90, 240, 16}, "Scanning..."
    };

    // alert banner
    Text txt_alert_ {
        {0, 108, 240, 16}, ""
    };

    // persist progress bar
    Text txt_persist_bar_ {
        {0, 126, 240, 10}, ""
    };

    // button row
    Button btn_start_ {
        {0, 220, 80, 20}, "Start"
    };
    Button btn_stop_ {
        {84, 220, 80, 20}, "Stop"
    };
    Button btn_back_ {
        {168, 220, 72, 20}, "Back"
    };

    bool running_ {false};

    // ── helpers ──────────────────────────────────────
    void start_rx();
    void stop_rx();
    void tick(uint32_t now_ms);
    void draw_spectrum();
    void draw_persist_bar();
    void trigger_alert();
    void clear_alert();
    int  count_hot_bins(float threshold_db) const;

    MessageHandlerRegistration message_handler_spectrum {
        Message::ID::SpectrumStatistics,
        [this](const Message* const p) {
            const auto& msg = *reinterpret_cast<const SpectrumStatisticsMessage*>(p);
            this->on_spectrum(msg.statistics);
        }
    };
};

} // namespace ui