#include "ui_tetra_detect.hpp"
#include "baseband_api.hpp"
#include "oversample.hpp"
#include "portapack.hpp"

using namespace portapack;


namespace ui::external_app::tetra_detect {

// ════════════════════════════════════════════════
//  Radio init  (same pattern as detector_rx)
// ════════════════════════════════════════════════
size_t TetraDetectView::init_radio() {
    audio::output::stop();
    receiver_model.disable();
    baseband::shutdown();

    baseband::run_image(portapack::spi_flash::image_tag_capture);
    receiver_model.set_modulation(ReceiverModel::Mode::Capture);
    baseband::set_sample_rate(TETRA_BW, get_oversample_rate(TETRA_BW));

    auto sr = get_actual_sample_rate(TETRA_BW);
    receiver_model.set_sampling_rate(sr);
    receiver_model.set_baseband_bandwidth(filter_bandwidth_for_sampling_rate(sr));

    audio::set_rate(audio::Rate::Hz_24000);
    audio::output::start();
    receiver_model.set_headphone_volume(receiver_model.headphone_volume());
    receiver_model.enable();
    return 0;
}

// ════════════════════════════════════════════════
//  Constructor
// ════════════════════════════════════════════════
TetraDetectView::TetraDetectView(NavigationView& nav)
    : nav_{nav} {
    add_children({
        &labels_gains,
        &field_lna, &field_vga, &field_amp,
        &label_freq, &field_freq,
        &labels_params,
        &field_threshold, &field_persist, &field_gate,
        &text_power, &text_rssi_vals,
        &text_persist_label, &text_persist_val,
        &text_status,
        &rssi_graph, &rssi,
    });

    // restore saved values into widgets
    field_threshold.set_value(threshold_db_);
    field_persist.set_value(persist_target_);
    field_gate.set_value(gate_sec_);
    field_freq.set_value(390'000'000);  // default EU TETRA

    // wire up param changes → member vars
    field_threshold.on_change = [this](int32_t v) { threshold_db_  = v; clear_alert(); };
    field_persist.on_change   = [this](int32_t v) { persist_target_ = v; };
    field_gate.on_change      = [this](int32_t v) { gate_sec_       = v; };

    field_freq.on_change = [this](rf::Frequency f) {
        receiver_model.set_target_frequency(f);
    };

    rssi.set_vertical_rssi(true);
    rssi.set_peak(true, 3000);
    rssi_graph.set_nb_columns(256);

    init_radio();
    receiver_model.set_target_frequency(field_freq.value());
}

// ════════════════════════════════════════════════
//  Destructor
// ════════════════════════════════════════════════
TetraDetectView::~TetraDetectView() {
    receiver_model.disable();
    audio::output::stop();
    baseband::shutdown();
}

void TetraDetectView::focus() {
    field_freq.focus();
}

// ════════════════════════════════════════════════
//  Statistics update  (fires ~30× per second)
// ════════════════════════════════════════════════
void TetraDetectView::on_statistics_update(const ChannelStatistics& statistics) {
    // update RSSI graph
    rssi_graph.add_values(rssi.get_min(), rssi.get_avg(), rssi.get_max(), statistics.max_db);
    rssi.set_db(statistics.max_db);

    text_power.set("Pwr: " + to_string_dec_int(statistics.max_db) + " dB");
    text_rssi_vals.set(
        "RSSI:" +
        to_string_dec_uint(rssi_graph.get_graph_min()) + "/" +
        to_string_dec_uint(rssi_graph.get_graph_avg()) + "/" +
        to_string_dec_uint(rssi_graph.get_graph_max()));

    bool above = (statistics.max_db > threshold_db_);

    // ── persistence filter ────────────────────────
    if (above) {
        persist_count_++;
    } else {
        persist_count_ = 0;
        gating_        = false;
        clear_alert();
    }

    // clamp display
    int32_t display_count = persist_count_;
    if (display_count > persist_target_) display_count = persist_target_;

    text_persist_val.set(
        to_string_dec_int(display_count) + "/" +
        to_string_dec_int(persist_target_));

    if (persist_count_ < persist_target_) {
        if (persist_count_ > 0)
            text_status.set("Candidate " +
                to_string_dec_int(persist_count_) + "/" +
                to_string_dec_int(persist_target_));
        else
            text_status.set("Scanning...");
        return;
    }

    // ── time gate ────────────────────────────────
    uint32_t now = chTimeNow();  // ChibiOS ticks (1 ms each)
    if (!gating_) {
        gating_       = true;
        gate_start_ms_ = now;
    }

    uint32_t elapsed_ms = now - gate_start_ms_;
    uint32_t needed_ms  = (uint32_t)gate_sec_ * 1000;

    text_status.set(
        "Gate: " + to_string_dec_uint(elapsed_ms / 1000) +
        "/" + to_string_dec_int(gate_sec_) + "s");

    if (elapsed_ms >= needed_ms) {
        trigger_alert();
    }
}

// ════════════════════════════════════════════════
//  Frame sync timer  (fires every display frame)
// ════════════════════════════════════════════════
void TetraDetectView::on_timer() {
    // Nothing extra needed — statistics handler drives detection.
    // Could add scan-hop logic here if desired.
    (void)0;
}

// ════════════════════════════════════════════════
//  Alert
// ════════════════════════════════════════════════
void TetraDetectView::trigger_alert() {
    alert_active_ = true;
    text_status.set("*** TETRA DETECTED ***");
    text_status.set_style(Theme::getInstance()->error_dark);

    uint32_t now = chTimeNow();
    if ((now - last_beep_ms_) > 5000) {   // 5 s cooldown between beeps
        last_beep_ms_ = now;
        // 1000 Hz tone, 24 kHz sample rate, 300 ms
        baseband::request_audio_beep(1000, 24000, 300);
    }
}

void TetraDetectView::clear_alert() {
    if (!alert_active_) return;
    alert_active_  = false;
    gating_        = false;
    persist_count_ = 0;
    text_status.set_style(Theme::getInstance()->bg_darkest);
    text_status.set("Scanning...");
}

}  // namespace ui::external_app::tetra_detect