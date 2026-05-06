#include "ui_tetra_detector.hpp"
#include "audio.hpp"
#include "tone_key.hpp"
#include "portapack.hpp"
#include "portapack_hal.hpp"
#include "baseband_api.hpp"
#include "string_format.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace portapack;

namespace ui {

// ════════════════════════════════════════════════════
//  Constructor / Destructor
// ════════════════════════════════════════════════════
TetraDetectorView::TetraDetectorView(NavigationView& nav)
    : nav_(nav)
{
    add_children({
        &lbl_title_,
        &lbl_freq_caption_,
        &field_freq_,
        &lbl_thresh_,
        &field_thresh_,
        &lbl_persist_,
        &field_persist_,
        &lbl_gate_,
        &field_gate_,
        &txt_status_,
        &txt_alert_,
        &txt_persist_bar_,
        &btn_start_,
        &btn_stop_,
        &btn_back_,
    });

    // defaults
    field_freq_.set_value(TETRA_DEFAULT_FREQ_HZ);
    field_thresh_.set_value((int)TETRA_THRESHOLD_DB);
    field_persist_.set_value(TETRA_PERSIST_FRAMES);
    field_gate_.set_value((int)TETRA_TIME_GATE_SEC);

    btn_start_.on_select = [this](Button&) { start_rx(); };
    btn_stop_.on_select  = [this](Button&) { stop_rx();  };
    btn_back_.on_select  = [this](Button&) {
        stop_rx();
        nav_.pop();
    };

    field_freq_.on_change = [this](rf::Frequency f) {
        if (running_)
            receiver_model.set_target_frequency(f);
    };
}

TetraDetectorView::~TetraDetectorView() {
    stop_rx();
}

void TetraDetectorView::focus() {
    btn_start_.focus();
}

// ════════════════════════════════════════════════════
//  RX control
// ════════════════════════════════════════════════════
void TetraDetectorView::start_rx() {
    if (running_) return;
    running_ = true;
    persist_count_ = 0;
    gate_elapsed_  = 0.0f;
    alert_active_  = false;
    beep_cooldown_ = 0.0f;
    last_tick_ms_  = portapack::system_clock.now_ms();

    receiver_model.set_target_frequency(field_freq_.value());
    receiver_model.set_sampling_rate(TETRA_SAMPLE_RATE);
    receiver_model.set_baseband_bandwidth(TETRA_SAMPLE_RATE / 2);
    receiver_model.set_modulation(ReceiverModel::Mode::SpectrumAnalysis);
    receiver_model.enable();

    baseband::set_spectrum(TETRA_FFT_SIZE, 1);  // fft_size, trigger every frame

    txt_status_.set("Running...");
    txt_alert_.set("");
}

void TetraDetectorView::stop_rx() {
    if (!running_) return;
    running_ = false;
    receiver_model.disable();
    audio::output::stop();
    txt_status_.set("Stopped.");
    txt_alert_.set("");
    clear_alert();
}

// ════════════════════════════════════════════════════
//  Spectrum message handler
// ════════════════════════════════════════════════════
void TetraDetectorView::on_spectrum(const SpectrumStatistics& stats) {
    if (!running_) return;

    // copy magnitudes into our buffer (int8 dBFS, -128..0)
    std::memcpy(spectrum_buf_.data(), stats.db, TETRA_FFT_SIZE);

    uint32_t now_ms = portapack::system_clock.now_ms();
    tick(now_ms);
    draw_spectrum();
    draw_persist_bar();
}

// ════════════════════════════════════════════════════
//  Detection logic  (persistence + time gate)
// ════════════════════════════════════════════════════
void TetraDetectorView::tick(uint32_t now_ms) {
    float dt = (now_ms - last_tick_ms_) / 1000.0f;
    last_tick_ms_ = now_ms;
    if (beep_cooldown_ > 0.0f) beep_cooldown_ -= dt;

    float threshold = (float)field_thresh_.value();
    int   min_bins  = TETRA_MIN_BINS;
    int   persist   = field_persist_.value();
    float gate      = (float)field_gate_.value();

    int hot = count_hot_bins(threshold);
    bool detected = (hot >= min_bins);

    // ── persistence filter ───────────────────────────
    if (detected) {
        persist_count_++;
    } else {
        persist_count_ = 0;
        gate_elapsed_  = 0.0f;
        clear_alert();
        txt_status_.set(
            "Scanning... hot:" + to_string_dec_int(hot)
        );
        return;
    }

    if (persist_count_ < persist) {
        gate_elapsed_ = 0.0f;
        txt_status_.set(
            "Candidate... " + to_string_dec_int(persist_count_)
            + "/" + to_string_dec_int(persist)
        );
        return;
    }

    // ── time gate ────────────────────────────────────
    gate_elapsed_ += dt;
    txt_status_.set(
        "Gate: " + to_string_dec_int((int)(gate_elapsed_ * 10) / 10)
        + "/" + to_string_dec_int((int)gate) + "s  bins:"
        + to_string_dec_int(hot)
    );

    if (gate_elapsed_ >= gate) {
        trigger_alert();
    }
}

int TetraDetectorView::count_hot_bins(float threshold_db) const {
    int count = 0;
    for (int i = 0; i < TETRA_FFT_SIZE; i++) {
        if ((float)spectrum_buf_[i] > threshold_db)
            count++;
    }
    return count;
}

// ════════════════════════════════════════════════════
//  Alert
// ════════════════════════════════════════════════════
void TetraDetectorView::trigger_alert() {
    alert_active_ = true;
    txt_alert_.set("*** TETRA DETECTED ***");

    if (beep_cooldown_ <= 0.0f) {
        beep_cooldown_ = TETRA_BEEP_COOLDOWN_SEC;
        audio::output::start();
        tone_key::set(1000, 300);   // 1000 Hz, 300 ms
    }
}

void TetraDetectorView::clear_alert() {
    if (!alert_active_) return;
    alert_active_ = false;
    txt_alert_.set("");
    audio::output::stop();
}

// ════════════════════════════════════════════════════
//  Drawing
// ════════════════════════════════════════════════════
void TetraDetectorView::draw_spectrum() {
    // collapse 256 FFT bins into BAR_COUNT bars
    int bins_per_bar = TETRA_FFT_SIZE / BAR_COUNT;
    float threshold  = (float)field_thresh_.value();

    Painter painter;
    int x = 0;
    for (int b = 0; b < BAR_COUNT; b++) {
        float sum = 0;
        for (int i = 0; i < bins_per_bar; i++)
            sum += spectrum_buf_[b * bins_per_bar + i];
        float a